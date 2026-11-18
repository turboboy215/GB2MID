/*Square*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <stddef.h>
#include "SHARED.H"
#include "SQUARE.H"

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
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int drvVers;
int curInst;
int progBank;
int curVol;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char SqrMagicBytesA[13] = { 0x19, 0x11, 0x02, 0xCE, 0xCD, 0x53, 0x40, 0x11, 0x22, 0xCE, 0xCD, 0x53, 0x40 };
const char SqrMagicBytesB[7] = { 0x5F, 0x16, 0x00, 0x19, 0x2A, 0xEA, 0x04 };
const char SqrMagicBytesC[7] = { 0x5F, 0x16, 0x00, 0x19, 0x2A, 0xEA, 0x09 };

const unsigned char NoteLen1[15] = { 0x60, 0x48, 0x30, 0x24, 0x20, 0x18, 0x12, 0x10, 0x0C, 0x09, 0x08, 0x06, 0x04, 0x03, 0x02 };
const unsigned char NoteLen2[13] = { 0x60, 0x48, 0x30, 0x20, 0x24, 0x18, 0x10, 0x12, 0x0C, 0x08, 0x06, 0x04, 0x03 };
const unsigned char NoteLen3[16] = { 0x90, 0x60, 0x48, 0x30, 0x24, 0x20, 0x18, 0x12, 0x10, 0x0C, 0x09, 0x08, 0x06, 0x04, 0x03, 0x02 };

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
void Sqrsong2mid(int songNum, long ptrs[4]);

void SqrProc(int bank)
{
	curInst = 0;
	foundTable = 0;
	firstPtr = 0;
	drvVers = SQUARE_VER_FFL1;

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

	/*Try to search the bank for song table loader - Method 1: Final Fantasy Legend 1*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], SqrMagicBytesA, 13)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			drvVers = SQUARE_VER_FFL1;
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 2: Final Fantasy Legend 2/Adventure*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], SqrMagicBytesB, 7)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			drvVers = SQUARE_VER_FFL2;
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 3: Final Fantasy Legend 3*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], SqrMagicBytesC, 7)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			drvVers = SQUARE_VER_FFL3;
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		if (drvVers == SQUARE_VER_FFL1)
		{
			while ((ReadLE16(&romData[i]) > bankSize) && (ReadLE16(&romData[i]) < (bankSize * 2)))
			{
				for (j = 0; j < 3; j++)
				{
					seqPtrs[j] = ReadLE16(&romData[i + (j * 2)]);
					printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
				}
				Sqrsong2mid(songNum, seqPtrs);
				i += 6;
				songNum++;
			}
		}

		else if (drvVers == SQUARE_VER_FFL2)
		{
			while ((ReadLE16(&romData[i]) > bankSize) && (ReadLE16(&romData[i]) < (bankSize * 2)))
			{
				for (j = 0; j < 3; j++)
				{
					if (j == 0)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 2]);
					}
					else if (j == 1)
					{
						seqPtrs[j] = ReadLE16(&romData[i]);
					}
					else if (j == 2)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 4]);
					}

					printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
				}
				Sqrsong2mid(songNum, seqPtrs);
				i += 6;
				songNum++;
			}
		}

		else if (drvVers == SQUARE_VER_FFL3)
		{
			while ((ReadLE16(&romData[i]) > bankSize) && (ReadLE16(&romData[i]) < (bankSize * 2)))
			{
				for (j = 0; j < 4; j++)
				{
					if (j == 0)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 2]);
					}
					else if (j == 1)
					{
						seqPtrs[j] = ReadLE16(&romData[i]);
					}
					else if (j == 2)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 4]);
					}
					else if (j == 3)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 6]);
					}
					printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
				}
				Sqrsong2mid(songNum, seqPtrs);
				i += 8;
				songNum++;
			}
		}
	}

	free(romData);

}

/*Convert the song data to MIDI*/
void Sqrsong2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	int curTrack = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int songTempo = 0;
	int octave = 0;
	long jumpPos = 0;
	long jumpPosRet = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
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
	int holdNote = 0;
	long tempPos = 0;

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

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		if (drvVers != SQUARE_VER_FFL3)
		{
			trackCnt = 3;
		}
		else
		{
			trackCnt = 4;
		}

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

		switch (drvVers)
		{
		case SQUARE_VER_FFL1:
			/*Fall-through*/
		default:
			SQUARE_STATUS_NOTE_MIN = 0x00;
			SQUARE_STATUS_NOTE_MAX = 0xBF;
			SQUARE_STATUS_REST_MIN = 0xC0;
			SQUARE_STATUS_REST_MAX = 0xCF;
			SQUARE_STATUS_TIE_MIN = 0xD0;
			SQUARE_STATUS_TIE_MAX = 0xDF;
			SQUARE_STATUS_OCTAVE_MIN = 0xF0;
			SQUARE_STATUS_OCTAVE_MAX = 0xF4;
			EventMap[0xE0] = SQUARE_EVENT_TEMPO;
			for (j = 0xE1; j <= 0xEF; j++)
			{
				EventMap[j] = SQUARE_EVENT_VOL;
			}
			for (j = 0xF0; j <= 0xF4; j++)
			{
				EventMap[j] = SQUARE_EVENT_OCTAVE;
			}
			EventMap[0xF5] = SQUARE_EVENT_NRX1;
			EventMap[0xF6] = SQUARE_EVENT_NOP;
			EventMap[0xF7] = SQUARE_EVENT_ENV;
			EventMap[0xF8] = SQUARE_EVENT_WAVEFORM;
			EventMap[0xF9] = SQUARE_EVENT_REPEAT1X;
			EventMap[0xFA] = SQUARE_EVENT_REPEAT2X;
			EventMap[0xFB] = SQUARE_EVENT_REPEAT3X;
			EventMap[0xFC] = SQUARE_EVENT_REPEAT_END;
			EventMap[0xFD] = SQUARE_EVENT_COND_JUMP;
			EventMap[0xFE] = SQUARE_EVENT_JUMP;
			EventMap[0xFF] = SQUARE_EVENT_STOP;
			break;
		case SQUARE_VER_FFL2:
			SQUARE_STATUS_NOTE_MIN = 0x00;
			SQUARE_STATUS_NOTE_MAX = 0xCF;
			SQUARE_STATUS_TIE = 0x0E;
			SQUARE_STATUS_REST = 0x0F;
			SQUARE_STATUS_OCTAVE_MIN = 0xD0;
			SQUARE_STATUS_OCTAVE_MAX = 0xD7;
			SQUARE_STATUS_OCTAVE_UP_MIN = 0xD8;
			SQUARE_STATUS_OCTAVE_UP_MAX = 0xDB;
			SQUARE_STATUS_OCTAVE_DOWN_MIN = 0xDC;
			SQUARE_STATUS_OCTAVE_DOWN_MAX = 0xDF;
			for (j = 0xD0; j <= 0xD7; j++)
			{
				EventMap[j] = SQUARE_EVENT_OCTAVE;
			}
			for (j = 0xD8; j <= 0xDB; j++)
			{
				EventMap[j] = SQUARE_EVENT_OCTAVE_UP;
			}
			for (j = 0xDC; j <= 0xDF; j++)
			{
				EventMap[j] = SQUARE_EVENT_OCTAVE_DOWN;
			}
			EventMap[0xE0] = SQUARE_EVENT_VOL;
			EventMap[0xE1] = SQUARE_EVENT_JUMP;
			EventMap[0xE2] = SQUARE_EVENT_REPEAT1_END;
			EventMap[0xE3] = SQUARE_EVENT_REPEAT1_START;
			EventMap[0xE4] = SQUARE_EVENT_VIBRATO;
			EventMap[0xE5] = SQUARE_EVENT_DUTY;
			EventMap[0xE6] = SQUARE_EVENT_PAN;
			EventMap[0xE7] = SQUARE_EVENT_TEMPO;
			EventMap[0xE8] = SQUARE_EVENT_WAVEFORM;
			EventMap[0xE9] = SQUARE_EVENT_REPEAT2_END;
			EventMap[0xEA] = SQUARE_EVENT_REPEAT2_START;
			EventMap[0xEB] = SQUARE_EVENT_COND_JUMP;
			EventMap[0xEC] = SQUARE_EVENT_DOUBLE_TIME;
			EventMap[0xF0] = SQUARE_EVENT_VOL;
			EventMap[0xF1] = SQUARE_EVENT_JUMP;
			EventMap[0xF2] = SQUARE_EVENT_REPEAT1_END;
			EventMap[0xF3] = SQUARE_EVENT_REPEAT1_START;
			EventMap[0xF4] = SQUARE_EVENT_VIBRATO;
			EventMap[0xF5] = SQUARE_EVENT_DUTY;
			EventMap[0xF6] = SQUARE_EVENT_PAN;
			EventMap[0xF7] = SQUARE_EVENT_TEMPO;
			EventMap[0xF8] = SQUARE_EVENT_WAVEFORM;
			EventMap[0xF9] = SQUARE_EVENT_REPEAT2_END;
			EventMap[0xFA] = SQUARE_EVENT_REPEAT2_START;
			EventMap[0xFB] = SQUARE_EVENT_COND_JUMP;
			EventMap[0xFC] = SQUARE_EVENT_DOUBLE_TIME;
			EventMap[0xFF] = SQUARE_EVENT_STOP;
			break;
		case SQUARE_VER_FFL3:
			SQUARE_STATUS_NOTE_MIN = 0x00;
			SQUARE_STATUS_NOTE_MAX = 0xBF;
			SQUARE_STATUS_TIE_MIN = 0xC0;
			SQUARE_STATUS_TIE_MAX = 0xCF;
			SQUARE_STATUS_REST_MIN = 0xD0;
			SQUARE_STATUS_REST_MAX = 0xDF;
			SQUARE_STATUS_OCTAVE_MIN = 0xE0;
			SQUARE_STATUS_OCTAVE_MAX = 0xE7;
			SQUARE_STATUS_OCTAVE_UP_MIN = 0xE8;
			SQUARE_STATUS_OCTAVE_UP_MAX = 0xEB;
			SQUARE_STATUS_OCTAVE_DOWN_MIN = 0xEC;
			SQUARE_STATUS_OCTAVE_DOWN_MAX = 0xEF;
			for (j = 0xE0; j <= 0xE7; j++)
			{
				EventMap[j] = SQUARE_EVENT_OCTAVE;
			}
			for (j = 0xE8; j <= 0xEB; j++)
			{
				EventMap[j] = SQUARE_EVENT_OCTAVE_UP;
			}
			for (j = 0xEC; j <= 0xEF; j++)
			{
				EventMap[j] = SQUARE_EVENT_OCTAVE_DOWN;
			}
			EventMap[0xF0] = SQUARE_EVENT_VOL;
			EventMap[0xF1] = SQUARE_EVENT_JUMP;
			EventMap[0xF2] = SQUARE_EVENT_REPEAT1_END;
			EventMap[0xF3] = SQUARE_EVENT_REPEAT1_START;
			EventMap[0xF4] = SQUARE_EVENT_VIBRATO;
			EventMap[0xF5] = SQUARE_EVENT_DUTY;
			EventMap[0xF6] = SQUARE_EVENT_PAN;
			EventMap[0xF7] = SQUARE_EVENT_TEMPO;
			EventMap[0xF8] = SQUARE_EVENT_WAVEFORM;
			EventMap[0xF9] = SQUARE_EVENT_REPEAT2_END;
			EventMap[0xFA] = SQUARE_EVENT_REPEAT2_START;
			EventMap[0xFB] = SQUARE_EVENT_COND_JUMP;
			EventMap[0xFC] = SQUARE_EVENT_NOP1;
			EventMap[0xFD] = SQUARE_EVENT_NOP;
			EventMap[0xFE] = SQUARE_EVENT_INS_PARAMS;
			EventMap[0xFF] = SQUARE_EVENT_STOP;
			break;
		}
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
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
			repeat1 = -1;
			repeat2 = -1;
			jumpPos = 0;
			jumpPosRet = 0;
			holdNote = 0;
			curVol = 120;

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

			if (ptrs[curTrack] >= bankSize && ptrs[curTrack] <= (bankSize * 2))
			{
				seqEnd = 0;
				seqPos = ptrs[curTrack];

				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
					command[4] = romData[seqPos + 4];
					command[5] = romData[seqPos + 5];
					command[6] = romData[seqPos + 6];
					command[7] = romData[seqPos + 7];

					if (curTrack == 2)
					{
						curTrack = 2;
					}

					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);

					if (command[0] >= SQUARE_STATUS_NOTE_MIN && command[0] <= SQUARE_STATUS_NOTE_MAX)
					{
						if (drvVers != SQUARE_VER_FFL2)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							curNote = lowNibble + (octave * 12) + 24;
							if (curTrack < 2)
							{
								curNote += 12;
							}
							if (drvVers == SQUARE_VER_FFL1)
							{
								curNoteLen = NoteLen1[highNibble] * 5;
							}
							else
							{
								curNoteLen = NoteLen3[highNibble] * 5;
							}
							midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							firstNote = 0;
							holdNote = 1;
						}
						else
						{
							curNoteLen = NoteLen2[lowNibble] * 5;
							/*Tie*/
							if (highNibble == SQUARE_STATUS_TIE)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
								masterDelay += curNoteLen;
							}
							else if (highNibble == SQUARE_STATUS_REST)
							{
								if (holdNote == 1)
								{
									tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									curDelay = 0;
									holdNote = 0;
									midPos = tempPos;
								}
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
								masterDelay += curNoteLen;
							}
							else
							{
								if (holdNote == 1)
								{
									tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									curDelay = 0;
									holdNote = 0;
									midPos = tempPos;
								}
								curNote = highNibble + (octave * 12) + 24;
								if (curTrack < 2)
								{
									curNote += 12;
								}
								midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = curNoteLen;
								ctrlDelay += curNoteLen;
								masterDelay += curNoteLen;
								firstNote = 0;
								holdNote = 1;
							}
						}
						seqPos++;
					}

					else if (command[0] >= SQUARE_STATUS_TIE_MIN && command[0] <= SQUARE_STATUS_TIE_MAX && drvVers != SQUARE_VER_FFL2)
					{
						if (drvVers == SQUARE_VER_FFL1)
						{
							curNoteLen = NoteLen1[highNibble] * 5;
						}
						else
						{
							curNoteLen = NoteLen3[highNibble] * 5;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;
					}

					else if (command[0] >= SQUARE_STATUS_REST_MIN && command[0] <= SQUARE_STATUS_REST_MAX && drvVers != SQUARE_VER_FFL2)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						if (drvVers == SQUARE_VER_FFL1)
						{
							curNoteLen = NoteLen1[highNibble] * 5;
						}
						else
						{
							curNoteLen = NoteLen3[highNibble] * 5;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_UNKNOWN0 || EventMap[command[0]] == SQUARE_EVENT_NOP)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_UNKNOWN1 || EventMap[command[0]] == SQUARE_EVENT_NOP1)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_TEMPO)
					{
						if (drvVers == SQUARE_VER_FFL1)
						{
							songTempo = command[1] * 2.5;
						}
						else
						{
							songTempo = command[1] * 1.25;
						}

						if (tempo != songTempo)
						{
							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							if (drvVers == SQUARE_VER_FFL1)
							{
								tempo = (command[1] * 2.5);
							}
							else
							{
								tempo = (command[1] * 1.25);
							}
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
						}
						seqPos += 2;

					}

					else if (EventMap[command[0]] == SQUARE_EVENT_VOL)
					{
						if (drvVers == SQUARE_VER_FFL1)
						{
							seqPos++;
						}
						else
						{
							if (curTrack != 2)
							{
								seqPos += 3;
							}
							else
							{
								seqPos += 2;
							}
						}

					}

					else if (EventMap[command[0]] == SQUARE_EVENT_OCTAVE)
					{
						octave = command[0] - SQUARE_STATUS_OCTAVE_MIN;
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_NRX1)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_ENV)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_WAVEFORM)
					{
						if (drvVers == SQUARE_VER_FFL1)
						{
							seqPos += 2;
						}
						else
						{
							seqPos += 3;
						}

					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT1X)
					{
						repeat1 = 1;
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT2X)
					{
						repeat1 = 2;
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT3X)
					{
						repeat1 = 3;
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT_END)
					{
						jumpPos = ReadLE16(&romData[seqPos + 1]);
						if (repeat1 > 0)
						{
							seqPos = jumpPos;
							repeat1--;
						}
						else
						{
							seqPos += 3;
						}
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_COND_JUMP)
					{
						if (drvVers == SQUARE_VER_FFL1)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]);
							if (repeat1 <= 0)
							{
								seqPos = jumpPos;
							}
							else
							{
								seqPos += 3;
							}
						}
						else
						{
							jumpPos = ReadLE16(&romData[seqPos + 2]);
							if (repeat1 == command[1])
							{
								seqPos = jumpPos;
							}
							else
							{
								seqPos += 4;
							}
						}
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_JUMP)
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

					else if (EventMap[command[0]] == SQUARE_EVENT_STOP)
					{
						seqEnd = 1;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_OCTAVE_UP)
					{
						octave += ((command[0] - SQUARE_STATUS_OCTAVE_UP_MIN) + 1);
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_OCTAVE_DOWN)
					{
						octave -= ((command[0] - SQUARE_STATUS_OCTAVE_DOWN_MIN) + 1);
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT1_END)
					{
						jumpPos = ReadLE16(&romData[seqPos + 1]);
						if (repeat1 > 1)
						{
							seqPos = jumpPos;
							repeat1--;
						}
						else if (repeat1 == -1)
						{
							seqEnd = 1;
						}
						else
						{
							seqPos += 3;
						}
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT1_START)
					{
						repeat1 = command[1];
						seqPos += 2;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_VIBRATO)
					{
						seqPos += 3;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_DUTY)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_PAN)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT2_END)
					{
						jumpPos = ReadLE16(&romData[seqPos + 1]);
						if (repeat2 > 1)
						{
							seqPos = jumpPos;
							repeat2--;
						}
						else if (repeat2 == -1)
						{
							seqEnd = 1;
						}
						else
						{
							seqPos += 3;
						}
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_REPEAT2_START)
					{
						repeat2 = command[1];
						seqPos += 2;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_DOUBLE_TIME)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == SQUARE_EVENT_INS_PARAMS)
					{
						if (curTrack != 1)
						{
							seqPos += 7;
						}
						else
						{
							songTempo = command[1] * 1.25;

							if (tempo != songTempo)
							{
								ctrlMidPos++;
								valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
								ctrlDelay = 0;
								ctrlMidPos += valSize;
								WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
								ctrlMidPos += 3;
								tempo = (command[1] * 1.25);
								WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
								ctrlMidPos += 2;
							}
							seqPos += 8;
						}
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
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