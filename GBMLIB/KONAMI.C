/*Konami*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "KONAMI.H"

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
int format;
int mb;
int multiplier;
int curVol;
int isSFX;
int manualFormat;
int fmtOverride;
int mbOverride;
int mbMethod;
int tempoCheck;
int cv2Fix;
int pdFix;
long tempLen;
int multiBanks;
int curBank;
int f1Fix;

char folderName[100];

unsigned char* romData;
unsigned char* exBankData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char KonMagicBytes1[3] = { 0x40, 0x2A, 0xEA };
const char KonMagicBytes1b[3] = { 0x1E, 0x2A, 0xEA };
const char KonMagicBytes2[3] = { 0x40, 0x7E, 0xEA };
const char KonMagicBytes3[5] = { 0x16, 0x00, 0x19, 0x5E, 0x23 };
const char KonMagicBytes4[3] = { 0xCF, 0x5D, 0x54 };
const char KonMagicBytes5[3] = { 0xCF, 0x2A, 0xEA };
const char KonMagicBytes6[4] = { 0xCD, 0x81, 0x4C, 0x2A };
const char KonMagicBytes6b[4] = { 0xCD, 0x8B, 0x4C, 0x2A };
const char KonMagicBytes6c[4] = { 0xCD, 0x03, 0x4D, 0x2A };
const char KonMagicBytes7[3] = { 0x61, 0x7E, 0xEA };
const char KonMagicBytes7b[3] = { 0x64, 0x7E, 0xEA };
const char KonMagicBytes7c[3] = { 0x5F, 0x7E, 0xEA };
const char KonMagicBytes8[4] = { 0xEA, 0xCF, 0xC0, 0x21 };
const char KonMagicBytes9[3] = { 0x48, 0x2A, 0xEA };
const char KonMagicBytes10[3] = { 0x55, 0x2A, 0xEA };
const char KonMagicBytes11[3] = { 0xF5, 0xEF, 0xF1 };
const char KonMagicBytes12[3] = { 0xCB, 0x7F, 0x28 };
const char KonMagicBytes13[3] = { 0x49, 0x2A, 0xEA };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void Konsong2mid(int songNum, long);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);

void KonProc(int bank, char parameters[4][50])
{
	foundTable = 0;
	format = 0;
	mb = 0;
	multiplier = 5;
	curVol = 100;
	isSFX = 0;
	manualFormat = 0;
	fmtOverride = 0;
	mbOverride = 0;
	mbMethod = 0;
	tempoCheck = 0;
	cv2Fix = 0;
	pdFix = 0;
	tempLen = 0;
	f1Fix = 0;

	bank1 = bank;
	if (parameters[1][0] != 0)
	{
		fmtOverride = 1;
		manualFormat = strtoul(parameters[0], NULL, 16);
		mbOverride = 1;
		mbMethod = strtoul(parameters[1], NULL, 16);
	}

	else if (parameters[0][0] != 0)
	{
		fmtOverride = 1;
		manualFormat = strtoul(parameters[0], NULL, 16);

		if (manualFormat == 10)
		{
			manualFormat = 3;
			f1Fix = 1;
		}
	}

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Try to search the bank for song table loader - Method 1: Castlevania 1/2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes1, 3)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			multiplier = 10;

			if (romData[i - 1] != 0x33)
			{
				format = 1;
				multiplier = 5;
				if (romData[i - 8] != 0xEA)
				{
					format = 3;
				}

				/*Fix for Tiny Toon Adventures*/
				if (tableOffset == 0x4F2F)
				{
					format = 7;
				}
			}
			foundTable = 1;
			if (tableOffset == 0x4A79)
			{
				cv2Fix = 1;
			}
		}
	}

	/*Method 2: Nemesis*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes2, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;
		}
	}

	/*Method 3: Motocross Maniacs*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes3, 5)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 2;
		}
	}

	/*Method 4: NFL Football*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes2, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;
		}
	}

	/*Method 5: Skate or Die*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes5, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			multiplier = 10;
			format = 0;
			if (tableOffset == 0x5ABD)
			{

				format = 8;
				multiplier = 5;

			}

		}
	}

	/*Method 6: Konami Golf (E)*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes6, 4)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;

		}
	}

	/*Method 6b: Konami Golf (J)*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes6b, 4)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;

		}
	}

	/*Method 6c: Ultra Golf (U)*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes6c, 4)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;

		}
	}

	/*Method 7: TMNT2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes8, 4)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 4;

		}
	}

	/*Method 8: Batman*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes9, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 6;
		}
	}

	/*Method 9: Tiny Toon Adventures 2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes10, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 6;
		}
	}

	/*Method 11: TTA Wacky Sports*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes13, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 6;
		}
	}

	/*Try to search the bank for song table loader - Block Game (Collection 2)*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes1b, 3)) && foundTable != 1 && bank == 0x13)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			multiplier = 10;
			foundTable = 1;
		}
	}


	/*Multi-banked method 1: Nemesis 2*/
	if (ReadLE16(&romData[0]) >= bankAmt && ReadLE16(&romData[0]) < bankSize * 2 && foundTable != 1)
	{
		tableOffset = 0x4000;
		printf("Song table starts at 0x%04x...\n", tableOffset);
		foundTable = 1;
		format = 5;

		/*"Override" for Nemesis 2*/
		if (ReadLE16(&romData[0x28B]) == 0x07C4 || ReadLE16(&romData[0x450]) == 0x07C4 || ReadLE16(&romData[0x267]) == 0x07C4)
		{
			format = 3;
		}
		if (ReadLE16(&romData[0]) == 0x4000 || ReadLE16(&romData[0]) == 0x4002)
		{
			mb = 2;
		}
	}

	/*Multi-banked method 2b: Goemon 1*/
	if (ReadLE16(&romData[0x3000]) > bankAmt && ReadLE16(&romData[0x3000]) < bankSize * 2 && foundTable != 1)
	{
		if (bank != 0x0F)
		{
			tableOffset = 0x7000;
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 5;
		}

	}

	/*Multi-banked method 2: Parodius*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes7, 3)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;
			mb = 1;
			pdFix = 1;

		}
	}

	/*Multi-banked method 2b: Variation of above for Harry Potter 3*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes7b, 3)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;
			mb = 1;
			pdFix = 1;

		}
	}

	/*Multi-banked method 2c: Variation of above for Konami GB Collection Vol. 2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes7c, 3)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;
			mb = 1;
			pdFix = 1;

		}
	}

	/*Multi-banked method 3: Kid Dracula*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes11, 3)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 7;
			mb = 1;

		}
	}

	if (foundTable == 1)
	{
		if (fmtOverride == 1)
		{
			format = manualFormat;
		}
		if (mbOverride == 1)
		{
			mb = mbMethod;
		}
		i = tableOffset - bankAmt;
		if (ReadLE16(&romData[i]) >= bankSize * 2 || ReadLE16(&romData[i]) <= bankSize || format == 1 || format == 3)
		{
			i += 2;
		}
		songNum = 1;
		while (ReadLE16(&romData[i]) > bankAmt)
		{
			songPtr = ReadLE16(&romData[i]);
			if (songPtr == 0x4000 || songPtr == 0x4002)
			{
				i += 2;
			}
			if ((songPtr == 0x438C || songPtr == 0x47A6) && format == 9 && songNum == 1)
			{
				i += 2;
				songPtr = ReadLE16(&romData[i]);
			}
			if (tableOffset == 0x4000 && songPtr > 0x7000)
			{
				break;
			}

			if (songPtr == 0x7840)
			{
				break;
			}
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0)
			{
				Konsong2mid(songNum, songPtr);
			}
			i += 2;
			songNum++;
		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}

}

void Konsong2mid(int songNum, long ptr)
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
	int curIns = 0;
	float chanSpeed = 0;
	int octave = 0;
	int transpose = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	int repeat2 = 0;
	long repeat2Pt = 0;
	long jumpPos = 0;
	long jumpPosRet = 0;
	int setRep1 = 0;
	int setRep2 = 0;
	int setLoop = 0;
	unsigned char command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long loop = 0;
	int songBank = 0;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int romBank = bank1 - 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int lenVal = 0;
	int rest = 0;
	int isJump = 0;
	int tempoVal = 0;
	int f9Cnt = 0;
	long f9Pos = 0;
	int lastNote = 0;
	long chanStart = 0;
	int fbSwitch = 0;
	int fcSwitch = 0;
	int fbJump = 0;
	int fcJump = 0;

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
			if (mb == 0)
			{
				mask = romData[romPos + 1];
			}
			else if (mb == 1)
			{
				/*Get the current bank*/
				songBank = romData[romPos + 1];
				fseek(rom, ((songBank)*bankSize), SEEK_SET);
				exBankData = (unsigned char*)malloc(bankSize);
				fread(exBankData, 1, bankSize, rom);

				mask = romData[romPos + 2];
			}
			else if (mb == 2)
			{
				/*Get the current bank*/
				lowNibble = (romData[romPos] >> 4);
				highNibble = (romData[romPos] & 15);

				songBank = romBank + lowNibble;
				fseek(rom, ((songBank)*bankSize), SEEK_SET);
				exBankData = (unsigned char*)malloc(bankSize);
				fread(exBankData, 1, bankSize, rom);

				mask = romData[romPos + 1];
			}
			else if (mb == 3)
			{
				/*Get the current bank*/
				lowNibble = (romData[romPos] >> 4);
				highNibble = (romData[romPos] & 15);

				songBank = romBank + lowNibble;
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				exBankData = (unsigned char*)malloc(bankSize);
				fread(exBankData, 1, bankSize, rom);

				mask = romData[romPos + 1];
			}


			/*Format 1 (Castlevania 1)*/
			if (format == 0)
			{
				for (k = 0; k < 4; k++)
				{
					maskArray[k] = 0;
				}


				/*Try to get active channels for sound effects*/
				maskArray[3] = mask >> 1 & 1;
				maskArray[0] = mask & 1;
				isSFX = 1;

				if (maskArray[0] == 0 && maskArray[3] == 0)
				{
					maskArray[3] = mask >> 5 & 1;
					maskArray[2] = mask >> 4 & 1;
					maskArray[1] = mask >> 3 & 1;
					maskArray[0] = mask >> 2 & 1;
					isSFX = 0;
				}

				else
				{
				}
			}

			/*Format 2 (Castlevania 2/Nemesis)*/
			else if (format == 1 || format >= 3)
			{
				for (k = 0; k < 4; k++)
				{
					maskArray[k] = 0;
				}


				/*Try to get active channels for sound effects*/
				maskArray[3] = mask >> 1 & 1;
				maskArray[2] = mask >> 2 & 1;
				maskArray[0] = mask & 1;
				isSFX = 1;

				if (maskArray[0] == 0 && maskArray[2] == 0 && maskArray[3] == 0)
				{
					maskArray[3] = mask >> 6 & 1;
					maskArray[2] = mask >> 5 & 1;
					maskArray[1] = mask >> 4 & 1;
					maskArray[0] = mask >> 3 & 1;
					isSFX = 0;
				}
			}

			/*Format 3 (Motocross Maniacs)*/
			else if (format == 2)
			{
				for (k = 0; k < 4; k++)
				{
					maskArray[k] = 0;
				}


				/*Try to get active channels for sound effects*/
				maskArray[3] = mask >> 2 & 1;
				maskArray[0] = mask >> 1 & 1;
				isSFX = 1;

				if (maskArray[0] == 0 && maskArray[3] == 0)
				{
					maskArray[3] = mask >> 6 & 1;
					maskArray[1] = mask >> 5 & 1;
					maskArray[2] = mask >> 4 & 1;
					maskArray[0] = mask >> 3 & 1;
					isSFX = 0;
				}
			}

			if (mb == 0 || mb == 2 || mb == 3)
			{
				romPos += 2;
			}
			else if (mb == 1)
			{
				romPos += 3;

			}

			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				firstNote = 1;
				holdNote = 0;
				chanSpeed = 1;
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				octave = 3;
				midPos += 8;
				midTrackBase = midPos;

				curDelay = 0;
				seqEnd = 0;

				curNote = 0;
				curNoteLen = 0;
				repeat1 = -1;
				repeat2 = -1;
				transpose = 0;
				jumpPos = 0;
				jumpPosRet = 0;
				songLoopAmt = 0;
				songLoopPt = 0;
				rest = 0;
				curVol = 100;
				isJump = 0;
				f9Cnt = -1;
				fbSwitch = 0;
				fcSwitch = 0;

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

				if (maskArray[curTrack] == 0)
				{
					seqEnd = 1;
				}
				else
				{
					seqEnd = 0;
					seqPos = ReadLE16(&romData[romPos]);
					chanStart = seqPos;
					setRep1 = 0;
					setRep2 = 0;
					setLoop = 0;
				}

				while (seqEnd == 0 && seqPos < bankSize * 2 && midPos < 48000)
				{
					if (mb == 0)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos + 1 - bankAmt];
						command[2] = romData[seqPos + 2 - bankAmt];
						command[3] = romData[seqPos + 3 - bankAmt];
					}
					else if (mb == 1 || mb == 2 || mb == 3)
					{
						command[0] = exBankData[seqPos - bankAmt];
						command[1] = exBankData[seqPos + 1 - bankAmt];
						command[2] = exBankData[seqPos + 2 - bankAmt];
						command[3] = exBankData[seqPos + 3 - bankAmt];
					}

					if ((format == 7 || format == 8) && command[0] >= 0xD0 && command[0] < 0xE0)
					{
						if (command[0] == 0xDA)
						{
							lowNibble = (command[1] >> 4);
							highNibble = (command[1] & 15);

							/*Transpose up*/
							if (lowNibble == 0)
							{
								transpose = highNibble;
							}

							/*Transpose down*/
							else if (lowNibble == 1)
							{
								transpose = highNibble * -1;
							}

							else if (lowNibble > 1)
							{
								;
							}
						}
						seqPos += 2;
					}

					/*Play note*/
					else if (command[0] < 0xE0)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);


						if (highNibble > 0)
						{
							lenVal = highNibble;
						}
						else if (highNibble == 0)
						{
							lenVal = 16;
						}

						if (format == 5 && chanSpeed > 0x80)
						{
							chanSpeed = 0.25;
						}

						if (lowNibble > 0)
						{
							curNote = lowNibble + (12 * octave) + transpose - 1;
							if (format == 5 && lowNibble == 13)
							{
								curNote = lastNote;
							}

							if (curTrack < 3)
							{
								curNote += 12;
							}
							curNoteLen = lenVal * chanSpeed * multiplier;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curIns);
							lastNote = curNote;
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}

						/*Rest*/
						else if (lowNibble == 0)
						{
							rest = lenVal;
							curDelay += (rest * chanSpeed * multiplier);
						}

						seqPos++;

					}

					/*Set parameters*/
					else if (command[0] == 0xE0)
					{
						if (curTrack < 3)
						{
							chanSpeed = command[1];
							if (format == 6)
							{
								lowNibble = (command[1] >> 4);
								highNibble = (command[1] & 15);

								if (lowNibble >= 2)
								{
									octave = lowNibble - 3;

									if (octave == 9)
									{
										octave = 6;
									}
								}
								if (highNibble == 0)
								{
									highNibble = 16;
								}
								chanSpeed = highNibble;
							}
							if (isSFX == 1)
							{
								if (multiplier == 5)
								{
									chanSpeed = chanSpeed / 4;
								}
								else if (multiplier == 10)
								{
									chanSpeed = chanSpeed / 2;
								}

							}
							if (format == 8 && romData[seqPos + 4 - bankAmt] == 0x0F)
							{
								seqPos += 2;
							}

							if (format < 9)
							{
								seqPos += 4;
							}
							else if (format == 9)
							{
								if (isSFX == 1)
								{
									seqPos += 3;
								}
								else if (isSFX == 0)
								{
									seqPos += 4;
								}
							}

						}

						else if (curTrack == 3)
						{
							chanSpeed = command[1];
							seqPos += 2;
						}
					}

					/*Set octave*/
					else if (command[0] > 0xE0 && command[0] < 0xE7)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);
						octave = highNibble;
						seqPos++;
					}

					/*Set channel speed*/
					else if (command[0] == 0xE7)
					{
						chanSpeed = command[1];
						seqPos += 2;
					}

					/*Set note decay?*/
					else if (command[0] == 0xE8)
					{
						seqPos += 2;
					}

					/*Set channel pan?*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Change instrument or transpose*/
					else if (command[0] == 0xEA)
					{
						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);

						/*Transpose up*/
						if (format < 4 || format == 7 || format == 8)
						{
							if (lowNibble == 0)
							{
								transpose = highNibble;
							}

							/*Transpose down*/
							else if (lowNibble == 1)
							{
								transpose = highNibble * -1;
							}

							else if (lowNibble > 1)
							{
								;
							}
						}


						if (format == 4 || format == 5 || format == 6 || format == 9)
						{
							/*Transpose up*/
							if (lowNibble == 7)
							{
								transpose = highNibble;
							}

							/*Transpose down*/
							else if (lowNibble == 8)
							{
								transpose = highNibble * -1;
							}

							if (command[1] == 0x0F)
							{
								seqPos++;
							}
						}

						seqPos += 2;
					}

					/*Effect/portamento*/
					else if (command[0] == 0xEB)
					{
						if (command[1] != 0x00 && command[1] < 0x80)
						{
							seqPos += 3;
						}
						else if (command[1] >= 0x80)
						{
							if (format == 5 || format == 6 || format == 9)
							{
								seqPos += 2;
							}
							else
							{
								seqPos += 3;
							}

						}
						else if (command[1] == 0x00)
						{
							seqPos += 2;
						}

					}

					/*Effect 2?*/
					else if (command[0] == 0xEC)
					{
						seqPos += 2;
					}

					/*Set envelope?*/
					else if (command[0] == 0xED)
					{
						seqPos += 2;
					}

					/*Set tempo*/
					else if (command[0] == 0xEE)
					{
						tempoVal = command[1];

						if (tempoVal < 0x10)
						{
							tempoCheck = 160;
						}
						else if (tempoVal >= 0x10 && tempoVal < 0x20)
						{
							tempoCheck = 150;
						}
						else if (tempoVal >= 0x20 && tempoVal < 0x30)
						{
							tempoCheck = 135;
						}
						else if (tempoVal >= 0x30 && tempoVal < 0x40)
						{
							tempoCheck = 120;
						}
						else if (tempoVal >= 0x40 && tempoVal < 0x50)
						{
							tempoCheck = 115;
						}
						else if (tempoVal >= 0x50 && tempoVal < 0x60)
						{
							tempoCheck = 110;
						}
						else if (tempoVal >= 0x60 && tempoVal < 0x70)
						{
							tempoCheck = 100;
						}
						else if (tempoVal >= 0x70 && tempoVal < 0x80)
						{
							tempoCheck = 90;
						}
						else if (tempoVal >= 0x80 && tempoVal < 0x90)
						{
							tempoCheck = 80;
						}
						else if (tempoVal >= 0x90 && tempoVal < 0xA0)
						{
							tempoCheck = 70;
						}
						else if (tempoVal >= 0xA0 && tempoVal < 0xB0)
						{
							tempoCheck = 60;
						}
						else if (tempoVal >= 0xB0 && tempoVal < 0xC0)
						{
							tempoCheck = 50;
						}
						else if (tempoVal >= 0xC0 && tempoVal < 0xD0)
						{
							tempoCheck = 40;
						}
						else if (tempoVal >= 0xD0 && tempoVal < 0xE0)
						{
							tempoCheck = 30;
						}
						else if (tempoVal >= 0xE0 && tempoVal < 0xF0)
						{
							tempoCheck = 20;
						}
						else if (tempoVal >= 0xF0)
						{
							tempoCheck = 10;
						}
						if (tempo != tempoCheck)
						{
							tempo = tempoCheck;
							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
						}
						seqPos += 2;
					}

					/*Set panning?*/
					else if (command[0] == 0xEF)
					{
						seqPos += 2;
					}

					/*Set note volume?*/
					else if (command[0] >= 0xF0 && command[0] < 0xFB)
					{
						if (command[0] == 0xF0)
						{
							curVol = 100;
							seqPos++;
						}
						else if (command[0] == 0xF1)
						{
							curVol = 95;
							seqPos++;
						}
						else if (command[0] == 0xF2)
						{
							curVol = 90;
							seqPos++;
						}
						else if (command[0] == 0xF3)
						{
							curVol = 80;
							seqPos++;
						}
						else if (command[0] == 0xF4)
						{
							curVol = 70;
							seqPos++;
						}
						else if (command[0] == 0xF5)
						{
							curVol = 60;
							seqPos++;
						}
						else if (command[0] == 0xF6)
						{

							curVol = 50;
							seqPos++;
						}
						else if (command[0] == 0xF7)
						{
							curVol = 40;
							seqPos++;
						}
						else if (command[0] == 0xF8)
						{
							curVol = 30;
							if (format == 3 || format == 4 || format == 5)
							{
								curVol = 100;
							}
							seqPos++;
						}
						else if (command[0] == 0xF9)
						{
							if (format < 5 || format == 7 || format == 8)
							{
								curVol = 20;
								seqPos++;
							}

							else if (format == 5 || format == 6 || format == 9)
							{
								curVol = 100;
								if (command[1] == 0xFB)
								{
									if (repeat1 == -1)
									{
										fbSwitch = 1;
										seqPos += 2;
									}
									else if (repeat1 == 1)
									{
										fbSwitch = 0;
										seqPos = fbJump;
									}
									else if (repeat1 > 1)
									{
										seqPos += 2;
									}

								}
								else if (command[1] == 0xFC)
								{
									if (repeat2 == -1)
									{
										fcSwitch = 1;
										seqPos += 2;
									}
									else if (repeat2 == 1)
									{
										fcSwitch = 0;
										seqPos = fcJump;
									}
									else if (repeat2 > 1)
									{
										seqPos += 2;
									}
								}
								else
								{
									if (f9Cnt == -1)
									{
										f9Cnt = command[1];
										if (mb == 0)
										{
											f9Pos = ReadLE16(&romData[seqPos + 2 - bankAmt]);
										}
										else if (mb >= 1)
										{
											f9Pos = ReadLE16(&exBankData[seqPos + 2 - bankAmt]);
										}
									}
									else if (f9Cnt > 1)
									{
										f9Cnt--;
										seqPos += 4;
									}

									else if (f9Cnt == 1)
									{
										seqPos = f9Pos;
										f9Cnt = -1;
										f9Pos = 0;
									}
								}


							}
						}
						else if (command[0] == 0xFA)
						{
							if (format < 3 || format == 7 || format == 8)
							{
								curVol = 10;
								seqPos++;
							}
							if (format == 3 || format == 4)
							{
								curVol = 100;
								if (f1Fix == 1)
								{
									seqPos--;
								}
								seqPos += 2;
							}
							else if (format == 5 || format == 6 || format == 9)
							{
								/*Fix for Woody Woodpecker Racing*/
								if (format == 9 && seqPos == chanStart)
								{
									seqPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
								}
								else
								{
									seqEnd = 1;
								}
							}

						}

						if ((format == 3 || format == 4 || format == 5 || format == 6 || format == 9) && command[0] == 0xF8)
						{
							seqPos++;
						}
					}

					/*Start of repeat point OR repeat 1 (later format)*/
					else if (command[0] == 0xFB)
					{
						if (format < 3 || format == 7 || format == 8)
						{
							repeat1Pt = seqPos + 1;
							seqPos++;
						}

						else if (format == 3 || format == 4 || format == 5 || format == 6 || format == 9)
						{
							if (setRep1 == 0)
							{
								setRep1 = 1;
								repeat1Pt = seqPos + 1;
								seqPos++;
							}
							else if (setRep1 == 1)
							{
								if (fbSwitch == 1)
								{
									fbJump = seqPos;
									fbSwitch = 2;
								}
								if (repeat1 > 1)
								{
									seqPos = repeat1Pt;
									repeat1--;
								}

								else if (repeat1 == 1)
								{
									seqPos += 2;
									repeat1 = -1;
									repeat1Pt = seqPos;
									setRep1 = 0;
								}

								else if (repeat1 == -1)
								{
									repeat1 = command[1];
								}
							}
						}
					}

					/*Return from jump OR for later format, repeat 2*/
					else if (command[0] == 0xFC)
					{
						if (format < 3 || format == 7 || format == 8)
						{
							if (pdFix == 0)
							{
								if (mb == 0)
								{
									tempLen = ReadLE16(&romData[seqPos - bankAmt + 1]);
								}
								else if (mb > 0)
								{
									tempLen = ReadLE16(&exBankData[seqPos - bankAmt + 1]);
								}
								if (tempLen == 0xFFFF)
								{
									seqEnd = 1;
								}
							}
							else if (pdFix == 1)
							{
								seqPos = jumpPosRet;
							}



							if (jumpPosRet != 0)
							{
								seqPos = jumpPosRet;
							}
							else
							{
								seqPos++;
							}
						}
						else if (format == 3 || format == 4 || format == 5 || format == 6 || format == 9)
						{
							if (setRep2 == 0)
							{
								setRep2 = 1;
								repeat2Pt = seqPos + 1;
								seqPos++;
							}
							else if (setRep2 == 1)
							{
								if (fcSwitch == 1)
								{
									fcJump = seqPos;
									fcSwitch = 2;
								}
								if (repeat2 > 1)
								{
									seqPos = repeat2Pt;
									repeat2--;
								}

								else if (repeat2 == 1)
								{
									seqPos += 2;
									repeat2 = -1;
									repeat2Pt = seqPos;
									setRep2 = 0;
								}
								else if (repeat2 == -1)
								{
									repeat2 = command[1];
								}
							}
						}


					}

					/*Go to position*/
					else if (command[0] == 0xFD)
					{
						if (format < 3 || format == 7 || format == 8)
						{
							if (mb == 0)
							{
								jumpPos = ReadLE16(&romData[seqPos - bankAmt + 1]);
								jumpPosRet = seqPos + 3;
							}
							else if (mb == 1 || mb == 2 || mb == 3)
							{
								jumpPos = ReadLE16(&exBankData[seqPos - bankAmt + 1]);
								jumpPosRet = seqPos + 3;
							}

							if (jumpPos < seqPos && (format < 3 || format == 7 || format == 8))
							{
								if (format >= 7 && mb > 0)
								{
									if (jumpPos > chanStart)
									{
										seqEnd = 1;
									}
									else
									{
										seqPos = jumpPos;
									}
								}

								/*Fixed loop points for Castlevania 2*/
								else if (format == 1 && cv2Fix == 1)
								{
									if (songNum == 84 && ptr == 0x4CC7)
									{
										seqPos = jumpPos;
									}
									else if (songNum == 86 && ptr == 0x4CEB)
									{
										if (jumpPos == 0x5739 || jumpPos == 0x5749 || jumpPos == 0x5752 || jumpPos == 0x5758 || jumpPos == 0x576A || jumpPos == 0x5779 || jumpPos == 0x5788 || jumpPos == 0x5797)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else if (songNum == 88 && ptr == 0x4D0F)
									{
										if (jumpPos == 0x60AA || jumpPos == 0x60C3 || jumpPos == 0x60D9 || jumpPos == 0x60EC || jumpPos == 0x60F7 || jumpPos == 0x610E || jumpPos == 0x61A3 || jumpPos == 0x61C0 || jumpPos == 0x61DD)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else if (songNum == 91 && ptr == 0x4D19)
									{
										if (jumpPos == 0x5A47 || jumpPos == 0x5A56 || jumpPos == 0x5A5F || jumpPos == 0x5A6F || jumpPos == 0x5A80
											|| jumpPos == 0x5A8E || jumpPos == 0x5A9F || jumpPos == 0x5AB3 || jumpPos == 0x5ABD || jumpPos == 0x5ACE || jumpPos == 0x5ADF
											|| jumpPos == 0x5AFA || jumpPos == 0x5B00 || jumpPos == 0x5B06 || jumpPos == 0x5B0C || jumpPos == 0x5B1C)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else if (songNum == 92 && ptr == 0x4D41)
									{
										if (jumpPos == 0x66D5 || jumpPos == 0x6724 || jumpPos == 0x6738 || jumpPos == 0x674F || jumpPos == 0x675F || jumpPos == 0x6771)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else if (songNum == 96 && ptr == 0x4D4B)
									{
										if (jumpPos == 0x6B4F || jumpPos == 0x6B54 || jumpPos == 0x6B76 || jumpPos == 0x6B4F || jumpPos == 0x6B54 || jumpPos == 0x6B7B || jumpPos == 0x6B95)
										{
											seqPos = jumpPos;
										}
										else if (jumpPos == 0x6CEF || jumpPos == 0x6D00 || jumpPos == 0x6CEF || jumpPos == 0x6D11 || jumpPos == 0x6CEF || jumpPos == 0x6D1A)
										{
											seqPos = jumpPos;
										}
										else if (jumpPos == 0x6B4F || jumpPos == 0x6B65 || jumpPos == 0x6B76 || jumpPos == 0x6B88 || jumpPos == 0x6B76 || jumpPos == 0x6BA3)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else if (songNum == 98 && ptr == 0x4D5F)
									{
										if (jumpPos == 0x70E0 || jumpPos == 0x710B || jumpPos == 0x7140 || jumpPos == 0x7222 || jumpPos == 0x719D || jumpPos == 0x7261 || jumpPos == 0x7166 || jumpPos == 0x7168 || jumpPos == 0x719B)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else if (songNum == 102 && ptr == 0x4D91)
									{
										seqPos = jumpPos;
									}
									else if (songNum == 105 && ptr == 0x4DA5)
									{
										if (jumpPos == 0x7B62 || jumpPos == 0x7B6E || jumpPos == 0x7B7A || jumpPos == 0x7B90 || jumpPos == 0x7B8D)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else if (songNum == 106 && ptr == 0x4DAF)
									{
										if (jumpPos == 0x7DBF || jumpPos == 0x7DE3 || jumpPos == 0x7DE5 || jumpPos == 0x7E30 || jumpPos == 0x7E4C || jumpPos == 0x7E6D)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else
									{
										seqEnd = 1;
									}

								}

								/*Fix for Parodius last song end*/
								else if (format == 1 && pdFix == 1)
								{
									if (songNum == 106 && ptr == 0x709B)
									{
										if (jumpPos == 0x7D09 || jumpPos == 0x7D8A || jumpPos == 0x7C28 || jumpPos == 0x7CAA || jumpPos == 0x7E10 || jumpPos == 0x7E5F || jumpPos == 0x7EB0 || jumpPos == 0x7BD9 || jumpPos == 0x7EBE)
										{
											seqPos = jumpPos;
										}
										else
										{
											seqEnd = 1;
										}
									}
									else
									{
										seqEnd = 1;
									}
								}
								else
								{
									seqEnd = 1;
								}


							}

							else
							{
								seqPos = jumpPos;
							}
						}

						else if (format == 3 || format == 4 || format == 5 || format == 6 || format == 9)
						{
							if (isJump == 0)
							{
								if (mb == 0)
								{
									jumpPos = ReadLE16(&romData[seqPos - bankAmt + 1]);
								}
								else if (mb > 0)
								{
									jumpPos = ReadLE16(&exBankData[seqPos - bankAmt + 1]);
								}

								jumpPosRet = seqPos + 3;
								isJump = 1;
								seqPos = jumpPos;
							}

							else if (isJump == 1)
							{
								seqPos = jumpPosRet;
								isJump = 0;
							}
						}



					}

					/*Repeat section starting from FB value OR start/end of loop point (later format)*/
					else if (command[0] == 0xFE)
					{
						if (format < 3 || format == 7 || format == 8)
						{
							if (command[1] != 0xFF)
							{
								if (repeat1 > 1)
								{
									seqPos = repeat1Pt;
									repeat1--;
								}

								else if (repeat1 == 1)
								{
									seqPos += 2;
									repeat1 = -1;
									repeat1Pt = seqPos;
								}

								else if (repeat1 == -1)
								{
									repeat1 = command[1];

									if (repeat1 > 40)
									{
										repeat1 = 1;
									}
								}


							}

							/*Go to loop point?*/
							else if (command[1] == 0xFF)
							{
								if (mb == 0)
								{
									loop = ReadLE16(&romData[seqPos - bankAmt + 2]);
								}
								else if (mb == 1 || mb == 2 || mb == 3)
								{
									loop = ReadLE16(&exBankData[seqPos - bankAmt + 2]);
								}

								seqEnd = 1;
							}
						}

						else if (format == 3 || format == 4 || format == 5 || format == 6 || format == 9)
						{
							if (setLoop == 0)
							{
								loop = seqPos + 1;
								setLoop = 1;
								seqPos++;
							}
							else if (setLoop == 1)
							{
								seqEnd = 1;
							}

						}

					}

					/*End of sequence*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					/*Unknown command*/
					else
					{
						seqPos += 2;
					}
				}
				/*End of track*/
				if (maskArray[curTrack] != 0)
				{
					romPos += 2;
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
			if (exBankData != NULL)
			{
				free(exBankData);
			}
			fclose(mid);
		}
}