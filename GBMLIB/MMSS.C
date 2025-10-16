/*M.M.S.S. (Multi Music Sound System) - Meldac/KAZe*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MMSS.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long seqTablePtrLoc;
long seqTableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int drvVers;
int songBank;
int verOverride;
int ptrOverride;
int stopCvt;
int multiBanks;
int curBank;

char folderName[100];

const char MMSSMagicBytesA[5] = { 0x06, 0x00, 0x4F, 0x09, 0x01 };
const char MMSSMagicBytesSeq[4] = { 0x5F, 0x3E, 0x00, 0xCE };
const char MMSSMagicBytesSeq2[9] = { 0x87, 0x30, 0x01, 0x14, 0x83, 0x5F, 0x7A, 0xCE, 0x00 };
const char MMSSMagicBytesB[6] = { 0x77, 0x24, 0x0D, 0x20, 0xD8, 0x21 };
const char MMSSMagicBytesA2[8] = { 0x09, 0x01, 0x02, 0xC9, 0x2A, 0x12, 0x02, 0x03 };
const char MMSSMagicBytesA3[8] = { 0x09, 0x01, 0x02, 0xDE, 0x2A, 0x12, 0x02, 0x03 };

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
void MMSSsong2mid(int songNum, long songPtrs[4]);

void MMSSProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	verOverride = 0;
	ptrOverride = 0;
	bankAmt = 0x4000;

	if (bank < 2)
	{
		bank = 2;
		bankAmt = 0x0000;
	}

	if (parameters[1][0] != 0x00)
	{
		tableOffset = strtol(parameters[1], NULL, 16);
		ptrOverride = 1;
		foundTable = 1;

		drvVers = strtol(parameters[0], NULL, 16);
		verOverride = 1;

	}

	else if (parameters[0][0] != 0x00)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		verOverride = 1;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	if (ptrOverride != 1)
	{

		/*Try to search the bank for sequence table loader - Method 1*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], MMSSMagicBytesA, 5)) && foundTable != 1)
			{
				tablePtrLoc = i - 2;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (verOverride != 1)
				{
					drvVers = 1;
				}
			}
		}

		/*Try to search the bank for sequence table loader - Method 1B*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], MMSSMagicBytesA2, 8)) && foundTable != 1)
			{
				tablePtrLoc = i - 2;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (verOverride != 1)
				{
					drvVers = 1;
				}
			}
		}

		/*Try to search the bank for sequence table loader - Method 1C*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], MMSSMagicBytesA3, 8)) && foundTable != 1)
			{
				tablePtrLoc = i - 2;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (verOverride != 1)
				{
					drvVers = 1;
				}
			}
		}


		/*Try to search the bank for song table loader - Method 2*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], MMSSMagicBytesB, 6)) && foundTable == 0)
			{
				tablePtrLoc = i + 6;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (verOverride != 1)
				{
					drvVers = 3;
				}
			}
		}
	}

	/*Try to search the bank for sequence table loader - Method 1*/
	if (foundTable == 1 && drvVers == 1)
	{
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], MMSSMagicBytesSeq, 4)) && foundTable == 1)
			{
				seqTablePtrLoc = i - 1;
				printf("Found pointer to sequence table at address 0x%04x!\n", seqTablePtrLoc);
				seqTableOffset = romData[seqTablePtrLoc] + (romData[seqTablePtrLoc + 5] * 0x100);
				printf("Sequence table starts at 0x%04x...\n", seqTableOffset);
				foundTable = 2;

			}
		}
	}

	/*Try to search the bank for sequence table loader - Method 1*/
	if (foundTable == 1 && drvVers == 1)
	{
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], MMSSMagicBytesSeq2, 9)) && foundTable == 1)
			{
				seqTablePtrLoc = i - 2;
				printf("Found pointer to sequence table at address 0x%04x!\n", seqTablePtrLoc);
				seqTableOffset = ReadLE16(&romData[seqTablePtrLoc]);
				printf("Sequence table starts at 0x%04x...\n", seqTableOffset);
				foundTable = 2;

			}
		}
	}

	if (drvVers == 1 && foundTable == 2)
	{
		stopCvt = 0;
		i = tableOffset;
		songNum = 1;
		firstPtr = ReadLE16(&romData[i]);
		if (drvVers == 1)
		{
			while (i < seqTableOffset)
			{
				for (j = 0; j < 4; j++)
				{
					if (romData[i + j] >= 0x80)
					{
						songPtrs[j] = 0x0000;
					}
					else
					{
						songPtrs[j] = ReadLE16(&romData[seqTableOffset + (romData[i + j]) * 2]);
					}
					printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), songPtrs[j]);
				}

				songBank = bank;
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				MMSSsong2mid(songNum, songPtrs);
				free(exRomData);
				i += 4;
				songNum++;
			}
		}
		free(romData);
	}
	else if (drvVers == 2 && foundTable == 1)
	{
		stopCvt = 0;
		i = tableOffset;
		songNum = 1;

		while (songNum <= 57)
		{
			for (j = 0; j < 4; j++)
			{
				if (ReadLE16(&romData[i + (j * 2)]) == 0x8000)
				{
					songPtrs[j] = 0x0000;
				}
				else
				{
					songPtrs[j] = ReadLE16(&romData[i + (j * 2)]);
				}
				printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), songPtrs[j]);
			}
			songBank = bank;
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);
			MMSSsong2mid(songNum, songPtrs);
			free(exRomData);
			i += 8;
			songNum++;
		}

		free(romData);
	}
	else if (drvVers == 3 && foundTable == 1)
	{
		stopCvt = 0;
		i = tableOffset;
		songNum = 1;

		/*Skip SFX data*/
		while (ReadLE16(&romData[i]) == 0xFF00)
		{
			i += 8;
		}

		while (ReadLE16(&romData[i]) != 0x0000)
		{
			for (j = 0; j < 4; j++)
			{
				if (ReadLE16(&romData[i + (j * 2)]) == 0xFF00)
				{
					songPtrs[j] = 0x0000;
				}
				else
				{
					songPtrs[j] = ReadLE16(&romData[i + (j * 2)]);

					if (songPtrs[j] >= 0x8000 && songPtrs[j] <= 0xC000)
					{
						songPtrs[j] -= 0x4000;
						songBank = bank + 1;
					}
					else if (songPtrs[j] >= 0xC000)
					{
						songPtrs[j] -= 0x8000;
						songBank = bank + 2;
					}
					else
					{
						songBank = bank;
					}
				}
				printf("Song %i channel %i: 0x%04X (bank %02X)\n", songNum, (j + 1), songPtrs[j], songBank);
			}
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);
			MMSSsong2mid(songNum, songPtrs);
			free(exRomData);
			i += 8;
			songNum++;
		}
		free(romData);
	}
	else if (drvVers == 4 && foundTable == 1)
	{
		stopCvt = 0;
		i = tableOffset;
		songNum = 1;
		firstPtr = 0x8000;

		while (stopCvt != 1)
		{
			if (ReadLE16(&romData[i + 1]) <= 0x8001 && ReadLE16(&romData[i + 1]) >= bankAmt)
			{
				songBank = romData[i] + 1;
				songPtrs[0] = ReadLE16(&romData[i + 1]);
				songPtrs[1] = ReadBE16(&romData[i + 3]);
				songPtrs[2] = ReadLE16(&romData[i + 5]);
				songPtrs[3] = ReadBE16(&romData[i + 7]);

				for (j = 0; j < 4; j++)
				{
					if (songPtrs[j] == 0x8000 || songPtrs[j] == 0x8001)
					{
						songPtrs[j] = 0x0000;
					}
					printf("Song %i channel %i: 0x%04X (bank %02X)\n", songNum, (j + 1), songPtrs[j], songBank);
				}
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				MMSSsong2mid(songNum, songPtrs);
				free(exRomData);
				songNum++;
				i += 9;
			}
			else
			{
				stopCvt = 1;
			}
		}
		free(romData);
		fclose(rom);
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
void MMSSsong2mid(int songNum, long songPtrs[4])
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
	int tempo = 120;
	int tempoVal;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int transpose = 0;
	int repeats[16][2];
	int repeatNum;
	int repeat1 = 0;
	int repeat1Start = 0;
	int repeat2 = 0;
	int repeat2Start = 0;
	int repeat3 = 0;
	int repeat3Start = 0;
	int repeat4 = 0;
	int repeat4Start = 0;
	int repeat5 = 0;
	int repeat5Start = 0;
	unsigned char command[4];
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
	long masterDelay = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long macro4Pos = 0;
	long macro4Ret = 0;
	long macro5Pos = 0;
	long macro5Ret = 0;
	long jumpAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;

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

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
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

			if (songPtrs[curTrack] >= bankAmt && songPtrs[curTrack] < 0x8000)
			{
				seqPos = songPtrs[curTrack];
				curDelay = 0;
				ctrlDelay = 0;
				masterDelay = 0;
				seqEnd = 0;

				curNote = 0;
				curNoteLen = 0;
				inMacro = 0;
				repeat1 = -1;
				repeat2 = -1;
				transpose = 0;

				for (j = 0; j < 16; j++)
				{
					repeats[j][0] = -1;
					repeats[j][1] = 0;
				}
				repeatNum = 0;

				if (seqPos == 0x0000 && bankAmt == 0x0000)
				{
					seqEnd = 1;
				}

				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];

					/*Play note/rest*/
					if (command[0] <= 0xBF)
					{
						curNoteLen = ((command[1] & 0x1F) + 1) * 16;

						if (command[0] == 0x6F)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						else
						{
							curNote = command[0] + 24 + transpose;
							if (curTrack == 2)
							{
								curNote -= 12;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						seqPos += 2;
					}

					/*Repeat the following section*/
					else if (command[0] >= 0xD0 && command[0] <= 0xD3)
					{
						if (drvVers == 1 || drvVers == 4)
						{
							repeatNum++;
							repeats[repeatNum][0] = command[1];
							seqPos += 2;
						}
						else
						{
							seqPos++;
						}
					}

					/*End of repeat section*/
					else if (command[0] >= 0xD4 && command[0] <= 0xD7)
					{
						if (drvVers == 1 || drvVers == 4)
						{
							if (repeats[repeatNum][0] > 1)
							{
								repeats[repeatNum][1] = ReadLE16(&exRomData[seqPos + 1]);
								repeats[repeatNum][0]--;
								seqPos = repeats[repeatNum][1];
							}
							else
							{
								repeats[repeatNum][0] = -1;
								repeatNum--;
								seqPos += 3;
							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Reset channel envelope attack*/
					else if (command[0] == 0xD8)
					{
						seqPos++;
					}

					/*Set channel envelope attack*/
					else if (command[0] >= 0xD9 && command[0] <= 0xDB)
					{
						seqPos++;
					}

					/*Unknown command DC-DF*/
					else if (command[0] >= 0xDC && command[0] <= 0xDF)
					{
						seqPos++;
					}

					/*Set noise*/
					else if (command[0] >= 0xE0 && command[0] <= 0xE7)
					{
						seqPos++;
					}

					/*Set channel instrument parameters*/
					else if (command[0] >= 0xE8 && command[0] <= 0xEF)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] >= 0xF0 && command[0] <= 0xF3)
					{
						seqPos++;
					}

					/*Repeat section*/
					else if (command[0] == 0xF4)
					{
						if (drvVers == 2 || drvVers == 3)
						{
							repeatNum++;
							repeats[repeatNum][0] = command[1];
							repeats[repeatNum][1] = seqPos + 2;
							seqPos += 2;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*End of repeat section*/
					else if (command[0] == 0xF5)
					{
						if (drvVers == 2 || drvVers == 3)
						{
							if (repeats[repeatNum][0] > 1)
							{
								repeats[repeatNum][0]--;
								seqPos = repeats[repeatNum][1];
							}
							else
							{
								repeats[repeatNum][0] = -1;
								repeatNum--;
								seqPos++;
							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Reset sweep*/
					else if (command[0] == 0xF6)
					{
						seqPos++;
					}

					/*Set sweep*/
					else if (command[0] == 0xF7)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0xF8)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Set speed*/
					else if (command[0] == 0xF9)
					{
						if (drvVers == 3)
						{
							tempo = command[1] * 7;

							if (tempo < 2)
							{
								tempo = 120;
							}
							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
						}
						else
						{
							tempo = command[1] * 120;

							if (tempo < 2)
							{
								tempo = 120;
							}
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

					/*Set global volume*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Unknown command FB*/
					else if (command[0] == 0xFB)
					{
						seqPos += 2;
					}

					/*Go to macro*/
					else if (command[0] == 0xFC)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro1Ret = seqPos + 3;
							seqPos = macro1Pos;
							inMacro = 1;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro2Ret = seqPos + 3;
							seqPos = macro2Pos;
							inMacro = 2;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro3Ret = seqPos + 3;
							seqPos = macro3Pos;
							inMacro = 3;
						}
						else if (inMacro == 3)
						{
							macro4Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro4Ret = seqPos + 3;
							seqPos = macro4Pos;
							inMacro = 4;
						}
						else if (inMacro == 4)
						{
							macro5Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro5Ret = seqPos + 3;
							seqPos = macro5Pos;
							inMacro = 5;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Jump to position*/
					else if (command[0] == 0xFD)
					{
						seqEnd = 1;
					}

					/*Return from macro*/
					else if (command[0] == 0xFE)
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
						else if (inMacro == 4)
						{
							seqPos = macro4Ret;
							inMacro = 3;
						}
						else if (inMacro == 5)
						{
							seqPos = macro5Ret;
							inMacro = 4;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*End of channel*/
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
}