/*Mark Cooksey*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MCOOKSEY.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
long macroPtrLoc;
long macroOffset;
long sfxPtrLoc;
long sfxOffset;
long lowerPtrs;
long upperPtrs;
long lowerMacros;
long upperMacros;
int i, j, s;
int sysMode;
char outfile[1000000];
const char MCMagicBytes[10] = { 0x6F, 0x26, 0x00, 0x29, 0x54, 0x5D, 0x29, 0x29, 0x19, 0x11 };
const char MCMacroFindOld[13] = { 0xE1, 0x11, 0xFE, 0xFF, 0x19, 0x3E, 0x01, 0x22, 0x03, 0x0A, 0xCB, 0x27, 0x11 };
const char MCMacroFindNew[5] = { 0xCB, 0x27, 0x30, 0x06, 0x11 };
const char MCMagicBytesNES1[7] = { 0x06, 0x0A, 0x0A, 0x65, 0x06, 0xAA, 0xBD };
const char MCMagicBytesNES2[7] = { 0x7B, 0x0A, 0x0A, 0x65, 0x7B, 0xAA, 0xBD };
const char MCMagicBytesNES3[7] = { 0xFC, 0x0A, 0x0A, 0x65, 0xFC, 0xAA, 0xBD };
const char MCMagicBytesNES4[9] = { 0x06, 0x0A, 0x0A, 0x65, 0x06, 0x65, 0x06, 0xAA, 0xBD };
const char MCMacroFindNES1[6] = { 0xA0, 0x01, 0xB1, 0xF6, 0xA8, 0xB9 };
const char MCMacroFindNES2[6] = { 0xA0, 0x01, 0xB1, 0x00, 0xA8, 0xB9 };
const char MCMacroFindNES3[6] = { 0xA0, 0x01, 0xB1, 0x75, 0xA8, 0xB9 };
const char MCSFXFind[5] = { 0xCB, 0x27, 0x85, 0x6F, 0x30 };
const char MCMagicBytesGG[6] = { 0x85, 0x6F, 0x30, 0x01, 0x24, 0x7E };
const char MCMacroFindGG[6] = { 0x7E, 0x87, 0x83, 0x5F, 0x30, 0x01 };
int curStepTab[16];
unsigned long macroList[500];
long seqPtrs[5];
long stepPtr;
long nextPtr;
long endPtr;
int stepAmt;
int songNum;
long bankAmt;
int highestMacro = 1;
int fiveChan = 0;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

char* argv3;

char string1[4];

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);

void MCsong2mid(int songNum, long ptrs[], long nextPtr);
void MCsong2midNES(int songNum, long ptrs[]);

void MCgetMacroList(unsigned long list[], long offset, long sfxTable);

void MCProc(int bank)
{
	sysMode = 1;
	/*Check for NES ROM header*/
	fgets(string1, 4, rom);
	if (!memcmp(string1, "NES", 1))
	{
		fseek(rom, (((bank - 1) * bankSize)) + 0x10, SEEK_SET);
		if (sysMode != 2)
		{
			printf("Detected NES header - switching to NES format.\n");
			sysMode = 2;
		}
	}
	else
	{
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	}
	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		if (sysMode == 1)
		{
			bankAmt = 0;
		}
		else
		{
			bankAmt = bankSize;
		}

	}
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], MCMagicBytes, 10))
		{
			tablePtrLoc = bankAmt + i + 10;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	/*Search for sound effects table*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], MCSFXFind, 5))
		{
			sfxPtrLoc = bankAmt + i - 2;
			printf("Found pointer to sound effects table at address 0x%04x!\n", sfxPtrLoc);
			sfxOffset = ReadLE16(&romData[sfxPtrLoc - bankAmt]);
			printf("Sound effects table starts at 0x%04x...\n", sfxOffset);
			break;
		}
	}

	/*Now try to search the bank for macro table loader*/
	for (i = 0; i < bankSize; i++)
	{
		/*First, try old method (games before 1999)*/
		if (!memcmp(&romData[i], MCMacroFindOld, 13))
		{
			macroPtrLoc = bankAmt + i + 13;
			printf("Found pointer to macro table at address 0x%04x!\n", macroPtrLoc);
			macroOffset = ReadLE16(&romData[macroPtrLoc - bankAmt]);
			printf("Macro table starts at 0x%04x...\n", macroOffset);
			MCgetMacroList(macroList, macroOffset, sfxOffset);
			break;
		}

		/*Now try new method (games from 1999-)*/
		else if (!memcmp(&romData[i], MCMacroFindNew, 5))
		{
			macroPtrLoc = bankAmt + i + 5;
			printf("Found pointer to macro table at address 0x%04x!\n", macroPtrLoc);
			macroOffset = ReadLE16(&romData[macroPtrLoc - bankAmt]);
			printf("Macro table starts at 0x%04x...\n", macroOffset);
			MCgetMacroList(macroList, macroOffset, sfxOffset);
			break;
		}
	}

	/*NES version...*/
	if (sysMode == 2)
	{
		/*Try to search the bank for song table loader - Method 1*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], MCMagicBytesNES1, 7))
			{
				bankAmt = 0x8000;
				tablePtrLoc = bankAmt + i + 7;
				tableOffset = tablePtrLoc;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				lowerPtrs = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				upperPtrs = ReadLE16(&romData[tablePtrLoc + 6 - bankAmt]);
				printf("Song tables start at 0x%04x and 0x%04x...\n", lowerPtrs, upperPtrs);
				break;
			}
		}

		/*Try to search the bank for song table loader - Method 2*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], MCMagicBytesNES2, 7))
			{
				bankAmt = 0x8000;
				tablePtrLoc = bankAmt + i + 7;
				tableOffset = tablePtrLoc;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				lowerPtrs = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				upperPtrs = ReadLE16(&romData[tablePtrLoc + 6 - bankAmt]);
				printf("Song tables start at 0x%04x and 0x%04x...\n", lowerPtrs, upperPtrs);
				break;
			}
		}

		/*Try to search the bank for song table loader - Method 3*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], MCMagicBytesNES3, 7))
			{
				bankAmt = 0xC000;
				tablePtrLoc = bankAmt + i + 7;
				tableOffset = tablePtrLoc;

				if (ReadLE16(&romData[tablePtrLoc - bankAmt]) < 0xC000)
				{
					bankAmt = 0x8000;
					tablePtrLoc -= 0x4000;
					tableOffset -= 0x4000;
				}
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				lowerPtrs = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				upperPtrs = ReadLE16(&romData[tablePtrLoc + 6 - bankAmt]);
				printf("Song tables start at 0x%04x and 0x%04x...\n", lowerPtrs, upperPtrs);
				break;
			}
		}

		/*Try to search the bank for song table loader - Method 4 - Joe & Mac: Caveman Ninja*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], MCMagicBytesNES4, 9))
			{
				bankAmt = 0x8000;
				tablePtrLoc = bankAmt + i + 9;
				tableOffset = tablePtrLoc;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				lowerPtrs = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				upperPtrs = ReadLE16(&romData[tablePtrLoc + 6 - bankAmt]);
				printf("Song tables start at 0x%04x and 0x%04x...\n", lowerPtrs, upperPtrs);
				fiveChan = 1;
				break;
			}
		}

		/*Now try to search the bank for macro table loader*/
		for (i = 0; i < bankSize; i++)
		{
			/*Method 1*/
			if (!memcmp(&romData[i], MCMacroFindNES1, 6))
			{
				macroPtrLoc = bankAmt + i + 6;
				printf("Found pointer to macro table at address 0x%04x!\n", macroPtrLoc);
				lowerMacros = ReadLE16(&romData[macroPtrLoc - bankAmt]);
				upperMacros = ReadLE16(&romData[macroPtrLoc + 6 - bankAmt]);
				printf("Macro tables start at 0x%04x and 0x%04x...\n", lowerMacros, upperMacros);
				break;

			}

			/*Method 2*/
			if (!memcmp(&romData[i], MCMacroFindNES2, 6))
			{
				macroPtrLoc = bankAmt + i + 6;
				printf("Found pointer to macro table at address 0x%04x!\n", macroPtrLoc);
				lowerMacros = ReadLE16(&romData[macroPtrLoc - bankAmt]);
				upperMacros = ReadLE16(&romData[macroPtrLoc + 6 - bankAmt]);
				printf("Macro tables start at 0x%04x and 0x%04x...\n", lowerMacros, upperMacros);
				break;

			}

			/*Method 1*/
			if (!memcmp(&romData[i], MCMacroFindNES3, 6))
			{
				macroPtrLoc = bankAmt + i + 6;
				printf("Found pointer to macro table at address 0x%04x!\n", macroPtrLoc);
				lowerMacros = ReadLE16(&romData[macroPtrLoc - bankAmt]);
				upperMacros = ReadLE16(&romData[macroPtrLoc + 6 - bankAmt]);
				printf("Macro tables start at 0x%04x and 0x%04x...\n", lowerMacros, upperMacros);
				break;

			}


		}
	}

	/*Game Gear version...*/
	if (sysMode == 3)
	{
		/*Try to search the bank for song table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], MCMagicBytesGG, 6))
			{
				if (ReadLE16(&romData[i - 2]) >= 0x8000)
				{
					bankAmt = 0x8000;
				}
				tablePtrLoc = bankAmt + i - 2;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				break;
			}
		}

		/*Now try to search the bank for macro table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], MCMacroFindGG, 6))
			{
				macroPtrLoc = bankAmt + i - 3;
				printf("Found pointer to macro table at address 0x%04x!\n", macroPtrLoc);
				macroOffset = ReadLE16(&romData[macroPtrLoc - bankAmt]);
				printf("Macro table starts at 0x%04x...\n", macroOffset);
				break;
			}
		}

	}

	if (tableOffset != NULL)
	{
		songNum = 1;
		if (sysMode == 1)
		{
			i = tableOffset;
			while ((nextPtr = ReadLE16(&romData[i + 10 - bankAmt])) >= bankAmt && (nextPtr = ReadLE16(&romData[i + 10 - bankAmt])) != 9839)
			{
				seqPtrs[0] = ReadLE16(&romData[i - bankAmt]);
				printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 2 - bankAmt]);
				printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[i + 4 - bankAmt]);
				printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[i + 6 - bankAmt]);
				printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
				stepPtr = ReadLE16(&romData[i + 8 - bankAmt]);
				printf("Song %i step table: 0x%04x\n", songNum, stepPtr);
				endPtr = nextPtr - bankAmt;

				for (j = 0; j < 16; j++)
				{
					curStepTab[j] = (romData[stepPtr + j - bankAmt]) * 5;
				}

				MCsong2mid(songNum, seqPtrs, endPtr);
				i += 10;
				songNum++;
			}

			seqPtrs[0] = ReadLE16(&romData[i - bankAmt]);
			printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
			seqPtrs[1] = ReadLE16(&romData[i + 2 - bankAmt]);
			printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
			seqPtrs[2] = ReadLE16(&romData[i + 4 - bankAmt]);
			printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
			seqPtrs[3] = ReadLE16(&romData[i + 6 - bankAmt]);
			printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
			stepPtr = ReadLE16(&romData[i + 8 - bankAmt]);
			printf("Song %i step table: 0x%04x\n", songNum, stepPtr);
			endPtr = bankSize;

			for (j = 0; j < 16; j++)
			{
				curStepTab[j] = (romData[stepPtr + j - bankAmt]) * 5;
			}

			MCsong2mid(songNum, seqPtrs, endPtr);
		}
		else if (sysMode == 2)
		{
			i = lowerPtrs;
			j = upperPtrs;

			while (j < lowerPtrs)
			{
				if (fiveChan != 1)
				{
					seqPtrs[0] = romData[i - bankAmt] + (romData[j - bankAmt] * 0x100);
					printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
					seqPtrs[1] = romData[i + 1 - bankAmt] + (romData[j + 1 - bankAmt] * 0x100);
					printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
					seqPtrs[2] = romData[i + 2 - bankAmt] + (romData[j + 2 - bankAmt] * 0x100);
					printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
					seqPtrs[3] = romData[i + 3 - bankAmt] + (romData[j + 3 - bankAmt] * 0x100);
					printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
					stepPtr = romData[i + 4 - bankAmt] + (romData[j + 4 - bankAmt] * 0x100);
					printf("Song %i step table: 0x%04x\n", songNum, stepPtr);

					for (s = 0; s < 16; s++)
					{
						curStepTab[s] = (romData[stepPtr + s - bankAmt]) * 5;
					}
					MCsong2midNES(songNum, seqPtrs);
					i += 5;
					j += 5;
					songNum++;
				}
				else if (fiveChan == 1)
				{
					seqPtrs[0] = romData[i + 0x2000 - bankAmt] + (romData[j + 0x2000 - bankAmt] * 0x100);
					printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
					seqPtrs[1] = romData[i + 0x2000 + 1 - bankAmt] + (romData[j + 0x2000 + 1 - bankAmt] * 0x100);
					printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
					seqPtrs[2] = romData[i + 0x2000 + 2 - bankAmt] + (romData[j + 0x2000 + 2 - bankAmt] * 0x100);
					printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
					seqPtrs[3] = romData[i + 0x2000 + 3 - bankAmt] + (romData[j + 0x2000 + 3 - bankAmt] * 0x100);
					printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
					seqPtrs[4] = romData[i + 0x2000 + 4 - bankAmt] + (romData[j + 0x2000 + 4 - bankAmt] * 0x100);
					printf("Song %i channel 5: 0x%04x\n", songNum, seqPtrs[4]);
					stepPtr = romData[i + 0x2000 + 5 - bankAmt] + (romData[j + 0x2000 + 5 - bankAmt] * 0x100);
					printf("Song %i step table: 0x%04x\n", songNum, stepPtr);

					for (s = 0; s < 16; s++)
					{
						curStepTab[s] = (romData[stepPtr + 0x2000 + s - bankAmt]) * 5;
					}
					MCsong2midNES(songNum, seqPtrs);
					i += 6;
					j += 6;
					songNum++;
				}

			}
		}

		else if (sysMode == 3)
		{
			i = tableOffset;
			while ((nextPtr = ReadLE16(&romData[i + 10 - bankAmt])) >= bankAmt)
			{
				seqPtrs[0] = ReadLE16(&romData[i - bankAmt]);
				printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 2 - bankAmt]);
				printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[i + 4 - bankAmt]);
				printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[i + 6 - bankAmt]);
				printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
				stepPtr = ReadLE16(&romData[i + 8 - bankAmt]);
				printf("Song %i step table: 0x%04x\n", songNum, stepPtr);
				endPtr = nextPtr - bankAmt;

				for (j = 0; j < 16; j++)
				{
					curStepTab[j] = (romData[stepPtr + j - bankAmt]) * 5;
				}

				MCsong2mid(songNum, seqPtrs, endPtr);
				i += 10;
				songNum++;
			}
		}


	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
}

/*Convert the song data to MIDI*/
void MCsong2mid(int songNum, long ptrs[], long nextPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;

	long switchPoint[10][2];

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

	long stepPtr = 0;
	float multiplier = 0;
	long tempo = 0;

	int curInst = 0;

	signed int macTranspose = 0;
	unsigned short macCount = 0;
	unsigned int macReturn = 0;
	unsigned long macroBase = 0;
	int curMacro = 0;

	unsigned char command[4];
	unsigned char lowNibble;
	unsigned char highNibble;
	long ctrlDelay = 0;
	long masterDelay = 0;

	int firstNote = 1;

	int timeVal = 0;

	int switchNum = 0;

	int j;


	midPos = 0;
	ctrlMidPos = 0;

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

		/*Get the initial tempo*/
		stepPtr = ptrs[4];
		tempo = 140;


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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			ctrlDelay = 0;
			masterDelay = 0;
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curVol = 0;
			curNoteLen = 0;
			switchNum = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);


			romPos = ptrs[curTrack] - bankAmt;

			command[0] = romData[romPos];
			command[1] = romData[romPos + 1];
			command[2] = romData[romPos + 2];
			command[3] = romData[romPos + 3];

			while (romPos < bankSize && trackEnd == 0)
			{
				command[0] = romData[romPos];
				command[1] = romData[romPos + 1];
				command[2] = romData[romPos + 2];
				command[3] = romData[romPos + 3];

				if (curTrack != 0)
				{
					for (j = 0; j < 10; j++)
					{
						if (masterDelay == switchPoint[j][0] && switchPoint[j][1] != 0)
						{
							stepPtr = switchPoint[j][1];
							for (k = 0; k < 16; k++)
							{
								curStepTab[k] = (romData[stepPtr + k - bankAmt]) * 5;
							}
							break;
						}
					}
				}



				switch (command[0])
				{
					/*Rest*/
				case 0x60:
					highNibble = (command[1] & 15);
					curNoteLen = curStepTab[highNibble];
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					romPos += 2;
					break;
					/*Stop channel*/
				case 0x61:
					trackEnd = 1;
					break;
					/*Go to track loop point*/
				case 0x62:
					trackEnd = 1;
					break;
					/*Change noise channel type?*/
				case 0x63:
					romPos += 2;
					break;
					/*Call macro*/
				case 0x64:
					curMacro = command[1];
					macTranspose = (signed char)command[2];
					macCount = command[3];
					if (sysMode == 1)
					{
						macroBase = macroList[curMacro];
					}
					else
					{
						macroBase = ReadLE16(&romData[macroOffset + (curMacro * 2) - bankAmt]) - bankAmt;
					}
					macReturn = romPos + 4;
					romPos = macroBase;
					break;
					/*End of macro*/
				case 0x65:
					if (macCount > 1)
					{
						romPos = macroBase;
						macCount--;
					}
					else
					{
						romPos = macReturn;
					}
					break;
					/*Set "has looped" flag*/
				case 0x66:
					romPos += 2;
					break;
					/*Set panning*/
				case 0x67:
					romPos += 2;
					break;
					/*Set step table/speed table*/
				case 0x68:
					if (sysMode == 1)
					{
						if (curTrack == 0)
						{
							stepPtr = ReadLE16(&romData[romPos + 1]);
							for (k = 0; k < 16; k++)
							{
								curStepTab[k] = (romData[stepPtr + k - bankAmt]) * 5;
							}
							switchPoint[switchNum][0] = masterDelay;
							switchPoint[switchNum][1] = stepPtr;
							switchNum++;
						}
						romPos += 3;
					}
					else
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 0.5;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						romPos += 2;
					}
					break;
					/*Set song tempo*/
				case 0x69:
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					tempo = command[1] * 0.6;
					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;

					romPos += 2;
					break;
					/*Set channel 1 panning*/
				case 0x6A:
					romPos += 2;
					break;
					/*Set channel 2 panning*/
				case 0x6B:
					romPos += 2;
					break;
					/*Set channel 3 panning*/
				case 0x6C:
					romPos += 2;
					break;
					/*Set channel 4 panning*/
				case 0x6D:
					romPos += 2;
					break;
				default:
					curNote = command[0];
					if (curNote >= 128)
					{
						if (curTrack == 3)
						{
							curNote += -128;
						}
						else
						{
							curNote += -140;
						}
					}
					curNote += macTranspose;
					curNote += 24;

					if (sysMode == 3 && curTrack != 3)
					{
						curNote -= 15;
					}
					else if (sysMode == 3 && curTrack == 3)
					{
						curNote += 24;
					}
					if (curTrack == 3)
					{
						curNote -= 12;
					}
					if (curNote >= 128)
					{
						curNote = 127;
					}
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					if (curInst != lowNibble)
					{
						curInst = lowNibble;
						firstNote = 1;
					}
					curNoteLen = curStepTab[highNibble];
					if ((lowNibble == 0 && command[0] == 36) || (lowNibble == 0 && command[0] == 0))
					{
						curDelay += curNoteLen;
					}
					else
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
					}

					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					romPos += 2;
					break;
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
		fclose(mid);
	}

}

/*Convert the song data to MIDI - NES edition*/
void MCsong2midNES(int songNum, long ptrs[])
{
	static const char* TRK_NAMES[5] = { "Square 1", "Square 2", "Triangle", "Noise", "PCM" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;

	long switchPoint[10][2];

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

	long stepPtr = 0;
	float multiplier = 0;
	long tempo = 0;

	int curInst = 0;

	signed int macTranspose = 0;
	unsigned short macCount = 0;
	unsigned int macReturn = 0;
	unsigned long macroBase = 0;
	int curMacro = 0;
	long macroPos = 0;

	unsigned char command[4];
	unsigned char lowNibble;
	unsigned char highNibble;
	long ctrlDelay = 0;
	long masterDelay = 0;

	int firstNote = 1;

	int timeVal = 0;

	int switchNum = 0;

	int j;


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

		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], trackCnt + 1);
		WriteBE16(&ctrlMidData[ctrlMidPos + 4], ticks);
		ctrlMidPos += 6;

		/*Get the initial tempo*/
		stepPtr = ptrs[4];
		tempo = 120;

		if (fiveChan != 1)
		{
			trackCnt = 4;
		}
		else if (fiveChan == 1)
		{
			trackCnt = 5;
		}


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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			ctrlDelay = 0;
			masterDelay = 0;
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curVol = 0;
			curNoteLen = 0;
			switchNum = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);


			if (fiveChan != 1)
			{
				romPos = ptrs[curTrack] - bankAmt;
			}
			else if (fiveChan == 1)
			{
				romPos = ptrs[curTrack] + 0x2000 - bankAmt;
			}


			command[0] = romData[romPos];
			command[1] = romData[romPos + 1];
			command[2] = romData[romPos + 2];
			command[3] = romData[romPos + 3];

			if (command[0] == 0 && command[1] == 0)
			{
				romPos += 2;
			}

			while (romPos < 0xFFFF && trackEnd == 0)
			{
				command[0] = romData[romPos];
				command[1] = romData[romPos + 1];
				command[2] = romData[romPos + 2];
				command[3] = romData[romPos + 3];

				switch (command[0])
				{
				case 0x5D:
					romPos += 2;
					break;
					/*Rest*/
				case 0x5F:
				case 0x60:
					highNibble = (command[1] & 15);
					curNoteLen = curStepTab[highNibble];
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					romPos += 2;
					break;
					/*Stop channel*/
				case 0x61:
					trackEnd = 1;
					break;
					/*Call macro*/
				case 0x62:
					curMacro = command[1];
					macTranspose = (signed char)command[2];
					macCount = command[3];

					if (fiveChan != 1)
					{
						macroBase = romData[lowerMacros + curMacro - bankAmt] + (romData[upperMacros + curMacro - bankAmt] * 0x100);
					}
					else if (fiveChan == 1)
					{
						macroBase = romData[lowerMacros + 0x2000 + curMacro - bankAmt] + (romData[upperMacros + 0x2000 + curMacro - bankAmt] * 0x100);
					}

					macReturn = romPos + 4;
					romPos = macroBase - bankAmt;

					if (fiveChan == 1)
					{
						romPos += 0x2000;
					}
					break;
					/*End of macro*/
				case 0x63:
					if (macCount > 1)
					{
						romPos = macroBase - bankAmt;

						if (fiveChan == 1)
						{
							romPos += 0x2000;
						}
						macCount--;
					}
					else
					{
						romPos = macReturn;
					}
					break;
					/*Go to track loop point*/
				case 0x64:
					trackEnd = 1;
					break;
				case 0x65:
					romPos += 2;

				default:
					curNote = command[0];
					if (curNote >= 128)
					{
						if (curTrack == 3)
						{
							curNote += -128;
						}
						else
						{
							curNote += -140;
						}
					}
					curNote += macTranspose;
					curNote += 24;
					if (curNote >= 128)
					{
						curNote = 127;
					}
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					if (curInst != lowNibble)
					{
						curInst = lowNibble;
						firstNote = 1;
					}
					curNoteLen = curStepTab[highNibble];
					if ((lowNibble == 0 && command[0] == 36) || (lowNibble == 0 && command[0] == 0))
					{
						curDelay += curNoteLen;
					}
					else
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
					}

					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					romPos += 2;
					break;
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
		fclose(mid);
	}

}

/*Get all the macro pointers from the table, and try to find the end of the table*/
void MCgetMacroList(unsigned long list[], long offset, long sfxTable)
{
	int j;
	unsigned long curValue;
	unsigned long curValue2;
	unsigned long tempCurValue;
	long newOffset = offset;
	long offset2 = offset - bankAmt;
	long initialValue = (ReadLE16(&romData[newOffset - bankAmt]));

	for (j = 0; j < 500; j++)
	{
		curValue = (ReadLE16(&romData[newOffset - bankAmt])) - bankAmt;
		curValue2 = (ReadLE16(&romData[newOffset - bankAmt]));
		if (curValue2 != sfxTable && curValue2 >= bankAmt && curValue2 != 65535)
		{
			/*Workaround for Kirikou (and possibly other games with "**PLANET**" padding (i.e. games developed by Planet)*/
			if (curValue2 == 21573)
			{
				tempCurValue = (ReadLE16(&romData[newOffset + 2 - bankAmt]));
				if (tempCurValue == 10794)
				{
					list[j] = NULL;
					highestMacro = j - 1;
					break;
				}
			}
			list[j] = curValue;
			newOffset += 2;
		}
		else
		{
			highestMacro = j;
			break;
		}


	}
}