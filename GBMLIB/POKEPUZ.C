/*Intelligent Systems (Pokémon Puzzle Challenge)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <direct.h>
#include "SHARED.H"
#include "POKEPUZ.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long seqPtrLoc;
long seqOffset;
int i, j;
char outfile[1000000];
int songNum;
int songPtr;
int songBank;
long seqPtrs[4];
long songTempo;
long nextPtr;
long endPtr;
long bankAmt;
int curVol;
int curInst;
int drvVers;
int foundTable;
int trackFmt;
int songBase;
int progBank;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
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
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void PokePuzsong2mid(int songNum, long songPtr);

void PokePuzProc(int bank)
{
	drvVers = POKEPUZ_VER_STD;
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

	tableOffset = 0x590E;
	i = tableOffset;
	songNum = 1;

	while (songNum <= 97)
	{
		songPtr = ReadLE16(&romData[i]);
		songBank = romData[i + 2];

		/*Copy data from the song bank*/
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, (songBank * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);

		printf("Song %i: 0x%04X, bank %02X\n", songNum, songPtr, songBank);
		PokePuzsong2mid(songNum, songPtr);
		free(exRomData);
		i += 3;
		songNum++;
	}

	fclose(rom);

}

/*Convert the song data to MIDI*/
void PokePuzsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;
	int repeats[5][2];
	int inMacro;
	int macros[4][2];
	long lenTab;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curVol = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 0;

	int curInst = 0;

	unsigned long seqPos = 0;

	unsigned char command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;

	midPos = 0;
	ctrlMidPos = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
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

		tempo = 150;

		/*Write initial MIDI information for "control" track*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D54726B);
		ctrlMidPos += 8;
		ctrlMidTrackBase = ctrlMidPos;

		/*Set channel name (blank)*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE16(&ctrlMidData[ctrlMidPos], 0xFF03);
		Write8B(&ctrlMidData[ctrlMidPos + 2], 0);
		ctrlMidPos += 2;

		/*Set initial tempo*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
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
		ctrlMidPos += 4;

		switch (drvVers)
		{
		case POKEPUZ_VER_STD:
			/*Fall-through*/
		default:
			POKEPUZ_STATUS_STOP = 0x00;
			POKEPUZ_STATUS_NOTE_LEN_MIN = 0x01;
			POKEPUZ_STATUS_NOTE_LEN_MAX = 0x6C;
			POKEPUZ_STATUS_DRUM_MIN = 0x6D;
			POKEPUZ_STATUS_DRUM_MAX = 0x89;
			POKEPUZ_STATUS_NOTE_MIN = 0x8A;
			POKEPUZ_STATUS_NOTE_MAX = 0xD1;
			POKEPUZ_STATUS_REST = 0xD2;
			POKEPUZ_STATUS_TIE = 0xD3;
			EventMap[0xD4] = POKEPUZ_EVENT_VIBRATO_ON;
			EventMap[0xD5] = POKEPUZ_EVENT_VIBRATO_OFF;
			EventMap[0xD6] = POKEPUZ_EVENT_TEMPO;
			EventMap[0xD7] = POKEPUZ_EVENT_TRANSPOSE;
			EventMap[0xD8] = POKEPUZ_EVENT_ENV_ON;
			EventMap[0xD9] = POKEPUZ_EVENT_ENV_OFF;
			EventMap[0xDA] = POKEPUZ_EVENT_MASTER_VOL;
			EventMap[0xDB] = POKEPUZ_EVENT_PAN_R;
			EventMap[0xDC] = POKEPUZ_EVENT_PAN_L;
			EventMap[0xDD] = POKEPUZ_EVENT_PAN_LR;
			EventMap[0xDE] = POKEPUZ_EVENT_DUTY0;
			EventMap[0xDF] = POKEPUZ_EVENT_DUTY1;
			EventMap[0xE0] = POKEPUZ_EVENT_DUTY2;
			EventMap[0xE1] = POKEPUZ_EVENT_DUTY3;
			EventMap[0xE2] = POKEPUZ_EVENT_WAVEFORM;
			EventMap[0xE3] = POKEPUZ_EVENT_TUNING;
			EventMap[0xE4] = POKEPUZ_EVENT_PITCH_TUNING;
			EventMap[0xE5] = POKEPUZ_EVENT_NOTE_DUR;
			EventMap[0xE6] = POKEPUZ_EVENT_NR_ENV;
			EventMap[0xE7] = POKEPUZ_EVENT_JUMP;
			EventMap[0xE8] = POKEPUZ_EVENT_REPEAT1;
			EventMap[0xE9] = POKEPUZ_EVENT_REPEAT2;
			EventMap[0xEA] = POKEPUZ_EVENT_REPEAT3;
			EventMap[0xEB] = POKEPUZ_EVENT_REPEAT4;
			EventMap[0xEC] = POKEPUZ_EVENT_CALL;
			EventMap[0xED] = POKEPUZ_EVENT_RETURN;
			for (j = 0xEE; j <= 0xFD; j++)
			{
				EventMap[j] = POKEPUZ_EVENT_NUDGE;
			}
			EventMap[0xFE] = POKEPUZ_EVENT_REPEAT_NOTE;
			EventMap[0xFF] = POKEPUZ_EVENT_PITCH_SLIDE;
			break;
		}

		/*Get the channel pointers*/

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&exRomData[songPtr + (curTrack * 2)]);
		}


		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			holdNote = 0;
			curVol = 120;
			inMacro = 0;
			for (j = 0; j < 5; j++)
			{
				repeats[j][0] = 0;
				repeats[j][1] = 0;
			}

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			seqPos = seqPtrs[curTrack];
			seqEnd = 0;
			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];

				if (command[0] == POKEPUZ_STATUS_STOP)
				{
					seqEnd = 1;
				}

				else if (command[0] >= POKEPUZ_STATUS_NOTE_LEN_MIN && command[0] <= POKEPUZ_STATUS_NOTE_LEN_MAX)
				{
					curNoteLen = (command[0] - POKEPUZ_STATUS_NOTE_LEN_MIN + 1) * 10;
					seqPos++;
				}

				else if (command[0] >= POKEPUZ_STATUS_DRUM_MIN && command[0] <= POKEPUZ_STATUS_DRUM_MAX)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
					}
					curNote = command[0] - POKEPUZ_STATUS_DRUM_MIN + 24;
					tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 1;
					midPos = tempPos;
					curDelay = curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (command[0] >= POKEPUZ_STATUS_NOTE_MIN && command[0] <= POKEPUZ_STATUS_NOTE_MAX)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
					}
					curNote = command[0] - POKEPUZ_STATUS_NOTE_MIN + transpose + 24;
					if (curTrack < 2)
					{
						curNote += 12;
					}
					tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 1;
					midPos = tempPos;
					curDelay = curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (command[0] == POKEPUZ_STATUS_REST)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
					}
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (command[0] == POKEPUZ_STATUS_TIE)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_VIBRATO_ON)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_VIBRATO_OFF)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_TEMPO)
				{
					tempo = command[1] * 10;

					if (tempo < 2)
					{
						tempo = 2;
					}

					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;

					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_ENV_ON)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_ENV_OFF)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_MASTER_VOL)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_PAN_L)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_PAN_R)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_PAN_LR)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_DUTY0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_DUTY1)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_DUTY2)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_DUTY3)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_WAVEFORM)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_PITCH_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_NOTE_DUR)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_NR_ENV)
				{
					if (curTrack != 2)
					{
						/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
						if ((command[1] & 0x08) == 0x00)
						{
							curVol = (command[1] & 0xF0) / 2;
						}
						else
						{
							if (((command[1] & 0xF0) / 2) == curVol)
							{
								curVol = (command[1] & 0xF0) / 2;
							}
							else
							{
								curVol = 120;
							}

						}

						if (curVol == 0)
						{
							curVol = 1;
						}
					}
					else
					{
						switch (command[1])
						{
						case 0x20:
							curVol = 120;
							break;
						case 0x40:
							curVol = 60;
							break;
						case 0x60:
							curVol = 30;
							break;
						case 0x00:
							curVol = 1;
							break;
						default:
							curVol = 120;
							break;
						}
					}
					seqPos += 2;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_JUMP)
				{
					if (ReadLE16(&exRomData[seqPos + 1]) > seqPos)
					{
						seqPos = ReadLE16(&exRomData[seqPos + 1]);
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_REPEAT1)
				{
					if (repeats[0][0] == command[1])
					{
						repeats[0][0]++;
						seqPos = ReadLE16(&exRomData[seqPos + 2]);
					}
					else
					{
						repeats[0][0] = 0;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_REPEAT2)
				{
					if (repeats[1][0] == command[1])
					{
						repeats[1][0]++;
						seqPos = ReadLE16(&exRomData[seqPos + 2]);
					}
					else
					{
						repeats[1][0] = 0;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_REPEAT3)
				{
					if (repeats[2][0] == command[1])
					{
						repeats[2][0]++;
						seqPos = ReadLE16(&exRomData[seqPos + 2]);
					}
					else
					{
						repeats[2][0] = 0;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_REPEAT4)
				{
					if (repeats[3][0] == command[1])
					{
						repeats[3][0]++;
						seqPos = ReadLE16(&exRomData[seqPos + 2]);
					}
					else
					{
						repeats[3][0] = 0;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_CALL)
				{
					inMacro++;

					/*No more macro levels available*/
					if (inMacro > 4)
					{
						seqEnd = 1;
					}
					else
					{
						macros[inMacro][0] = ReadLE16(&exRomData[seqPos + 1]);
						macros[inMacro][1] = seqPos + 3;
						seqPos = macros[inMacro][0];
					}
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_RETURN)
				{
					seqPos = macros[inMacro][1];
					if (inMacro > 0)
					{
						inMacro--;
					}
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_NUDGE)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_REPEAT_NOTE)
				{
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == POKEPUZ_EVENT_PITCH_SLIDE)
				{
					curDelay += (command[1] * 5);
					ctrlDelay += (command[1] * 5);
					seqPos += 5;
				}

				/*Unknown command*/
				else
				{
					seqPos++;
				}
			}

			/*End of track*/
			if (holdNote == 1)
			{
				tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
				holdNote = 0;
				midPos = tempPos;
				curDelay = 0;
			}

			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

		}
		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		trackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], trackSize);

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);

	}
}