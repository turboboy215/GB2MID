/*Capcom*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CAPCOM.H"

#define bankSize 16384
#define bankSizeNES 8192
#define ramSize 65536

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
long firstPtr;
long bankAmt;
int foundTable;
int curInst;
int isSong;
int curVol;
int usePALTempo;
char* argv3;

char string[4];
char spcString[12];

int format;
int drvVers;

char* tempPnt;
char OutFileBase[0x100];

long switchPoint[400][2];
int switchNum = 0;

int cvtSFX;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char CapMagicBytesANES[5] = { 0x84, 0x29, 0x0A, 0xAA, 0xBD };
const char CapMagicBytesBNES[7] = { 0xC9, 0xFF, 0xD0, 0x0B, 0xA9, 0x01, 0x85 };
const char CapMagicBytesBNES2[7] = { 0xC9, 0xFF, 0xD0, 0x03, 0x4C, 0x6A, 0x81 };
const char CapMagicBytesBGB[3] = { 0x79, 0x3D, 0x21 };
const char CapMagicBytesCNES[4] = { 0x90, 0x06, 0x38, 0xED };
const char CapMagicBytesCGB[5] = { 0x7C, 0x65, 0x6F, 0xB4, 0xC8 };
const char CapMagicBytesSNES1[3] = { 0x1C, 0x5D, 0xF5 };
const char CapMagicBytesSNES2[3] = { 0x8D, 0x00, 0xDD };
const char CapMagicBytesSNES3[6] = { 0xA3, 0x1C, 0x90, 0x02, 0xAB, 0xA3 };


/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Capsong2mid1(int songNum, long ptr);
void Capsong2mid2(int songNum, long ptr);
void Capsong2mid3(int songNum, long ptr);

void CapProc(int bank)
{
	foundTable = 0;
	curVol = 100;
	curInst = 0;
	switchNum = 0;
	usePALTempo = 0;
	cvtSFX = 0;
	isSong = 1;

	/*Check for NES ROM header*/
	fgets(string, 4, rom);
	if (!memcmp(string, "NES", 1))
	{
		fseek(rom, (((bank - 1) * bankSizeNES)) + 0x10, SEEK_SET);
		format = 1;
		bankAmt = 0x8000;
	}
	else if (!memcmp(spcString, "SNES-SPC700", 1))
	{
		fseek(rom, 0x100, SEEK_SET);
		format = 3;
		bankAmt = 0x0000;
		/*Copy filename from argument - based on code by ValleyBell*/
		/*strcpy(OutFileBase, argv[1]);*/
		tempPnt = strrchr(OutFileBase, '.');
		if (tempPnt == NULL)
		{
			tempPnt = OutFileBase + strlen(OutFileBase);
		}
		*tempPnt = 0;
	}
	else
	{
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		format = 2;
		bankAmt = 0x4000;
	}

	if (format == 1)
	{
		romData = (unsigned char*)malloc(bankSizeNES * 3);

		if (bank != 0x1F)
		{
			fread(romData, 1, (bankSizeNES * 3), rom);
		}

		/*Special configuration for MM4*/
		else if (bank == 0x1F)
		{
			fread(romData, 1, (bankSizeNES * 2), rom);
			fseek(rom, (0x1D * bankSizeNES) + 0x10, SEEK_SET);
			fread(romData + (bankSizeNES * 2), 1, bankSizeNES, rom);
		}

	}
	else if (format == 2)
	{
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}

	/*Special case for California Raisins*/
	if (format == 1 && bank == 0x0B && ReadBE16(&romData[1]) == 0x8080 && ReadBE16(&romData[3]) == 0x8180 && ReadBE16(&romData[5]) == 0x8452)
	{
		tableOffset = 0x8000;
		printf("Song table starts at 0x%04x...\n", tableOffset);
		foundTable = 1;
		drvVers = 3;
	}

	else if (format == 1)
	{
		/*Try to search the bank for song table loader (3rd driver)*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], CapMagicBytesCNES, 4)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 4;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				drvVers = 3;
			}
		}

		/*Try to search the bank for song table loader (2nd driver)*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], CapMagicBytesBNES, 7)) && foundTable != 1)
			{
				bankAmt = 0x8000;
				tablePtrLoc = bankAmt + i + 18;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				format = 1;
				drvVers = 2;
			}
		}

		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], CapMagicBytesBNES2, 7)) && foundTable != 1)
			{
				bankAmt = 0x8000;
				tablePtrLoc = bankAmt + i + 10;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				format = 1;
				drvVers = 2;
			}
		}

		/*Try to search the bank for song table loader (1st driver)*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], CapMagicBytesANES, 5)) && foundTable != 1)
			{
				bankAmt = 0x8000;
				tablePtrLoc = bankAmt + i + 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				drvVers = 1;
			}
		}
	}

	else if (format == 2)
	{
		/*Try to search the bank for song table loader (common driver)*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], CapMagicBytesCGB, 5)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i - 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				drvVers = 3;
			}
		}

		for (i = 0; i < bankSize; i++)
		{
			/*Try to search the bank for song table loader (DuckTales)*/
			if ((!memcmp(&romData[i], CapMagicBytesBGB, 3)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 3;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				format = 2;
				drvVers = 2;
			}
		}

	}

	else if (format == 3)
	{
		/*Try to search the bank for song table loader (early driver)*/
		for (i = 0; i < ramSize; i++)
		{
			if ((!memcmp(&romData[i], CapMagicBytesSNES1, 3)) && foundTable != 1)
			{
				tablePtrLoc = i + 3;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				drvVers = 1;
			}
		}

		/*Try to search the bank for song table loader (later driver)*/
		for (i = 0; i < ramSize; i++)
		{
			if ((!memcmp(&romData[i], CapMagicBytesSNES2, 3)) && foundTable != 1)
			{
				tablePtrLoc = i - 8;
				printf("Found pointer to song at address 0x%04x!\n", tablePtrLoc);
				songPtr = romData[i - 5] + (romData[i - 8] * 0x100);
				printf("Song starts at 0x%04x...\n", songPtr);
				foundTable = 1;
				drvVers = 2;
			}
		}
		}


	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;
		isSong = 1;

		if (drvVers == 3)
		{
			if (format == 1)
			{
				i++;
			}

			while (isSong == 1)
			{
				songPtr = ReadBE16(&romData[i]);

				if (songPtr < bankAmt && songPtr != 0x0000)
				{
					isSong = 0;
				}

				if (isSong == 1)
				{
					if (songPtr == 0x0000)
					{
						printf("Song %i: 0x%04X (Empty, skipped)\n", songNum, songPtr);
					}

					else if (romData[songPtr - bankAmt] == 0x00)
					{
						printf("Song %i: 0x%04X\n", songNum, songPtr);
						Capsong2mid3(songNum, songPtr);
					}
					else
					{
						printf("Song %i: 0x%04X (SFX, skipped)\n", songNum, songPtr);
					}

					i += 2;
					songNum++;
				}

			}
		}

		else if (drvVers == 2)
		{
			if (format == 1)
			{
				while (ReadLE16(&romData[i]) >= 0x8000)
				{
					songPtr = ReadLE16(&romData[i]);
					if (romData[songPtr - bankAmt] == 0x0F || romData[songPtr - bankAmt] == 0x07)
					{
						printf("Song %i: 0x%04X\n", songNum, songPtr);
						Capsong2mid2(songNum, songPtr);
					}
					else
					{
						printf("Song %i: 0x%04X (SFX, skipped)\n", songNum, songPtr);
					}

					i += 2;
					songNum++;
				}
			}
			else if (format == 2)
			{
				while (songNum <= 11)
				{
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Capsong2mid2(songNum, songPtr);
					i += 2;
					songNum++;
				}
			}
			else if (format == 3)
			{
				songNum = 1;
				Capsong2mid3(songNum, songPtr);
				songNum++;

				/*Look for SFX/additional music table*/
				for (i = 0; i < ramSize; i++)
				{
					if ((!memcmp(&romData[i], CapMagicBytesSNES3, 6)) && foundTable != 2)
					{
						tablePtrLoc = i - 4;
						tableOffset = romData[i - 4] + (romData[i - 1] * 0x100);
						printf("SFX/additional music table: 0x%04X\n", tableOffset);
						foundTable = 2;
					}
				}

				if (foundTable == 2)
				{
					i = tableOffset + 1;

					if (ReadBE16(&romData[tableOffset]) == 0x0000)
					{
						i++;
					}
					firstPtr = ReadBE16(&romData[i]);

					while (i < firstPtr)
					{
						songPtr = ReadBE16(&romData[i]);
						if (songPtr != 0x0000 && songPtr != 0xFFFF)
						{
							if (romData[songPtr - bankAmt] == 0x00)
							{
								printf("Song %i: 0x%04X\n", songNum, songPtr);
								Capsong2mid3(songNum, songPtr);
							}
							else
							{
								if (cvtSFX == 1)
								{
									printf("Song %i: 0x%04X (SFX)\n", songNum, songPtr);
									Capsong2mid3(songNum, songPtr);
								}
								else
								{
									printf("Song %i: 0x%04X (SFX, skipped)\n", songNum, songPtr);
								}
							}
						}
						else
						{
							printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
						}

						i += 2;
						songNum++;
					}
				}
			}
		}

		else if (drvVers == 1)
		{
			if (format == 1)
			{
				songNum = 1;

				while (ReadLE16(&romData[i]) >= 0x8000)
				{
					songPtr = ReadLE16(&romData[i]);
					if (romData[songPtr - bankAmt] == 0x0E || romData[songPtr - bankAmt] == 0x0F || romData[songPtr - bankAmt] == 0x02 || romData[songPtr - bankAmt] == 0x01)
					{
						printf("Song %i: 0x%04X\n", songNum, songPtr);
						Capsong2mid1(songNum, songPtr);
					}
					else
					{
						printf("Song %i: 0x%04X (SFX, skipped)\n", songNum, songPtr);
					}

					i += 2;
					songNum++;
				}
			}

			else if (format == 3)
			{
				songNum = 1;
				/*Check for "active song"*/
				for (i = 0; i < ramSize; i++)
				{
					if ((!memcmp(&romData[i], CapMagicBytesSNES2, 3)))
					{
						songPtr = romData[i - 5] + (romData[i - 8] * 0x100);

						if (songPtr < 0x6000)
						{
							if (tableOffset != 0x2002)
							{
								printf("Song %i: 0x%04X\n", songNum, songPtr);
								Capsong2mid3(songNum, songPtr);
								songNum++;
							}
						}

						break;
					}
				}
				i = tableOffset + 1;
				if (ReadBE16(&romData[tableOffset]) == 0x0000)
				{
					i++;
				}
				firstPtr = ReadBE16(&romData[i]);

				if (i == 0x2004)
				{
					firstPtr = 0x2100;
				}

				while (i < firstPtr)
				{
					songPtr = ReadBE16(&romData[i]);
					if (songPtr != 0x0000 && songPtr != 0xFFFF)
					{
						if (romData[songPtr - bankAmt] == 0x00)
						{
							printf("Song %i: 0x%04X\n", songNum, songPtr);
							Capsong2mid3(songNum, songPtr);
						}
						else
						{
							if (cvtSFX == 1)
							{
								printf("Song %i: 0x%04X (SFX)\n", songNum, songPtr);
								Capsong2mid3(songNum, songPtr);
							}
							else
							{
								printf("Song %i: 0x%04X (SFX, skipped)\n", songNum, songPtr);
							}
						}
					}
					else
					{
						printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
					}

					i += 2;
					songNum++;
				}
			}

		}


	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}

	free(romData);
}

void Capsong2mid1(int songNum, long ptr)
{
	const char* TRK_NAMES_NES[5] = { "Square 1", "Square 2", "Triangle", "Noise", "PCM" };
	const char* TRK_NAMES_GB[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	long romPos = 0;
	long seqPos = 0;
	unsigned int midPos = 0;
	long songPtrs[4];
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	int ticks = 120;
	long tempo = 450;
	int k = 0;
	int seqEnd = 0;
	int freq = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curVol = 0;
	int subVal = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int dotted = 0;
	int valSize = 0;
	long tempPos = 0;
	long trackSize = 0;
	int firstNote = 1;
	int override = 0;
	int tempoVal = 0;
	int triplet = 0;

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

	sprintf(outfile, "song%i.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
		exit(2);
	}
	else
	{
		romPos = songPtr - bankAmt;
		romPos++;

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			songPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 4;
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
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			repeat1 = -1;
			override = 0;
			dotted = 0;
			triplet = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;

			Write8B(&midData[midPos], strlen(TRK_NAMES_NES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES_NES[curTrack]);
			midPos += strlen(TRK_NAMES_NES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (songPtrs[curTrack] == 0)
			{
				seqEnd = 1;
			}
			else
			{
				seqPos = songPtrs[curTrack] - bankAmt;
				seqEnd = 0;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				/*Set tempo*/
				if (command[0] == 0x1F)
				{
					if (tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}


						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = (65535 / command[1]) / 37;
						if (usePALTempo == 1)
						{
							tempo = tempo * 0.83;
						}
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 2;
				}

				/*Triplet mode*/
				else if (command[0] == 0x30)
				{
					triplet = 1;
					seqPos++;
				}

				/*Set instrument*/
				else if (command[0] == 0x3F)
				{
					curInst = command[1];
					if (curInst >= 0x80)
					{
						curInst = 0;
					}
					firstNote = 1;
					seqPos += 2;
				}

				/*Set base note*/
				else if (command[0] == 0x5F)
				{
					freq = command[1] + 12;
					seqPos += 2;
				}

				/*Repeat section*/
				else if (command[0] == 0x7F)
				{
					if (command[1] == 0)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pt = ReadLE16(&romData[seqPos + 2]) - bankAmt;
						}
						else if (repeat1 > 0)
						{
							seqPos = repeat1Pt;
							repeat1--;
						}
						else if (repeat1 == 0)
						{
							repeat1 = -1;
							seqPos += 4;
						}
					}
				}

				/*Command 9F (invalid)*/
				else if (command[0] == 0x9F)
				{
					seqPos++;
				}

				/*Command BF (invalid)*/
				else if (command[0] == 0xBF)
				{
					seqPos++;
				}

				/*Dotted note*/
				else if (command[0] == 0xDF)
				{
					dotted = 1;
					seqPos++;
				}

				/*End of channel*/
				else if (command[0] == 0xFF)
				{
					seqEnd = 1;
				}

				/*Play note/rest*/
				else
				{
					if (command[0] >= 0x00 && command[0] <= 0x1E)
					{
						subVal = 0x00;
						curNoteLen = 15;
					}

					else if (command[0] >= 0x20 && command[0] <= 0x3E)
					{
						subVal = 0x20;
						curNoteLen = 30;
					}

					else if (command[0] >= 0x40 && command[0] <= 0x5E)
					{
						subVal = 0x40;
						curNoteLen = 60;
					}

					else if (command[0] >= 0x60 && command[0] <= 0x7E)
					{
						subVal = 0x60;
						curNoteLen = 120;
					}

					else if (command[0] >= 0x80 && command[0] <= 0x9E)
					{
						subVal = 0x80;
						curNoteLen = 240;
					}

					else if (command[0] >= 0xA0 && command[0] <= 0xBE)
					{
						subVal = 0xA0;
						curNoteLen = 480;
					}

					else if (command[0] >= 0xC0 && command[0] <= 0xDE)
					{
						subVal = 0xC0;
						curNoteLen = 960;
					}

					else if (command[0] >= 0xE0 && command[0] <= 0xFE)
					{
						subVal = 0xE0;
						curNoteLen = 1920;

						if (curTrack == 2)
						{
							curTrack = 2;
						}
					}

					if (triplet == 1)
					{
						curNoteLen = (curNoteLen * 2) / 3;
						triplet = 0;
					}

					if (dotted == 1)
					{
						curNoteLen = curNoteLen * 1.5;
						dotted = 0;
					}

					if (command[0] == 0x00 || command[0] == 0x20 || command[0] == 0x40 || command[0] == 0x60 || command[0] == 0x80 || command[0] == 0xA0 || command[0] == 0xC0 || command[0] == 0xE0)
					{
						curDelay += curNoteLen;
					}

					else
					{
						curNote = freq + (command[0] - subVal);

						if (curTrack == 2)
						{
							curNote -= 12;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						midPos = tempPos;
					}
					seqPos++;
				}
			}

			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (tempoVal == 1)
			{
				tempoVal = 2;
			}
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

void Capsong2mid2(int songNum, long ptr)
{
	const char* TRK_NAMES_NES[5] = { "Square 1", "Square 2", "Triangle", "Noise", "PCM" };
	const char* TRK_NAMES_GB[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	long romPos = 0;
	long seqPos = 0;
	unsigned int midPos = 0;
	long songPtrs[4];
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	int ticks = 120;
	long tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int freq = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curVol = 0;
	int subVal = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int dotted = 0;
	int valSize = 0;
	long tempPos = 0;
	long trackSize = 0;
	int firstNote = 1;
	int override = 0;
	int tempoVal = 0;

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

	sprintf(outfile, "song%i.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
		exit(2);
	}
	else
	{
		romPos = songPtr - bankAmt;

		if (format == 1)
		{
			romPos++;
		}
		else if (format == 2)
		{
			if (romData[romPos] == 0x01)
			{
				tempoVal = 35;
			}
			else if (romData[romPos] == 0x02)
			{
				tempoVal = 170;
			}
			else if (romData[romPos] == 0x03)
			{
				tempoVal = 200;
			}
			else if (romData[romPos] == 0x04)
			{
				tempoVal = 230;
			}
			else if (romData[romPos] == 0x05)
			{
				tempoVal = 245;
			}
			else if (romData[romPos] == 0x06)
			{
				tempoVal = 260;
			}
			else if (romData[romPos] == 0x07)
			{
				tempoVal = 270;
			}
			else
			{
				tempoVal = 280;
			}
			tempo = tempoVal * (romData[romPos + 1] + 1);
			romPos += 2;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			songPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
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
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			repeat1 = -1;
			override = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;

			if (format == 1)
			{
				Write8B(&midData[midPos], strlen(TRK_NAMES_NES[curTrack]));
				midPos++;
				sprintf((char*)&midData[midPos], TRK_NAMES_NES[curTrack]);
				midPos += strlen(TRK_NAMES_NES[curTrack]);
			}
			else
			{
				Write8B(&midData[midPos], strlen(TRK_NAMES_GB[curTrack]));
				midPos++;
				sprintf((char*)&midData[midPos], TRK_NAMES_GB[curTrack]);
				midPos += strlen(TRK_NAMES_GB[curTrack]);
			}

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (songPtrs[curTrack] == 0)
			{
				seqEnd = 1;
			}
			else
			{
				seqPos = songPtrs[curTrack] - bankAmt;
				seqEnd = 0;
			}

			while (seqEnd == 0)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				/*Set tempo*/
				if (command[0] == 0x00)
				{
					if (format == 1 && tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = (65535 / command[1]) / 37;

						if (usePALTempo == 1)
						{
							tempo = tempo * 0.83;
						}
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 2;
				}

				/*Set pitch envelope*/
				else if (command[0] == 0x01)
				{
					seqPos += 2;
				}

				/*Set duty*/
				else if (command[0] == 0x02)
				{
					seqPos += 2;
				}

				/*Set volume*/
				else if (command[0] == 0x03)
				{
					seqPos += 2;
				}

				/*Repeat section*/
				else if (command[0] == 0x04)
				{
					if (command[1] == 0)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pt = ReadLE16(&romData[seqPos + 2]) - bankAmt;
						}
						else if (repeat1 > 0)
						{
							seqPos = repeat1Pt;
							repeat1--;
						}
						else if (repeat1 == 0)
						{
							repeat1 = -1;
							seqPos += 4;
						}
					}
				}

				/*Set base note*/
				else if (command[0] == 0x05)
				{
					freq = command[1] + 12;
					seqPos += 2;
				}

				/*Dotted note*/
				else if (command[0] == 0x06)
				{
					dotted = 1;
					seqPos++;
				}

				/*Set volume envelope*/
				else if (command[0] == 0x07)
				{
					if (format == 1)
					{
						seqPos++;
					}
					seqPos += 2;
				}

				/*Set modulation*/
				else if (command[0] == 0x08)
				{
					seqPos += 2;
				}

				/*End of channel*/
				else if (command[0] == 0x09)
				{
					seqEnd = 1;
				}

				/*Play note OR rest*/
				else
				{
					if (command[0] >= 0x20 && command[0] <= 0x2F)
					{
						;
					}
					else if (command[0] >= 0x30 && command[0] <= 0x4F)
					{
						if (command[0] == 0x30)
						{
							override = 1;
						}
					}
					if (command[0] >= 0x40 && command[0] <= 0x5F)
					{
						curNoteLen = 30;
						if (override == 1)
						{
							curNoteLen = 20;
							override = 0;
						}
						subVal = 0x40;
					}

					else if (command[0] >= 0x60 && command[0] <= 0x7F)
					{
						curNoteLen = 60;
						if (override == 1)
						{
							curNoteLen = 40;
							override = 0;
						}
						subVal = 0x60;
					}

					else if (command[0] >= 0x80 && command[0] <= 0x9F)
					{
						curNoteLen = 120;
						if (override == 1)
						{
							curNoteLen = 80;
							override = 0;
						}
						subVal = 0x80;
					}

					else if (command[0] >= 0xA0 && command[0] <= 0xBF)
					{
						curNoteLen = 240;
						if (override == 1)
						{
							curNoteLen = 160;
							override = 0;
						}
						subVal = 0xA0;
					}

					else if (command[0] >= 0xC0 && command[0] <= 0xDF)
					{
						curNoteLen = 480;
						if (override == 1)
						{
							curNoteLen = 320;
							override = 0;
						}
						subVal = 0xC0;
					}

					else if (command[0] >= 0xE0)
					{
						curNoteLen = 960;
						if (override == 1)
						{
							curNoteLen = 640;
							override = 0;
						}
						subVal = 0xE0;
					}

					if (dotted == 1)
					{
						curNoteLen = curNoteLen * 1.5;
						dotted = 0;
					}

					if (command[0] == 0x40 || command[0] == 0x60 || command[0] == 0x80 || command[0] == 0xA0 || command[0] == 0xC0 || command[0] == 0xE0)
					{
						curDelay += curNoteLen;
					}
					else
					{
						if (command[0] >= 0x40)
						{
							curNote = freq + (command[0] - subVal);

							if (curTrack == 2)
							{
								curNote -= 12;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							midPos = tempPos;
						}

					}
					seqPos++;
				}
			}

			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (tempoVal == 1)
			{
				tempoVal = 2;
			}

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

void Capsong2mid3(int songNum, long ptr)
{
	const char* TRK_NAMES_NES[5] = { "Square 1", "Square 2", "Triangle", "Noise", "PCM" };
	const char* TRK_NAMES_GB[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	long seqPos = 0;
	unsigned int midPos = 0;
	long songPtrs[8];
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	int ticks = 120;
	int k = 0;
	int seqEnd = 0;
	int octave = 0;
	int transpose1 = 0;
	int transpose2 = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int valSize = 0;
	long trackSize = 0;
	int lastNote = 0;
	int tempByte = 0;
	long tempPos = 0;
	long tempo = 150;
	int curInst = 0;
	int flag1 = 0;
	int flag2 = 0;
	int flag3 = 0;
	int flag4 = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	int repeat3 = 0;
	int repeat4 = 0;
	long repeat1Pt = 0;
	long repeat2Pt = 0;
	long repeat3Pt = 0;
	long repeat4Pt = 0;
	long break1 = 0;
	long break2 = 0;
	long break3 = 0;
	long break4 = 0;
	long loopPt = 0;
	int firstNote = 1;
	int timeVal = 0;
	int triplet = 0;
	int connect = 0;
	int tripNotes = 0;
	int dotted = 0;
	int highOct = 0;
	unsigned char mask = 0;
	int maskArray[8];
	int subVal = 0;
	long oldPos = 0;
	int connExt = 0;
	long speedCtrl = 0;
	long masterDelay = 0;
	int prevNote = 0;

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
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
		exit(2);
	}
	else
	{
		/*Get channel sequence pointers*/
		songPtrs[0] = ReadBE16(&romData[ptr + 1 - bankAmt]);
		songPtrs[1] = ReadBE16(&romData[ptr + 3 - bankAmt]);
		songPtrs[2] = ReadBE16(&romData[ptr + 5 - bankAmt]);
		songPtrs[3] = ReadBE16(&romData[ptr + 7 - bankAmt]);

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

		switchNum = 0;

		for (switchNum = 0; switchNum < 400; switchNum++)
		{
			switchPoint[switchNum][0] = -1;
			switchPoint[switchNum][1] = 0;
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
			seqEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			repeat3 = -1;
			repeat4 = -1;
			highOct = 0;
			transpose1 = 0;
			transpose2 = 0;
			triplet = 0;
			connect = 0;
			dotted = 0;
			octave = 0;
			connExt = 0;
			speedCtrl = 0;
			curVol = 100;

			switchNum = 0;

			ctrlDelay = 0;
			masterDelay = 0;


			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			if (format == 1)
			{
				Write8B(&midData[midPos], strlen(TRK_NAMES_NES[curTrack]));
				midPos++;
				sprintf((char*)&midData[midPos], TRK_NAMES_NES[curTrack]);
				midPos += strlen(TRK_NAMES_NES[curTrack]);
			}
			else
			{
				Write8B(&midData[midPos], strlen(TRK_NAMES_GB[curTrack]));
				midPos++;
				sprintf((char*)&midData[midPos], TRK_NAMES_GB[curTrack]);
				midPos += strlen(TRK_NAMES_GB[curTrack]);
			}


			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (songPtrs[curTrack] == 0)
			{
				seqEnd = 1;
			}
			else
			{
				seqPos = songPtrs[curTrack] - bankAmt;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];


				/*Check for global transpose*/
				if (curTrack != 0)
				{
					for (switchNum = 0; switchNum < 90; switchNum++)
					{
						if (masterDelay >= switchPoint[switchNum][0] && switchPoint[switchNum][0] != -1)
						{
							if (curTrack == 2)
							{
								curTrack = 2;
							}
							transpose2 = switchPoint[switchNum][1];
						}
					}
				}

				/*Triplets*/
				if (command[0] == 0x00)
				{
					if (triplet == 0)
					{
						triplet = 1;
					}
					else if (triplet == 1)
					{
						triplet = 0;
					}
					seqPos++;
				}

				/*Connect notes*/
				else if (command[0] == 0x01)
				{
					if (connect == 0)
					{
						connect = 1;
					}

					else if (connect == 1)
					{
						connect = 0;
					}
					seqPos++;
				}

				/*Dotted note*/
				else if (command[0] == 0x02)
				{
					dotted = 1;
					seqPos++;
				}

				/*Higher octave*/
				else if (command[0] == 0x03)
				{
					if (highOct == 0)
					{
						highOct = 1;
					}
					else if (highOct == 1)
					{
						highOct = 0;
					}

					seqPos++;
				}

				/*Channel flags*/
				else if (command[0] == 0x04)
				{
					mask = command[1];
					maskArray[7] = mask & 1;
					maskArray[6] = mask >> 1 & 1;
					maskArray[5] = mask >> 2 & 1;
					maskArray[4] = mask >> 3 & 1;
					maskArray[3] = mask >> 4 & 1;
					maskArray[2] = mask >> 5 & 1;
					maskArray[1] = mask >> 6 & 1;
					maskArray[0] = mask >> 7 & 1;

					if (maskArray[0] == 1)
					{
						connect = 1;
					}
					else if (maskArray[1] == 0)
					{
						connect = 0;
					}

					if (maskArray[1] == 1)
					{
						connect = 1;
					}
					else if (maskArray[1] == 0)
					{
						connect = 0;
					}

					if (maskArray[2] == 1)
					{
						triplet = 1;
					}
					else if (maskArray[2] == 0)
					{
						triplet = 0;
					}

					if (maskArray[3] == 1)
					{
						dotted = 1;
					}
					else if (maskArray[3] == 0)
					{
						dotted = 0;
					}

					if (maskArray[4] == 1)
					{
						highOct = 1;
					}
					else if (maskArray[4] == 0)
					{
						highOct = 0;
					}


					seqPos += 2;
				}

				/*Set tempo/speed*/
				else if (command[0] == 0x05)
				{
					speedCtrl = (ReadBE16(&romData[seqPos + 1])) / 3.45;
					if (speedCtrl != tempo)
					{
						tempo = speedCtrl;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 3;
				}

				/*Set note size*/
				else if (command[0] == 0x06)
				{
					seqPos += 2;
				}

				/*Set volume*/
				else if (command[0] == 0x07)
				{
					if (curTrack < 2)
					{
						if (command[1] >= 0x0F)
						{
							curVol = 100;
						}
						else if (command[1] <= 0x0E && command[1] >= 0x0B)
						{
							curVol = 90;
						}
						else if (command[1] == 0x0A)
						{
							curVol = 80;
						}
						else if (command[1] <= 0x09 && command[1] >= 0x07)
						{
							curVol = 70;
						}
						else if (command[1] == 0x06)
						{
							curVol = 50;
						}
						else if (command[1] == 0x05)
						{
							curVol = 40;
						}
						else if (command[1] == 0x04)
						{
							curVol = 30;
						}
						else if (command[1] == 0x03)
						{
							curVol = 20;
						}
						else if (command[1] == 0x02)
						{
							curVol = 10;
						}
						else if (command[1] == 0x01)
						{
							curVol = 5;
						}
						else if (command[1] == 0x00)
						{
							curVol = 0;
						}
					}

					else if (curTrack == 2)
					{
						if (command[1] >= 0x04)
						{
							curVol = 100;
						}

						else if (command[1] == 0x03)
						{
							curVol = 80;
						}

						else if (command[1] == 0x02)
						{
							curVol = 70;
						}

						else if (command[1] == 0x01)
						{
							curVol = 25;
						}

						else if (command[1] == 0x00)
						{
							curVol = 0;
						}
					}
					seqPos += 2;
				}

				/*Set vibrato*/
				else if (command[0] == 0x08)
				{
					if (format == 1 || format == 3)
					{
						curInst = command[1];
						if (curInst >= 0x80)
						{
							curInst = 0;
						}
						firstNote = 1;
					}
					seqPos += 2;
				}

				/*Set octave*/
				else if (command[0] == 0x09)
				{
					if (command[1] < 8)
					{
						octave = command[1];
					}
					seqPos += 2;
				}

				/*Global transpose*/
				else if (command[0] == 0x0A)
				{
					if (curTrack == 0)
					{
						transpose2 = (signed char)command[1];
						switchPoint[switchNum][0] = masterDelay;
						switchPoint[switchNum][1] = transpose2;
						switchNum++;
					}
					seqPos += 2;
				}

				/*Transpose*/
				else if (command[0] == 0x0B)
				{
					transpose1 = (signed char)command[1];
					seqPos += 2;
				}

				/*Set tuning*/
				else if (command[0] == 0x0C)
				{
					seqPos += 2;
				}

				/*Pitch slide*/
				else if (command[0] == 0x0D)
				{
					tempPos = WriteDeltaTime(midData, midPos, curDelay);
					midPos += tempPos;
					Write8B(&midData[midPos], (0xE0 | curTrack));
					Write8B(&midData[midPos + 1], 0);
					Write8B(&midData[midPos + 2], 0x40);
					Write8B(&midData[midPos + 3], 0);
					curDelay = 0;
					firstNote = 1;
					midPos += 3;
					seqPos += 2;
				}

				/*Repeat (1)*/
				else if (command[0] == 0x0E)
				{
					if (repeat1 == -1)
					{
						repeat1 = command[1];
						repeat1Pt = ReadBE16(&romData[seqPos + 2]);
					}

					else if (repeat1 > 0)
					{
						seqPos = repeat1Pt - bankAmt;
						repeat1--;
					}
					else if (repeat1 == 0)
					{
						seqPos += 4;
						repeat1 = -1;
					}
				}

				/*Repeat (2)*/
				else if (command[0] == 0x0F)
				{
					if (repeat2 == -1)
					{
						repeat2 = command[1];
						repeat2Pt = ReadBE16(&romData[seqPos + 2]);
					}

					else if (repeat2 > 0)
					{
						seqPos = repeat2Pt - bankAmt;
						repeat2--;
					}
					else if (repeat2 == 0)
					{
						seqPos += 4;
						repeat2 = -1;
					}
				}

				/*Repeat (3)*/
				else if (command[0] == 0x10)
				{
					if (repeat3 == -1)
					{
						repeat3 = command[1];
						repeat3Pt = ReadBE16(&romData[seqPos + 2]);
					}

					else if (repeat3 > 0)
					{
						seqPos = repeat3Pt - bankAmt;
						repeat3--;
					}
					else if (repeat3 == 0)
					{
						seqPos += 4;
						repeat3 = -1;
					}
				}

				/*Repeat (4)*/
				else if (command[0] == 0x11)
				{
					if (repeat4 == -1)
					{
						repeat4 = command[1];
						repeat4Pt = ReadBE16(&romData[seqPos + 2]);
					}

					else if (repeat4 > 0)
					{
						seqPos = repeat4Pt - bankAmt;
						repeat4--;
					}
					else if (repeat4 == 0)
					{
						seqPos += 4;
						repeat4 = -1;
					}
				}

				/*Break (1)*/
				else if (command[0] == 0x12)
				{
					if (repeat1 == 0)
					{
						mask = command[1];
						maskArray[7] = mask & 1;
						maskArray[6] = mask >> 1 & 1;
						maskArray[5] = mask >> 2 & 1;
						maskArray[4] = mask >> 3 & 1;
						maskArray[3] = mask >> 4 & 1;
						maskArray[2] = mask >> 5 & 1;
						maskArray[1] = mask >> 6 & 1;
						maskArray[0] = mask >> 7 & 1;

						if (maskArray[0] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[1] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[2] == 1)
						{
							triplet = 1;
						}
						else if (maskArray[2] == 0)
						{
							triplet = 0;
						}

						if (maskArray[3] == 1)
						{
							dotted = 1;
						}
						else if (maskArray[3] == 0)
						{
							dotted = 0;
						}

						if (maskArray[4] == 1)
						{
							highOct = 1;
						}
						else if (maskArray[4] == 0)
						{
							highOct = 0;
						}
						/*octave = mask & 7;*/

						break1 = ReadBE16(&romData[seqPos + 2]);
						seqPos = break1 - bankAmt;
						repeat1 = -1;
					}

					else
					{
						seqPos += 4;
					}

				}

				/*Break (2)*/
				else if (command[0] == 0x13)
				{
					if (repeat2 == 0)
					{
						mask = command[1];
						maskArray[7] = mask & 1;
						maskArray[6] = mask >> 1 & 1;
						maskArray[5] = mask >> 2 & 1;
						maskArray[4] = mask >> 3 & 1;
						maskArray[3] = mask >> 4 & 1;
						maskArray[2] = mask >> 5 & 1;
						maskArray[1] = mask >> 6 & 1;
						maskArray[0] = mask >> 7 & 1;

						if (maskArray[0] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[1] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[2] == 1)
						{
							triplet = 1;
						}
						else if (maskArray[2] == 0)
						{
							triplet = 0;
						}

						if (maskArray[3] == 1)
						{
							dotted = 1;
						}
						else if (maskArray[3] == 0)
						{
							dotted = 0;
						}

						if (maskArray[4] == 1)
						{
							highOct = 1;
						}
						else if (maskArray[4] == 0)
						{
							highOct = 0;
						}

						break2 = ReadBE16(&romData[seqPos + 2]);
						seqPos = break2 - bankAmt;
						repeat2 = -1;
					}

					else
					{
						seqPos += 4;
					}
				}

				/*Break (3)*/
				else if (command[0] == 0x14)
				{
					if (repeat3 == 0)
					{
						mask = command[1];
						maskArray[7] = mask & 1;
						maskArray[6] = mask >> 1 & 1;
						maskArray[5] = mask >> 2 & 1;
						maskArray[4] = mask >> 3 & 1;
						maskArray[3] = mask >> 4 & 1;
						maskArray[2] = mask >> 5 & 1;
						maskArray[1] = mask >> 6 & 1;
						maskArray[0] = mask >> 7 & 1;

						if (maskArray[0] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[1] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[2] == 1)
						{
							triplet = 1;
						}
						else if (maskArray[2] == 0)
						{
							triplet = 0;
						}

						if (maskArray[3] == 1)
						{
							dotted = 1;
						}
						else if (maskArray[3] == 0)
						{
							dotted = 0;
						}

						if (maskArray[4] == 1)
						{
							highOct = 1;
						}
						else if (maskArray[4] == 0)
						{
							highOct = 0;
						}

						break3 = ReadBE16(&romData[seqPos + 2]);
						seqPos = break3 - bankAmt;
						repeat3 = -1;
					}

					else
					{
						seqPos += 4;
					}
				}

				/*Break (4)*/
				else if (command[0] == 0x15)
				{
					if (repeat4 == 0)
					{
						mask = command[1];
						maskArray[7] = mask & 1;
						maskArray[6] = mask >> 1 & 1;
						maskArray[5] = mask >> 2 & 1;
						maskArray[4] = mask >> 3 & 1;
						maskArray[3] = mask >> 4 & 1;
						maskArray[2] = mask >> 5 & 1;
						maskArray[1] = mask >> 6 & 1;
						maskArray[0] = mask >> 7 & 1;

						if (maskArray[0] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[1] == 1)
						{
							connect = 1;
						}
						else if (maskArray[1] == 0)
						{
							connect = 0;
						}

						if (maskArray[2] == 1)
						{
							triplet = 1;
						}
						else if (maskArray[2] == 0)
						{
							triplet = 0;
						}

						if (maskArray[3] == 1)
						{
							dotted = 1;
						}
						else if (maskArray[3] == 0)
						{
							dotted = 0;
						}

						if (maskArray[4] == 1)
						{
							highOct = 1;
						}
						else if (maskArray[4] == 0)
						{
							highOct = 0;
						}

						break4 = ReadBE16(&romData[seqPos + 2]);
						seqPos = break4 - bankAmt;
						repeat4 = -1;
					}

					else
					{
						seqPos += 4;
					}
				}

				/*Loop point*/
				else if (command[0] == 0x16)
				{
					seqEnd = 1;
				}

				/*End of sequence*/
				else if (command[0] == 0x17)
				{
					seqEnd = 1;
				}

				/*Set duty*/
				else if (command[0] == 0x18)
				{
					seqPos += 2;
				}

				/*Set envelope*/
				else if (command[0] == 0x19)
				{
					seqPos += 2;
				}

				/*Play note*/
				else if (command[0] >= 0x20)
				{
					if (command[0] >= 0x20 && command[0] < 0x40)
					{
						curNoteLen = 7;
						subVal = 0x20;
					}
					else if (command[0] >= 0x40 && command[0] < 0x60)
					{
						curNoteLen = 15;
						subVal = 0x40;
					}
					else if (command[0] >= 0x60 && command[0] < 0x80)
					{
						curNoteLen = 30;
						subVal = 0x60;
					}
					else if (command[0] >= 0x80 && command[0] < 0xA0)
					{
						curNoteLen = 60;
						subVal = 0x80;
					}
					else if (command[0] >= 0xA0 && command[0] < 0xC0)
					{
						curNoteLen = 120;
						subVal = 0xA0;
					}
					else if (command[0] >= 0xC0 && command[0] < 0xE0)
					{
						curNoteLen = 240;
						subVal = 0xC0;
					}
					else if (command[0] >= 0xE0)
					{
						curNoteLen = 480;
						subVal = 0xE0;
					}

					if (triplet == 1)
					{
						curNoteLen = curNoteLen * 2 / 3;
					}

					if (dotted == 1)
					{
						curNoteLen = curNoteLen * 1.5;
						dotted = 0;
					}

					else if (dotted == 2)
					{
						curNoteLen = curNoteLen * 1.5;
					}

					if (command[0] == 0x20 || command[0] == 0x40 || command[0] == 0x60 || command[0] == 0x80 || command[0] == 0xA0 || command[0] == 0xC0 || command[0] == 0xE0)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
					}
					else
					{
						if (connect == 0)
						{
							curNote = command[0] - subVal + 23 + (octave * 12) + transpose1 + transpose2;
							if (highOct == 1)
							{
								curNote += 24;
							}

							if (format == 1 && curTrack != 3)
							{
								curNote -= 12;
							}

							if (format == 3 && curNote > 24)
							{
								curNote -= 24;
							}

							curNoteLen += connExt;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							midPos = tempPos;
							connExt = 0;
						}
						else if (connect == 1)
						{
							if (format == 3)
							{
								curNote = command[0] - subVal + 23 + (octave * 12) + transpose1 + transpose2;
								if (highOct == 1)
								{
									curNote += 24;
								}

								if (format == 1 && curTrack != 3)
								{
									curNote -= 12;
								}

								if (format == 3 && curNote > 24)
								{
									curNote -= 24;
								}
								if (curNote != prevNote && prevNote != -1)
								{
									tempPos = WriteNoteEvent(midData, midPos, prevNote, connExt, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									masterDelay += curNoteLen;
									midPos = tempPos;
									connExt = 0;
								}
							}
							connExt += curNoteLen;
						}
						prevNote = curNote;
					}
					seqPos++;
				}

				/*Unknown command*/
				else
				{
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