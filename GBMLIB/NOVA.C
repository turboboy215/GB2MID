/*Nova/Arc System Works*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "NOVA.H"

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
int banked;
long bankMap;
long songBank;
int curInst;
int verOverride;
int ptrOverride;
int multiBanks;
int curBank;
int bankNum;
int bankOverride;

char folderName[100];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char NovaMagicBytesA[4] = { 0x4F, 0x06, 0x00, 0x21 };
const char NovaMagicBytesB[3] = { 0x19, 0x7E, 0xCD };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Novasong2mid(int songNum, long ptr, int songBank);

void NovaProc(int bank, char parameters[4][50])
{
	foundTable = 0;
	curInst = 0;
	drvVers = 1;
	verOverride = 0;
	ptrOverride = 0;
	bankNum = bank;
	bankOverride = 0;

	if (parameters[1][0] != 0)
	{
		tableOffset = strtoul(parameters[1], NULL, 16);
		ptrOverride = 1;
		foundTable = 1;
		drvVers = strtoul(parameters[0], NULL, 16);
		verOverride = 1;

		if (drvVers > 11 || drvVers < 1)
		{
			printf("ERROR: Invalid version specified!\n");
			exit(2);
		}
	}
	else if (parameters[0][0] != 0)
	{
		drvVers = strtoul(parameters[0], NULL, 16);
		verOverride = 1;

		if (drvVers > 11 || drvVers < 1)
		{
			printf("ERROR: Invalid version specified!\n");
			exit(2);
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

	if (ptrOverride == 0)
	{
		/*Try to search the bank for song table loader - Multi-banked version*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], NovaMagicBytesB, 3)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i - 2;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc + 8 - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				bankMap = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Bank mapping starts at 0x%04x...\n", bankMap);
				foundTable = 1;
				drvVers = 2;
			}
		}

		/*Single-banked version*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], NovaMagicBytesA, 4)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 4;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				if (verOverride == 0)
				{
					drvVers = 1;
				}
			}
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;

		songNum = 1;

		if (drvVers == 2 && bankMap == 0x496F && bankNum != 0x78)
		{
			bankOverride = 1;
		}
		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			if (drvVers != 2 && drvVers != 11)
			{
				songBank = bank - 1;
				fseek(rom, (songBank * bankSize), SEEK_SET);
				if (songBank != 0)
				{
					exRomData = (unsigned char*)malloc(bankSize);
					fread(exRomData, 1, bankSize, rom);
				}
				else if (songBank == 0)
				{
					if (drvVers == 5)
					{
						/*Banks 1 and 10 are used together*/
						fseek(rom, 0, SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, bankSize, rom);
						fseek(rom, (15 * bankSize), SEEK_SET);
						fread(exRomData + bankSize, 1, bankSize, rom);
					}
					else if (drvVers == 6)
					{
						/*Banks 1 and 1E are used together*/
						fseek(rom, 0, SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, bankSize, rom);
						fseek(rom, (30 * bankSize), SEEK_SET);
						fread(exRomData + bankSize, 1, bankSize, rom);
					}
					else if (drvVers == 7)
					{
						/*Banks 1 and 8 are used together*/
						fseek(rom, 0, SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, bankSize, rom);
						fseek(rom, (7 * bankSize), SEEK_SET);
						fread(exRomData + bankSize, 1, bankSize, rom);
					}
					else if (drvVers == 8)
					{
						/*Banks 1 and 14 are used together*/
						fseek(rom, 0, SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, bankSize, rom);
						fseek(rom, (19 * bankSize), SEEK_SET);
						fread(exRomData + bankSize, 1, bankSize, rom);
					}
					else if (drvVers == 9)
					{
						/*Banks 1 and 20 are used together*/
						fseek(rom, 0, SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, bankSize, rom);
						fseek(rom, (31 * bankSize), SEEK_SET);
						fread(exRomData + bankSize, 1, bankSize, rom);
					}
					else if (drvVers == 10)
					{
						/*Banks 1 and 3 are used together*/
						fseek(rom, 0, SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, bankSize, rom);
						fseek(rom, (2 * bankSize), SEEK_SET);
						fread(exRomData + bankSize, 1, bankSize, rom);
					}
					else
					{
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, (bankSize * 2), rom);
					}

				}
				if (exRomData[songPtr - bankAmt] > 0 && exRomData[songPtr - bankAmt] <= 4)
				{
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Novasong2mid(songNum, songPtr, songBank);
				}
				else
				{
					printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
				}

			}
			else if (drvVers == 2)
			{
				songBank = romData[bankMap + (songNum - 1) - bankAmt];

				/*Fix "incorrect" bank values for unlicensed games with audio stolen from RPG Tsukuru GB*/
				if (bankOverride == 1)
				{
					if (bankNum == 0x11)
					{
						if (songBank == 0x77)
						{
							songBank = 0x2C;
						}
						else if (songBank == 0x78)
						{
							songBank = 0x2D;
						}
						else if (songBank == 0x79)
						{
							songBank = 0x10;
						}
						else if (songBank == 0x7A)
						{
							songBank = 0x11;
						}
					}
					else if (bankNum == 0x0F)
					{
						if (songBank == 0x77)
						{
							songBank = 0x0E;
						}
						else if (songBank == 0x78)
						{
							songBank = 0x18;
						}
						else if (songBank == 0x79)
						{
							songBank = 0x1C;
						}
						else if (songBank == 0x7A)
						{
							songBank = 0x1A;
						}
					}
					else if (bankNum == 0x0C)
					{
						if (songBank == 0x77)
						{
							songBank = 0x0B;
						}
						else if (songBank == 0x78)
						{
							songBank = 0x0C;
						}
						else if (songBank == 0x79)
						{
							songBank = 0x0D;
						}
						else if (songBank == 0x7A)
						{
							songBank = 0x0E;
						}
					}
					else if (bankNum == 0x36)
					{
						if (songBank == 0x77)
						{
							songBank = 0x35;
						}
						else if (songBank == 0x78)
						{
							songBank = 0x37;
						}
						else if (songBank == 0x79)
						{
							songBank = 0x3D;
						}
						else if (songBank == 0x7A)
						{
							songBank = 0x3F;
						}
					}
				}
				fseek(rom, (songBank * bankSize), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize);
				fread(exRomData, 1, bankSize, rom);
				if (exRomData[songPtr - bankAmt] > 0 && exRomData[songPtr - bankAmt] <= 4)
				{
					printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, songBank);
					Novasong2mid(songNum, songPtr, songBank);
				}
				else
				{
					printf("Song %i: 0x%04X (bank %02X) (empty, skipped)\n", songNum, songPtr, songBank);
				}
			}

			/*"Pirate" variant for Zhong Zhuan Qi Bing*/
			else if (drvVers == 11)
			{
				songBank = romData[i + 2];

				if (songBank == 0x0C)
				{
					songBank = 0x7B;
				}
				if (songBank == 0x0D)
				{
					songBank = 0x7C;
				}
				else if (songBank == 0x0E)
				{
					songBank = 0x7D;
				}
				else if (songBank == 0x0F)
				{
					songBank = 0x7E;
				}
				fseek(rom, (songBank* bankSize), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize);
				fread(exRomData, 1, bankSize, rom);
				if (exRomData[songPtr - bankAmt] > 0 && exRomData[songPtr - bankAmt] <= 4)
				{
					printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, songBank);
					Novasong2mid(songNum, songPtr, songBank);
				}
				else
				{
					printf("Song %i: 0x%04X (bank %02X) (empty, skipped)\n", songBank, songNum, songPtr);
				}
				i++;
			}

			i += 2;
			songNum++;
		}
	}
	else
	{
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}
}

/*Convert the song data to MIDI*/
void Novasong2mid(int songNum, long ptr, int songBank)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 0;
	int k = 0;
	int ticks = 120;
	int tempo = 150;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int chanSpeed = 0;
	int transpose1[4];
	int transpose2 = 0;
	int repeat[5];
	long repeatPos[5];
	long macro1Pos = 0;
	int macro1Num = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	int macro2Num = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	int macro3Num = 0;
	long macro3Ret = 0;
	long condJump1Pos = 0;
	int condJump1Num = 0;
	long condJump2Pos = 0;
	int condJump2Num = 0;
	int inMacro = 0;
	unsigned char command[5];
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
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int envelope = 0;

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
		romPos = ptr - bankAmt;
		trackCnt = exRomData[romPos];
		if (trackCnt > 4)
		{
			trackCnt = 0;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			activeChan[curTrack] = 0x0000;
			transpose1[curTrack] = 0;
		}

		for (k = 0; k < 5; k++)
		{
			repeat[k] = -1;
			repeatPos[k] = 0;
		}

		if (drvVers == 3)
		{
			tempo = exRomData[romPos + 2] * 1.2;
			romPos += 3;
		}
		else
		{
			romPos++;
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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			mask = exRomData[romPos + 1];

			switch (mask)
			{
			case 0x13:
				k = 0;
				break;
			case 0x18:
				k = 1;
				break;
			case 0x1D:
				k = 2;
				break;
			case 0x22:
				k = 3;
				break;
			default:
				k = 0;
				break;
			}

			activeChan[k] = ReadLE16(&exRomData[romPos + 2]);
			transpose1[k] = (signed char)exRomData[romPos + 4];
			romPos += 6;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

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
			if (activeChan[curTrack] >= bankAmt && activeChan[curTrack] < 0x8000)
			{
				firstNote = 1;
				seqPos = activeChan[curTrack] - bankAmt;
				seqEnd = 0;
				inMacro = 0;
				condJump1Num = -1;
				condJump2Num = -1;
				transpose2 = 0;
				macro1Num = -1;
				macro2Num = -1;
				curDelay = 0;
				ctrlDelay = 0;

				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*Set note size*/
					if (command[0] <= 0x7F)
					{
						/*Play drum note*/
						if ((drvVers == 1 || drvVers == 2 || drvVers >= 6) && curTrack == 3)
						{
							curNote = command[0] + 24;
							if (command[1] < 0x80)
							{
								curNoteLen = command[1] * 5;
								seqPos++;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							seqPos++;
						}
						else
						{
							chanSpeed = command[0];
							curNoteLen = chanSpeed * 5;
							if (drvVers != 3)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
							}
							if (drvVers == 3)
							{
								curNoteLen = chanSpeed * 10;
							}
							seqPos++;
						}
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (command[1] < 0x80 && drvVers != 3)
						{
							chanSpeed = command[1];
							seqPos++;
						}
						if (drvVers != 3)
						{
							curDelay += (chanSpeed * 5);
							ctrlDelay += (chanSpeed * 5);
						}
						else if (drvVers == 3)
						{
							curDelay += (chanSpeed * 10);
							ctrlDelay += (chanSpeed * 10);
						}

						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x81 && command[0] <= 0xDF)
					{
						curNote = (command[0] - 0x81) + 24;

						curNote += transpose1[curTrack];
						curNote += transpose2;


						if (command[1] < 0x80 && drvVers != 3)
						{
							chanSpeed = command[1];
							seqPos++;
						}
						curNoteLen = chanSpeed * 5;
						if (drvVers == 3)
						{
							curNoteLen = chanSpeed * 10;
						}

						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Invalid commands*/
					else if (command[0] >= 0xE0 && command[0] <= 0xE2)
					{
						seqPos += 2;
					}

					/*End of channel (no loop)*/
					else if (command[0] == 0xE3)
					{
						seqEnd = 1;
					}

					/*Set envelope*/
					else if (command[0] == 0xE4)
					{
						seqPos += 2;
					}

					/*Jump to position (usually song loop)*/
					else if (command[0] == 0xE5)
					{
						if (drvVers != 3)
						{
							if ((ReadLE16(&exRomData[seqPos + 1]) - bankAmt) > seqPos)
							{
								seqPos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
							}
							else
							{
								seqEnd = 1;
							}

						}
						else if (drvVers == 3)
						{
							if (condJump1Num == -1 && condJump2Num == -1)
							{
								seqEnd = 1;
							}
							else
							{
								seqPos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
							}

						}
					}

					/*Set transpose (additional)*/
					else if (command[0] == 0xE6)
					{
						if (curTrack != 3)
						{
							transpose2 = (signed char)command[1];
						}
						else if (curTrack == 3)
						{
							transpose2 = 12;
						}

						seqPos += 2;
					}

					/*Repeat section*/
					else if (command[0] == 0xE7)
					{
						if (repeat[command[1]] == -1)
						{
							repeat[command[1]] = command[2];
							repeatPos[command[1]] = ReadLE16(&exRomData[seqPos + 3]) - bankAmt;
						}
						else if (repeat[command[1]] > 1)
						{
							seqPos = repeatPos[command[1]];
							repeat[command[1]]--;
						}
						else
						{
							seqPos += 5;
							repeat[command[1]] = -1;
						}
					}

					/*Set distortion effect?*/
					else if (command[0] == 0xE8)
					{
						seqPos += 2;
					}

					/*Set pan?*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Set tempo (Another Bible)*/
					else if (command[0] >= 0xEA && command[0] <= 0xEB)
					{
						if (command[0] == 0xEA)
						{
							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							tempo = command[2] * 0.6;
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
							seqPos += 3;
						}
						else
						{
							seqPos += 2;
						}
					}

					/*Go to macro*/
					else if (command[0] == 0xEC)
					{

						if (curTrack == 1)
						{
							curTrack = 1;
						}
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
							macro1Ret = seqPos + 3;
							seqPos = macro1Pos;
							inMacro = 1;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
							macro2Ret = seqPos + 3;
							seqPos = macro2Pos;
							inMacro = 2;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
							macro3Ret = seqPos + 3;
							seqPos = macro3Pos;
							inMacro = 3;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Exit macro after X times*/
					else if (command[0] == 0xED)
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
						else if (inMacro == 3)
						{
							seqPos = macro3Ret;
							inMacro = 2;
						}
						else
						{
							seqPos++;
						}
					}

					/*Set duty*/
					else if (command[0] == 0xEE)
					{
						seqPos += 2;
					}

					/*Set sweep*/
					else if (command[0] == 0xEF)
					{
						seqPos += 2;
					}

					/*Turn off vibrato?*/
					else if (command[0] == 0xF0)
					{
						seqPos++;
					}

					/*Set vibrato?*/
					else if (command[0] == 0xF1)
					{
						seqPos += 2;
						if (drvVers == 1)
						{
							seqPos--;
						}
					}

					/*Turn on vibrato?*/
					else if (command[0] == 0xF2)
					{
						seqPos++;
					}

					/*Set waveform*/
					else if (command[0] == 0xF3)
					{
						seqPos += 2;
					}

					/*End of channel (no loop) (v2)?*/
					else if (command[0] == 0xF4)
					{
						seqEnd = 1;
					}

					/*Set note size*/
					else if (command[0] == 0xF5)
					{
						if (drvVers != 3)
						{
							seqPos += 2;
						}
						/*Conditional jump 1*/
						else if (drvVers == 3)
						{
							if (condJump1Num == -1)
							{
								condJump1Num = command[1];
								condJump1Pos = ReadLE16(&exRomData[seqPos + 2]) - bankAmt;
							}
							else if (condJump1Num > 1)
							{
								seqPos += 4;
								condJump1Num--;
							}
							else
							{
								seqPos = condJump1Pos;
								condJump1Num = -1;
							}
						}
					}

					/*Set tempo*/
					else if (command[0] == 0xF6)
					{
						if (drvVers != 3)
						{
							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							tempo = command[1] * 0.6;
							if (tempo < 2)
							{
								tempo = 150;
							}
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
							seqPos += 2;
						}
						else if (drvVers == 3)
						{
							if (condJump2Num == -1)
							{
								condJump2Num = command[1];
								condJump2Pos = ReadLE16(&exRomData[seqPos + 2]) - bankAmt;
							}
							else if (condJump2Num > 1)
							{
								seqPos += 4;
								condJump2Num--;
							}
							else
							{
								seqPos = condJump2Pos;
								condJump2Num = -1;
							}
						}

					}

					/*Invalid commands*/
					else if (command[0] >= 0xF7 && command[0] <= 0xF9)
					{
						seqPos += 2;
					}

					/*Hold note?*/
					else if (command[0] == 0xFA)
					{
						chanSpeed = command[1];
						curNoteLen = chanSpeed * 5;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Invalid commands*/
					else if (command[0] >= 0xFB)
					{
						seqPos += 2;
					}
				}
			}

			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			if (songNum == 10 && curTrack == 1)
			{
				songNum = 10;
			}

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
		free(exRomData);
		fclose(mid);
	}
}