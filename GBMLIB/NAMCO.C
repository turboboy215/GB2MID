/*Namco*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "NAMCO.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long offset;
long globalTable;
long tableOffset;
long nextTable;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long seqPtrs[4];
long songPtr;
int numSongs;
long masterBank;
int songBank;
long bankAmt;
long sizeBank;
int curInst;
int curInsts[4];
long firstPtr;
int drvVers;
int cfgPtr;
int exitError;
int fileExit;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* multiMidData[4];
unsigned char* ctrlMidData;

long midLength;

char* argv3;
char string1[100];
char string2[100];
char NamcocheckStrings[5][100] = { "version=", "bank=", "size=", "start=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Namcosong2mid(int songNum, long ptr);

void NamcoProc(int bank, char parameters[4][100])
{
	curInsts[0] = 0;
	curInsts[1] = 0;
	curInsts[2] = 0;
	curInsts[3] = 0;
	firstPtr = 0;
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the driver version value*/
		fgets(string1, 9, cfg);
		if (memcmp(string1, NamcocheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}

		fgets(string1, 3, cfg);
		drvVers = strtod(string1, NULL);
		fgets(string1, 3, cfg);
		printf("Version: %01X\n", drvVers);

		/*Get the song bank*/
		fgets(string1, 6, cfg);
		if (memcmp(string1, NamcocheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}
		fgets(string1, 5, cfg);

		masterBank = strtol(string1, NULL, 16);
		printf("Bank: %01X\n", masterBank);
		fgets(string1, 3, cfg);

		/*Get the bank size*/
		fgets(string1, 6, cfg);
		if (memcmp(string1, NamcocheckStrings[2], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}
		fgets(string1, 5, cfg);
		sizeBank = strtol(string1, NULL, 16);
		printf("Memory: 0x%04X\n", sizeBank);
		fgets(string1, 3, cfg);

		/*Get the start of the table*/
		fgets(string1, 7, cfg);
		if (memcmp(string1, NamcocheckStrings[3], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}
		fgets(string1, 5, cfg);
		globalTable = strtol(string1, NULL, 16);
		printf("Table start: 0x%04X\n", globalTable);
		fgets(string1, 3, cfg);

		/*Copy the data from the ROM*/
		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(sizeBank + 0x4000);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, sizeBank, rom);

		tableOffset = ReadLE16(&romData[globalTable]);
		nextTable = ReadLE16(&romData[globalTable + 2]);

		printf("Song table: 0x%04X (ends 0x%04X)\n", tableOffset, nextTable);

		i = tableOffset;
		songNum = 1;

		if (drvVers == 1)
		{
			i++;
		}
		else if (drvVers == 2)
		{
			i += 2;
		}
		else
		{
			i += 3;
		}

		while (i < nextTable)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Namcosong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
		free(romData);
	}
}

/*Convert the song data to MIDI*/
void Namcosong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int trackSizes[4];
	int onOff[4];
	int transpose = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	int firstNotes[4];
	unsigned int midPos = 0;
	unsigned int midPosM[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	long ctrlTrackSize = 0;
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	int rowTime = 0;
	int holdNotes[4];
	int delayTime[4];
	int prevNotes[4][2];
	int trackSpeed = 1;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long macro4Pos = 0;
	long macro4Ret = 0;
	int mask;
	int maskArray[4];
	int transposes[4];

	midLength = 0x10000;
	for (j = 0; j < 4; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < 4; k++)
		{
			multiMidData[k][j] = 0;
		}

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
			midPosM[curTrack] = 0;
			firstNotes[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			curNoteLens[curTrack] = 0;
			curDelays[curTrack] = 0;
			seqEnd = 0;
			midTrackBase = midPosM[curTrack];

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);

			onOff[curTrack] = 0;
			holdNotes[curTrack] = 0;
			delayTime[curTrack] = 0;
			prevNotes[curTrack][0] = 0;
			prevNotes[curTrack][1] = 0;
		}

		/*Convert the sequence data*/
		seqPos = ptr;
		inMacro = 0;
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNotes[curTrack] = 1;
			transposes[curTrack] = 0;
		}
		seqEnd = 0;

		while (seqEnd == 0)
		{
			command[0] = romData[seqPos];
			command[1] = romData[seqPos + 1];
			command[2] = romData[seqPos + 2];
			command[3] = romData[seqPos + 3];
			command[4] = romData[seqPos + 4];
			command[5] = romData[seqPos + 5];
			command[6] = romData[seqPos + 6];
			command[7] = romData[seqPos + 7];

			/*Set parameters*/
			lowNibble = (command[0] >> 4);
			highNibble = (command[0] & 15);

			/*Set parameters*/
			if (highNibble == 0x00)
			{
				/*Set channel mask*/
				if (lowNibble == 0x00)
				{
					mask = command[1];
					seqPos += 2;
				}

				/*Set tempo*/
				else if (lowNibble == 0x04)
				{
					trackSpeed = command[1];
					seqPos += 2;
				}

				/*End of song/exit macro*/
				else if (lowNibble == 0x08)
				{
					if (inMacro == 0)
					{
						seqEnd = 1;
					}
					else if (inMacro == 1)
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
					else
					{
						seqEnd = 1;
					}
				}

				/*Unknown*/
				else
				{
					seqPos += 2;
				}
			}


			/*Go to macro/loop*/
			else if (highNibble == 0x03)
			{
				if (lowNibble == 0x00)
				{
					seqEnd = 1;
				}
				else
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&romData[seqPos + 1]);
						macro1Ret = seqPos + 3;
						seqPos = macro1Pos;
						inMacro = 1;
					}
					else if (inMacro == 1)
					{
						macro2Pos = ReadLE16(&romData[seqPos + 1]);
						macro2Ret = seqPos + 3;
						seqPos = macro2Pos;
						inMacro = 2;
					}
					else if (inMacro == 2)
					{
						macro3Pos = ReadLE16(&romData[seqPos + 1]);
						macro3Ret = seqPos + 3;
						seqPos = macro3Pos;
						inMacro = 3;
					}
					else if (inMacro == 3)
					{
						macro4Pos = ReadLE16(&romData[seqPos + 1]);
						macro4Ret = seqPos + 3;
						seqPos = macro4Pos;
						inMacro = 1;
					}
					else
					{
						seqEnd = 1;
					}
				}
			}

			/*Set volume*/
			else if (highNibble == 0x04)
			{
				mask = lowNibble;

				maskArray[0] = mask >> 3 & 1;
				maskArray[1] = mask >> 2 & 1;
				maskArray[2] = mask >> 1 & 1;
				maskArray[3] = mask & 1;
				seqPos++;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (maskArray[curTrack] != 0x00)
					{
						seqPos++;
					}
				}
			}

			/*Play note/rest*/
			else if (highNibble == 0x05)
			{
				mask = lowNibble;

				maskArray[0] = mask >> 3 & 1;
				maskArray[1] = mask >> 2 & 1;
				maskArray[2] = mask >> 1 & 1;
				maskArray[3] = mask & 1;
				seqPos++;

				if (lowNibble == 0)
				{
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						curDelays[curTrack] += (trackSpeed * 5);
					}
				}
				else
				{
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (maskArray[curTrack] != 0x00)
						{
							curNotes[curTrack] = (romData[seqPos] / 2 + 23);
							curNoteLens[curTrack] = trackSpeed * 5;
							tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							firstNotes[curTrack] = 0;
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = 0;
							seqPos++;
						}
						else
						{
							curDelays[curTrack] += (trackSpeed * 5);
						}
					}
				}
			}

			/*Set instrument*/
			else if (highNibble == 0x06)
			{
				mask = lowNibble;

				maskArray[0] = mask >> 3 & 1;
				maskArray[1] = mask >> 2 & 1;
				maskArray[2] = mask >> 1 & 1;
				maskArray[3] = mask & 1;
				seqPos++;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (maskArray[curTrack] != 0x00)
					{
						curInsts[curTrack] = romData[seqPos];
						firstNotes[curTrack] = 1;
						seqPos++;
					}
				}
			}

			/*Other channel settings*/
			else if (highNibble >= 0x07 && highNibble <= 0x0D)
			{
				mask = lowNibble;

				maskArray[0] = mask >> 3 & 1;
				maskArray[1] = mask >> 2 & 1;
				maskArray[2] = mask >> 1 & 1;
				maskArray[3] = mask & 1;
				seqPos++;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (maskArray[curTrack] != 0x00)
					{
						seqPos++;
					}
				}
			}

			/*Set transpose*/
			else if (highNibble == 0x0E)
			{
				mask = lowNibble;

				maskArray[0] = mask >> 3 & 1;
				maskArray[1] = mask >> 2 & 1;
				maskArray[2] = mask >> 1 & 1;
				maskArray[3] = mask & 1;
				seqPos++;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (maskArray[curTrack] != 0x00)
					{
						transposes[curTrack] = (signed char)romData[seqPos];
						seqPos++;
					}
				}
			}

			/*Channel unknown*/
			else if (highNibble == 0x0F)
			{
				{
					mask = lowNibble;

					maskArray[0] = mask >> 3 & 1;
					maskArray[1] = mask >> 2 & 1;
					maskArray[2] = mask >> 1 & 1;
					maskArray[3] = mask & 1;
					seqPos++;

					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (maskArray[curTrack] != 0x00)
						{
							seqPos++;
						}
					}
				}
			}

			/*Unknown command*/
			else
			{
				seqPos++;
			}
		}

		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
		}

		free(multiMidData[0]);
		fclose(mid);
	}
}