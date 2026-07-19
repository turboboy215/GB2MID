/*Wicked Witch Software*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <math.h>
#include "WICKED.H"

#define bankSize 16384

FILE* rom, * xm, * data, * cfg;
long bank;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
int totalSongs;
int songBank;
int songTempo;
long songPtr;
long bankAmt;
int numSongs;
int songNum;
int songStart;
int patRows;
int curInst;
int drvVers;
int curVol;

unsigned char* romData;
unsigned char* exRomData;

unsigned char* xmData;
unsigned char* endData;
long xmLength;

char string1[100];
char string2[100];
char WickedcheckStrings[3][100] = { "numSongs=", "bank=", "offset=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void Wickedsong2xm(int songNum, int startPos);

void WickedProc(int bank, char parameters[4][500])
{
	drvVers = WICKED_VER_STD;
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, WickedcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtol(string1, NULL, 16);

		printf("Number of songs: %i\n", numSongs);
		fgets(string1, 3, cfg);

		/*Process each song*/
		songNum = 1;

		while (songNum <= numSongs)
		{
			/*Skip new line*/
			fgets(string1, 3, cfg);

			/*Skip the first line*/
			fgets(string1, 13, cfg);

			/*Get the song bank*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, WickedcheckStrings[1], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songBank = strtol(string1, NULL, 16);

			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			/*Get the start of the song*/
			fgets(string1, 8, cfg);
			if (memcmp(string1, WickedcheckStrings[2], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songStart = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			printf("Song %i: 0x%04X, bank %02X\n", songNum, songStart, songBank);
			Wickedsong2xm(songNum, songStart);
			free(romData);
			songNum++;

		}
		fclose(cfg);

	}
}

/*Convert the song data to XM*/
void Wickedsong2xm(int songNum, int startPos)
{
	int curPat = 0;
	unsigned char command[2];
	long curPos = 0;
	int index = 0;
	int curSeq = 0;
	int romPos = 0;
	long xmPos = 0;
	int patBank = 0;
	long patPos = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	long packPos = 0;
	long tempPos = 0;
	int rowsLeft = 0;
	int curChan = 0;
	int numPats = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long patSize = 0;
	int curNote;
	unsigned int mask;
	int cellData[3];

	int l = 0;

	xmLength = 0x10000;
	xmData = ((unsigned char*)malloc(xmLength));

	for (l = 0; l < xmLength; l++)
	{
		xmData[l] = 0;
	}

	sprintf(outfile, "song%d.xm", songNum);
	if ((xm = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.xm!\n", songNum);
		exit(2);
	}
	else
	{
		curPos = startPos;
		bpm = 150;
		songTempo = romData[startPos];
		numPats = romData[startPos + 1];

		curPos += 3;

		xmPos = 0;
		/*Write the header*/
		sprintf((char*)&xmData[xmPos], "Extended Module: ");
		xmPos += 17;
		sprintf((char*)&xmData[xmPos], "                     ");
		xmPos += 20;
		Write8B(&xmData[xmPos], 0x1A);
		xmPos++;
		sprintf((char*)&xmData[xmPos], "FastTracker v2.00   ");
		xmPos += 20;
		WriteBE16(&xmData[xmPos], 0x0401);
		xmPos += 2;

		/*Header size: 20 + number of patterns (256)*/
		WriteLE32(&xmData[xmPos], 276);
		xmPos += 4;

		/*Song length*/
		WriteLE16(&xmData[xmPos], numPats);
		xmPos += 2;

		/*Restart position*/
		WriteLE16(&xmData[xmPos], 0);
		xmPos += 2;

		/*Number of channels*/
		WriteLE16(&xmData[xmPos], channels);
		xmPos += 2;

		/*Number of patterns*/
		WriteLE16(&xmData[xmPos], numPats);
		xmPos += 2;

		/*Number of instruments*/
		WriteLE16(&xmData[xmPos], 32);
		xmPos += 2;

		/*Flags: Linear frequency*/
		WriteLE16(&xmData[xmPos], 1);
		xmPos += 2;

		/*Default tempo (ticks)*/
		defTicks = songTempo;
		WriteLE16(&xmData[xmPos], defTicks);
		xmPos += 2;

		/*Default tempo (BPM), always the same for our case*/
		WriteLE16(&xmData[xmPos], bpm);
		xmPos += 2;

		/*Pattern table*/
		for (l = 0; l < numPats; l++)
		{
			Write8B(&xmData[xmPos], l);
			xmPos++;
		}
		xmPos += (256 - l);

		for (curPat = 0; curPat < numPats; curPat++)
		{
			/*Get information from header*/
			patBank = romData[curPos];
			patPos = ReadLE16(&romData[curPos + 1]);

			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, (patBank * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);

			/*Get number of rows for pattern*/
			patRows = exRomData[patPos];
			patPos++;

			/*First, pattern header*/
			/*Pattern header length*/
			WriteLE32(&xmData[xmPos], 9);
			xmPos += 4;

			/*Packing type = 0*/
			Write8B(&xmData[xmPos], 0);
			xmPos++;

			/*Number of rows*/
			WriteLE16(&xmData[xmPos], patRows);
			xmPos += 2;

			/*Packed pattern data - fill in later*/
			packPos = xmPos;
			WriteLE16(&xmData[xmPos], 0);
			xmPos += 2;

			/*Now the actual pattern data...*/
			patSize = 0;

			for (rowsLeft = patRows; rowsLeft > 0; rowsLeft--)
			{
				for (curChan = 0; curChan < 4; curChan++)
				{
					mask = exRomData[patPos];

					cellData[0] = 0;
					cellData[1] = 0;
					cellData[2] = 0;

					/*Empty cell*/
					if (mask == 0x80)
					{
						patPos++;
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
					else
					{
						patPos++;

						if ((mask & 0x01) != 0x00)
						{
							curNote = exRomData[patPos];
							curInst = exRomData[patPos + 1];
							cellData[0] = 1;
							cellData[1] = 1;
							patPos += 2;
						}
						if ((mask & 0x04) != 0x00)
						{
							curVol = exRomData[patPos];
							cellData[2] = 1;
							patPos++;
						}

						if (cellData[0] != 0)
						{
							if (cellData[2] != 0)
							{
								Write8B(&xmData[xmPos], 0x87);
								Write8B(&xmData[xmPos + 1], curNote);
								Write8B(&xmData[xmPos + 2], curInst);
								Write8B(&xmData[xmPos + 3], curVol);
								xmPos += 4;
								patSize += 4;
							}
							else
							{
								Write8B(&xmData[xmPos], 0x83);
								Write8B(&xmData[xmPos + 1], curNote);
								Write8B(&xmData[xmPos + 2], curInst);
								xmPos += 3;
								patSize += 3;
							}

						}
						else
						{
							Write8B(&xmData[xmPos], 0x84);
							Write8B(&xmData[xmPos + 1], curVol);
							xmPos += 2;
							patSize += 2;
						}
					}
				}
			}
			WriteLE16(&xmData[packPos], patSize);
			free(exRomData);
			curPos += 3;

		}
		fwrite(xmData, xmPos, 1, xm);

		/*Add data to end of XM file*/
		if ((data = fopen("XMDATA.DAT", "rb")) == NULL)
		{
			printf("ERROR: Unable to open file XMDATA.DAT!\n");
			exit(1);
		}
		else
		{
			endData = ((unsigned char*)malloc(11744));
			fread(endData, 1, 11744, data);
			fwrite(endData, 11744, 1, xm);
			xmPos += 11744;
		}

		free(xmData);
		free(endData);
		fclose(xm);

	}

}