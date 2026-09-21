/*Jaleco*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "JALECO.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int foundTable;
int curInst;
long firstPtr;

long bankAmt;

int curVol;
int drvVers;

const char JalecoMagicBytes[5] = { 0x47, 0x4F, 0x57, 0x5F, 0xCD };

int JaleconoteVals[] = { 0, 0, 0, 0, 0,                /* 0-4   base */
	12, 24, 36, 48, 60, 72,       /* 5-10  +1 octave each */
	72, 72, 72, 72,               /* 11-14 top hold */
	1, 1, 1, 1, 1,                /* 15-19 */
	13, 25, 37, 49, 61, 73,
	73, 73, 73, 73,
	2, 2, 2, 2, 2,                /* 30-34 */
	14, 26, 38, 50, 62, 74,
	74, 74, 74, 74,
	3, 3, 3, 3, 3,                /* 45-49 */
	15, 27, 39, 51, 63, 75,
	75, 75, 75, 75,
	4, 4, 4, 4, 4,                /* 60-64 */
	16, 28, 40, 52, 64, 76,
	76, 76, 76, 76,
	5, 5, 5, 5, 5,                /* 75-79 */
	17, 29, 41, 53, 65, 77,
	77, 77, 77, 77,
	6, 6, 6, 6, 6,                /* 90-94 */
	18, 30, 42, 54, 66, 77,       /* 95-100 */
	77, 77, 77, 77,
	7, 7, 7, 7, 7,                /* 105-109 */
	19, 31, 43, 55, 67, 79,       /* 110-115 */
	79, 79, 79, 79,
	8, 8, 8, 8, 8,                /* 120-124 */
	20, 32, 44, 56, 68, 80,       /* 125-130 */
	80, 80, 80, 80,
	9, 9, 9, 9, 9,                /* 135-139 */
	21, 33, 45, 57, 69, 81,       /* 140-145 */
	81, 81, 81, 81,
	10, 10, 10, 10, 10,           /* 150-154 */
	22, 34, 46, 58, 70, 82,       /* 155-160 */
	82, 82, 82, 82,
	11, 11, 11, 11, 11,           /* 165-169 */
	23, 35, 47, 59, 71, 83,       /* 170-175 */
	83, 83, 83, 83,
	12, 12, 12, 12, 12,           /* 180-184 */
	24, 36, 48, 60, 72, 127,      /* 185-190 (190 clamped, sentinel) */
	127, 127, 127, 127,           /* 191-194 clamped, sentinel */ };

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

int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Jalecosong2mid(int songNum, long songPtr);


void JalecoProc(int bank, char parameters[4][100])
{
	drvVers = JALECO_VER_AS;
	curVol = 120;
	curInst = 0;
	firstPtr = 0;
	foundTable = 0;

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);

		if (drvVers < JALECO_VER_HSPP || drvVers > JALECO_VER_SD)
		{
			drvVers = JALECO_VER_AS;
		}
	}

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

	/*Try to search the bank for song table loader - Method 1: Mega Man 3/Bionic Commando*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], JalecoMagicBytes, 5)) && foundTable != 1)
		{
			tablePtrLoc = i + 11;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;
		while (ReadLE16(&romData[i]) < (bankSize * 2))
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Jalecosong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}

		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}

}

/*Convert the song data to MIDI*/
void Jalecosong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int chanSpeed = 0;
	int octave = 0;
	int transpose = 0;
	int macros[8][2];
	int macroNum;
	int repeats[8][2];
	int repeatNum;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int masterDelay = 0;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int noteDurFrac = 0;
	int noteDurAbs = 0;
	int noteMode = 0;
	int curNoteDur = 0;

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
		ctrlMidData[j] = 0;
	}

	for (j = 0; j < 8; j++)
	{
		repeats[j][0] = 0;
		repeats[j][1] = 0;
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

		romPos = songPtr;

		JALECO_STATUS_NOTE_MIN = 0x00;
		JALECO_STATUS_NOTE_MAX = 0xFE;
		JALECO_STATUS_REST = 0xB8;
		JALECO_STATUS_COMMAND = 0xFF;

		switch (drvVers)
		{
		case JALECO_VER_HSPP:
			EventMap[0x00] = JALECO_EVENT_SWEEP_OFF;
			EventMap[0x01] = JALECO_EVENT_NOTE_DUR_FRAC;
			EventMap[0x02] = JALECO_EVENT_NOTE_DUR_ABS;
			EventMap[0x03] = JALECO_EVENT_TRANSPOSE;
			EventMap[0x04] = JALECO_EVENT_PAN;
			EventMap[0x05] = JALECO_EVENT_SPEED;
			EventMap[0x06] = JALECO_EVENT_JUMP;
			EventMap[0x07] = JALECO_EVENT_ENV_PARAMS;
			EventMap[0x08] = JALECO_EVENT_ENV;
			EventMap[0x09] = JALECO_EVENT_SWEEP_ON;
			EventMap[0x0A] = JALECO_EVENT_TREMOLO;
			EventMap[0x0B] = JALECO_EVENT_WAVEFORM;
			EventMap[0x0C] = JALECO_EVENT_VOL;
			EventMap[0x0D] = JALECO_EVENT_REPEAT_START;
			EventMap[0x0E] = JALECO_EVENT_REPEAT_END;
			EventMap[0x0F] = JALECO_EVENT_STOP;
			EventMap[0x10] = JALECO_EVENT_CALL;
			EventMap[0x11] = JALECO_EVENT_RETURN;
			EventMap[0x12] = JALECO_EVENT_TUNING;
			EventMap[0x13] = JALECO_EVENT_SWEEP_OFF;
			for (j = 0x14; j < 0xFF; j++)
			{
				EventMap[j] = JALECO_EVENT_SWEEP_OFF;
			}
			break;
		case JALECO_VER_AS:
			/*Fall-through*/
		case JALECO_VER_SD:
		default:
			EventMap[0x00] = JALECO_EVENT_PAN;
			EventMap[0x01] = JALECO_EVENT_ENV;
			EventMap[0x02] = JALECO_EVENT_VOL;
			EventMap[0x03] = JALECO_EVENT_ENV_SEQ;
			EventMap[0x04] = JALECO_EVENT_VIB_SEQ;
			EventMap[0x05] = JALECO_EVENT_NOTE_DUR_FRAC;
			EventMap[0x06] = JALECO_EVENT_NOTE_DUR_ABS;
			EventMap[0x07] = JALECO_EVENT_TRANSPOSE;
			EventMap[0x08] = JALECO_EVENT_SPEED;
			EventMap[0x09] = JALECO_EVENT_JUMP;
			EventMap[0x0A] = JALECO_EVENT_SWEEP_ON;
			EventMap[0x0B] = JALECO_EVENT_WAVEFORM;
			EventMap[0x0C] = JALECO_EVENT_REPEAT_START;
			EventMap[0x0D] = JALECO_EVENT_REPEAT_END;
			EventMap[0x0E] = JALECO_EVENT_STOP;
			EventMap[0x0F] = JALECO_EVENT_CALL;
			EventMap[0x10] = JALECO_EVENT_RETURN;
			EventMap[0x11] = JALECO_EVENT_TUNING;
			EventMap[0x12] = JALECO_EVENT_SWEEP_OFF;
			for (j = 0x13; j < 0xFF; j++)
			{
				EventMap[j] = JALECO_EVENT_SWEEP_OFF;
			}
			break;
		}

		/*Get channel pointers*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos + (curTrack * 2)]);
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPos = seqPtrs[curTrack];
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 3;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			macroNum = 0;
			transpose = 0;
			noteDurFrac = 8;
			noteDurAbs = 20;
			noteMode = 0;
			curNoteDur = 0;
			repeatNum = 0;
			curVol = 120;

			int curFreq;

			if (seqPtrs[curTrack] == 0x0000)
			{
				seqEnd = 1;
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

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				if (command[0] == JALECO_STATUS_REST)
				{
					curNoteLen = command[1] * chanSpeed * 5;

					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;

					seqPos += 2;
				}

				else if (command[0] == JALECO_STATUS_COMMAND)
				{
					seqPos++;
					if (EventMap[command[1]] == JALECO_EVENT_UNKNOWN0)
					{
						seqPos++;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_PAN)
					{
						seqPos += 2;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_ENV_PARAMS)
					{
						if (drvVers == JALECO_VER_HSPP)
						{
							if (command[2] == 0x00)
							{
								curVol = 10;
							}
							else if (command[2] == 0x01)
							{
								curVol = 40;
							}
							else if (command[2] == 0x02)
							{
								curVol = 60;
							}
							else
							{
								curVol = 120;
							}
							seqPos += 5;
						}
						else
						{
							curVol = command[2] * 0x10;
							if (curVol == 0)
							{
								curVol = 1;
							}
							if (curVol > 120)
							{
								curVol = 120;
							}
							seqPos += 4;
						}

					}

					else if (EventMap[command[1]] == JALECO_EVENT_ENV)
					{
						if (drvVers == JALECO_VER_HSPP)
						{
							noteMode = 1;
						}
						seqPos += 4;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_ENV_SEQ)
					{
						seqPos += 3;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_VIB_SEQ)
					{
						seqPos += 3;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_NOTE_DUR_FRAC)
					{
						noteDurFrac = command[2];
						if (drvVers == JALECO_VER_AS)
						{
							noteMode = 1;
						}
						seqPos += 2;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_NOTE_DUR_ABS)
					{
						noteDurAbs = command[2];
						if (drvVers == JALECO_VER_AS)
						{
							noteMode = 0;
						}
						seqPos += 2;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_TRANSPOSE)
					{
						transpose = (signed char)command[2];
						seqPos += 2;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_SPEED)
					{
						chanSpeed = command[2];
						seqPos += 2;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_JUMP)
					{
						if (ReadLE16(&romData[seqPos + 1]) > seqPos)
						{
							seqPos = ReadLE16(&romData[seqPos + 1]);
						}
						else
						{
							seqEnd = 1;
						}
					}

					else if (EventMap[command[1]] == JALECO_EVENT_SWEEP_ON)
					{
						seqPos += 4;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_WAVEFORM)
					{
						seqPos += 3;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_REPEAT_START)
					{
						if (drvVers != JALECO_VER_SD)
						{
							repeats[command[2]][0] = command[3];
							seqPos += 3;
						}
						else
						{
							repeatNum++;
							repeats[repeatNum][0] = command[2];
							repeats[repeatNum][1] = seqPos + 2;
							seqPos += 2;
						}

					}

					else if (EventMap[command[1]] == JALECO_EVENT_REPEAT_END)
					{
						if (drvVers != JALECO_VER_SD)
						{
							if (repeats[command[2]][0] > 1)
							{
								repeats[command[2]][0]--;
								seqPos = ReadLE16(&romData[seqPos + 2]);
							}
							else
							{
								seqPos += 4;
							}
						}
						else
						{
							if (repeats[repeatNum][0] > 1)
							{
								repeats[repeatNum][0]--;
								seqPos = repeats[repeatNum][1];
							}
							else
							{
								repeatNum--;
								seqPos++;
							}
						}

					}

					else if (EventMap[command[1]] == JALECO_EVENT_STOP)
					{
						seqEnd = 1;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_CALL)
					{
						macroNum++;

						if (macroNum >= 8)
						{
							seqEnd = 1;
						}
						macros[macroNum][0] = ReadLE16(&romData[seqPos + 1]);
						macros[macroNum][1] = seqPos + 3;
						seqPos = macros[macroNum][0];
					}

					else if (EventMap[command[1]] == JALECO_EVENT_RETURN)
					{
						if (macroNum > 0)
						{
							seqPos = macros[macroNum][1];
							macroNum--;
						}
						else
						{
							seqPos++;
						}
					}

					else if (EventMap[command[1]] == JALECO_EVENT_TUNING)
					{
						seqPos += 2;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_SWEEP_OFF)
					{
						seqPos++;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_TREMOLO)
					{
						seqPos += 4;
					}

					else if (EventMap[command[1]] == JALECO_EVENT_VOL)
					{
						if (command[2] == 0x00)
						{
							curVol = 10;
						}
						else if (command[2] == 0x01)
						{
							curVol = 60;
						}
						else if (command[2] == 0x02)
						{
							curVol = 100;
						}
						else
						{
							curVol = 120;
						}
						if (drvVers == JALECO_VER_HSPP)
						{
							noteMode = 0;
						}
						seqPos += 2;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}

				}

				/*Play note*/
				else
				{
					curNote = command[0] + transpose;
					curNote *= 2;
					curNote = JaleconoteVals[curNote / 2];

					if (curTrack < 2)
					{
						curNote += 36;
					}
					else
					{
						curNote += 24;
					}

					curNoteLen = command[1] * chanSpeed * 5;
					if (noteMode == 0)
					{
						curNoteDur = ((command[1] * noteDurFrac * chanSpeed) / 8 + 1) * 5;

					}
					else
					{
						curNoteDur = command[1] * chanSpeed * 5;
					}

					if (curNoteDur <= curNoteLen)
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteDur, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = curNoteLen - curNoteDur;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
					}
					else
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
					}

					seqPos += 2;
				}

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
		free(exRomData);
		fclose(mid);
	}
}
