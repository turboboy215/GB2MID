/*Unknown (3D Pocket Pool/3D Pool Stars)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "3DPOOL.H"
#include "WAV.H"

#define bankSize 16384

struct header wavHeader;

/*CGB frame rate: 4194304 / 70224*/
#define FRAME_RATE (4194304.0 / 70224.0)

FILE* rom, * mid, * wav;
long bank;
long offset;
long tableOffset;
int i, j;
char outfile[1000000];
int curInst;
long bankAmt;
long songPtr;
int mode;

char* param1;

int curVol;
int drvVers;
int songNum;
int numSongs;
int phraseNum;
int sampNum;
int numSam;
int freq;
long sampPtr;
long sampLen;
int sampTabBank;
int sampBank;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* multiMidData[4];

unsigned char* ctrlMidData;

/*Raw copy of the instrument table (bank 9)*/
unsigned char instTab[POOL3D_MAX_INST * 5];

long midLength;

/*Per-track note state (track 2 = wave, track 3 = noise)*/
unsigned int midPosM[4];
unsigned int curNotes[4];
int holdNotes[4];
long noteEnds[4];
long lastTimes[4];

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
unsigned long TempoToMicro(int tempo);
double TickSecs(int tempo);
double RateHz(int rate);
void EndNote(int curTrack, long endTime);
void Pool3Dsong2mid(int songNum, long songPtr, int startOrder);
void Pool3Dsam2wav(int sampNum, long ptr, int bank, long len);
unsigned char gb_read_byte(int bank, int cpu);

/*MIDI tempo (microseconds per quarter note) for a driver tempo value.
  A row lasts 896/tempo frames and a quarter note is 4 rows.*/
unsigned long TempoToMicro(int tempo)
{
	return (unsigned long)((4.0 * POOL3D_ROW_UNITS * 1000000.0) / (tempo * FRAME_RATE) + 0.5);
}

/*Length of one MIDI tick in seconds at a driver tempo value*/
double TickSecs(int tempo)
{
	return ((double)POOL3D_ROW_UNITS / tempo / FRAME_RATE) / POOL3D_TICKS_PER_ROW;
}

/*Playback rate of a rate index, from the rate table: 2097152 / (2048 - freq)*/
double RateHz(int rate)
{
	long pos = POOL3D_RATE_TAB + ((rate - 1) * 4);
	int freq = romData[pos] | ((romData[pos + 1] & 0x07) << 8);
	return 2097152.0 / (2048 - freq);
}

/*Stop the note held on a track: at the time it runs out by itself, or at endTime if
  it is still sounding then (a new note, or the end of the song, cuts it off)*/
void EndNote(int curTrack, long endTime)
{
	long offTime;

	if (holdNotes[curTrack] == 1)
	{
		offTime = noteEnds[curTrack];
		if (offTime > endTime)
		{
			offTime = endTime;
		}
		midPosM[curTrack] = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], 0, offTime - lastTimes[curTrack], 0, curTrack, 0);
		lastTimes[curTrack] = offTime;
		holdNotes[curTrack] = 0;
	}
}

void Pool3DProc(int bank, char parameters[4][100])
{
	drvVers = POOL3D_VER_STD;
	curVol = 120;
	curInst = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	bankAmt = bankSize;

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Instrument table (needed for the sample lengths)*/
	memset(instTab, 0, sizeof(instTab));
	fseek(rom, (POOL3D_INST_BANK * bankSize) + (POOL3D_INST_TAB - bankSize), SEEK_SET);
	fread(instTab, 1, sizeof(instTab), rom);

	tableOffset = strtol(parameters[0], NULL, 16);
	mode = POOL3D_MODE_MUS;

	if (parameters[1][0] != 0x00)
	{
		param1 = parameters[1];
		/*Extract music to MIDI*/
		if (strcmp(param1, "m") == 0 || strcmp(param1, "M") == 0)
		{
			mode = POOL3D_MODE_MUS;
		}

		/*Extract samples*/
		else if (strcmp(param1, "s") == 0 || strcmp(param1, "S") == 0)
		{
			mode = POOL3D_MODE_SAMP;
		}
		else
		{
			free(romData);
			fclose(rom);
			printf("ERROR: Invalid mode switch!\n");
			exit(1);
		}
	}

	if (mode == POOL3D_MODE_MUS)
	{
		i = tableOffset;
		songNum = 1;
		numSongs = POOL3D_NUM_SONGS;

		while (songNum <= numSongs)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			i += 2;

			/*Special case: the second song is split into phrases. The game starts
			  it with C = order number and the driver then loops that one order,
			  so each order is converted on its own.*/
			if (songNum == POOL3D_PHRASE_SONG)
			{
				for (phraseNum = 0; phraseNum < romData[songPtr]; phraseNum++)
				{
					printf("  Phrase %i (order %i)\n", phraseNum + 1, phraseNum);
					Pool3Dsong2mid(songNum, songPtr, phraseNum);
				}
			}
			else
			{
				Pool3Dsong2mid(songNum, songPtr, -1);
			}
			songNum++;
		}
	}
	else if (mode == POOL3D_MODE_SAMP)
	{
		/*First, music samples*/
		sampTabBank = 0x0A;
		numSam = 25;
		freq = 8192;
		sampNum = 1;
		i = 0x4000;

		while (sampNum <= numSam)
		{
			sampLen = ReadLE16(&romData[i]);
			sampBank = romData[i + 2] + 1;
			sampPtr = ReadLE16(&romData[i + 3]);

			if (sampLen != 0x0000)
			{
				printf("Sample %i: 0x%04X (bank %02X, length %04X bytes)\n", sampNum, sampPtr, sampBank, sampLen);
				Pool3Dsam2wav(sampNum, sampPtr, sampBank, sampLen);
			}
			else
			{
				printf("Sample %i: 0x%04X (bank %02X, length %04X bytes) (empty, skipped)\n", sampNum, sampPtr, sampBank, sampLen);
			}
			i += 5;
			sampNum++;
		}

		/*Then, sound effects samples*/
		sampTabBank = 0x3D;

		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, ((sampTabBank - 1) * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
		numSam = 31;
		i = 0x4000;

		while (sampNum <= numSam)
		{
			sampLen = ReadLE16(&exRomData[i]);
			sampPtr = ReadLE16(&exRomData[i + 2]);
			sampBank = sampTabBank;

			if (sampLen != 0x0000)
			{
				printf("Sample %i: 0x%04X (bank %02X, length %04X bytes)\n", sampNum, sampPtr, sampBank, sampLen);
				if (sampNum == 30 || sampNum == 31)
				{
					freq = 4096;
				}
				else
				{
					freq = 8192;
				}
				Pool3Dsam2wav(sampNum, sampPtr, sampBank, sampLen);
			}
			else
			{
				printf("Sample %i: 0x%04X (bank %02X, length %04X bytes) (empty, skipped)\n", sampNum, sampPtr, sampBank, sampLen);
			}

			i += 4;
			sampNum++;
		}
	}

	free(romData);
}

/*Convert the song data to MIDI.
  startOrder = -1 plays the order list through once (C = $FF on hardware).
  startOrder >= 0 converts only that order, the way the driver loops it when $CF01 is set.*/
void Pool3Dsong2mid(int songNum, long songPtr, int startOrder)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	int curTrack = 0;
	int seqEnd = 0;
	unsigned int seqPos = 0;
	unsigned int romPos = 0;
	long ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	long ctrlTime = 0;
	int valSize = 0;
	long trackSizes[4];
	long ctrlTrackSize = 0;
	int tempo = POOL3D_DEFAULT_TEMPO;
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int firstNotes[4];
	long seqTime = 0;
	int curInsts[4];
	int numOrders;
	int curOrder;
	int lastOrder;
	int curPat;
	long patTab;
	long patPtr;
	int seqInst = -1;
	int rowNoise = -1;
	int rowLen = 0;
	int rate;
	int volBits;
	int envVol;
	int envPeriod;
	int nr42;
	long sampLen;
	long noteLen;
	double sampSecs;

	romPos = songPtr;
	numOrders = romData[romPos];
	patTab = romPos + 1 + numOrders;

	if (startOrder >= 0)
	{
		curOrder = startOrder;
		lastOrder = startOrder;
	}
	else
	{
		curOrder = 0;
		lastOrder = numOrders - 1;
	}

	midLength = 0x10000;

	for (j = 0; j < trackCnt; j++)
	{
		multiMidData[j] = (unsigned char*)calloc(midLength, 1);
	}

	ctrlMidData = (unsigned char*)calloc(midLength, 1);

	/*Build the event map from the bit patterns of the command byte (decoded LSB-first)*/
	switch (drvVers)
	{
	case POOL3D_VER_STD:
		/*Fall-through*/
	default:
		for (k = 0; k < 256; k++)
		{
			/*vv rrrrr 0 = note*/
			if ((k & 0x01) == 0x00)
			{
				EventMap[k] = POOL3D_EVENT_NOTE;
			}
			/*nnnnnn 11 = wait (n + 1) rows*/
			else if ((k & 0x03) == 0x03)
			{
				EventMap[k] = POOL3D_EVENT_WAIT;
			}
			/*c tttt 101 = noise hit*/
			else if ((k & 0x07) == 0x05)
			{
				EventMap[k] = POOL3D_EVENT_NOISE;
			}
			/*iiiii 001 = set instrument i - 1*/
			else
			{
				EventMap[k] = POOL3D_EVENT_SET_INST;
			}
		}
		EventMap[0x01] = POOL3D_EVENT_NEXT_PAT;
		break;
	}

	/*Get the starting tempo from the first pattern to play*/
	curPat = romData[songPtr + 1 + curOrder];
	patPtr = ReadLE16(&romData[patTab + (curPat * 2)]);
	if (romData[patPtr] != 0x00 && romData[patPtr] != 0xFF)
	{
		tempo = romData[patPtr];
	}

	if (startOrder >= 0)
	{
		sprintf(outfile, "song%d_%d.mid", songNum, startOrder + 1);
	}
	else
	{
		sprintf(outfile, "song%d.mid", songNum);
	}

	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file %s!\n", outfile);
		exit(2);
	}
	else
	{
		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], trackCnt + 1);
		WriteBE16(&ctrlMidData[ctrlMidPos + 4], ticks);
		ctrlMidPos += 6;

		/*Write initial MIDI information for "control" track*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D54726B);
		ctrlMidPos += 8;
		ctrlMidTrackBase = ctrlMidPos;

		/*Set channel name (blank)*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE16(&ctrlMidData[ctrlMidPos], 0xFF03);
		Write8B(&ctrlMidData[ctrlMidPos + 2], 0);
		ctrlMidPos += 3;

		/*Set initial tempo*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 3;

		WriteBE24(&ctrlMidData[ctrlMidPos], TempoToMicro(tempo));
		ctrlMidPos += 3;

		/*Set time signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5804);
		ctrlMidPos += 3;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x04021808);
		ctrlMidPos += 4;

		/*Set key signature (C major)*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5902);
		ctrlMidPos += 3;
		Write8B(&ctrlMidData[ctrlMidPos], 0);
		Write8B(&ctrlMidData[ctrlMidPos + 1], 0);
		ctrlMidPos += 2;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			noteEnds[curTrack] = 0;
			lastTimes[curTrack] = 0;
			curNotes[curTrack] = 0;
			curInsts[curTrack] = -1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			/*Add track header*/
			valSize = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
			midPosM[curTrack] += valSize;
			WriteBE16(&multiMidData[curTrack][midPosM[curTrack]], 0xFF03);
			midPosM[curTrack] += 2;
			Write8B(&multiMidData[curTrack][midPosM[curTrack]], strlen(TRK_NAMES[curTrack]));
			midPosM[curTrack]++;
			sprintf((char*)&multiMidData[curTrack][midPosM[curTrack]], TRK_NAMES[curTrack]);
			midPosM[curTrack] += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		ctrlTime = 0;
		seqTime = 0;

		while (curOrder <= lastOrder)
		{
			curPat = romData[songPtr + 1 + curOrder];
			romPos = patTab + (curPat * 2);
			patPtr = ReadLE16(&romData[romPos]);
			seqPos = patPtr;

			/*Get tempo from first byte of pattern (0 or $FF = keep the current tempo)*/
			if (romData[seqPos] != 0x00 && romData[seqPos] != 0xFF && romData[seqPos] != tempo)
			{
				tempo = romData[seqPos];

				valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, seqTime - ctrlTime);
				ctrlMidPos += valSize;
				WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
				ctrlMidPos += 3;
				WriteBE24(&ctrlMidData[ctrlMidPos], TempoToMicro(tempo));
				ctrlMidPos += 3;
				ctrlTime = seqTime;
			}
			seqPos++;

			/*Loading a pattern resets the instrument ($CF09 = $FF): notes are silent until one is set*/
			seqInst = -1;
			seqEnd = 0;

			while (seqEnd == 0)
			{
				/*Safety: stop at the end of the bank*/
				if (seqPos >= (bankSize * 2))
				{
					seqEnd = 1;
					break;
				}

				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				seqPos++;

				/*Next pattern*/
				if (EventMap[command[0]] == POOL3D_EVENT_NEXT_PAT)
				{
					seqEnd = 1;
				}

				/*Play sample note on the wave channel: vv rrrrr 0*/
				else if (EventMap[command[0]] == POOL3D_EVENT_NOTE)
				{
					rate = (command[0] >> 1) & 0x1F;
					volBits = command[0] >> 6;
					curTrack = 2;

					/*The driver skips the note if no instrument is set; the old sample keeps playing*/
					if (seqInst >= 0 && rate >= POOL3D_RATE_MIN && rate <= POOL3D_RATE_MAX)
					{
						/*A new note restarts the channel*/
						EndNote(curTrack, seqTime);

						curNotes[curTrack] = POOL3D_NOTE_BASE + rate;

						/*Volume LUT: 1 = full, 2 = half, 3 = quarter*/
						if (volBits == 2)
						{
							curVol = 90;
						}
						else if (volBits == 3)
						{
							curVol = 64;
						}
						else
						{
							curVol = 127;
						}

						/*The sample is one-shot: the note lasts until the sample ends
						  (length x 2 nibbles at the note's rate) or the next note*/
						if (seqInst < POOL3D_MAX_INST)
						{
							sampLen = ReadLE16(&instTab[seqInst * 5]);
						}
						else
						{
							sampLen = 0;
						}

						if (sampLen > 0)
						{
							sampSecs = (sampLen * 2.0) / RateHz(rate);
							noteLen = (long)(sampSecs / TickSecs(tempo) + 0.5);
							if (noteLen < 1)
							{
								noteLen = 1;
							}
							noteEnds[curTrack] = seqTime + noteLen;
						}
						else
						{
							noteEnds[curTrack] = LONG_MAX;
						}

						/*Program change on the first note and whenever the instrument changes*/
						if (curInsts[curTrack] != seqInst)
						{
							firstNotes[curTrack] = 1;
							curInsts[curTrack] = seqInst;
						}

						midPosM[curTrack] = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], 0, seqTime - lastTimes[curTrack], firstNotes[curTrack], curTrack, seqInst);
						firstNotes[curTrack] = 0;
						holdNotes[curTrack] = 1;
						lastTimes[curTrack] = seqTime;
					}

					/*A note ends the row*/
					rowLen = 1;
				}

				/*Wait: nnnnnn 11, the row lasts n + 1 rows*/
				else if (EventMap[command[0]] == POOL3D_EVENT_WAIT)
				{
					rowLen = (command[0] >> 2) + 1;
				}

				/*Noise hit: c tttt 101 (c = 1: more events on the same row)*/
				else if (EventMap[command[0]] == POOL3D_EVENT_NOISE)
				{
					/*Several hits on one row are written to the channel in the same frame,
					  so only the last one is heard*/
					rowNoise = (command[0] >> 3) & 0x0F;

					if ((command[0] & 0x80) == 0)
					{
						rowLen = 1;
					}
				}

				/*Set instrument: iiiii 001*/
				else if (EventMap[command[0]] == POOL3D_EVENT_SET_INST)
				{
					seqInst = (command[0] >> 3) - 1;
				}

				/*End of row: write the noise hit and advance time*/
				if (rowLen > 0)
				{
					if (rowNoise >= 0)
					{
						curTrack = 3;
						EndNote(curTrack, seqTime);

						/*NR42: initial volume (bits 4-7) and envelope period (bits 0-2)*/
						nr42 = romData[POOL3D_NOISE_TAB + rowNoise];
						envVol = nr42 >> 4;
						envPeriod = nr42 & 0x07;

						/*Bits 0-1 of the type select the decay length*/
						switch (rowNoise & 0x03)
						{
						case 0:
							curNotes[curTrack] = 38;	/*Long: snare*/
							break;
						case 1:
							curNotes[curTrack] = 42;	/*Shortest: closed hi-hat*/
							break;
						default:
							curNotes[curTrack] = 46;	/*Short: open hi-hat*/
							break;
						}

						curVol = envVol * 16;
						if (curVol > 127)
						{
							curVol = 127;
						}
						if (curVol < 1)
						{
							curVol = 1;
						}

						/*The envelope steps down once every period/64 seconds*/
						noteLen = (long)(((envVol * envPeriod) / 64.0) / TickSecs(tempo) + 0.5);
						if (noteLen < 1)
						{
							noteLen = 1;
						}

						/*Nothing else can happen on this channel before the row ends,
						  so a hit that dies out within the row is written in one go*/
						if (noteLen <= (rowLen * POOL3D_TICKS_PER_ROW))
						{
							midPosM[curTrack] = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], noteLen, seqTime - lastTimes[curTrack], firstNotes[curTrack], curTrack, 0);
							lastTimes[curTrack] = seqTime + noteLen;
							holdNotes[curTrack] = 0;
						}
						else
						{
							midPosM[curTrack] = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], noteLen, seqTime - lastTimes[curTrack], firstNotes[curTrack], curTrack, 0);
							lastTimes[curTrack] = seqTime;
							noteEnds[curTrack] = seqTime + noteLen;
							holdNotes[curTrack] = 1;
						}
						firstNotes[curTrack] = 0;
						rowNoise = -1;
					}

					seqTime += rowLen * POOL3D_TICKS_PER_ROW;
					rowLen = 0;
				}
			}

			curOrder++;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			/*Stop anything still sounding when the song loops*/
			EndNote(curTrack, seqTime);

			/*End of track*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
			midPosM[curTrack] += 4;
			firstNotes[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		/*End of control track (placed at the loop point so the song length is kept)*/
		valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, seqTime - ctrlTime);
		ctrlMidPos += valSize;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 3;

		/*Calculate MIDI channel size*/
		ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			free(multiMidData[curTrack]);
		}
		free(ctrlMidData);
		fclose(mid);
	}

}

/*Convert the sample data to WAV*/
void Pool3Dsam2wav(int sampNum, long ptr, int bank, long len)
{
	char name[64];
	int sampPos = 0;
	int rawPos = 0;
	int k = 0;
	int c = 0;
	int c1 = 0;
	int s = 0;
	int fileSize = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int endSamp;
	int cpu;
	int b;
	int totalLen = 0;
	int bit;

	sprintf(name, "sample%i.wav", sampNum);
	if ((wav = fopen(name, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file sample%i.wav!\n", sampNum);
		exit(2);
	}
	else
	{
		fileSize = 36;

		cpu = ptr;
		b = bank - 1;

		totalLen = 0;

		fseek(wav, 44, SEEK_SET);

		while (totalLen < len)
		{
			c = gb_read_byte(b, cpu);
			/*Convert the 4-bit PCM to 8-bit*/
			lowNibble = c >> 4;
			highNibble = c & 0x0F;
			s = (lowNibble | (lowNibble * 0x10));
			fputc(s, wav);
			fileSize++;
			s = (highNibble | (highNibble * 0x10));
			fputc(s, wav);
			fileSize++;
			cpu++;
			totalLen++;
		}

		/*Fill in the header data*/
		fseek(wav, 0, SEEK_SET);
		memcpy(wavHeader.riffID, "RIFF", 4);
		memcpy(wavHeader.waveID, "WAVE", 4);
		memcpy(wavHeader.fmtID, "fmt ", 4);
		memcpy(wavHeader.dataID, "data", 4);

		wavHeader.fileSize = fileSize;
		wavHeader.blockAlign = 16;
		wavHeader.dataFmt = 1;
		wavHeader.channels = 1;
		wavHeader.sampleRate = freq;
		wavHeader.byteRate = freq * 1 * 8 / 8;
		wavHeader.bytesPerSamp = 1 * 8 / 8;
		wavHeader.bits = 8;
		wavHeader.dataSize = fileSize - 36;

		fwrite(&wavHeader, sizeof(wavHeader), 1, wav);

		fclose(wav);
	}
}