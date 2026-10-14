/*GHX (Shin'en)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <direct.h>
#include "SHARED.H"
#include "GHX.H"

#define bankSize 16384

FILE* rom, * xm, * data;
long bank;
long offset;
long headerOffset;
int i, j;
int trFix = 0;
char outfile[1000000];
long bankAmt;
int numSongs = 0;
int songNum;
int patRows = 0;
long songTable = 0;
long insTable = 0;
long patTable = 0;
long songInfo[4];
char songNames[50][21];
int multiBanks;
int totalSeqs;
int curBank;
int foundTable;
int curVol;
int curInst;
int drvVers;
int curSong;

char folderName[100];

unsigned char* romData;
unsigned char* xmData;
unsigned char* endData;
long xmLength;

/*Bytes to look for before finding GHX "header" - end of vibrato table*/
const char magicBytes[8] = { 0x00, 0xFA, 0xF5, 0xF2, 0xF1, 0xF2, 0xF5, 0xFA };
/*"GHX" - start of "header"*/
const char headerBytes[3] = { 0x47, 0x48, 0x58 };
/*Bytes to check for start of song name list - "SONG"*/
const char songTitle[4] = { 0x53, 0x4F, 0x4E, 0x47 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void GHXsong2xm(int songNum, long info[4]);
void GHXgetSongTitles(char names[50][21]);

/*Function prototypes*/
void GHXProc(int bank)
{
	drvVers = GHX_VER_STD;
	curVol = 120;
	curInst = 0;
	foundTable = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	bankAmt = bankSize;

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	for (i = 0; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], magicBytes, 8))
		{
			for (j = i; j < (bankSize * 2); j++)
			{
				if (!memcmp(&romData[j], headerBytes, 3))
				{
					headerOffset = j;
					printf("GHX header found at address 0x%04X!\n", headerOffset);
					foundTable = 1;
					break;
				}
			}
			if (foundTable == 1)
			{
				numSongs = romData[headerOffset + 3];
				if (numSongs == 0)
				{
					numSongs = 1;
				}
				printf("Number of songs: %i\n", numSongs);
				patRows = romData[headerOffset + 4];
				printf("Rows per pattern: %i\n", patRows);
				patTable = ReadLE16(&romData[headerOffset + 6]);
				printf("Sequence data table: 0x%04X\n", patTable);
				insTable = ReadLE16(&romData[headerOffset + 8]);
				printf("Instrument table: 0x%04X\n", insTable);
				songTable = ReadLE16(&romData[headerOffset + 10]);
				printf("Song order table: 0x%04X\n", songTable);
				GHXgetSongTitles(songNames);

				i = songTable;
				for (curSong = 1; curSong <= numSongs; curSong++)
				{
					printf("\nSong %i:\n", curSong);
					if (songNames[0][0] != '\0')
					{
						printf("Title: %s\n", songNames[curSong - 1]);
					}
					songInfo[0] = romData[i] + 1;
					printf("Number of patterns: %i\n", songInfo[0]);
					songInfo[1] = ReadLE16(&romData[i + 1]);
					printf("Song data address: 0x%04X\n", songInfo[1]);
					songInfo[2] = romData[i + 3] + 1;
					printf("Loop pattern relative to final pattern: %i\n", songInfo[2]);
					songInfo[3] = ReadLE16(&romData[i + 4]);
					printf("Pattern loop address: 0x%04X\n", songInfo[3]);
					if (songInfo[0] < 128)
					{
						GHXsong2xm(curSong, songInfo);
					}
					else
					{
						if (curSong == 3 && songInfo[1] == 0x5187 && bank == 0x0A)
						{
							songInfo[0] = 1;
							GHXsong2xm(curSong, songInfo);
						}
						else
						{
							printf("Invalid song, skipping.\n");
						}
					}

					i += 6;
				}

			}
			else
			{
				printf("ERROR: Magic bytes not found!\n");
				exit(-1);
			}

		}
	}


	free(romData);

}

/*Convert the song data to XM*/
void GHXsong2xm(int songNum, long info[4])
{
	int curPat = 0;
	long pattern[7];
	unsigned char command[3];
	long curPos = 0;
	int index = 0;
	int curSeq = 0;
	signed int transpose[3] = { 0, 0, 0 };
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	long romPos = 0;
	long xmPos = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	long packPos = 0;
	long tempPos = 0;
	int rowsLeft = 0;
	int curChan = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long patSize = 0;
	int curNote;
	int curVols[4];
	int chanMask = 0x80;

	int l = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	xmLength = 0x10000;
	xmData = ((unsigned char*)malloc(xmLength));

	for (l = 0; l < xmLength; l++)
	{
		xmData[l] = 0;
	}

	if (multiBanks == 0)
	{
		sprintf(outfile, "song%d.xm", songNum);
	}
	else
	{
		sprintf(outfile, "Bank %i/song%d.xm", (curBank + 1), songNum);
	}
	if ((xm = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.xm!\n", songNum);
		exit(2);
	}
	else
	{
		xmPos = 0;
		/*Write the header*/
		sprintf((char*)&xmData[xmPos], "Extended Module: ");
		xmPos += 17;
		if (songNames[0][0] != '\0')
		{
			sprintf((char*)&xmData[xmPos], songNames[curSong - 1]);
		}
		else
		{
			sprintf((char*)&xmData[xmPos], "                     ");
		}
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
		WriteLE16(&xmData[xmPos], info[0]);
		xmPos += 2;

		/*Restart position*/
		WriteLE16(&xmData[xmPos], 0);
		xmPos += 2;

		/*Number of channels*/
		WriteLE16(&xmData[xmPos], channels);
		xmPos += 2;

		/*Number of patterns*/
		WriteLE16(&xmData[xmPos], info[0]);
		xmPos += 2;

		/*Number of instruments*/
		WriteLE16(&xmData[xmPos], 32);
		xmPos += 2;

		/*Flags: Linear frequency*/
		WriteLE16(&xmData[xmPos], 1);
		xmPos += 2;

		/*Default tempo (ticks)*/
		WriteLE16(&xmData[xmPos], defTicks);
		xmPos += 2;

		/*Default tempo (BPM), always the same for our case*/
		WriteLE16(&xmData[xmPos], bpm);
		xmPos += 2;

		/*Pattern table*/
		for (l = 0; l < info[0]; l++)
		{
			Write8B(&xmData[xmPos], l);
			xmPos++;
		}
		xmPos += (256 - l);

		romPos = info[1];
		/*Check to see if pattern data follows usual or Tomb Raider format*/
		if (ReadLE16(&romData[romPos]) > headerOffset && ReadLE16(&romData[romPos]) < (headerOffset * 2) && songNum == 1)
		{
			drvVers = GHX_VER_TR;
		}

		switch (drvVers)
		{
		case GHX_VER_STD:
			/*Fall-through*/
		case GHX_VER_TR:
		default:
			GHX_STATUS_NONE = 0x00;
			GHX_STATUS_PROG_CHANGE_VOL = 0x40;
			GHX_STATUS_EFFECT = 0x80;
			break;
		}

		for (curPat = 0; curPat < info[0]; curPat++)
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
			rowsLeft = patRows;
			patSize = 0;

			/*The typical pattern format*/
			if (drvVers == GHX_VER_STD)
			{
				for (index = 0; index < 7; index++)
				{
					pattern[index] = romData[romPos + index];
				}

				/*Get channel information*/
				c1Pos = ReadLE16(&romData[patTable + (pattern[0] * 2)]);
				transpose[0] = pattern[1];
				c2Pos = ReadLE16(&romData[patTable + (pattern[2] * 2)]);
				transpose[1] = pattern[3];
				c3Pos = ReadLE16(&romData[patTable + (pattern[4] * 2)]);
				transpose[2] = pattern[5];
				c4Pos = ReadLE16(&romData[patTable + (pattern[6] * 2)]);
				romPos += 7;
			}

			/*Alternate pattern format used for Tomb Raider*/
			else
			{
				for (index = 0; index < 11; index++)
				{
					if (index == 0)
					{
						pattern[0] = ReadLE16(&romData[romPos + index]);
					}
					else if (index == 2)
					{
						pattern[1] = romData[romPos + index];
					}
					else if (index == 3)
					{
						pattern[2] = ReadLE16(&romData[romPos + index]);
					}
					else if (index == 5)
					{
						pattern[3] = romData[romPos + index];
					}
					else if (index == 6)
					{
						pattern[4] = ReadLE16(&romData[romPos + index]);
					}
					else if (index == 8)
					{
						pattern[5] = romData[romPos + index];
					}
					else if (index == 9)
					{
						pattern[6] = ReadLE16(&romData[romPos + index]);
					}
				}
				/*Get channel information*/
				c1Pos = pattern[0];
				transpose[0] = pattern[1];
				c2Pos = pattern[2];
				transpose[1] = pattern[3];
				c3Pos = pattern[4];
				transpose[2] = pattern[5];
				c4Pos = pattern[6];
				romPos += 11;
			}

			for (rowsLeft = patRows; rowsLeft > 0; rowsLeft--)
			{

				for (curChan = 0; curChan < 4; curChan++)
				{
					if (curChan == 0)
					{
						tempPos = c1Pos;
					}
					else if (curChan == 1)
					{
						tempPos = c2Pos;
					}
					else if (curChan == 2)
					{
						tempPos = c3Pos;
					}
					else if (curChan == 3)
					{
						tempPos = c4Pos;
					}

					/*Row: [note | $40 = instrument byte follows | $80 = effect byte follows] [instrument] [effect]*/
					command[0] = romData[tempPos];
					command[1] = 0;
					command[2] = 0;
					tempPos++;
					if ((command[0] & GHX_STATUS_PROG_CHANGE_VOL) != 0x00)
					{
						command[1] = romData[tempPos];
						tempPos++;
					}
					if ((command[0] & GHX_STATUS_EFFECT) != 0x00)
					{
						command[2] = romData[tempPos];
						tempPos++;
					}

					if (curChan == 0)
					{
						c1Pos = tempPos;
					}
					else if (curChan == 1)
					{
						c2Pos = tempPos;
					}
					else if (curChan == 2)
					{
						c3Pos = tempPos;
					}
					else if (curChan == 3)
					{
						c4Pos = tempPos;
					}

					/*Work out the channel mask*/
					chanMask = 0x80;
					curNote = 0;
					curInst = 0;
					curVol = -1;
					lowNibble = 0;
					highNibble = 0;

					/*Note (bits 0-5): 1 = C-2, which is XM note 25*/
					if ((command[0] & 0x3F) != 0x00)
					{
						curNote = (command[0] & 0x3F) + 36;
						if (curChan != 3)
						{
							curNote += (signed char)transpose[curChan];
						}
						if (curNote < 1)
						{
							curNote = 1;
						}
						else if (curNote > 96)
						{
							curNote = 96;
						}
						chanMask |= 0x01;
					}

					/*Instrument byte: bits 0-5 = instrument + 1 (0 = volume change only), bits 6-7 = volume*/
					if ((command[0] & GHX_STATUS_PROG_CHANGE_VOL) != 0x00)
					{
						curInst = command[1] & 0x3F;
						if (curInst != 0)
						{
							chanMask |= 0x02;
						}

						if (curChan == 2)
						{
							/*Wave channel: 1 = 25%, 2 = 50%, 3 = 100%, 0 = instrument level (or mute without instrument)*/
							switch (command[1] >> 6)
							{
							case 1:
								curVol = 16;
								break;
							case 2:
								curVol = 32;
								break;
							case 3:
								curVol = 64;
								break;
							default:
								if (curInst == 0)
								{
									curVol = 0;
								}
								break;
							}
						}
						else
						{
							/*Envelope volume shifted right by 0/1/2/4: full, 1/2, 1/4, off*/
							switch (command[1] >> 6)
							{
							case 0:
								curVol = 64;
								break;
							case 1:
								curVol = 32;
								break;
							case 2:
								curVol = 16;
								break;
							case 3:
								curVol = 0;
								break;
							}
						}

						if (curVol >= 0)
						{
							curVols[curChan] = curVol;
							chanMask |= 0x04;
						}
					}

					/*Effect byte: low nibble = command, high nibble = parameter; only $F (speed) is used by the driver*/
					if ((command[0] & GHX_STATUS_EFFECT) != 0x00)
					{
						lowNibble = command[2] & 0x0F;
						highNibble = command[2] >> 4;
						if (lowNibble == 0x0F && highNibble != 0x00)
						{
							chanMask |= 0x08 | 0x10;
						}
					}

					/*Write the packed data*/
					Write8B(&xmData[xmPos], chanMask);
					xmPos++;
					patSize++;

					if ((chanMask & 0x01) != 0x00)
					{
						Write8B(&xmData[xmPos], curNote);
						xmPos++;
						patSize++;
					}
					if ((chanMask & 0x02) != 0x00)
					{
						Write8B(&xmData[xmPos], curInst);
						xmPos++;
						patSize++;
					}
					if ((chanMask & 0x04) != 0x00)
					{
						Write8B(&xmData[xmPos], 0x10 + curVol);
						xmPos++;
						patSize++;
					}
					if ((chanMask & 0x08) != 0x00)
					{
						/*XM effect F = set speed*/
						Write8B(&xmData[xmPos], 0x0F);
						xmPos++;
						patSize++;
					}
					if ((chanMask & 0x10) != 0x00)
					{
						Write8B(&xmData[xmPos], highNibble);
						xmPos++;
						patSize++;
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


/*Get the titles for each song (if present)*/
void GHXgetSongTitles(char names[50][21])
{
	long curPos = 0;
	long ptrOffset = 0;
	int k, l = 0;
	for (curPos = 0; curPos < (bankSize * 2); curPos++)
	{
		if (!memcmp(&romData[curPos], songTitle, 4))
		{
			ptrOffset = curPos;
			printf("Song table list: 0x%04X!\n", headerOffset);
			break;
		}
	}
	if (ptrOffset != 0)
	{
		curPos += 4;
		for (k = 0; k < numSongs; k++)
		{
			for (l = 0; l < 20; l++)
			{
				songNames[k][l] = romData[curPos + l];
			}
			songNames[k][20] = '\0';
			curPos += 20;
		}
	}
}