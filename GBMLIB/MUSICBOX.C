/*MusicBox (by BlackBox)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "MUSICBOX.H"

#define bankSize 16384

FILE* rom, * xm, * data, * cfg;
long bank;
long tablePtrLoc;
long tableOffset;
long stepPtrLoc;
long stepOffset;
long stepTab;
int i, j;
char outfile[1000000];
long bankAmt;
int numSongs;
int songNum;
int songStart;
int songEnd;
int songLen;
int songPos;
int patRows;
int songBank;
long songTable;
long insTable;
long patTable;
int drvVers;
int curInst;

unsigned char* romData;
unsigned char* songData;
unsigned char* exRomData;
unsigned char* xmData;
unsigned char* endData;
long xmLength;

char string1[100];
char string2[100];
char MusicBoxcheckStrings[6][100] = { "numSongs=", "version=", "bank=", "start=", "end=", "pos" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void MusicBoxcopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd);
void MusicBoxsong2xm(int songNum, int songLen, int startPos);

void MusicBoxcopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd)
{
	int k = dataStart;
	int l = 0;

	songLen = dataEnd - dataStart;
	songData = (unsigned char*)malloc(bankSize);

	while (k <= dataEnd)
	{
		songData[l] = romData[k];
		k++;
		l++;
	}
	free(romData);
}

void MusicBoxProc(int bank, char parameters[4][100])
{
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, MusicBoxcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtol(string1, NULL, 16);

		printf("Number of songs: %i\n", numSongs);
		fgets(string1, 3, cfg);

		/*Get the driver version*/
		fgets(string1, 9, cfg);
		if (memcmp(string1, MusicBoxcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		drvVers = strtol(string1, NULL, 16);

		if (drvVers != MUSICBOX_VER_STD && drvVers != MUSICBOX_VER_DICE)
		{
			printf("ERROR: Invalid version number!\n");
			exit(2);
		}

		printf("Version: %i\n", drvVers);

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
			if (memcmp(string1, MusicBoxcheckStrings[2], 1))
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
			fgets(string1, 7, cfg);
			if (memcmp(string1, MusicBoxcheckStrings[3], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songStart = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			/*Get the end of the song*/
			fgets(string1, 5, cfg);
			if (memcmp(string1, MusicBoxcheckStrings[4], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songEnd = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			songLen = songEnd - songStart + 1;

			/*Get the position of the song*/
			fgets(string1, 5, cfg);
			if (memcmp(string1, MusicBoxcheckStrings[5], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songPos = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			songLen = songEnd - songStart + 1;

			printf("Song %i: 0x%04X (bank: %02X, size: 0x%04X, position: %02X)\n", songNum, songStart, songBank, songLen, songPos);

			MusicBoxcopyData(romData, songData, songStart, songEnd);
			MusicBoxsong2xm(songNum, songLen, songPos);
			songNum++;
		}
	}
}

/*Convert the song data to XM*/
void MusicBoxsong2xm(int songNum, int songLen, int startPos)
{
	int curPat = 0;
	long pattern[4];
	unsigned char command[2];
	long curPos = 0;
	int index = 0;
	int curSeq = 0;
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	long xmPos = 0;
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
	int freqVal;
	int instChan;
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
		if (drvVers == MUSICBOX_VER_STD)
		{
			bpm = 150;
			numPats = 56;
			/*Get the number of patterns for the XM*/
			songPos = 0x0100 + songPos;
			l = 0;
			while (l < 0x100)
			{
				if (songData[songPos + l] == 0xFE || songData[songPos + l] == 0xFF)
				{
					numPats = l;
					break;
				}
				l++;
			}
		}

		else if (drvVers == MUSICBOX_VER_DICE)
		{
			bpm = 80;
			/*Get "decryption" bytes*/
			decByte1 = songData[0x00E2];
			decByte2 = songData[0x007A];

			/*Get the number of patterns for the XM*/
			songPos = 0x0300 + songPos;
			l = 0;
			while (l < 0x100)
			{
				tempByte = songData[songPos + l] ^ decByte1;
				tempByte = tempByte ^ 0x29;
				if (tempByte == 0xFE || tempByte == 0xFF)
				{
					numPats = l;
					break;
				}

				l++;
			}
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

			if (drvVers == MUSICBOX_VER_STD)
			{
				c1Pos = 0x0400 + (songData[0x0100 + curPat + startPos] * 0x80);
				c2Pos = 0x0400 + (songData[0x0140 + curPat + startPos] * 0x80);
				c3Pos = 0x0400 + (songData[0x0180 + curPat + startPos] * 0x80);
				c4Pos = 0x0400 + (songData[0x01C0 + curPat + startPos] * 0x80);
			}
			else if (drvVers == MUSICBOX_VER_DICE)
			{
				tempByte = songData[0x0300 + curPat + startPos] ^ decByte1;
				tempByte = tempByte ^ 0x29;
				c1Pos = 0x0700 + (tempByte * 0x80);

				tempByte = songData[0x0340 + curPat + startPos] ^ decByte1;
				tempByte = tempByte ^ 0x29;
				c2Pos = 0x0700 + (tempByte * 0x80);

				tempByte = songData[0x0380 + curPat + startPos] ^ decByte1;
				tempByte = tempByte ^ 0x29;
				c3Pos = 0x0700 + (tempByte * 0x80);

				tempByte = songData[0x03C0 + curPat + startPos] ^ decByte1;
				tempByte = tempByte ^ 0x29;
				c4Pos = 0x0700 + (tempByte * 0x80);
			}


			for (rowsLeft = 64; rowsLeft > 0; rowsLeft--)
			{
				for (curChan = 0; curChan < 4; curChan++)
				{
					if (drvVers == MUSICBOX_VER_STD)
					{
						if (curChan == 0)
						{
							command[0] = songData[c1Pos];
							command[1] = songData[c1Pos + 1];
						}

						else if (curChan == 1)
						{
							command[0] = songData[c2Pos];
							command[1] = songData[c2Pos + 1];
						}

						else if (curChan == 2)
						{
							command[0] = songData[c3Pos];
							command[1] = songData[c3Pos + 1];
						}

						else if (curChan == 3)
						{
							command[0] = songData[c4Pos];
							command[1] = songData[c4Pos + 1];
						}
					}
					else if (drvVers == MUSICBOX_VER_DICE)
					{
						if (curChan == 0)
						{
							tempByte = songData[c1Pos] ^ decByte2;
							command[0] = tempByte ^ 0xB1;

							tempByte = songData[c1Pos + 1] ^ decByte2;
							command[1] = tempByte ^ 0xB1;
						}

						else if (curChan == 1)
						{
							tempByte = songData[c2Pos] ^ decByte2;
							command[0] = tempByte ^ 0xB1;

							tempByte = songData[c2Pos + 1] ^ decByte2;
							command[1] = tempByte ^ 0xB1;
						}

						else if (curChan == 2)
						{
							tempByte = songData[c3Pos] ^ decByte2;
							command[0] = tempByte ^ 0xB1;

							tempByte = songData[c3Pos + 1] ^ decByte2;
							command[1] = tempByte ^ 0xB1;
						}

						else if (curChan == 3)
						{
							tempByte = songData[c4Pos] ^ decByte2;
							command[0] = tempByte ^ 0xB1;

							tempByte = songData[c4Pos + 1] ^ decByte2;
							command[1] = tempByte ^ 0xB1;
						}
					}


					/*Empty row*/
					if (command[0] == 0x00 && command[1] == 0x00)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}

					/*Note*/
					else if (command[1] == 0x00)
					{
						curNote = command[0] + 24;
						Write8B(&xmData[xmPos], 0x81);
						Write8B(&xmData[xmPos + 1], curNote);
						xmPos += 2;
						patSize += 2;
					}

					/*Instrument*/
					else if (command[0] == 0x00)
					{
						curInst = command[1];
						Write8B(&xmData[xmPos], 0x82);
						Write8B(&xmData[xmPos + 1], curInst);
						xmPos += 2;
						patSize += 2;
					}

					/*Note & Instrument*/
					else
					{
						curNote = command[0] + 24;
						curInst = command[1];
						Write8B(&xmData[xmPos], 0x83);
						Write8B(&xmData[xmPos + 1], curNote);
						Write8B(&xmData[xmPos + 2], curInst);
						xmPos += 3;
						patSize += 3;
					}

					if (curChan == 0)
					{
						c1Pos += 2;
					}
					else if (curChan == 1)
					{
						c2Pos += 2;
					}
					else if (curChan == 2)
					{
						c3Pos += 2;
					}
					else if (curChan == 3)
					{
						c4Pos += 2;
					}
				}
			}
			WriteLE16(&xmData[packPos], patSize);
		}

		fwrite(xmData, xmPos, 1, xm);

		/*Add data to end of XM file*/
		if ((data = fopen("xmdata.dat", "rb")) == NULL)
		{
			printf("ERROR: Unable to open file xmdata.dat!\n");
			exit(1);
		}
		else
		{
			endData = ((unsigned char*)malloc(11744));
			fread(endData, 1, 11744, data);
			fwrite(endData, 11744, 1, xm);
			xmPos += 11744;
		}


		free(songData);
		free(xmData);
		free(endData);
		fclose(xm);
	}
}