/*Torus (2nd driver)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "TORUS2.H"

#define bankSize 16384

FILE* rom, * xm, * data;
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
int curInst;
int drvVers;
int curVol;

unsigned char* romData;
unsigned char* exRomData;

unsigned char* xmData;
unsigned char* endData;
long xmLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void Torus2song2xm(int songNum, long songPtr);

void Torus2Proc(int bank, char parameters[4][100])
{
	if (bank < 2)
	{
		bank = 2;
	}

	tableOffset = strtol(parameters[0], NULL, 16);

	totalSongs = strtol(parameters[1], NULL, 16);

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	i = tableOffset;
	songNum = 1;

	while (songNum <= totalSongs)
	{
		songPtr = ReadLE16(&romData[i]);
		songTempo = romData[i + 2];
		songBank = romData[i + 3];
		printf("Song %i: 0x%04X, bank %02X, tempo: %02X\n", songNum, songPtr, songBank, songTempo);

		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, (songBank * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
		Torus2song2xm(songNum, songPtr);
		free(exRomData);
		i += 4;
		songNum++;
	}
	fclose(rom);

}

/*Convert the song data to XM*/
void Torus2song2xm(int songNum, long songPtr)
{
	int curPat = 0;
	long pattern[4];
	unsigned char command[4];
	long curPos = 0;
	long xmPos = 0;
	long patPos = 0;
	int channels = 3;
	int defTicks = 6;
	int bpm = 150;
	long packPos = 0;
	long tempPos = 0;
	int rowsLeft = 0;
	int curChan = 0;
	int numPats = 0;
	int patRows = 64;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long patSize = 0;
	int curNote;
	int freqVal;
	int instChan;
	int newInst = 0;
	int curInsts[4];
	unsigned char decByte1;
	unsigned char decByte2;
	unsigned char tempByte;

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
		bpm = 150;
		numPats = 0;
		patRows = 64;
		/*Get the number of patterns for the XM*/
		curPos = songPtr;
		while (exRomData[curPos + 1] != 0xFF)
		{
			numPats++;
			curPos += 2;
		}

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
		defTicks = 6;
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

		for (curChan = 0; curChan < 4; curChan++)
		{
			curInsts[curChan] = 0;
		}

		for (curPat = 0; curPat < numPats; curPat++)
		{
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
			rowsLeft = 64;
			patSize = 0;

			patPos = ReadLE16(&exRomData[songPtr + (curPat * 2)]);

			for (rowsLeft = 64; rowsLeft > 0; rowsLeft--)
			{
				for (curChan = 0; curChan < channels; curChan++)
				{
					command[0] = exRomData[patPos];
					command[1] = exRomData[patPos + 1];
					command[2] = exRomData[patPos + 2];
					command[3] = exRomData[patPos + 3];
					/*Empty cell*/
					if ((command[0] & 0x80) == 0)
					{
						patPos++;
						xmData[xmPos] = 0x80;
						xmPos++;
						patSize++;
					}
					else
					{
						if ((command[0] & 0x40) != 0x00)
						{
							curInsts[curChan] = exRomData[patPos + 3] + 1;
							newInst = 1;
						}
						else
						{
							newInst = 0;
						}

						curNote = (command[0] & 0x3F) + 25;
						if (curChan != 2)
						{
							curNote += 12;
						}
						curVol = command[1];
						patPos += 3;
						if (newInst != 0)
						{
							patPos++;
						}

						Write8B(&xmData[xmPos], 0x83);
						Write8B(&xmData[xmPos + 1], curNote);
						Write8B(&xmData[xmPos + 2], curInsts[curChan]);
						xmPos += 3;
						patSize += 3;
					}
				}
			}
			WriteLE16(&xmData[packPos], patSize);
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