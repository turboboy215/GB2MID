/*GHX (Shin'en)*/
#include <stdio.h>
#include <string.h>
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
unsigned long seqList[500];
char songNames[50][21];
int multiBanks;
int totalSeqs;
int curBank;

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
void GHXgetMacroList(unsigned long list[], long offset);
void GHXgetSongTitles(char names[50][21]);

/*Function prototypes*/
void GHXProc(int bank)
{
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

	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], magicBytes, 8))
		{
			for (j = i; j < bankSize; j++)
			{
				if (!memcmp(&romData[j], headerBytes, 3))
				{
					headerOffset = j + bankAmt;
					printf("GHX header found at address 0x%04X!\n", headerOffset);
					break;
				}
			}
			if (headerOffset != NULL)
			{
				numSongs = romData[headerOffset - bankAmt + 3];
				if (numSongs == 0)
				{
					numSongs = 1;
				}
				printf("Number of songs: %i\n", numSongs);
				patRows = romData[headerOffset - bankAmt + 4];
				printf("Rows per pattern: %i\n", patRows);
				patTable = ReadLE16(&romData[headerOffset - bankAmt + 6]);
				printf("Sequence data table: 0x%04X\n", patTable);
				insTable = ReadLE16(&romData[headerOffset - bankAmt + 8]);
				printf("Instrument table: 0x%04X\n", insTable);
				songTable = ReadLE16(&romData[headerOffset - bankAmt + 10]);
				printf("Song table: 0x%04X\n", songTable);
				GHXgetMacroList(seqList, patTable);
				GHXgetSongTitles(songNames);

				i = songTable;
				for (songNum = 1; songNum <= numSongs; songNum++)
				{
					printf("\nSong %i:\n", songNum);
					if (songNames[0][0] != '\0')
					{
						printf("Title: %s\n", songNames[songNum - 1]);
					}
					songInfo[0] = romData[i - bankAmt] + 1;
					printf("Number of patterns: %i\n", songInfo[0]);
					songInfo[1] = ReadLE16(&romData[i - bankAmt + 1]);
					printf("Song data address: 0x%04X\n", songInfo[1]);
					songInfo[2] = romData[i - bankAmt + 3] + 1;
					printf("Loop pattern relative to final pattern: %i\n", songInfo[2]);
					songInfo[3] = ReadLE16(&romData[i - bankAmt + 4]);
					printf("Pattern loop address: 0x%04X\n", songInfo[3]);
					if (songInfo[0] < 128)
					{
						GHXsong2xm(songNum, songInfo);
					}
					else
					{
						if (songNum == 3 && songInfo[1] == 0x5187 && bank == 0x0A)
						{
							songInfo[0] = 1;
							GHXsong2xm(songNum, songInfo);
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
			sprintf((char*)&xmData[xmPos], songNames[songNum - 1]);
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

		romPos = info[1] - bankAmt;
		/*Check to see if pattern data follows usual or Tomb Raider format*/
		if (ReadLE16(&romData[romPos]) > headerOffset && ReadLE16(&romData[romPos]) < (headerOffset * 2) && songNum == 1)
		{
			trFix = 1;
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
			if (trFix == 0)
			{
				for (index = 0; index < 7; index++)
				{
					pattern[index] = romData[romPos + index];
				}

				/*Get channel information*/
				c1Pos = seqList[pattern[0]];
				transpose[0] = pattern[1];
				c2Pos = seqList[pattern[2]];
				transpose[1] = pattern[3];
				c3Pos = seqList[pattern[4]];
				transpose[2] = pattern[5];
				c4Pos = seqList[pattern[6]];
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
				c1Pos = pattern[0] - bankAmt;
				transpose[0] = pattern[1];
				c2Pos = pattern[2] - bankAmt;
				transpose[1] = pattern[3];
				c3Pos = pattern[4] - bankAmt;
				transpose[2] = pattern[5];
				c4Pos = pattern[6] - bankAmt;
				romPos += 11;
			}

			for (rowsLeft = patRows; rowsLeft > 0; rowsLeft--)
			{

				for (curChan = 0; curChan < 4; curChan++)
				{
					if (curChan == 0)
					{
						command[0] = romData[c1Pos];
						command[1] = romData[c1Pos + 1];
						command[2] = romData[c1Pos + 2];
					}
					else if (curChan == 1)
					{
						command[0] = romData[c2Pos];
						command[1] = romData[c2Pos + 1];
						command[2] = romData[c2Pos + 2];
					}
					else if (curChan == 2)
					{
						command[0] = romData[c3Pos];
						command[1] = romData[c3Pos + 1];
						command[2] = romData[c3Pos + 2];
					}
					else if (curChan == 3)
					{
						command[0] = romData[c4Pos];
						command[1] = romData[c4Pos + 1];
						command[2] = romData[c4Pos + 2];
					}

					/*Empty row*/
					if (command[0] == 0)
					{
						Write8B(&xmData[xmPos], 0x80);
						if (curChan == 0)
						{
							c1Pos++;
						}
						else if (curChan == 1)
						{
							c2Pos++;
						}
						else if (curChan == 2)
						{
							c3Pos++;
						}
						else if (curChan == 3)
						{
							c4Pos++;
						}
						xmPos++;
						patSize++;
					}

					/*Pitch slide*/
					else if (command[0] > 0 && command[0] < 0x40)
					{
						Write8B(&xmData[xmPos], 0x98);
						Write8B(&xmData[xmPos + 1], 0x01);
						Write8B(&xmData[xmPos + 2], command[0] / 4.5);
						if (curChan == 0)
						{
							c1Pos++;
						}
						else if (curChan == 1)
						{
							c2Pos++;
						}
						else if (curChan == 2)
						{
							c3Pos++;
						}
						else if (curChan == 3)
						{
							c4Pos++;
						}
						xmPos += 3;
						patSize += 3;
					}

					/*Change volume*/
					else if (command[0] == 0x40)
					{
						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);
						Write8B(&xmData[xmPos], 0x98);
						Write8B(&xmData[xmPos + 1], 0x0C);
						if (curChan != 2)
						{
							if (lowNibble == 12)
							{
								lowNibble = 0;
							}
							else if (lowNibble == 8)
							{
								lowNibble = 32;
							}
							else if (lowNibble == 4)
							{
								lowNibble = 48;
							}
							else if (lowNibble == 0)
							{
								lowNibble = 64;
							}
						}
						else
						{
							if (lowNibble == 8)
							{
								lowNibble = 50;
							}
							if (lowNibble == 4)
							{
								lowNibble = 64;
							}
							else if (lowNibble == 3)
							{
								lowNibble = 48;
							}
							else if (lowNibble == 2)
							{
								lowNibble = 32;
							}
							else if (lowNibble == 1)
							{
								lowNibble = 16;
							}
							else if (lowNibble == 0)
							{
								lowNibble = 0;
							}
						}
						Write8B(&xmData[xmPos + 2], lowNibble);
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
						xmPos += 3;
						patSize += 3;
					}

					/*Standard note + instrument*/
					else if (command[0] > 0x40 && command[0] < 0x80)
					{
						curNote = command[0] - 28;
						if (curChan == 0 || curChan == 1 || curChan == 2)
						{
							curNote += transpose[curChan];
						}
						Write8B(&xmData[xmPos], 0x83);
						Write8B(&xmData[xmPos + 1], curNote);
						Write8B(&xmData[xmPos + 2], command[1]);
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
						xmPos += 3;
						patSize += 3;
					}

					/*Tempo (ticks) change*/
					else if (command[0] == 0x80)
					{
						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);
						Write8B(&xmData[xmPos], 0x98);
						Write8B(&xmData[xmPos + 1], 0x0F);
						Write8B(&xmData[xmPos + 2], lowNibble);
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
						xmPos += 3;
						patSize += 3;
					}

					/*High drum note?*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x40;
						if (curChan != 3)
						{
							curNote += transpose[curChan];
						}
						Write8B(&xmData[xmPos], 0x83);
						Write8B(&xmData[xmPos + 1], curNote);
						Write8B(&xmData[xmPos + 2], command[1]);
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
						xmPos += 3;
						patSize += 3;
					}

					/*Note/instrument + tempo (ticks) change*/
					else if (command[0] >= 0xC0)
					{
						curNote = command[0] - 156;
						lowNibble = (command[2] >> 4);
						highNibble = (command[2] & 15);
						if (curChan != 3)
						{
							curNote += transpose[curChan];
						}
						Write8B(&xmData[xmPos], 0x9B);
						Write8B(&xmData[xmPos + 1], curNote);
						Write8B(&xmData[xmPos + 2], command[1]);
						Write8B(&xmData[xmPos + 3], 0x0F);
						Write8B(&xmData[xmPos + 4], lowNibble);

						if (curChan == 0)
						{
							c1Pos += 3;
						}
						else if (curChan == 1)
						{
							c2Pos += 3;
						}
						else if (curChan == 2)
						{
							c3Pos += 3;
						}
						else if (curChan == 3)
						{
							c4Pos += 3;
						}
						xmPos += 5;
						patSize += 5;

					}

				}
				WriteLE16(&xmData[packPos], patSize);
			}

		}


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
	fclose(xm);
}


/*Get the pointers to each sequence*/
void GHXgetMacroList(unsigned long list[], long offset)
{
	int cnt = 0;
	unsigned long curValue;
	unsigned long curValue2;
	long newOffset = offset;
	long offset2 = offset - bankAmt;

	for (cnt = 0; cnt < 500; cnt++)
	{
		curValue = (ReadLE16(&romData[newOffset - bankAmt])) - bankAmt;
		curValue2 = (ReadLE16(&romData[newOffset - bankAmt]));
		if (curValue2 >= bankAmt && curValue2 < (bankAmt * 2))
		{
			list[cnt] = curValue;
			newOffset += 2;
		}
		else
		{
			totalSeqs = cnt;
			break;
		}
	}
}

/*Get the titles for each song (if present)*/
void GHXgetSongTitles(char names[50][21])
{
	long curPos = 0;
	long ptrOffset = 0;
	int k, l = 0;
	for (curPos = 0; curPos < bankSize; curPos++)
	{
		if (!memcmp(&romData[curPos], songTitle, 4))
		{
			ptrOffset = curPos + bankAmt;
			printf("Song table list:  0x%04X!\n", headerOffset);
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