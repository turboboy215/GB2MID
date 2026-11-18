/*Xantera*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include "SHARED.H"
#include "XANTERA.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long songBank;
long songPtr;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
long bankAmt;
int exitError;
int curTrack;
int curInst;
int drvVers;
int curVol;
int curVols[4];

char string1[100];
char string2[100];
char XanteracheckStrings[3][100] = { "numSongs=", "bank=", "offset=" };
unsigned char* romData;
unsigned char* exRomData;
unsigned char* cfgData;
unsigned char* midData;
unsigned char* multiMidData[4];
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);

void Xanterasong2mid(int songNum, long ptr);

int gbFreq2Note(unsigned int freq);

void XanteraProc(int bank, char parameters[4][100])
{
	curInst = 0;
	drvVers = XANTERA_VER_STD;
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, XanteracheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 4, cfg);
		numSongs = strtod(string1, NULL);
		printf("Total # of songs: %i\n", numSongs);

		/*Skip new line*/
		fgets(string1, 2, cfg);

		songNum = 1;

		while (songNum <= numSongs && exitError == 0)
		{
			/*Skip new line*/
			fgets(string1, 2, cfg);
			/*Skip the first line*/
			fgets(string1, 10, cfg);

			/*Now look for the "bank"*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, XanteracheckStrings[1], 1))
			{
				exitError = 1;
			}

			fgets(string1, 5, cfg);
			songBank = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 2, cfg);

			/*Now look for the "offset"*/
			fgets(string1, 8, cfg);
			if (memcmp(string1, XanteracheckStrings[2], 1))
			{
				exitError = 1;
			}

			fgets(string1, 5, cfg);
			songPtr = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 2, cfg);

			printf("Song %i: Bank %04X, Offset 0x%04X\n", songNum, songBank, songPtr);

			/*Copy the current bank data from the ROM*/
			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);

			Xanterasong2mid(songNum, songPtr);
			free(romData);
			songNum++;
		}
		fclose(cfg);
	}

}

/*Noise instrument table (NR41, NR42 low nibble, NR43, NR44) - identical in
  every known game using the driver*/
static const unsigned char XanNoiseTable[10][4] =
{
	{ 0x00, 0x01, 0x51, 0xC0 },
	{ 0x00, 0x02, 0x22, 0xC0 },
	{ 0x00, 0x02, 0x22, 0xC7 },
	{ 0x3D, 0x04, 0x00, 0xC0 },
	{ 0x36, 0x04, 0x00, 0xC0 },
	{ 0x31, 0x04, 0x01, 0xC0 },
	{ 0x37, 0x04, 0x5A, 0xC0 },
	{ 0x00, 0x04, 0xA0, 0xC0 },
	{ 0x00, 0x02, 0x20, 0xC0 },
	{ 0x00, 0x09, 0x20, 0xC0 }
};

/*GM drum note for each noise instrument (index 10-15 read past the table in
  the driver, so they get a generic note)*/
static const unsigned char XanDrumMap[16] =
{
	36, 40, 40, 42, 44, 46, 37, 35, 38, 49, 45, 45, 45, 45, 45, 45
};

/*MIDI ticks per frame: 120 ticks per beat at 150 BPM = 5 ticks per 1/60 s*/
#define FRAME_TICKS 5

/*Length of an envelope decay in frames (0 = sustained)*/
int XanEnvLen(int env)
{
	int vol = env >> 4;
	int pace = env & 7;

	if ((env & 0x08) || pace == 0)
	{
		return 0;
	}

	/*Each step is pace/64 s*/
	return ((vol * pace * 60) + 63) / 64;
}

/*Close the note playing on a channel at frame "endTime"*/
void XanNoteOff(int chan, long endTime, int* onOff, int* curNotes, int* noteStarts, int* maxLens, int* noteVels, int* curTimes, int* firstNotes, unsigned int* midPosM)
{
	long len;
	long delay;

	if (onOff[chan] == 0)
	{
		return;
	}

	len = endTime - noteStarts[chan];
	if (maxLens[chan] > 0 && len > maxLens[chan])
	{
		len = maxLens[chan];
	}

	if (len > 0)
	{
		delay = noteStarts[chan] - curTimes[chan];
		curVol = noteVels[chan];
		midPosM[chan] = WriteNoteEvent(multiMidData[chan], midPosM[chan], curNotes[chan], len * FRAME_TICKS, delay * FRAME_TICKS, firstNotes[chan], chan, curInst);
		firstNotes[chan] = 0;
		curTimes[chan] = noteStarts[chan] + len;
	}

	onOff[chan] = 0;
}

/*Convert the song data to MIDI*/
void Xanterasong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int curTempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNotes[4];
	int noteStarts[4];
	int maxLens[4];
	int noteVels[4];
	int onOff[4];
	long totalTime = 0;
	unsigned char command[6];
	unsigned char lowNibble = 0;
	int firstNotes[4];
	unsigned int midPosM[4];
	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;
	long trackSize = 0;
	long ctrlTrackSize = 0;
	int curTimes[4];
	int env = 0;
	int period = 0;
	int ch3Vol = 0x20;
	int nameLen = 0;
	int noiseInst = 0;
	int noiseLen = 0;
	int evtCnt = 0;

	midLength = 0x10000;
	for (j = 0; j < 4; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < 4; k++)
		{
			multiMidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
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

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / curTempo);
		ctrlMidPos += 3;

		/*Set time signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5804);
		ctrlMidPos += 3;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x04021808);
		ctrlMidPos += 4;

		/*Set key signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5902);
		ctrlMidPos += 5;

		/*End of control track*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 3;

		ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE32(&ctrlMidData[ctrlMidTrackBase - 4], ctrlTrackSize);

		/*Anything not listed below ends the song*/
		for (j = 0; j < 256; j++)
		{
			EventMap[j] = XANTERA_EVENT_STOP;
		}

		switch (drvVers)
		{
		case XANTERA_VER_STD:
			/*Fall-through*/
		default:
			EventMap[0x10] = XANTERA_EVENT_CH1_NOTE;
			EventMap[0x11] = XANTERA_EVENT_CH2_NOTE;
			EventMap[0x12] = XANTERA_EVENT_CH3_NOTE;
			EventMap[0x13] = XANTERA_EVENT_CH4_NOTE;
			for (j = 0x20; j <= 0x2F; j++)
			{
				EventMap[j] = XANTERA_EVENT_CH3_OFF;
			}
			for (j = 0x30; j <= 0x3F; j++)
			{
				EventMap[j] = XANTERA_EVENT_CH3_VOL;
			}
			for (j = 0x40; j <= 0x4F; j++)
			{
				EventMap[j] = XANTERA_EVENT_CH3_WAVEFORM;
			}
			for (j = 0x50; j <= 0x52; j++)
			{
				EventMap[j] = XANTERA_EVENT_PITCH_MOD;
			}
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			firstNotes[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;

			/*Set track name*/
			nameLen = strlen(TRK_NAMES[curTrack]);
			WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
			midPosM[curTrack]++;
			WriteBE16(&multiMidData[curTrack][midPosM[curTrack]], 0xFF03);
			Write8B(&multiMidData[curTrack][midPosM[curTrack] + 2], nameLen);
			midPosM[curTrack] += 3;
			memcpy(&multiMidData[curTrack][midPosM[curTrack]], TRK_NAMES[curTrack], nameLen);
			midPosM[curTrack] += nameLen;

			onOff[curTrack] = 0;
			curTimes[curTrack] = 0;
			curNotes[curTrack] = 0;
			noteStarts[curTrack] = 0;
			maxLens[curTrack] = 0;
			noteVels[curTrack] = 100;
		}

		seqPos = ptr;
		seqEnd = 0;
		totalTime = 0;

		/*Each event: time (big-endian, frames since the start) | command | data*/
		while (seqEnd == 0 && seqPos < (bankSize * 2 - 6) && midPosM[0] < 0xF000 && midPosM[1] < 0xF000 && midPosM[2] < 0xF000 && midPosM[3] < 0xF000)
		{
			command[0] = romData[seqPos];
			command[1] = romData[seqPos + 1];
			command[2] = romData[seqPos + 2];
			command[3] = romData[seqPos + 3];
			command[4] = romData[seqPos + 4];
			command[5] = romData[seqPos + 5];
			lowNibble = (command[2] & 0x0F);

			if (ReadBE16(&romData[seqPos]) < totalTime || ReadBE16(&romData[seqPos]) == 0xFFFF)
			{
				seqEnd = 1;
			}
			else
			{
				totalTime = ReadBE16(&romData[seqPos]);

				/*Square 1/2 note: env, period hi, period lo*/
				if (EventMap[command[2]] == XANTERA_EVENT_CH1_NOTE || EventMap[command[2]] == XANTERA_EVENT_CH2_NOTE)
				{
					curTrack = lowNibble;
					env = command[3];
					period = ((command[4] & 7) << 8) | command[5];
					XanNoteOff(curTrack, totalTime, onOff, curNotes, noteStarts, maxLens, noteVels, curTimes, firstNotes, midPosM);

					/*Volume 0 with a decreasing envelope turns the DAC off*/
					if ((env & 0xF8) != 0)
					{
						curNotes[curTrack] = gbFreq2Note(period);
						noteStarts[curTrack] = totalTime;
						maxLens[curTrack] = XanEnvLen(env);
						noteVels[curTrack] = ((env >> 4) * 127) / 15;
						if (noteVels[curTrack] == 0)
						{
							noteVels[curTrack] = 60;
						}
						onOff[curTrack] = 1;
					}
					seqPos += 6;
				}

				/*Wave note: vol | period hi, period lo*/
				else if (EventMap[command[2]] == XANTERA_EVENT_CH3_NOTE)
				{
					curTrack = 2;
					period = ((command[3] & 7) << 8) | command[4];
					ch3Vol = command[3] & 0x60;
					XanNoteOff(curTrack, totalTime, onOff, curNotes, noteStarts, maxLens, noteVels, curTimes, firstNotes, midPosM);

					if (ch3Vol != 0)
					{
						/*The wave channel sounds an octave lower*/
						curNotes[curTrack] = gbFreq2Note(period) - 12;
						if (curNotes[curTrack] < 0)
						{
							curNotes[curTrack] = 0;
						}
						noteStarts[curTrack] = totalTime;
						maxLens[curTrack] = 0;
						noteVels[curTrack] = (ch3Vol == 0x20) ? 127 : (ch3Vol == 0x40) ? 80 : 40;
						onOff[curTrack] = 1;
					}
					seqPos += 5;
				}

				/*Noise note: vol << 4 | instrument*/
				else if (EventMap[command[2]] == XANTERA_EVENT_CH4_NOTE)
				{
					curTrack = 3;
					noiseInst = command[3] & 0x0F;
					XanNoteOff(curTrack, totalTime, onOff, curNotes, noteStarts, maxLens, noteVels, curTimes, firstNotes, midPosM);

					if ((command[3] & 0xF0) != 0)
					{
						curNotes[curTrack] = XanDrumMap[noiseInst];
						noteStarts[curTrack] = totalTime;
						noteVels[curTrack] = ((command[3] >> 4) * 127) / 15;

						/*Length: envelope decay and/or length counter*/
						noiseLen = 8;
						if (noiseInst < 10)
						{
							noiseLen = XanEnvLen((command[3] & 0xF0) | XanNoiseTable[noiseInst][1]);
							if (XanNoiseTable[noiseInst][3] & 0x40)
							{
								k = ((64 - (XanNoiseTable[noiseInst][0] & 0x3F)) * 60 + 255) / 256;
								if (noiseLen == 0 || k < noiseLen)
								{
									noiseLen = k;
								}
							}
						}
						if (noiseLen < 1)
						{
							noiseLen = 1;
						}
						maxLens[curTrack] = noiseLen;
						onOff[curTrack] = 1;
					}
					seqPos += 4;
				}

				/*Wave channel off*/
				else if (EventMap[command[2]] == XANTERA_EVENT_CH3_OFF)
				{
					XanNoteOff(2, totalTime, onOff, curNotes, noteStarts, maxLens, noteVels, curTimes, firstNotes, midPosM);
					seqPos += 3;
				}

				/*Wave channel volume (NR32 = n << 4)*/
				else if (EventMap[command[2]] == XANTERA_EVENT_CH3_VOL)
				{
					ch3Vol = (lowNibble << 4) & 0x60;
					if (ch3Vol == 0)
					{
						XanNoteOff(2, totalTime, onOff, curNotes, noteStarts, maxLens, noteVels, curTimes, firstNotes, midPosM);
					}
					seqPos += 3;
				}

				/*Load waveform - no effect on the MIDI*/
				else if (EventMap[command[2]] == XANTERA_EVENT_CH3_WAVEFORM)
				{
					seqPos += 3;
				}

				/*New period without retrigger (lo, hi): continue as a new note*/
				else if (EventMap[command[2]] == XANTERA_EVENT_PITCH_MOD)
				{
					curTrack = lowNibble;
					period = ((command[4] & 7) << 8) | command[3];
					if (onOff[curTrack] == 1)
					{
						k = noteVels[curTrack];
						env = maxLens[curTrack];
						noiseLen = noteStarts[curTrack];
						XanNoteOff(curTrack, totalTime, onOff, curNotes, noteStarts, maxLens, noteVels, curTimes, firstNotes, midPosM);

						/*Note still sounding: start the new pitch for the rest of it*/
						if (env == 0 || (totalTime - noiseLen) < env)
						{
							curNotes[curTrack] = gbFreq2Note(period);
							if (curTrack == 2)
							{
								curNotes[curTrack] -= 12;
							}
							noteStarts[curTrack] = totalTime;
							maxLens[curTrack] = (env == 0) ? 0 : env - (totalTime - noiseLen);
							noteVels[curTrack] = k;
							onOff[curTrack] = 1;
						}
					}
					seqPos += 5;
				}

				/*End of song*/
				else
				{
					seqEnd = 1;
				}

				evtCnt++;
			}
		}

		/*Close any notes still playing*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			XanNoteOff(curTrack, totalTime, onOff, curNotes, noteStarts, maxLens, noteVels, curTimes, firstNotes, midPosM);
		}

		printf("  %i events, %li frames\n", evtCnt, totalTime);

		/*End of tracks*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
			midPosM[curTrack]++;
			WriteBE24(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
			midPosM[curTrack] += 3;

			trackSize = midPosM[curTrack] - 8;
			WriteBE32(&multiMidData[curTrack][4], trackSize);
		}

		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
		}

		for (k = 0; k < 4; k++)
		{
			free(multiMidData[k]);
		}
		free(ctrlMidData);
		fclose(mid);
	}
}