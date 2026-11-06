/*Biox #1 (Japan System House)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "BIOX1.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;

int i, j;
char outfile[1000000];
int songNum;
long songPtr;
long seqPtrs[4];
long bankAmt;
int curVol;
int curInst;
int drvVers;
int foundTable;
int progBank;
int verOverride;
long firstPtr;
long secondPtr;

const char Biox1MagicBytes[6] = { 0xCB, 0x27, 0x6F, 0x26, 0x00, 0x11 };

const unsigned char Biox1NoteLens[16] = { 0x02, 0x03, 0x04, 0x06, 0x09, 0x08, 0x0C, 0x12, 0x10, 0x18, 0x24, 0x20, 0x30, 0x48, 0x40, 0x60 };

unsigned char* romData;
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
void Biox1song2mid(int songNum, long ptr);

void Biox1Proc(int bank, char parameters[4][100])
{
	drvVers = BIOX1_VER_NB;
	curVol = 120;
	curInst = 0;
	foundTable = 0;
	verOverride = 0;

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers < BIOX1_VER_NB && drvVers > BIOX1_VER_PB)
		{
			fclose(rom);
			printf("ERROR: Invalid version number!\n");
			exit(-1);
		}
	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Biox1MagicBytes, 6) && foundTable != 1)
		{
			tablePtrLoc = i + 6;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		firstPtr = ReadLE16(&romData[tableOffset]);
		if (drvVers == BIOX1_VER_NB)
		{
			while (i != firstPtr)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Biox1song2mid(songNum, songPtr);
				i += 2;
				songNum++;
			}
		}
		else
		{
			firstPtr = ReadLE16(&romData[tableOffset + 8]);
			while (i != firstPtr)
			{
				songPtr = i;
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Biox1song2mid(songNum, songPtr);
				i += 8;
				songNum++;
			}
		}

		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(1);
	}

}

/*Convert the song data to MIDI*/
void Biox1song2mid(int songNum, long ptr)
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
	int macros[8][2];
	int repeats[8][2];
	int inMacro;
	int curSpeed;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
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
	int octave = 0;
	int noteLenAdd = 0;
	int volVal = 0;
	int tempLen;
	int tie;

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
		/*Get pointers*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[songPtr + (curTrack * 2)]);
		}

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
		case BIOX1_VER_NB:
			/*Fall-through*/
		case BIOX1_VER_PB:
		default:
			BIOX1_STATUS_NOTE_MIN = 0x00;
			BIOX1_STATUS_NOTE_MAX = 0x9F;
			BIOX1_STATUS_NOTE_LEN_MIN = 0xA0;
			BIOX1_STATUS_NOTE_LEN_MAX = 0xAF;
			for (j = 0xB0; j <= 0xDF; j++)
			{
				EventMap[j] = BIOX1_EVENT_STOP;
			}
			EventMap[0xE0] = BIOX1_EVENT_STOP;
			EventMap[0xE1] = BIOX1_EVENT_SWEEP;
			EventMap[0xE2] = BIOX1_EVENT_ENV_SEQ;
			EventMap[0xE3] = BIOX1_EVENT_DUTY;
			EventMap[0xE4] = BIOX1_EVENT_VIBRATO;
			EventMap[0xE5] = BIOX1_EVENT_ENV;
			EventMap[0xE6] = BIOX1_EVENT_TEMPO;
			EventMap[0xE7] = BIOX1_EVENT_NOP2;
			EventMap[0xE8] = BIOX1_EVENT_NOP2;
			EventMap[0xE9] = BIOX1_EVENT_STOP;
			EventMap[0xEA] = BIOX1_EVENT_STOP;
			EventMap[0xEB] = BIOX1_EVENT_PAN;
			EventMap[0xEC] = BIOX1_EVENT_STOP;
			EventMap[0xED] = BIOX1_EVENT_STOP;
			EventMap[0xEE] = BIOX1_EVENT_CALL;
			EventMap[0xEF] = BIOX1_EVENT_RETURN;
			EventMap[0xF0] = BIOX1_EVENT_REPEAT;
			EventMap[0xF1] = BIOX1_EVENT_JUMP;
			EventMap[0xF2] = BIOX1_EVENT_GLOBAL_VOL;
			EventMap[0xF3] = BIOX1_EVENT_TIE;
			EventMap[0xF4] = BIOX1_EVENT_RAW_NOTE;
			EventMap[0xF5] = BIOX1_EVENT_SLUR;
			for (j = 0xF6; j < 0xFD; j++)
			{
				EventMap[j] = BIOX1_EVENT_STOP;
			}
			EventMap[0xFE] = BIOX1_EVENT_NOTE_LEN;
			EventMap[0xFF] = BIOX1_EVENT_STOP;
			break;

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
			noteLenAdd = 0;
			inMacro = 0;
			volVal = 0xF0;
			transpose = 0;
			curSpeed = 1;
			curVol = 120;
			octave = 0;
			tempLen = 0;
			tie = 0;
			for (j = 0; j < 8; j++)
			{
				macros[j][0] = 0;
				macros[j][1] = 0;
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
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				lowNibble = (command[0] >> 4);
				highNibble = (command[0] & 15);

				if (command[0] >= BIOX1_STATUS_NOTE_MIN && command[0] <= BIOX1_STATUS_NOTE_MAX)
				{
					tie = 0;
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					/*Play note*/
					if (highNibble < 0x0D)
					{
						curNote = (lowNibble * 12) + highNibble + 24;
						midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 1;
						firstNote = 0;
					}
					if (command[1] >= BIOX1_STATUS_NOTE_LEN_MIN && command[1] <= BIOX1_STATUS_NOTE_LEN_MAX)
					{
						curSpeed = Biox1NoteLens[command[1] - BIOX1_STATUS_NOTE_LEN_MIN] * 5;
						seqPos++;
					}
					curDelay += curSpeed;
					ctrlDelay += curSpeed;
					masterDelay += curSpeed;
					seqPos++;

				}

				else if (command[0] >= BIOX1_STATUS_NOTE_LEN_MIN && command[0] <= BIOX1_STATUS_NOTE_LEN_MAX)
				{
					curSpeed = Biox1NoteLens[command[0] - BIOX1_STATUS_NOTE_LEN_MIN] * 5;
					if (tie != 1)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
					}
					curDelay += curSpeed;
					ctrlDelay += curSpeed;
					masterDelay += curSpeed;
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_UNKNOWN0 || EventMap[command[0]] == BIOX1_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_UNKNOWN1 || EventMap[command[0]] == BIOX1_EVENT_NOP1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_UNKNOWN2 || EventMap[command[0]] == BIOX1_EVENT_NOP2)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_ENV_SEQ)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_VIBRATO)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_ENV)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_TEMPO)
				{
					if (command[1] != 0x00)
					{
						double ticksPerSec;
						ticksPerSec = 4096.0 / (0x100 - command[1]);	// timer interrupt rate

						tempo = ticksPerSec * 2.5;

						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);

						if (tempo < 2)
						{
							tempo = 2;
						}
						ctrlMidPos += 2;
					}

					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_CALL)
				{
					/*Macro is too high*/
					if (inMacro >= 3)
					{
						seqEnd = 1;
					}
					else
					{
						inMacro++;
						macros[inMacro][0] = ReadLE16(&romData[seqPos + 1]);
						macros[inMacro][1] = seqPos + 3;
						seqPos = macros[inMacro][0];
					}
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_RETURN)
				{
					if (inMacro <= 0)
					{
						seqEnd = 1;
					}
					else
					{
						seqPos = macros[inMacro][1];
						inMacro--;
					}

				}

				else if (EventMap[command[0]] == BIOX1_EVENT_REPEAT)
				{
					if (repeats[command[2]][1] != command[1])
					{
						repeats[command[2]][1]++;
						seqPos = ReadLE16(&romData[seqPos + 3]);
					}
					else
					{
						repeats[command[2]][1] = 0;
						seqPos += 5;
					}
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_JUMP)
				{
					if ((ReadLE16(&romData[seqPos + 1])) > seqPos)
					{
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_GLOBAL_VOL)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_TIE)
				{
					tie = 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_RAW_NOTE)
				{
					tie = 0;
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					/*Play note*/
					if (highNibble < 0x0D)
					{
						curNote = (lowNibble * 12) + highNibble + 24;
						midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 1;
						firstNote = 0;
					}
					curDelay += curSpeed;
					ctrlDelay += curSpeed;
					masterDelay += curSpeed;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_SLUR)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_NOTE_LEN)
				{
					curSpeed = command[1] * 5;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX1_EVENT_STOP)
				{
					seqEnd = 1;
				}

				/*Unknown command*/
				else
				{
					seqPos++;
				}


			}

			if (holdNote == 1)
			{
				tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
				curDelay = 0;
				holdNote = 0;
				midPos = tempPos;
			}

			/*End of track*/
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