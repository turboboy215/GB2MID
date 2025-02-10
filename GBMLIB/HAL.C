/*HAL Laboratory*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "HAL.H"

#define bankSize 16384

FILE* rom, * mid;
long bank1;
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
int format;
int noteTable[6];
int curNoteVol;
int noteTables[40][6];
long noteTableOffset;
int tempLen;
int foundNoteTable;
int useExBank;
int segmented;
long segOffset;
int segNum;
int nextSegPtr;
int subSegPtr;
float chanSpeed;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char HALMagicBytesA[6] = { 0xB8, 0xC8, 0x58, 0x16, 0x00, 0x21 };
const char HALMagicBytesB[4] = { 0x19, 0x19, 0x2A, 0x5F };
const char HALMagicBytesC[5] = { 0xC9, 0x58, 0x16, 0x00, 0x21 };
const char HALMagicBytesD[4] = { 0x4F, 0x06, 0x00, 0x21 };
const char HALfirstNoteTable[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
const char HALaltNoteTable[8] = { 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x01, 0x02 };
const char HALsegSearch[5] = { 0x86, 0x5F, 0x16, 0x00, 0x21 };
const char HALgb2Notes[] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
						 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
						 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
						 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
						 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
						 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 76,
						 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
						 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
						 96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,
						 108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,
						 120,121,122,123,124,125,126,127
};

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void HALsong2mid(int songNum, long ptr);
void HALProc(int bank);

void HALProc(int bank)
{
	foundTable = 0;
	firstPtr = 0;
	bank1 = bank;
	bankA = 0;
	tempPos = 0;
	format = 0;
	curNoteVol = 100;
	noteTableOffset = 0;
	foundNoteTable = 0;
	useExBank = 0;
	segmented = 0;
	segOffset = 0;
	segNum = 0;
	nextSegPtr = 0;
	subSegPtr = 0;
	chanSpeed = 0;

	if (bank1 != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((bank1 - 1) * bankSize), SEEK_SET);

	if (bank1 != 1)
	{
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}
	else if (bank1 == 1)
	{
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize * 2, rom);
	}


	/*Also copy data from bank 1 or the previous bank since some games store important data there*/

	if (bank1 == 0x20 || bank1 == 0x06)
	{
		fseek(rom, ((bank1 - 2) * bankSize), SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize);
		fread(exRomData, 1, bankSize, rom);
	}
	else
	{
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize);
		fread(exRomData, 1, bankSize, rom);

		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&exRomData[i], HALsegSearch, 5)))
			{
				segOffset = ReadLE16(&exRomData[i + 5]);
				segmented = 1;
				break;
			}
		}
	}


	/*Try to search the bank for song table loader - Method 1*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], HALMagicBytesA, 6)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 6;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], HALMagicBytesB, 4)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 3 - Adventures of Lolo*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&exRomData[i], HALMagicBytesC, 5)) && foundTable != 1)
		{
			tablePtrLoc = i + 5;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&exRomData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			bankA = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 4 - Ghostbusters 2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], HALMagicBytesD, 4)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;
			noteTableOffset = 0x46F3;
		}
	}

	/*Method 5 - Trax*/
	if (ReadLE16(&romData[0]) == 0x4299)
	{
		tableOffset = 0x4000;
		printf("Song table starts at 0x%04x...\n", tableOffset);
		foundTable = 1;
		format = 1;
		noteTableOffset = 0x406D;
	}

	/*Get note table values*/
	if (foundTable == 1 && format == 0)
	{
		/*Look for the start of the note tables*/
		if (bank1 != 0x20 && bank1 != 0x06 && bank1 != 0x10)
		{
			for (i = 0; i < bankSize; i++)
			{
				if ((!memcmp(&romData[i], HALfirstNoteTable, 6)) && romData[i + 8] != 0xFF)
				{
					noteTableOffset = i + bankAmt;
					break;
				}
			}
		}

		/*Fix for Othello/Othello World and Shanghai*/
		if (bank1 == 1)
		{
			for (i = 0; i < bankSize * 2; i++)
			{
				if ((!memcmp(&romData[i], HALfirstNoteTable, 6)) && i > 0x4000)
				{
					noteTableOffset = i + bankAmt;
					break;
				}
			}

			for (i = 0; i < bankSize * 2; i++)
			{
				if ((!memcmp(&romData[i], HALaltNoteTable, 8)) && (romData[i - 1] == 0x3A || romData[i - 1] == 0x39 || romData[i - 1] == 0x42))
				{
					noteTableOffset = i + bankAmt;
					break;
				}
			}
		}

		/*For Kirby's Dream Land 2*/
		else if (bank1 == 0x20 && format == 0)
		{
			for (i = 0; i < bankSize; i++)
			{
				if ((!memcmp(&exRomData[i], HALfirstNoteTable, 6)))
				{
					noteTableOffset = i + bankAmt;
					useExBank = 1;
					break;
				}
			}
		}

		else if (bank1 == 0x10 && format == 0)
		{
			/*For Vegas Stakes*/
			if (exRomData[0x1993] == 0x07)
			{
				for (i = 0; i < bankSize; i++)
				{
					if ((!memcmp(&romData[i], HALfirstNoteTable, 6)))
					{
						noteTableOffset = 0x58C2;
						useExBank = 0;
						break;
					}
				}
			}
			else
			{
				/*For Adventures of Lolo*/
				for (i = 0; i < bankSize; i++)
				{
					if ((!memcmp(&romData[i], HALfirstNoteTable, 6)))
					{
						noteTableOffset = i + bankAmt;
						break;
					}
				}
			}

		}

		else if (bank1 == 0x06 && format == 0)
		{
			/*For Kirby's Star Stacker*/
			if (ReadLE16(&romData[0]) == 0x4FAE)
			{
				for (i = 0; i < bankSize; i++)
				{
					if ((!memcmp(&exRomData[i], HALfirstNoteTable, 6)))
					{
						noteTableOffset = i + bankAmt;
						useExBank = 1;
						break;
					}
				}
			}
			else
			{
				/*For Kirby's Dream Land*/
				for (i = 0; i < bankSize; i++)
				{
					if ((!memcmp(&romData[i], HALfirstNoteTable, 6)))
					{
						noteTableOffset = i + bankAmt;
						break;
					}
				}
			}

		}

		tempLen = noteTableOffset - bankAmt;

		for (i = 0; i < 40; i++)
		{
			for (j = 0; j < 6; j++)
			{
				if (useExBank == 0)
				{
					noteTables[i][j] = romData[tempLen + j];
				}
				else if (useExBank == 1)
				{
					noteTables[i][j] = exRomData[tempLen + j];
				}

			}
			tempLen += 6;
		}
	}



	if (foundTable == 1)
	{
		if (bankA == 0)
		{
			i = tableOffset - bankAmt;
		}
		else if (bankA == 1)
		{
			i = tableOffset;
		}

		firstPtr = ReadLE16(&romData[i]);
		songNum = 1;
		if (bankA == 0)
		{
			if (bank1 != 1)
			{
				while (ReadLE16(&romData[i]) > bankSize && ReadLE16(&romData[i]) < (bankSize * 2))
				{
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					if (songPtr != 0)
					{
						HALsong2mid(songNum, songPtr);
					}
					i += 2;
					songNum++;
				}
			}
			else if (bank1 == 1)
			{
				while (ReadLE16(&romData[i]) > 0x1000 && ReadLE16(&romData[i]) < (bankSize * 2))
				{
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					if (songPtr != 0)
					{
						HALsong2mid(songNum, songPtr);
					}
					i += 2;
					songNum++;
				}
			}

		}

		else if (bankA == 1)
		{
			while (ReadLE16(&exRomData[i]) > bankSize && ReadLE16(&exRomData[i]) < (bankSize * 2))
			{
				songPtr = ReadLE16(&exRomData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				if (songPtr != 0)
				{
					HALsong2mid(songNum, songPtr);
				}
				i += 2;
				songNum++;
			}
		}

	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
	free(romData);
	free(exRomData);
	return 0;
}

void HALsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int chanNum = 0;
	int activeChan[4];
	int chanPtrs[4];
	int chanMask = 0;
	int noteVol = 0;
	int chanVol = 0;
	int freq = 0;
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
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	int repeat2 = 0;
	long repeat2Pt = 0;
	long loop = 0;
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

	midPos = 0;
	ctrlMidPos = 0;
	int rest = 0;
	long tempLoopPt = 0;
	long segPtr = 0;
	int segCnt = 0;
	int hasPlayedMacro = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

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

		romPos = ptr - bankAmt;

		for (k = 0; k < 4; k++)
		{
			chanPtrs[k] = 0;
		}

		if (format == 0)
		{
			/*Get information about each track*/
			romPos = ptr - bankAmt;
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
				romPos += 3;

			}

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				firstNote = 1;
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;
				curDelay = 0;
				seqEnd = 0;

				curNote = 0;
				curNoteLen = 0;
				repeat1 = -1;
				repeat1Pt = -1;
				repeat2 = -1;
				repeat2Pt = -1;
				macro1Pos = 0;
				macro1Ret = 0;
				freq = 0;
				rest = 0;
				hasPlayedNote = 0;
				inMacro = 0;
				tempLoopPt = 1;
				curNoteVol = 100;
				segCnt = 0;
				hasPlayedMacro = 0;

				if (curTrack == 3)
				{
					freq = 32;
				}

				if (segmented == 1 && songNum >= 24 && songNum <= 34)
				{
					if (songNum > 24 && curTrack == 0)
					{
						segOffset += 2;
					}
					nextSegPtr = ReadLE16(&exRomData[segOffset]);
					segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
					subSegPtr = 0;
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

				if (chanPtrs[curTrack] != 0)
				{
					seqPos = chanPtrs[curTrack];

					while (seqEnd == 0)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos + 1 - bankAmt];
						command[2] = romData[seqPos + 2 - bankAmt];
						command[3] = romData[seqPos + 3 - bankAmt];


						/*For Adventures of Lolo*/
						if (segmented == 1 && songNum >= 24 && songNum <= 34)
						{
							if (songNum == 24)
							{
								if (segPtr == 0x5B39 && seqPos == 0x5B40)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B40 && seqPos == 0x5B47)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B47 && seqPos == 0x5B4D)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B4D && seqPos == 0x5B53)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B5C && seqPos == 0x5B63)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5B70 && seqPos == 0x5B77)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B77 && seqPos == 0x5B7D)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B7D && seqPos == 0x5B83)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B83 && seqPos == 0x5B89)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B89 && seqPos == 0x5B8E)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5B8E && seqPos == 0x5B95)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5BA2 && seqPos == 0x5BAC)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
									command[0] = romData[seqPos - bankAmt];
									command[1] = romData[seqPos + 1 - bankAmt];
									command[2] = romData[seqPos + 2 - bankAmt];
									command[3] = romData[seqPos + 3 - bankAmt];
								}
								else if (segPtr == 0x5BA7 && seqPos == 0x5BAE)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5BB5 && seqPos == 0x5BBC)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5BBC && seqPos == 0x5BC1)
								{
									seqEnd = 1;
								}

							}

							if (songNum == 25)
							{
								if (segPtr == 0x5D49 && seqPos == 0x5D50)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D50 && seqPos == 0x5D57)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D57 && seqPos == 0x5D5D)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D5D && seqPos == 0x5D63)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D63 && seqPos == 0x5D6C)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D6C && seqPos == 0x5D73)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5D80 && seqPos == 0x5D87)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D87 && seqPos == 0x5D8D)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D8D && seqPos == 0x5D93)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D93 && seqPos == 0x5D99)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D99 && seqPos == 0x5D9E)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D9E && seqPos == 0x5DA5)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5DB2 && seqPos == 0x5DBE)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
									command[0] = romData[seqPos - bankAmt];
									command[1] = romData[seqPos + 1 - bankAmt];
									command[2] = romData[seqPos + 2 - bankAmt];
									command[3] = romData[seqPos + 3 - bankAmt];
								}
								else if (segPtr == 0x5DBE && seqPos == 0x5DC1)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DC1 && seqPos == 0x5DCA)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DCA && seqPos == 0x5DCC)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DCC && seqPos == 0x5DD3)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DD3 && seqPos == 0x5DDC)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
							}

							if (songNum == 26)
							{
								if (segPtr == 0x5BFD && seqPos == 0x5C06)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C06 && seqPos == 0x5C0E)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C0E && seqPos == 0x5C17)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C17 && seqPos == 0x5C1F)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C1F && seqPos == 0x5C28)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C28 && seqPos == 0x5C30)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5C3D && seqPos == 0x5C46)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C46 && seqPos == 0x5C4E)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C4E && seqPos == 0x5C57)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C57 && seqPos == 0x5C5F)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C5F && seqPos == 0x5C68)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C68 && seqPos == 0x5C70)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5C7D && seqPos == 0x5C89)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
									command[0] = romData[seqPos - bankAmt];
									command[1] = romData[seqPos + 1 - bankAmt];
									command[2] = romData[seqPos + 2 - bankAmt];
									command[3] = romData[seqPos + 3 - bankAmt];
								}
								else if (segPtr == 0x5C83 && seqPos == 0x5C8B)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C8B && seqPos == 0x5C94)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C94 && seqPos == 0x5C9C)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C9C && seqPos == 0x5CA5)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5CA5 && seqPos == 0x5CAD)
								{
									seqEnd = 1;
								}
							}

							if (songNum == 27)
							{
								if (segPtr == 0x5CEB && seqPos == 0x5CF4)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5CF4 && seqPos == 0x5CFC)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D05 && seqPos == 0x5D0D)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5D1A && seqPos == 0x5D23)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D23 && seqPos == 0x5D2B)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D2B && seqPos == 0x5D34)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5D34 && seqPos == 0x5D3C)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5C7D && seqPos == 0x5C89)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
									command[0] = romData[seqPos - bankAmt];
									command[1] = romData[seqPos + 1 - bankAmt];
									command[2] = romData[seqPos + 2 - bankAmt];
									command[3] = romData[seqPos + 3 - bankAmt];
								}
								else if (segPtr == 0x5C83 && seqPos == 0x5C8B)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C8B && seqPos == 0x5C94)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C94 && seqPos == 0x5C9C)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5C9C && seqPos == 0x5CA5)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5CA5 && seqPos == 0x5CAE)
								{
									seqEnd = 1;
								}

							}

							if (songNum == 28)
							{
								if (segPtr == 0x5DE9 && seqPos == 0x5DEE)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DEE && seqPos == 0x5DF4)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DF4 && seqPos == 0x5DF9)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DF9 && seqPos == 0x5DFF)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5DFF && seqPos == 0x5E05)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5E05 && seqPos == 0x5E0B)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5E0B && seqPos == 0x5E11)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
								else if (segPtr == 0x5E11 && seqPos == 0x5E16)
								{
									seqEnd = 1;
								}

								else if (segPtr == 0x5E71 && seqPos == 0x5E77)
								{
									nextSegPtr += 8;
									segPtr = ReadLE16(&exRomData[nextSegPtr + (curTrack * 2)]);
									seqPos = segPtr;
								}
							}
						}

						/*Play note*/
						if (command[0] < 0xC0)
						{
							hasPlayedNote++;
							rest = 0;
							/*Note length 1*/
							if (command[0] == 0x00)
							{
								curNote = freq;
								curNoteLen = 5 * noteTable[0];
							}
							else if (command[0] > 0x00 && command[0] < 0x10)
							{
								curNote = freq + command[0];
								curNoteLen = 5 * noteTable[0];
							}
							else if (command[0] == 0x10)
							{
								curDelay += 5 * noteTable[0];
								rest = 1;
							}
							else if (command[0] > 0x10 && command[0] < 0x20)
							{
								curNote = freq - (16 - (command[0] - 0x10));
								curNoteLen = 5 * noteTable[0];
							}
							/*Note length 2*/
							else if (command[0] == 0x20)
							{
								curNote = freq;
								curNoteLen = 5 * noteTable[1];
							}
							else if (command[0] > 0x20 && command[0] < 0x30)
							{
								curNote = freq + (command[0] - 0x20);
								curNoteLen = 5 * noteTable[1];
							}
							else if (command[0] == 0x30)
							{
								curDelay += 5 * noteTable[1];
								rest = 1;
							}
							else if (command[0] > 0x30 && command[0] < 0x40)
							{
								curNote = freq - (16 - (command[0] - 0x30));
								curNoteLen = 5 * noteTable[1];
							}
							/*Note length 3*/
							else if (command[0] == 0x40)
							{
								curNote = freq;
								curNoteLen = 5 * noteTable[2];
							}
							else if (command[0] > 0x40 && command[0] < 0x50)
							{
								curNote = freq + (command[0] - 0x40);
								curNoteLen = 5 * noteTable[2];
							}
							else if (command[0] == 0x50)
							{
								curDelay += 5 * noteTable[2];
								rest = 1;
							}
							else if (command[0] > 0x50 && command[0] < 0x60)
							{
								curNote = freq - (16 - (command[0] - 0x50));
								curNoteLen = 5 * noteTable[2];
							}
							/*Note length 4*/
							else if (command[0] == 0x60)
							{
								curNote = freq;
								curNoteLen = 5 * noteTable[3];
							}
							else if (command[0] > 0x60 && command[0] < 0x70)
							{
								curNote = freq + (command[0] - 0x60);
								curNoteLen = 5 * noteTable[3];
							}
							else if (command[0] == 0x70)
							{
								curDelay += 5 * noteTable[3];
								rest = 1;
							}
							else if (command[0] > 0x70 && command[0] < 0x80)
							{
								curNote = freq - (16 - (command[0] - 0x70));
								curNoteLen = 5 * noteTable[3];
							}
							/*Note length 5*/
							else if (command[0] == 0x80)
							{
								curNote = freq;
								curNoteLen = 5 * noteTable[4];
							}
							else if (command[0] > 0x80 && command[0] < 0x90)
							{
								curNote = freq + (command[0] - 0x80);
								curNoteLen = 5 * noteTable[4];
							}
							else if (command[0] == 0x90)
							{
								curDelay += 5 * noteTable[4];
								rest = 1;
							}
							else if (command[0] > 0x90 && command[0] < 0xA0)
							{
								curNote = freq - (16 - (command[0] - 0x90));
								curNoteLen = 5 * noteTable[4];
							}
							/*Note length 6*/
							else if (command[0] == 0xA0)
							{
								curNote = freq;
								curNoteLen = 5 * noteTable[5];
							}
							else if (command[0] > 0xA0 && command[0] < 0xB0)
							{
								curNote = freq + (command[0] - 0xA0);
								curNoteLen = 5 * noteTable[5];
							}
							else if (command[0] == 0xB0)
							{
								curDelay += 5 * noteTable[5];
								rest = 1;
							}
							else if (command[0] > 0xB0 && command[0] < 0xC0)
							{
								curNote = freq - (16 - (command[0] - 0xB0));
								curNoteLen = 5 * noteTable[5];
							}

							if (rest == 0)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}

							seqPos++;
						}

						/*Play note (custom length)*/
						else if (command[0] >= 0xC0 && command[0] < 0xE0)
						{
							hasPlayedNote++;
							rest = 0;
							if (command[0] == 0xC0)
							{
								curNote = freq;
								curNoteLen = command[1] * 5;
							}
							else if (command[0] > 0xC0 && command[0] < 0xD0)
							{
								curNote = freq + (command[0] - 0xC0);
								curNoteLen = command[1] * 5;
							}
							else if (command[0] == 0xD0)
							{
								curDelay += command[1] * 5;
								rest = 1;
							}
							else if (command[0] > 0xD0)
							{
								curNote = freq - (16 - (command[0] - 0xD0));
								curNoteLen = command[1] * 5;
							}

							if (bank1 == 0x20 && (curNoteLen == 5 || curNoteLen == 10) && (songNum == 13 || songNum == 16 || songNum == 17))
							{
								rest = 1;
							}


							if (rest == 0)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}

							seqPos += 2;
						}

						/*Unknown command E1*/
						else if (command[0] == 0xE1)
						{
							seqPos += 2;
						}

						/*Unknown command E2*/
						else if (command[0] == 0xE2)
						{
							seqPos += 3;
						}

						/*Frequency change?*/
						else if (command[0] == 0xE3)
						{
							seqPos += 2;
						}

						/*Set note volume*/
						else if (command[0] == 0xF0)
						{
							seqPos += 2;
						}

						/*Set "echo" effect?*/
						else if (command[0] == 0xF1)
						{
							seqPos += 2;
						}

						/*Set note length table*/
						else if (command[0] == 0xF2)
						{
							for (tempLen = 0; tempLen < 6; tempLen++)
							{
								noteTable[tempLen] = noteTables[command[1]][tempLen];
							}
							seqPos += 2;
						}

						/*Hold note*/
						else if (command[0] == 0xF3)
						{
							hasPlayedNote++;
							curDelay += command[1] * 5;
							rest = 1;
							seqPos += 2;
						}

						/*Set note size*/
						else if (command[0] == 0xF4)
						{
							seqPos += 2;
						}

						/*Set frequency*/
						else if (command[0] == 0xF5)
						{
							freq = command[1] + 9;
							if (freq >= 128)
							{
								freq = 0x7F;
							}
							seqPos += 2;
						}

						/*Set instrument*/
						else if (command[0] == 0xF6)
						{

							if (curTrack != 3)
							{
								curInst = command[1];
								if (curInst != command[0])
								{
									firstNote = 1;
								}
								if (curInst >= 0x80)
								{
									curInst = 0x00;
								}
							}



							seqPos += 2;
						}

						/*Unknown command F7*/
						else if (command[0] == 0xF7)
						{
							seqPos += 2;
						}

						/*Go to loop point*/
						else if (command[0] == 0xF8)
						{
							if (hasPlayedNote > 2)
							{
								if (tempLoopPt > 0)
								{
									tempLoopPt = ReadLE16(&romData[seqPos + 1 - bankAmt]);
								}
								if (bank1 == 0x20 && songNum == 38 && tempLoopPt == 0x7411 && tempLoopPt > 0)
								{
									seqPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
									tempLoopPt = 0;
								}
								else if (bank1 == 0x20 && songNum == 16 && tempLoopPt > 0)
								{
									seqPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
									tempLoopPt = 0;
								}
								else
								{
									seqEnd = 1;
								}


							}
							else
							{
								seqPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
							}
						}

						/*Unknown command F9*/
						else if (command[0] == 0xF9)
						{
							seqPos++;
						}

						/*Go to macro*/
						else if (command[0] == 0xFA)
						{
							if (inMacro == 0)
							{
								macro1Pos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
								macro1Ret = seqPos += 3;
								seqPos = macro1Pos;
								inMacro = 1;
							}
							else if (inMacro == 1)
							{
								macro2Pos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
								macro2Ret = seqPos += 3;
								seqPos = macro2Pos;
								inMacro = 2;
							}

						}

						/*Return from macro*/
						else if (command[0] == 0xFB)
						{
							if (inMacro == 1)
							{
								seqPos = macro1Ret;
								inMacro = 0;
							}
							else if (inMacro == 2)
							{
								seqPos = macro2Ret;
								inMacro = 1;
							}
							else
							{
								seqEnd = 1;
							}

						}

						/*Repeat section # of times*/
						else if (command[0] == 0xFC)
						{
							if (repeat1 == -1)
							{
								repeat1 = command[1];
								if (repeat1 == 222)
								{
									repeat1 = 1;
								}
								repeat1Pt = seqPos + 2;
								seqPos += 2;
							}
							else
							{
								if (repeat2 == 222)
								{
									repeat2 = 1;
								}
								repeat2 = command[1];
								repeat2Pt = seqPos + 2;
								seqPos += 2;
							}

						}

						/*End of repeat 1*/
						else if (command[0] == 0xFD)
						{
							if (repeat2 == -1)
							{
								if (repeat1 > 1)
								{
									seqPos = repeat1Pt;
									repeat1--;
								}
								else if (repeat1 <= 1)
								{
									repeat1 = -1;
									seqPos++;
								}
							}
							else if (repeat2 > -1)
							{
								if (repeat2 > 1)
								{
									seqPos = repeat2Pt;
									repeat2--;
								}
								else if (repeat2 <= 1)
								{
									repeat2 = -1;
									seqPos++;
								}
							}

						}

						/*Set channel volume?*/
						else if (command[0] == 0xFE)
						{
							seqPos += 2;
						}

						/*End song without loop*/
						else if (command[0] == 0xFF)
						{
							seqEnd = 1;
						}

						/*Unknown command*/
						else
						{
							seqPos++;
						}

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
			free(midData);
			free(ctrlMidData);
			fclose(mid);
		}

		/*Ghostbusters II and Trax use a slightly different song format*/
		else if (format == 1)
		{
			romPos = ptr - bankAmt;

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


			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				firstNote = 1;
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;
				curDelay = 0;
				seqEnd = 0;

				curNote = 0;
				curNoteLen = 0;
				repeat1 = -1;
				repeat1Pt = -1;
				repeat2 = -1;
				repeat2Pt = -1;
				macro1Pos = 0;
				macro1Ret = 0;
				freq = 0;
				rest = 0;
				hasPlayedNote = 0;
				inMacro = 0;
				tempLoopPt = 1;
				curNoteVol = 100;
				segCnt = 0;
				hasPlayedMacro = 0;

				if (curTrack == 3)
				{
					freq = 32;
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

				if (chanPtrs[curTrack] != 0)
				{
					seqPos = chanPtrs[curTrack];
					seqEnd = 0;

					while (seqEnd == 0)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos + 1 - bankAmt];
						command[2] = romData[seqPos + 2 - bankAmt];
						command[3] = romData[seqPos + 3 - bankAmt];

						/*Play note*/
						if (command[0] < 0x80)
						{
							rest = 0;
							if (command[0] == 0x7C)
							{
								curDelay += curNoteLen;
								rest = 1;
							}
							else
							{
								curNote = HALgb2Notes[command[0]] + 24;
							}

							if (rest == 0)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}

							seqPos++;
						}

						/*Change note length*/
						else if (command[0] >= 0x80 && command[0] < 0xE0)
						{
							lowNibble = command[0] - 0x80;
							curNoteLen = romData[(noteTableOffset - bankAmt) + lowNibble] * 5;

							seqPos++;
						}

						/*Unknown command E1*/
						else if (command[0] == 0xE1)
						{
							seqPos += 2;
						}

						/*Unknown command E2*/
						else if (command[0] == 0xE2)
						{
							seqPos += 3;
						}

						/*Frequency change?*/
						else if (command[0] == 0xE3)
						{
							seqPos += 2;
						}

						/*Set note size*/
						else if (command[0] == 0xF0)
						{
							seqPos += 2;
						}

						/*Set duty*/
						else if (command[0] == 0xF1)
						{
							seqPos += 2;
						}

						/*Set channel volume*/
						else if (command[0] == 0xF2)
						{
							seqPos += 2;
						}

						/*Unknown command F3*/
						else if (command[0] == 0xF3)
						{
							seqPos += 2;
						}

						/*Repeat section # of times*/
						else if (command[0] == 0xF4)
						{
							repeat1 = command[1];
							if (repeat1 == 222)
							{
								repeat1 = 1;
							}
							repeat1Pt = seqPos + 2;
							seqPos += 2;
						}

						/*End of repeat*/
						else if (command[0] == 0xF5)
						{
							if (repeat1 > 1)
							{
								seqPos = repeat1Pt;
								repeat1--;
							}
							else if (repeat1 <= 1)
							{
								repeat1 = -1;
								seqPos++;
							}
						}

						/*Go to macro*/
						else if (command[0] == 0xF6)
						{

							if (inMacro == 0)
							{
								macro1Pos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
								macro1Ret = seqPos += 3;
								seqPos = macro1Pos;
								inMacro = 1;
							}
							else if (inMacro == 1)
							{
								macro2Pos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
								macro2Ret = seqPos += 3;
								seqPos = macro2Pos;
								inMacro = 2;
							}
						}

						/*Return from macro*/
						else if (command[0] == 0xF7)
						{
							if (inMacro == 1)
							{
								seqPos = macro1Ret;
								inMacro = 0;
							}
							else if (inMacro == 2)
							{
								seqPos = macro2Ret;
								inMacro = 1;
							}
							else
							{
								seqEnd = 1;
							}
						}

						/*Pitch slide*/
						else if (command[0] == 0xF8)
						{
							seqPos += 2;
						}

						/*Unknown command F9*/
						else if (command[0] == 0xF9)
						{
							seqPos++;
						}

						/*Go to loop point*/
						else if (command[0] == 0xFB)
						{
							seqEnd = 1;
						}

						/*Go to loop point*/
						else if (command[0] == 0xFF)
						{
							seqEnd = 1;
						}

						/*Unknown command*/
						else
						{
							seqPos++;
						}
					}
					/*End of track*/
					WriteBE32(&midData[midPos], 0xFF2F00);
					midPos += 4;

					/*Calculate MIDI channel size*/
					trackSize = midPos - midTrackBase;
					WriteBE16(&midData[midTrackBase - 2], trackSize);
				}
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
}