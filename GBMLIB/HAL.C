/*HAL Laboratory*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "HAL.H"

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
int bankA;
int tempPos;
int noteLenTabLoc;
int noteLenTabOffset;
int segmented;
int segOffset;
int curVol;
int drvVers;
int curInst;
int progBank;
long patGroupTab;
long patPeriodTab;

int multiBanks;
int curBank;

char folderName[100];

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
void HALsong2mid(int songNum, long songPtr);

const char HALMagicBytesA[6] = { 0xB8, 0xC8, 0x58, 0x16, 0x00, 0x21 };
const char HALMagicBytesB[4] = { 0x19, 0x19, 0x2A, 0x5F };
const char HALMagicBytesC[5] = { 0xC9, 0x58, 0x16, 0x00, 0x21 };
const char HALMagicBytesD[4] = { 0x4F, 0x06, 0x00, 0x21 };
const char HALMagicBytesLen[11] = { 0x80, 0x6F, 0x1A, 0xE6, 0xE0, 0xCB, 0x37, 0xCB, 0x3F, 0x86, 0xC6 };
const char HALMagicBytesLen2[5] = { 0x06, 0x00, 0x09, 0x86, 0xC6 };
const char HALsegSearch[5] = { 0x86, 0x5F, 0x16, 0x00, 0x21 };

void HALProc(int bank, char parameters[4][100])
{
	drvVers = HAL_VER_KDL;
	curVol = 120;
	curInst = 0;
	progBank = 0;
	foundTable = 0;
	segmented = 0;

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

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers < HAL_VER_KDL && drvVers > HAL_VER_GB2)
		{
			printf("ERROR: Invalid version number!\n");
			free(romData);
			free(exRomData);
			fclose(rom);
			exit(-1);
		}
	}

	/*Copy bank 1 data if not bank 1*/
	if ((drvVers == HAL_VER_KDL2 && bank == 0x20) || (drvVers == HAL_VER_KSS && bank == 0x06))
	{
		bankAmt = bankSize;
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, ((bank - 2) * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
	}
	else
	{

		if (drvVers == HAL_VER_LOLO)
		{
			for (i = 0; i < bankSize; i++)
			{
				if ((!memcmp(&romData[i], HALsegSearch, 5)))
				{
					segOffset = ReadLE16(&romData[i + 5]);
					segmented = 1;
					break;
				}
			}
			/*Per-song pattern group and row period tables (ld hl,groups / add hl,de / ld a,[hl] / ld [x],a / ld hl,periods ...)*/
			if (segmented == 1)
			{
				segmented = 0;
				for (i = 0; i < bankSize - 14; i++)
				{
					if (romData[i] == 0x21 && romData[i + 3] == 0x19 && romData[i + 4] == 0x7E && romData[i + 5] == 0xEA && romData[i + 8] == 0x21 && romData[i + 11] == 0x19 && romData[i + 12] == 0x7E && romData[i + 13] == 0xEA)
					{
						patGroupTab = ReadLE16(&romData[i + 1]);
						patPeriodTab = ReadLE16(&romData[i + 9]);
						segmented = 1;
						printf("Pattern tables: groups 0x%04X, periods 0x%04X, pattern pointers 0x%04X\n", patGroupTab, patPeriodTab, segOffset);
						break;
					}
				}
			}
		}
		bankAmt = bankSize;
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
	}

	/*Try to search the bank for song table loader - Method 1*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], HALMagicBytesA, 6)) && foundTable != 1)
		{
			tablePtrLoc = i + 6;
			if (drvVers == HAL_VER_LOLO)
			{
				tablePtrLoc = 0x34B3;
				noteLenTabOffset = 0x7D69;
			}
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 2*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], HALMagicBytesB, 4)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&exRomData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 3 - Adventures of Lolo*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], HALMagicBytesC, 5)) && foundTable != 1)
		{
			tablePtrLoc = i + 5;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&exRomData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 4 - Ghostbusters 2*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], HALMagicBytesD, 4)) && foundTable != 1)
		{
			tablePtrLoc = i + 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&exRomData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			drvVers = HAL_VER_GB2;
			noteLenTabOffset = 0x46F3;
		}
	}

	/*Method 5 - Trax*/
	if (ReadLE16(&exRomData[0x4000]) == 0x4299)
	{
		tableOffset = 0x4000;
		printf("Song table starts at 0x%04X...\n", tableOffset);
		foundTable = 1;
		drvVers = HAL_VER_GB2;
		noteLenTabOffset = 0x406D;
	}

	if (drvVers != HAL_VER_GB2)
	{
		if (drvVers == HAL_VER_KDL)
		{
			for (i = 0; i < (bankSize * 2); i++)
			{
				if ((!memcmp(&exRomData[i], HALMagicBytesLen, 11)) && foundTable == 1)
				{
					noteLenTabLoc = i + 11;
					printf("Found pointer to note length table at address 0x%04X!\n", noteLenTabLoc);
					noteLenTabOffset = exRomData[noteLenTabLoc] + (exRomData[noteLenTabLoc + 5] * 0x100);
					printf("Note length table starts at 0x%04X...\n", noteLenTabOffset);
				}
			}

			for (i = 0; i < (bankSize * 2); i++)
			{
				if ((!memcmp(&exRomData[i], HALMagicBytesLen2, 5)) && foundTable == 1)
				{
					noteLenTabLoc = i + 5;
					printf("Found pointer to note length table at address 0x%04X!\n", noteLenTabLoc);
					noteLenTabOffset = exRomData[noteLenTabLoc] + (exRomData[noteLenTabLoc + 4] * 0x100);
					printf("Note length table starts at 0x%04X...\n", noteLenTabOffset);
				}
			}
		}
		else if (drvVers == HAL_VER_KDL2)
		{
			noteLenTabOffset = 0x5B6F;
		}
		else if (drvVers == HAL_VER_KSS)
		{
			noteLenTabOffset = 0x4F02;
		}
		else if (drvVers == HAL_VER_VS)
		{
			noteLenTabOffset = 0x58C2;
		}

	}

	i = tableOffset;
	songNum = 1;
	while (ReadLE16(&romData[i]) > 0x1000 && ReadLE16(&romData[i]) < 0x8000)
	{
		songPtr = ReadLE16(&romData[i]);
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		HALsong2mid(songNum, songPtr);
		i += 2;
		songNum++;
	}

	free(romData);

}

void HALsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int chanNum = 0;
	int activeChan[4];
	int chanPtrs[4];
	int chanMask = 0;
	int noteVol = 0;
	int chanVol = 0;
	int baseNote = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curInst = 0;
	int macros[4][2];
	int repeats[4][2];
	int k = 0;
	unsigned char command[4];
	int sfx = 0;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int amt = 0;
	int hasPlayedNote = 0;
	int inMacro = 0;
	int repeatNum = 0;
	int noteLenTabIndex;
	int transpose;
	int holdNote;
	int noteLenDur;
	int envVal;
	midPos = 0;
	ctrlMidPos = 0;
	int rest = 0;
	long tempLoopPt = 1;
	long segPtr = 0;
	int segCnt = 0;
	int hasPlayedMacro = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long masterDelay = 0;
	int noteDur;
	float noteRatio;
	int nextSegPtr;
	int patSong = 0;
	int patGroup = 0;
	int patVariant = 0;
	int patPeriod = 0;
	int patRow = 0;
	long patTable = 0;
	long rowEnd = 0;
	long rowCmds = 0;
	int patEntry[4];
	long over = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
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

		for (k = 0; k < 4; k++)
		{
			chanPtrs[k] = 0;
		}

		switch (drvVers)
		{
		case HAL_VER_GB2:
			HAL_STATUS_NOTE_MIN = 0x00;
			HAL_STATUS_NOTE_MAX = 0x7B;
			HAL_STATUS_REST = 0x7C;
			HAL_STATUS_NOTE_LEN_MIN = 0x80;
			HAL_STATUS_NOTE_LEN_MAX = 0xBF;
			for (j = 0xC0; j <= 0xEF; j++)
			{
				EventMap[j] = HAL_EVENT_UNKNOWN0;
			}
			EventMap[0xF0] = HAL_EVENT_ENV;
			EventMap[0xF1] = HAL_EVENT_DUTY;
			EventMap[0xF2] = HAL_EVENT_ATTENUATE;
			EventMap[0xF3] = HAL_EVENT_TRANSPOSE;
			EventMap[0xF4] = HAL_EVENT_REPEAT_START;
			EventMap[0xF5] = HAL_EVENT_REPEAT_END;
			EventMap[0xF6] = HAL_EVENT_CALL;
			EventMap[0xF7] = HAL_EVENT_RETURN;
			EventMap[0xF8] = HAL_EVENT_SWEEP_ON;
			EventMap[0xF9] = HAL_EVENT_SWEEP_OFF;
			EventMap[0xFA] = HAL_EVENT_UNKNOWN0;
			EventMap[0xFB] = HAL_EVENT_JUMP;
			EventMap[0xFC] = HAL_EVENT_NOP;
			EventMap[0xFD] = HAL_EVENT_SFX;
			EventMap[0xFE] = HAL_EVENT_UNKNOWN0;
			EventMap[0xFF] = HAL_EVENT_STOP;
			break;
		case HAL_VER_KDL:
			/*Fall-through*/
		case HAL_VER_KDL2:
		case HAL_VER_KSS:
		case HAL_VER_LOLO:
		default:
			HAL_STATUS_NOTE_MIN = 0x00;
			HAL_STATUS_NOTE_MAX = 0xE0;
			HAL_STATUS_REST = 0xD0;
			EventMap[0xE1] = HAL_EVENT_SYNC;
			EventMap[0xE2] = HAL_EVENT_PERIOD;
			EventMap[0xE3] = HAL_EVENT_TUNING;
			for (j = 0xE4; j <= 0xEF; j++)
			{
				EventMap[j] = HAL_EVENT_UNKNOWN0;
			}
			EventMap[0xF0] = HAL_EVENT_VOL;
			EventMap[0xF1] = HAL_EVENT_VOL_ADD;
			EventMap[0xF2] = HAL_EVENT_NOTE_LEN_TAB;
			EventMap[0xF3] = HAL_EVENT_TIE;
			EventMap[0xF4] = HAL_EVENT_GATE;
			EventMap[0xF5] = HAL_EVENT_BASE_NOTE;
			EventMap[0xF6] = HAL_EVENT_PROG_CHANGE;
			EventMap[0xF7] = HAL_EVENT_TUNING_TABLE;
			EventMap[0xF8] = HAL_EVENT_JUMP;
			EventMap[0xF9] = HAL_EVENT_UNKNOWN0;
			EventMap[0xFA] = HAL_EVENT_CALL;
			EventMap[0xFB] = HAL_EVENT_RETURN;
			EventMap[0xFC] = HAL_EVENT_REPEAT_START;
			EventMap[0xFD] = HAL_EVENT_REPEAT_END;
			EventMap[0xFE] = HAL_EVENT_PAN;
			EventMap[0xFF] = HAL_EVENT_STOP;
			break;
		}

		if (drvVers != HAL_VER_GB2)
		{
			/*Get information about each track*/
			romPos = songPtr;
			chanNum = romData[romPos];
			romPos++;

			for (k = 0; k < chanNum; k++)
			{
				chanMask = romData[romPos + 2];
				switch (chanMask)
				{
				case 0x00:
					curTrack = 0;
					break;
				case 0x04:
					curTrack = 1;
					break;
				case 0x10:
					curTrack = 2;
					break;
				case 0x0C:
					curTrack = 3;
					break;
				default:
					curTrack = 0;
					break;
				}
				chanPtrs[curTrack] = ReadLE16(&romData[romPos]);
				patEntry[curTrack] = k;
				romPos += 3;

			}

			/*Adventures of Lolo: songs with a pattern group. Every row period, the sequencer re-points all four
			  music tracks at the next row (of 8) of PatternTables[group * 5 + variant], which the game selects.
			  Each song's header is row 0 of one variant, so the song is converted with that variant's rows.*/
			if (drvVers == HAL_VER_LOLO && segmented == 1 && (songNum - 1) < (patPeriodTab - patGroupTab) && romData[patGroupTab + songNum - 1] != 0xFF)
			{
				patSong = 1;
				patGroup = romData[patGroupTab + songNum - 1];
				patPeriod = romData[patPeriodTab + songNum - 1];
				patVariant = 0;
				for (j = 0; j < 5; j++)
				{
					patTable = ReadLE16(&romData[segOffset + 2 * (patGroup * 5 + j)]);
					for (k = 0; k < 4; k++)
					{
						if (chanPtrs[k] != 0 && ReadLE16(&romData[patTable + patEntry[k] * 2]) != chanPtrs[k])
						{
							break;
						}
					}
					if (k == 4)
					{
						patVariant = j;
						break;
					}
				}
				patTable = ReadLE16(&romData[segOffset + 2 * (patGroup * 5 + patVariant)]);
				printf("Song %i: pattern group %i, variant %i, 8 rows of %i ticks (table 0x%04X)\n", songNum, patGroup, patVariant, patPeriod, patTable);
			}

		}
		/*Ghostbusters II and Trax use a slightly different song format*/
		else
		{
			romPos = songPtr;

			while (romData[romPos] != 0xFF)
			{
				chanMask = romData[romPos];
				switch (chanMask)
				{
				case 0x00:
					curTrack = 0;
					sfx = 0;
					break;
				case 0x01:
					curTrack = 1;
					sfx = 0;
					break;
				case 0x02:
					curTrack = 2;
					sfx = 0;
					break;
				case 0x03:
					curTrack = 3;
					sfx = 0;
					break;
				case 0x04:
					curTrack = 0;
					sfx = 1;
					break;
				case 0x05:
					curTrack = 1;
					sfx = 1;
					break;
				case 0x06:
					curTrack = 2;
					sfx = 1;
					break;
				case 0x07:
					curTrack = 3;
					sfx = 1;
					break;
				}

				chanPtrs[curTrack] = ReadLE16(&romData[romPos + 1]);
				romPos += 3;
			}

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
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			for (j = 0; j < 4; j++)
			{
				macros[j][0] = 0x0000;
				macros[j][1] = 0x0000;
			}
			for (j = 0; j < 4; j++)
			{
				repeats[j][0] = -1;
				repeats[j][1] = 0x0000;
			}
			baseNote = 0;
			rest = 0;
			hasPlayedNote = 0;
			inMacro = 0;
			repeatNum = 0;
			envVal = 0xFF;
			curVol = 120;
			segCnt = 0;
			hasPlayedMacro = 0;
			transpose = 0;
			noteLenTabIndex = noteLenTabOffset;
			holdNote = 0;
			tempLoopPt = 1;
			/*Row 0 is one tick shorter: the sequencer's timer has already counted the first update when the tracks start*/
			patRow = 0;
			rowEnd = (patPeriod - 1) * 5;
			rowCmds = 0;


			if (curTrack == 3)
			{
				baseNote = 32;
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

			if (chanPtrs[curTrack] != 0x0000)
			{
				seqPos = chanPtrs[curTrack];

				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);

					/*For Adventures of Lolo: row boundary reached (or a runaway row) - cut and move to the next row*/
					if (patSong == 1)
					{
						rowCmds++;
						if (rowCmds > 5000 && masterDelay < rowEnd)
						{
							curDelay += (rowEnd - masterDelay);
							ctrlDelay += (rowEnd - masterDelay);
							masterDelay = rowEnd;
						}
						if (masterDelay >= rowEnd)
						{
							/*A note or rest that runs past the boundary is cut there*/
							over = masterDelay - rowEnd;
							curDelay -= over;
							ctrlDelay -= over;
							masterDelay = rowEnd;
							patRow++;
							if (patRow >= 8)
							{
								seqEnd = 1;
								continue;
							}
							seqPos = ReadLE16(&romData[patTable + (patRow * 8) + (patEntry[curTrack] * 2)]);
							/*The sequencer resets the track's call/loop stack*/
							inMacro = 0;
							repeatNum = 0;
							rowEnd += patPeriod * 5;
							rowCmds = 0;
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];
							command[2] = romData[seqPos + 2];
							command[3] = romData[seqPos + 3];
						}
					}

					if (command[0] >= HAL_STATUS_NOTE_MIN && command[0] <= HAL_STATUS_NOTE_MAX && command[0] != HAL_STATUS_REST)
					{
						if (drvVers != HAL_VER_GB2)
						{
							if (holdNote == 1)
							{

								tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							if (command[0] < 0xC0)
							{
								curNoteLen = exRomData[noteLenTabIndex + (command[0] >> 5)] * 5;
								/*Rest*/
								if ((command[0] & 0x1F) == 0x10)
								{
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									masterDelay += curNoteLen;
								}
								else
								{
									if ((command[0] & 0x1F) < 0x10)
									{
										curNote = baseNote + (command[0] & 0x1F) + 21;
									}
									else
									{
										curNote = baseNote - (16 - (command[0] & 0x0F)) + 21;
									}
									if (curTrack < 2)
									{
										curNote += 12;
									}
									if (curTrack == 3)
									{
										curNote -= 12;
									}

									curVol = (envVal * 0x08);

									if (curTrack == 3)
									{
										curVol *= 2;
									}

									if (curVol < 1)
									{
										curVol = 1;
									}
									if (curVol > 120)
									{
										curVol = 120;
									}
									tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = curNoteLen;
									ctrlDelay += curNoteLen;
									masterDelay += curNoteLen;
									holdNote = 1;
								}
								seqPos++;
							}
							else
							{
								if ((command[0] - 0xC0) < 0x10)
								{
									curNote = baseNote + (command[0] - 0xC0) + 21;
								}
								else
								{
									curNote = baseNote - (16 - ((command[0] - 0xC0) & 0x0F)) + 21;
								}
								if (curTrack < 2)
								{
									curNote += 12;
								}
								if (curTrack == 3)
								{
									curNote -= 12;
								}
								curNoteLen = command[1] * 5;
								curVol = (envVal * 0x08);

								if (curVol < 1)
								{
									curVol = 1;
								}
								if (curVol > 120)
								{
									curVol = 120;
								}
								tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = curNoteLen;
								ctrlDelay += curNoteLen;
								masterDelay += curNoteLen;
								holdNote = 1;
								seqPos += 2;
							}
						}
						else
						{
							curNote = highNibble + (lowNibble * 12) + transpose + 24;
							if (curTrack < 2)
							{
								curNote += 12;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos++;
						}

						if (hasPlayedNote < 3)
						{
							hasPlayedNote++;
						}
					}

					else if (command[0] == HAL_STATUS_REST)
					{
						if (drvVers != HAL_VER_GB2)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							curDelay += (command[1] * 5);
							ctrlDelay += (command[1] * 5);
							masterDelay += (command[1] * 5);
							seqPos += 2;
						}
						else
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos++;
						}
					}

					else if ((command[0] >= HAL_STATUS_NOTE_LEN_MIN && command[0] <= HAL_STATUS_NOTE_LEN_MAX) && drvVers == HAL_VER_GB2)
					{
						curNoteLen = romData[noteLenTabOffset + (command[0] - HAL_STATUS_NOTE_LEN_MIN)] * 5;
						seqPos++;
					}

					else if (EventMap[command[0]] == HAL_EVENT_UNKNOWN0 || EventMap[command[0]] == HAL_EVENT_NOP)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == HAL_EVENT_SYNC)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_PERIOD)
					{
						seqPos += 3;
					}

					else if (EventMap[command[0]] == HAL_EVENT_TUNING)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_VOL)
					{
						envVal = command[1];
						if (envVal < 0 || envVal > 15)
						{
							envVal = 15;
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_VOL_ADD)
					{
						envVal += (signed char)command[1];
						if (envVal < 0 || envVal > 15)
						{
							envVal = 15;
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_NOTE_LEN_TAB)
					{
						noteLenTabIndex = noteLenTabOffset + (6 * command[1]);
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_TIE)
					{
						curDelay += (command[1] * 5);
						ctrlDelay += (command[1] * 5);
						masterDelay += (command[1] * 5);
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_GATE)
					{
						noteDur = command[1];
						noteRatio = (float)(noteDur) / 256;
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_BASE_NOTE)
					{
						baseNote = command[1];
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_PROG_CHANGE)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						curInst = command[1] & 0x7F;
						firstNote = 1;
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_TUNING_TABLE)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_JUMP && patSong == 1)
					{
						/*Inside a pattern row the jump is followed; the row boundary ends it*/
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}

					else if (EventMap[command[0]] == HAL_EVENT_JUMP)
					{
						if (hasPlayedNote > 2)
						{
							if (tempLoopPt > 0)
							{
								tempLoopPt = ReadLE16(&romData[seqPos + 1]);
							}
							if (bank == 0x20 && songNum == 38 && tempLoopPt == 0x7411 && tempLoopPt > 0)
							{
								seqPos = ReadLE16(&romData[seqPos + 1]);
								tempLoopPt = 0;
							}
							else if (bank == 0x20 && songNum == 16 && tempLoopPt > 0)
							{
								seqPos = ReadLE16(&romData[seqPos + 1]);
								tempLoopPt = 0;
							}
							else
							{
								seqEnd = 1;
							}


						}
						else
						{
							seqPos = ReadLE16(&romData[seqPos + 1]);
						}

					}

					else if (EventMap[command[0]] == HAL_EVENT_CALL)
					{
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

					else if (EventMap[command[0]] == HAL_EVENT_RETURN)
					{
						seqPos = macros[inMacro][1];
						if (inMacro > 0)
						{
							inMacro--;
						}
					}

					else if (EventMap[command[0]] == HAL_EVENT_REPEAT_START)
					{
						if (repeatNum >= 3)
						{
							seqEnd = 1;
						}
						else
						{
							repeatNum++;
							repeats[repeatNum][0] = seqPos + 2;
							repeats[repeatNum][1] = command[1];
							seqPos += 2;
						}
					}

					else if (EventMap[command[0]] == HAL_EVENT_REPEAT_END)
					{
						if (repeats[repeatNum][1] > 1)
						{
							seqPos = repeats[repeatNum][0];
							repeats[repeatNum][1]--;
						}
						else
						{
							seqPos++;
							if (repeatNum > 0)
							{
								repeatNum--;
							}
						}
					}

					else if (EventMap[command[0]] == HAL_EVENT_PAN)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_STOP && patSong == 1)
					{
						/*The track is silent until the next row restarts it*/
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						curDelay += (rowEnd - masterDelay);
						ctrlDelay += (rowEnd - masterDelay);
						masterDelay = rowEnd;
					}

					else if (EventMap[command[0]] == HAL_EVENT_STOP)
					{
						seqEnd = 1;
					}

					else if (EventMap[command[0]] == HAL_EVENT_ENV && drvVers == HAL_VER_GB2)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_DUTY && drvVers == HAL_VER_GB2)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_ATTENUATE && drvVers == HAL_VER_GB2)
					{
						envVal -= command[1];
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_TRANSPOSE && drvVers == HAL_VER_GB2)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_SWEEP_ON && drvVers == HAL_VER_GB2)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == HAL_EVENT_SWEEP_OFF && drvVers == HAL_VER_GB2)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == HAL_EVENT_SFX && drvVers == HAL_VER_GB2)
					{
						seqPos += 2;
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

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}

}
