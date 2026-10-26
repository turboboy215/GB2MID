/*Nintendo (common driver - Hirokazu Tanaka)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <direct.h>
#include "SHARED.H"
#include "NINTENDO.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long curPtr;
long songPtrs[4];
long songPtr;
long bankAmt;
int curVol;
int drvVers;
int foundTable;
int songTempo;
int curInst;
int progBank;

int multiBanks;
int curBank;

char folderName[100];

int totalLen;

int songTrans;
int startTrans;
int endList;

/*Most common variant*/
const unsigned char NintMagicBytes[3] = { 0xE6, 0x1F, 0xCD };

/*Lunar Chase/Super Mario Land 2*/
const unsigned char NintMagicBytes2[3] = { 0xE6, 0x3F, 0xCD };

/*Game Boy Wars*/
const unsigned char NintMagicBytes3[5] = { 0xC3, 0x78, 0x77, 0x47, 0x21 };

/*Game Boy Camera*/
const unsigned char NintMagicBytes4[3] = { 0xE6, 0x7F, 0xCD };

/*Metroid 2*/
const char NintMagicBytesM2[6] = { 0xE0, 0x25, 0xFA, 0xDC, 0xCE, 0x21 };

/*Wario Land*/
const char NintMagicBytesWL[6] = { 0xE0, 0x25, 0xFA, 0x1C, 0xA6, 0x21 };

/*Terrifying 9/11*/
const char NintMagicBytesHitek[2] = { 0x47, 0x21 };

unsigned char* romData;
unsigned char* multiMidData[8];
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
void Nintsong2mid(int songNum, long ptr);

void NintProc(int bank, char parameters[4][100])
{
	drvVers = NINT_VER_STD;
	curInst = 0;
	curVol = 120;
	foundTable = 0;
	endList = 0;

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);

		if (drvVers < NINT_VER_STD || drvVers > NINT_VER_HITEK)
		{
			printf("ERROR: Invalid version number!\n");
			exit(-1);
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

	/*Try to search the bank for base table*/
	for (i = bankSize; (i < bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes, 3) && ReadLE16(&romData[i + 3]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = i - 2;
			}

			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*If not found, try again for another version*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes2, 3) && ReadLE16(&romData[i + 3]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = i - 2;
			}

			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Another version*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes3, 5) && ReadLE16(&romData[i + 5]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = i + 5;
			}

			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Yet another version*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes4, 3) && ReadLE16(&romData[i + 3]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = i - 2;
			}

			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;

			break;
		}
	}

	if (drvVers == NINT_VER_MET2)
	{
		for (i = bankSize; i < (bankSize * 2); i++)
		{
			if (!memcmp(&romData[i], NintMagicBytesM2, 6))
			{
				tablePtrLoc = i + 6;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;

				break;
			}
		}

		for (i = bankSize; i < (bankSize * 2); i++)
		{
			if (!memcmp(&romData[i], NintMagicBytesWL, 6))
			{
				tablePtrLoc = i + 6;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;

				break;
			}
		}
	}

	if (drvVers == NINT_VER_HITEK)
	{
		for (i = bankSize; i < (bankSize * 2); i++)
		{
			if (!memcmp(&romData[i], NintMagicBytesHitek, 2) && romData[i - 2] == 0xCA && romData[i + 4] == 0xCD)
			{
				tablePtrLoc = i + 2;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
				break;
			}
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;
		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Nintsong2mid(songNum, songPtr);
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
		exit(1);
	}


}

/*Convert the song data to MIDI*/
void Nintsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[4];
	int curTrack = 0;
	int curNote = 0;
	int tempNote = 0;
	unsigned int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int curNoteLenTab;
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	int masterDelay = 0;
	int masterDelays[4];
	int octave = 4;
	int octaves[4];
	int seqEnd = 0;
	int tracksEnd[4] = { 0, 0, 0, 0 };
	int songEnd = 0;
	unsigned int seqPos = 0;
	unsigned int seqPosM[4];
	unsigned int patPos;
	unsigned int patPosM[4];
	unsigned int noteLensPtr;
	unsigned int romPos = 0;
	unsigned int midPos = 0;
	unsigned int midPosM[4];
	long ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	long tempPos = 0;
	int valSize = 0;
	long trackSize = 0;
	long trackSizes[4];
	long ctrlTrackSize = 0;
	int initTempo = 0;
	int tempo = 150;
	long jumpPos = 0;
	long jumpPosEnd = 0;
	int curVol = 0;
	int curVols[4];
	int curSpeeds[4];
	int holdNote = 0;
	int holdNotes[4];
	int transpose = 0;
	int transposes[4];
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int firstNote = 0;
	int firstNotes[4];
	long seqTime = 0;
	int curInsts[4];
	int repeats[4][2];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long framePtr;
	int numChan;
	int envVals[4];

	for (curTrack = 0; curTrack < trackCnt; curTrack++)
	{
		midPosM[curTrack] = 0;
	}

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < trackCnt; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < trackCnt; k++)
		{
			multiMidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < trackCnt; k++)
		{
			multiMidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	if (multiBanks == 0)
	{
		sprintf(outfile, "song%d.mid", songNum);
	}
	else
	{
		sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
	}
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
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

		switch (drvVers)
		{
		case NINT_VER_SML2:
			NINT_STATUS_END = 0x00;
			NINT_STATUS_REST = 0x01;
			NINT_STATUS_NOTE_MIN = 0x02;
			NINT_STATUS_NOTE_MAX = 0x90;
			EventMap[0x95] = NINT_EVENT_BEND_ON;
			EventMap[0x96] = NINT_EVENT_BEND_OFF;
			EventMap[0x97] = NINT_EVENT_DROP_ON;
			EventMap[0x98] = NINT_EVENT_DROP_OFF;
			EventMap[0x99] = NINT_EVENT_CH3_FADE_ON;
			EventMap[0x9A] = NINT_EVENT_CH3_FADE_OFF;
			EventMap[0x9B] = NINT_EVENT_REPEAT_START;
			EventMap[0x9C] = NINT_EVENT_REPEAT_END;
			EventMap[0x9D] = NINT_EVENT_INS_PARAMS;
			EventMap[0x9E] = NINT_EVENT_NOTE_LEN_TABLE;
			EventMap[0x9F] = NINT_EVENT_TRANSPOSE;
			for (j = 0xA0; j <= 0xAF; j++)
			{
				EventMap[j] = NINT_EVENT_NOTE_LEN;
			}
			break;
		case NINT_VER_LA:
			NINT_STATUS_END = 0x00;
			NINT_STATUS_REST = 0x01;
			NINT_STATUS_NOTE_MIN = 0x02;
			NINT_STATUS_NOTE_MAX = 0x90;
			EventMap[0x94] = NINT_EVENT_CH3_FADE_IN_ON;
			EventMap[0x95] = NINT_EVENT_CH3_SFX_RESUME;
			EventMap[0x96] = NINT_EVENT_CH3_SFX_PAUSE;
			EventMap[0x97] = NINT_EVENT_DROP_ON;
			EventMap[0x98] = NINT_EVENT_DROP_OFF;
			EventMap[0x99] = NINT_EVENT_CH3_FADE_ON;
			EventMap[0x9A] = NINT_EVENT_CH3_FADE_OFF;
			EventMap[0x9B] = NINT_EVENT_REPEAT_START;
			EventMap[0x9C] = NINT_EVENT_REPEAT_END;
			EventMap[0x9D] = NINT_EVENT_INS_PARAMS;
			EventMap[0x9E] = NINT_EVENT_NOTE_LEN_TABLE;
			EventMap[0x9F] = NINT_EVENT_TRANSPOSE;
			for (j = 0xA0; j <= 0xAF; j++)
			{
				EventMap[j] = NINT_EVENT_NOTE_LEN;
			}
			break;
		case NINT_VER_SMBDX:
			NINT_STATUS_END = 0x00;
			NINT_STATUS_REST = 0x01;
			NINT_STATUS_NOTE_MIN = 0x02;
			NINT_STATUS_NOTE_MAX = 0x90;
			EventMap[0x97] = NINT_EVENT_DROP_ON;
			EventMap[0x98] = NINT_EVENT_DROP_OFF;
			EventMap[0x99] = NINT_EVENT_REST_ENV;
			EventMap[0x9A] = NINT_EVENT_GLOBAL_VOL;
			EventMap[0x9B] = NINT_EVENT_REPEAT_START;
			EventMap[0x9C] = NINT_EVENT_REPEAT_END;
			EventMap[0x9D] = NINT_EVENT_INS_PARAMS;
			for (j = 0xA0; j <= 0xAF; j++)
			{
				EventMap[j] = NINT_EVENT_NOTE_LEN;
			}
			break;
		case NINT_VER_KIRBY:
			NINT_STATUS_END = 0x00;
			NINT_STATUS_REST = 0x01;
			NINT_STATUS_NOTE_MIN = 0x02;
			NINT_STATUS_NOTE_MAX = 0x90;
			NINT_PAT_STATUS_ENV = 0x94;
			NINT_PAT_STATUS_ADD_TRANSPOSE = 0x95;
			NINT_PAT_STATUS_TRANSPOSE = 0x96;
			EventMap[0x92] = NINT_EVENT_DRUM_KIT;
			EventMap[0x93] = NINT_EVENT_ADD_ENV;
			EventMap[0x94] = NINT_EVENT_ENV;
			EventMap[0x95] - NINT_EVENT_ADD_TRANSPOSE;
			EventMap[0x96] = NINT_EVENT_TRANSPOSE;
			EventMap[0x97] = NINT_EVENT_DROP_ON;
			EventMap[0x98] = NINT_EVENT_DROP_OFF;
			EventMap[0x99] = NINT_EVENT_REST_ENV;
			EventMap[0x9A] = NINT_EVENT_GLOBAL_VOL;
			EventMap[0x9B] = NINT_EVENT_REPEAT_START;
			EventMap[0x9C] = NINT_EVENT_REPEAT_END;
			EventMap[0x9D] = NINT_EVENT_INS_PARAMS;
			EventMap[0x9E] = NINT_EVENT_NOTE_LEN_TABLE;
			for (j = 0xA0; j <= 0xAF; j++)
			{
				EventMap[j] = NINT_EVENT_NOTE_LEN;
			}
			break;
		case NINT_VER_MET2:
			NINT_STATUS_END = 0x00;
			NINT_STATUS_REST = 0x01;
			NINT_STATUS_NOTE_MIN = 0x02;
			NINT_STATUS_NOTE_MAX = 0x9F;
			NINT_STATUS_ECHO1 = 0x03;
			NINT_STATUS_ECHO2 = 0x05;
			EventMap[0xF1] = NINT_EVENT_INS_PARAMS;
			EventMap[0xF2] = NINT_EVENT_NOTE_LEN_TABLE;
			EventMap[0xF3] = NINT_EVENT_TRANSPOSE;
			EventMap[0xF4] = NINT_EVENT_REPEAT_START;
			EventMap[0xF5] = NINT_EVENT_REPEAT_END;
			for (j = 0xA0; j <= 0xAF; j++)
			{
				EventMap[j] = NINT_EVENT_NOTE_LEN;
			}
			break;
		case NINT_VER_HITEK:
			NINT_STATUS_NOTE_MIN = 0x00;
			NINT_STATUS_NOTE_MAX = 0xBF;
			for (j = 0xC0; j <= 0xCF; j++)
			{
				EventMap[j] = NINT_EVENT_HITEK_REST;
			}
			for (j = 0xD0; j <= 0xDF; j++)
			{
				EventMap[j] = NINT_EVENT_INS_PARAMS;
			}
			for (j = 0xE0; j <= 0xE5; j++)
			{
				EventMap[j] = NINT_EVENT_HITEK_OCTAVE;
			}
			EventMap[0xE6] = NINT_EVENT_HITEK_SPEED;
			EventMap[0xE7] = NINT_EVENT_HITEK_CH3_FADE_OUT_ON;
			EventMap[0xE8] = NINT_EVENT_HITEK_CH3_FADE_OFF;
			EventMap[0xE9] = NINT_EVENT_CH3_FADE_IN_ON;
			EventMap[0xEA] = NINT_EVENT_TRANSPOSE;
			EventMap[0xEB] = NINT_EVENT_DROP_ON;
			EventMap[0xEC] = NINT_EVENT_DROP_OFF;
			EventMap[0xED] = NINT_EVENT_CH3_SFX_PAUSE;
			EventMap[0xEE] = NINT_EVENT_CH3_SFX_RESUME;
			EventMap[0xEF] = NINT_EVENT_HITEK_POKE;
			for (j = 0xF0; j <= 0xFF; j++)
			{
				EventMap[j] = NINT_EVENT_HITEK_END;
			}
			EventMap[0xFB] = NINT_EVENT_REPEAT_START;
			EventMap[0xFD] = NINT_EVENT_HITEK_JUMP;
			EventMap[0xFE] = NINT_EVENT_REPEAT_END;

			for (j = 0xA0; j <= 0xAF; j++)
			{
				EventMap[j] = NINT_EVENT_NOTE_LEN;
			}
			break;
		case NINT_VER_STD:
			/*Fall-through*/
		default:
			NINT_STATUS_END = 0x00;
			NINT_STATUS_REST = 0x01;
			NINT_STATUS_NOTE_MIN = 0x02;
			NINT_STATUS_NOTE_MAX = 0x90;
			EventMap[0x9B] = NINT_EVENT_REPEAT_START;
			EventMap[0x9C] = NINT_EVENT_REPEAT_END;
			EventMap[0x9D] = NINT_EVENT_INS_PARAMS;
			EventMap[0x9E] = NINT_EVENT_NOTE_LEN_TABLE;
			EventMap[0x9F] = NINT_EVENT_TRANSPOSE;
			for (j = 0xA0; j <= 0xAF; j++)
			{
				EventMap[j] = NINT_EVENT_NOTE_LEN;
			}
			break;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			songPtrs[curTrack] = 0x0000;
			repeats[curTrack][0] = -1;
			repeats[curTrack][1] = 0x0000;
		}


		/*Get channel pointers*/
		romPos = songPtr;
		if (drvVers == NINT_VER_SML2)
		{
			songTrans = (signed char)romData[romPos] / 2;
			noteLensPtr = ReadLE16(&romData[romPos] + 1);
			songPtrs[0] = ReadLE16(&romData[romPos] + 5);
			songPtrs[1] = ReadLE16(&romData[romPos] + 7);
			songPtrs[2] = ReadLE16(&romData[romPos] + 9);
			songPtrs[3] = ReadLE16(&romData[romPos] + 11);
		}
		else if (drvVers == NINT_VER_HITEK)
		{
			numChan = romData[romPos];
			romPos++;
			for (j = 0; j <= numChan; j++)
			{
				curTrack = romData[romPos];
				romPos++;

				if (curTrack > 3)
				{
					curTrack = 0;
				}

				songPtrs[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}
		}
		else
		{
			if (drvVers != NINT_VER_KIRBY)
			{
				songTrans = (signed char)romData[romPos] / 2;
			}
			else
			{
				songTrans = (signed char)romData[romPos];
			}
			noteLensPtr = ReadLE16(&romData[romPos] + 1);
			songPtrs[0] = ReadLE16(&romData[romPos] + 3);
			songPtrs[1] = ReadLE16(&romData[romPos] + 5);
			songPtrs[2] = ReadLE16(&romData[romPos] + 7);
			songPtrs[3] = ReadLE16(&romData[romPos] + 9);
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			curDelays[curTrack] = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			curNotes[curTrack] = 0;
			curNoteLens[curTrack] = 0;
			curInsts[curTrack] = 0;
			tracksEnd[curTrack] = 0;
			transposes[curTrack] = 0;
			transpose = 0;
			curSpeeds[curTrack] = 0;
			envVals[curTrack] = 0xFF;

			if (drvVers != NINT_VER_MET2 && drvVers != NINT_VER_KIRBY && drvVers != NINT_VER_HITEK)
			{
				if (songPtrs[curTrack] != 0x0000)
				{
					patPosM[curTrack] = songPtrs[curTrack];

					if (ReadLE16(&romData[patPosM[curTrack]]) == 0x0000)
					{
						for (j = 0; j < 4; j++)
						{
							songEnd = 1;
							tracksEnd[j] = 1;
						}
					}
					else if (ReadLE16(&romData[patPosM[curTrack]]) == 0xFFFF)
					{
						tracksEnd[curTrack] = 1;
					}
					else
					{
						seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
					}
				}
				else
				{
					seqPosM[curTrack] = songPtrs[curTrack];
					tracksEnd[curTrack] = 1;
				}

			}
			else if (drvVers == NINT_VER_KIRBY)
			{
				if (songPtrs[curTrack] != 0x0000)
				{
					patPosM[curTrack] = songPtrs[curTrack];
					seqEnd = 0;
					while (seqEnd == 0)
					{
						if (romData[patPosM[curTrack] + 1] == 0x00)
						{
							for (j = 0; j < 4; j++)
							{
								songEnd = 1;
								tracksEnd[j] = 1;
							}
							seqEnd = 1;
						}
						else if (romData[patPosM[curTrack] + 1] == 0xFF)
						{
							if (romData[patPosM[curTrack]] == NINT_PAT_STATUS_ENV)
							{
								envVals[curTrack] = romData[patPosM[curTrack]];
								patPosM[curTrack] += 4;
							}
							else if (romData[patPosM[curTrack]] == NINT_PAT_STATUS_ADD_TRANSPOSE)
							{
								transposes[curTrack] += ((signed char)romData[patPosM[curTrack] + 2]);
								patPosM[curTrack] += 4;
							}
							else if (romData[patPosM[curTrack]] == NINT_PAT_STATUS_TRANSPOSE)
							{
								transposes[curTrack] = (((signed char)romData[patPosM[curTrack] + 2]) / 2);
								patPosM[curTrack] += 4;
							}
							else
							{
								tracksEnd[curTrack] = 1;
								seqEnd = 1;
							}
						}
						else
						{
							seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
							seqEnd = 1;
						}
					}
				}
				else
				{
					seqPosM[curTrack] = songPtrs[curTrack];
					tracksEnd[curTrack] = 1;
				}
			}
			else if (drvVers == NINT_VER_MET2)
			{
				if (songPtrs[curTrack] != 0x0000)
				{
					patPosM[curTrack] = songPtrs[curTrack];

					if (ReadLE16(&romData[patPosM[curTrack]]) == 0x0000)
					{
						if (drvVers != NINT_VER_MET2)
						{
							for (j = 0; j < 4; j++)
							{
								songEnd = 1;
								tracksEnd[j] = 1;
							}
						}
						else
						{
							tracksEnd[curTrack] = 1;
						}

					}
					else if (ReadLE16(&romData[patPosM[curTrack]]) == 0x00F0)
					{
						tracksEnd[curTrack] = 1;
					}
					else
					{
						seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
					}
				}
				else
				{
					seqPosM[curTrack] = songPtrs[curTrack];
					tracksEnd[curTrack] = 1;
				}
			}
			else
			{
				seqPosM[curTrack] = songPtrs[curTrack];

				if (songPtrs[curTrack] == 0x0000)
				{
					tracksEnd[curTrack] = 1;
				}

			}

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

		ctrlDelay = 0;
		seqTime = 0;

		while (songEnd == 0)
		{
			if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1 && tracksEnd[3] == 1)
			{
				songEnd = 1;
			}

			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				if (tracksEnd[curTrack] != 1)
				{
					while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] != 1)
					{
						if (seqPosM[curTrack] >= 0x8000)
						{
							tracksEnd[curTrack] = 1;
						}
						else
						{
							command[0] = romData[seqPosM[curTrack]];
							command[1] = romData[seqPosM[curTrack] + 1];
							command[2] = romData[seqPosM[curTrack] + 2];
							command[3] = romData[seqPosM[curTrack] + 3];

							lowNibble = (command[0] >> 4);
							highNibble = (command[0] & 15);

							if (command[0] == NINT_STATUS_END && drvVers != NINT_VER_HITEK)
							{
								patPosM[curTrack] += 2;

								if (drvVers != NINT_VER_KIRBY)
								{
									if (ReadLE16(&romData[patPosM[curTrack]]) == 0x0000)
									{
										if (drvVers != NINT_VER_MET2)
										{
											for (j = 0; j < 4; j++)
											{
												tracksEnd[j] = 1;
											}
											songEnd = 1;
										}
										else
										{
											tracksEnd[curTrack] = 1;
										}
									}
									else if (ReadLE16(&romData[patPosM[curTrack]]) == 0xFFFF && drvVers != NINT_VER_MET2)
									{
										tracksEnd[curTrack] = 1;
									}
									else if (ReadLE16(&romData[patPosM[curTrack]]) == 0x00F0 && drvVers == NINT_VER_MET2)
									{
										tracksEnd[curTrack] = 1;
									}
									else
									{
										seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
									}
								}
								else
								{
									seqEnd = 0;
									while (seqEnd == 0)
									{
										if (romData[patPosM[curTrack] + 1] == 0x00)
										{
											for (j = 0; j < 4; j++)
											{
												songEnd = 1;
												tracksEnd[j] = 1;
											}
											seqEnd = 1;
										}
										else if (romData[patPosM[curTrack] + 1] == 0xFF)
										{
											if (romData[patPosM[curTrack]] == NINT_PAT_STATUS_ENV)
											{
												envVals[curTrack] = romData[patPosM[curTrack]];
												patPosM[curTrack] += 4;
											}
											else if (romData[patPosM[curTrack]] == NINT_PAT_STATUS_ADD_TRANSPOSE)
											{
												transposes[curTrack] += ((signed char)romData[patPosM[curTrack] + 2]);
												patPosM[curTrack] += 4;
											}
											else if (romData[patPosM[curTrack]] == NINT_PAT_STATUS_TRANSPOSE)
											{
												transposes[curTrack] = (((signed char)romData[patPosM[curTrack] + 2]));
												patPosM[curTrack] += 4;
											}
											else
											{
												tracksEnd[curTrack] = 1;
												seqEnd = 1;
											}
										}
										else
										{
											seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
											seqEnd = 1;
										}
									}
								}

							}

							else if (command[0] == NINT_STATUS_REST)
							{
								curDelays[curTrack] += curNoteLens[curTrack];
								masterDelays[curTrack] += curNoteLens[curTrack];
								seqPosM[curTrack]++;
							}

							else if (command[0] >= NINT_STATUS_NOTE_MIN && command[0] <= NINT_STATUS_NOTE_MAX)
							{
								if (drvVers != NINT_VER_HITEK)
								{
									curNotes[curTrack] = (command[0] / 2);

									if (curTrack != 3)
									{
										curNotes[curTrack] += (songTrans + transpose + transposes[curTrack]);
									}
									if (curNotes[curTrack] > 0)
									{
										curNotes[curTrack]--;
									}
								}
								else
								{
									curNotes[curTrack] = lowNibble + ((5 - octaves[curTrack]) * 12) + transpose;

									if (curNotes[curTrack] > 128)
									{
										curNotes[curTrack] = 0;
									}
									if (curNotes[curTrack] > 0)
									{
										curNotes[curTrack]--;
									}
									curNoteLens[curTrack] = ((curSpeeds[curTrack] + 1) * (highNibble + 1)) * 5;
								}
								curNotes[curTrack] += 24;

								if (curTrack != 2)
								{
									curNotes[curTrack] += 12;
								}

								if (curTrack < 2)
								{
									/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
									if ((envVals[curTrack] & 0x08) == 0x00)
									{
										curVols[curTrack] = (envVals[curTrack] & 0xF0) / 2;
									}
									else
									{
										if (((envVals[curTrack] & 0xF0) / 2) == curVols[curTrack])
										{
											curVols[curTrack] = (envVals[curTrack] & 0xF0) / 2;
										}
										else
										{
											curVols[curTrack] = 120;
										}

									}

									if (curVols[curTrack] == 0)
									{
										curVols[curTrack] = 1;
									}

								}
								else if (curTrack == 2)
								{
									switch ((envVals[curTrack] & 0x7F))
									{
									case 0x20:
										curVols[curTrack] = 120;
										break;
									case 0x40:
										curVols[curTrack] = 60;
										break;
									case 0x60:
										curVols[curTrack] = 30;
										break;
									case 0x00:
										curVols[curTrack] = 1;
										break;
									default:
										curVols[curTrack] = 120;
										break;
									}
								}
								curVol = curVols[curTrack];

								tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								firstNotes[curTrack] = 0;
								holdNotes[curTrack] = 1;
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								masterDelays[curTrack] += curNoteLens[curTrack];
								seqPosM[curTrack]++;
							}

							else if (command[0] == NINT_STATUS_ECHO1 && drvVers == NINT_VER_MET2)
							{
								curVol = curVols[curTrack] / 2;
								if (curVol == 0)
								{
									curVol = 1;
								}
								tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								firstNotes[curTrack] = 0;
								holdNotes[curTrack] = 1;
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								masterDelays[curTrack] += curNoteLens[curTrack];
								curVols[curTrack] = 120;
								seqPosM[curTrack]++;
							}

							else if (command[0] == NINT_STATUS_ECHO2 && drvVers == NINT_VER_MET2)
							{
								curVol = curVols[curTrack] / 2;
								if (curVol == 0)
								{
									curVol = 1;
								}
								curVol = curVols[curTrack];
								tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								firstNotes[curTrack] = 0;
								holdNotes[curTrack] = 1;
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								masterDelays[curTrack] += curNoteLens[curTrack];
								curVols[curTrack] = 120;
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_NOTE_LEN)
							{
								if (drvVers != NINT_VER_HITEK)
								{
									curNoteLens[curTrack] = romData[noteLensPtr + (command[0] & 0x0F)] * 5;
								}
								else
								{
									curNoteLens[curTrack] = (command[0] * curSpeeds[curTrack]) * 5;
								}

								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_REPEAT_START)
							{
								if (drvVers != NINT_VER_HITEK)
								{
									repeats[curTrack][0] = command[1];
									repeats[curTrack][1] = seqPosM[curTrack] + 2;
									seqPosM[curTrack] += 2;
								}
								else
								{
									repeats[curTrack][0] = 0;
									repeats[curTrack][1] = seqPosM[curTrack] + 1;
									seqPosM[curTrack]++;
								}

							}

							else if (EventMap[command[0]] == NINT_EVENT_REPEAT_END)
							{
								if (drvVers != NINT_VER_HITEK)
								{
									if (repeats[curTrack][0] > 1)
									{
										seqPosM[curTrack] = repeats[curTrack][1];
										repeats[curTrack][0]--;
									}
									else
									{
										repeats[curTrack][0] = -1;
										seqPosM[curTrack]++;
									}
								}
								else
								{
									if (repeats[curTrack][0] < command[1])
									{
										repeats[curTrack][0]++;
										seqPosM[curTrack] = repeats[curTrack][1];
									}
								}

							}

							else if (EventMap[command[0]] == NINT_EVENT_INS_PARAMS)
							{
								if (drvVers != NINT_VER_SMBDX && drvVers != NINT_VER_HITEK)
								{
									envVals[curTrack] = command[1];
								}
								else if (drvVers == NINT_VER_SMBDX)
								{
									if (command[1] == 0x00)
									{
										envVals[curTrack] = command[1];
									}
									else
									{
										envVals[curTrack] = romData[ReadLE16(&romData[seqPosM[curTrack + 1]])];
									}
								}
								else
								{
									curSpeeds[curTrack] = highNibble;
									envVals[curTrack] = command[1];
								}
								seqPosM[curTrack] += 4;
							}

							else if (EventMap[command[0]] == NINT_EVENT_NOTE_LEN_TABLE)
							{
								noteLensPtr = ReadLE16(&romData[seqPosM[curTrack] + 1]);
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == NINT_EVENT_TRANSPOSE)
							{
								if (drvVers != NINT_VER_HITEK && drvVers != NINT_VER_KIRBY)
								{
									transpose = ((signed char)command[1]) / 2;
								}
								else if (drvVers == NINT_VER_KIRBY)
								{
									transposes[curTrack] = ((signed char)command[1]);
								}
								else
								{
									transpose = (signed char)command[1];
								}
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == NINT_EVENT_BEND_ON)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_BEND_OFF)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_DROP_ON)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_DROP_OFF)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_CH3_FADE_ON)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_CH3_FADE_OFF)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_CH3_FADE_IN_ON)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_CH3_SFX_RESUME)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_CH3_SFX_PAUSE)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_REST_ENV)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == NINT_EVENT_GLOBAL_VOL)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == NINT_EVENT_DRUM_KIT)
							{
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == NINT_EVENT_ADD_ENV)
							{
								envVals[curTrack] += (signed char)command[1];
								envVals[curTrack] &= 0x00FF;
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == NINT_EVENT_ENV)
							{
								envVals[curTrack] = command[1];
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == NINT_EVENT_ADD_TRANSPOSE)
							{
								transposes[curTrack] += ((signed char)command[1]);
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_REST && drvVers == NINT_VER_HITEK)
							{
								curNoteLens[curTrack] = ((curSpeeds[curTrack] + 1) * (highNibble + 1)) * 5;
								curDelays[curTrack] += curNoteLens[curTrack];
								masterDelays[curTrack] += curNoteLens[curTrack];
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_OCTAVE && drvVers == NINT_VER_HITEK)
							{
								octaves[curTrack] = highNibble;
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_SPEED && drvVers == NINT_VER_HITEK)
							{
								curSpeeds[curTrack] = command[1];
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_CH3_FADE_OUT_ON && drvVers == NINT_VER_HITEK)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_CH3_FADE_OFF && drvVers == NINT_VER_HITEK)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_POKE && drvVers == NINT_VER_HITEK)
							{
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_END && drvVers == NINT_VER_HITEK)
							{
								for (j = 0; j < 4; j++)
								{
									tracksEnd[j] = 1;
								}
								songEnd = 1;
							}

							else if (EventMap[command[0]] == NINT_EVENT_HITEK_JUMP && drvVers == NINT_VER_HITEK)
							{
								if ((ReadLE16(&romData[seqPosM[curTrack] + 1])) > seqPosM[curTrack])
								{
									seqPosM[curTrack] = ReadLE16(&romData[seqPosM[curTrack] + 1]);
								}
								else
								{
									tracksEnd[curTrack] = 1;
								}

							}

							/*Unknown command*/
							else
							{
								seqPosM[curTrack]++;
							}
						}

					}
				}

			}

			seqTime += 5;
			ctrlDelay += 5;



		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{

			/*End of track*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
			midPosM[curTrack] += 4;
			firstNotes[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
			free(multiMidData[curTrack]);
		}

		free(ctrlMidData);
		fclose(mid);

	}

}
