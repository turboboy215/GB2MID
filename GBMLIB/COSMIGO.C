/*Cosmigo*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "COSMIGO.H"

#define bankSize 16384

FILE* rom, * xm, * data;
long bank;
long offset;
long songTable;
long seqTable;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int numRows;
int tempo;
int volume;
int isSFX;
int isMargo;
int CosCurInt[4];
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
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
void Cosmigosong2xm(int songNum, long ptr, int numRows, int tempo, int volume);

void CosmigoProc(int bank)
{
	foundTable = 0;
	isSFX = 0;
	isMargo = 0;
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

	if (ReadLE16(&romData[0x000C]) < 0x8000)
	{
		songTable = ReadLE16(&romData[0x000C]);
		printf("Song table: 0x%04X\n", songTable);
		seqTable = ReadLE16(&romData[0x000E]);
		printf("Sequence pointer table: 0x%04X\n", seqTable);
		foundTable = 1;
	}
	else
	{
		if (songTable = ReadLE16(&romData[0x0021]) == 0x4EF8)
		{
			isMargo = 1;
			songTable = ReadLE16(&romData[0x0021]);
			printf("Song table: 0x%04X\n", songTable);
			seqTable = ReadLE16(&romData[0x00DA]);
			printf("Sequence pointer table: 0x%04X\n", seqTable);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = songTable - bankAmt;
		songNum = 1;
		if (isMargo == 0)
		{
			while ((ReadLE16(&romData[i]) >= bankAmt) && ((ReadLE16(&romData[i]) < (bankSize * 2))))
			{
				songPtr = ReadLE16(&romData[i]);
				numRows = romData[i + 2];
				tempo = romData[i + 3];
				volume = romData[i + 4];
				printf("Song %i: 0x%04X, rows: %i, tempo: %i, volume: %i\n", songNum, songPtr, numRows, tempo, volume);
				if ((ReadLE16(&romData[i + 5]) - ReadLE16(&romData[i]) < 14) && songNum < 33)
				{
					isSFX = 1;
				}
				else
				{
					isSFX = 0;
				}
				if (numRows != 0 && romData[songPtr - bankAmt] < 0xFE)
				{
					Cosmigosong2xm(songNum, songPtr, numRows, tempo, volume);
				}
				songNum++;
				i += 5;
			}
		}
		else
		{
			while (songNum <= 7)
			{
				songPtr = ReadLE16(&romData[i]);
				numRows = romData[i + 2];
				tempo = romData[i + 3];
				volume = romData[i + 4];
				printf("Song %i: 0x%04X, rows: %i, tempo: %i, volume: %i\n", songNum, songPtr, numRows, tempo, volume);
				if (ReadLE16(&romData[i + 5]) - ReadLE16(&romData[i]) < 14)
				{
					isSFX = 1;
				}
				else
				{
					isSFX = 0;
				}
				if (numRows != 0 && romData[songPtr - bankAmt] < 0xFE)
				{
					Cosmigosong2xm(songNum, songPtr, numRows, tempo, volume);
				}

				songNum++;
				i += 5;
			}
		}
	}
	else
	{
		printf("ERROR: Song table not found!\n");
		exit(-1);
	}
}

/*Convert the song data to XM*/
void Cosmigosong2xm(int songNum, long ptr, int numRows, int tempo, int volume)
{
	int curPat = 0;
	long pattern[7];
	unsigned char command[3];
	long curPos = 0;
	int index = 0;
	int curSeq = 0;
	long romPos = 0;
	long seqPos = 0;
	long xmPos = 0;
	int curTrack = 0;
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	int emptySeq[4] = { 0, 0, 0, 0 };
	int seqEnd = 0;
	int songEnd = 1;
	int curNote = 0;
	int transpose[4] = { 0, 0, 0, 0 };
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	long packPos = 0;
	long tempPos = 0;
	long patSize = 0;
	int rowsLeft = 0;
	int patterns = 0;
	CosCurInt[0] = 1;
	CosCurInt[1] = 1;
	CosCurInt[2] = 1;
	CosCurInt[3] = 1;
	int mask = 0;

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

		/*Get the number of patterns*/
		if (isSFX == 1)
		{
			patterns = 1;
		}
		else
		{
			patterns = 0;
			romPos = ptr - bankAmt;

			while (romData[romPos] < 0xFE)
			{
				if (romData[romPos] < 0xFE)
				{
					patterns++;
					romPos += 7;
				}
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
		WriteLE16(&xmData[xmPos], patterns);
		xmPos += 2;

		/*Restart position*/
		WriteLE16(&xmData[xmPos], 0);
		xmPos += 2;

		/*Number of channels*/
		WriteLE16(&xmData[xmPos], channels);
		xmPos += 2;

		/*Number of patterns*/
		WriteLE16(&xmData[xmPos], patterns);
		xmPos += 2;

		/*Number of instruments*/
		WriteLE16(&xmData[xmPos], 32);
		xmPos += 2;

		/*Flags: Linear frequency*/
		WriteLE16(&xmData[xmPos], 1);
		xmPos += 2;

		/*Default tempo (ticks)*/
		WriteLE16(&xmData[xmPos], tempo);
		xmPos += 2;

		/*Default tempo (BPM), always the same for our case*/
		WriteLE16(&xmData[xmPos], bpm);
		xmPos += 2;

		/*Pattern table*/
		if (songNum == 33)
		{
			songNum = 33;
		}
		for (l = 0; l < patterns; l++)
		{
			Write8B(&xmData[xmPos], l);
			xmPos++;
		}
		xmPos += (256 - l);

		romPos = ptr - bankAmt;

		for (curPat = 0; curPat < patterns; curPat++)
		{
			/*First, pattern header*/
			/*Pattern header length*/
			WriteLE32(&xmData[xmPos], 9);
			xmPos += 4;

			/*Packing type = 0*/
			Write8B(&xmData[xmPos], 0);
			xmPos++;

			/*Number of rows*/
			WriteLE16(&xmData[xmPos], numRows);
			xmPos += 2;

			/*Packed pattern data - fill in later*/
			packPos = xmPos;
			WriteLE16(&xmData[xmPos], 0);
			xmPos += 2;

			/*Now the actual pattern data...*/
			rowsLeft = numRows;
			patSize = 0;

			for (index = 0; index < 7; index++)
			{
				pattern[index] = romData[romPos + index];
			}

			/*Get channel information*/
			if (pattern[0] < 0xFD)
			{
				emptySeq[0] = 0;
				c1Pos = ReadLE16(&romData[seqTable - bankAmt + (2 * pattern[0])]) - bankAmt;
			}
			else
			{
				emptySeq[0] = 1;
			}
			transpose[0] = (signed char)pattern[1];
			if (pattern[2] < 0xFD)
			{
				emptySeq[1] = 0;
				c2Pos = ReadLE16(&romData[seqTable - bankAmt + (2 * pattern[2])]) - bankAmt;
			}
			else
			{
				emptySeq[1] = 1;
			}
			transpose[1] = (signed char)pattern[3];
			if (pattern[4] < 0xFD)
			{
				emptySeq[2] = 0;
				c3Pos = ReadLE16(&romData[seqTable - bankAmt + (2 * pattern[4])]) - bankAmt;
			}
			else
			{
				emptySeq[2] = 1;
			}
			transpose[2] = (signed char)pattern[5];
			if (pattern[6] < 0xFD)
			{
				emptySeq[3] = 0;
				c4Pos = ReadLE16(&romData[seqTable - bankAmt + (2 * pattern[6])]) - bankAmt;
			}
			else
			{
				emptySeq[3] = 1;
			}

			for (rowsLeft = numRows; rowsLeft > 0; rowsLeft--)
			{
				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (curTrack == 0)
					{
						if (emptySeq[0] == 0)
						{
							command[0] = romData[c1Pos];
							command[1] = romData[c1Pos + 1];
							command[2] = romData[c1Pos + 2];

							curNote = command[2];

							if (curNote >= 0x80 && curNote < 0xFD)
							{
								curNote -= 0x80;
							}
							if (command[0] != 0x00)
							{
								CosCurInt[0] = command[0];
							}

							if (curNote < 0xFD)
							{
								if (command[1] == 0)
								{
									curNote += 25;
									curNote += transpose[0];
									Write8B(&xmData[xmPos], 0x83);
									Write8B(&xmData[xmPos + 1], curNote);
									Write8B(&xmData[xmPos + 2], CosCurInt[0]);
									xmPos += 3;
									patSize += 3;
								}
								else
								{
									lowNibble = (command[1] >> 4);
									highNibble = (command[1] & 15);

									if (highNibble != 0)
									{
										Write8B(&xmData[xmPos], 0x9B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], CosCurInt[0]);
										Write8B(&xmData[xmPos + 3], lowNibble);
										Write8B(&xmData[xmPos + 4], highNibble);
										xmPos += 5;
										patSize += 5;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], CosCurInt[0]);
										Write8B(&xmData[xmPos + 3], lowNibble);
										xmPos += 5;
										patSize += 5;
									}
								}

							}
							else if (curNote == 0xFD)
							{
								curNote = 0x61;
								Write8B(&xmData[xmPos], 0x81);
								Write8B(&xmData[xmPos + 1], curNote);
								xmPos += 2;
								patSize += 2;
							}
							else if (curNote == 0xFE)
							{
								if (command[1] == 0)
								{
									Write8B(&xmData[xmPos], 0x80);
									xmPos++;
									patSize++;
								}
								else
								{
									lowNibble = (command[1] >> 4);
									highNibble = (command[1] & 15);

									if (highNibble != 0)
									{
										Write8B(&xmData[xmPos], 0x98);
										Write8B(&xmData[xmPos + 1], lowNibble);
										Write8B(&xmData[xmPos + 2], highNibble);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x88);
										Write8B(&xmData[xmPos + 1], lowNibble);
										xmPos += 2;
										patSize += 2;
									}

								}

							}

						}
						else
						{
							Write8B(&xmData[xmPos], 0x80);
							xmPos++;
							patSize++;
						}
						c1Pos += 3;
					}
					else if (curTrack == 1)
					{
						if (emptySeq[1] == 0)
						{
							command[0] = romData[c2Pos];
							command[1] = romData[c2Pos + 1];
							command[2] = romData[c2Pos + 2];

							curNote = command[2];
							if (curNote >= 0x80 && curNote < 0xFD)
							{
								curNote -= 0x80;
							}
							if (command[0] != 0x00)
							{
								CosCurInt[1] = command[0];
							}

							if (curNote < 0xFD)
							{
								if (command[1] == 0)
								{
									curNote += 25;
									curNote += transpose[0];
									Write8B(&xmData[xmPos], 0x83);
									Write8B(&xmData[xmPos + 1], curNote);
									Write8B(&xmData[xmPos + 2], CosCurInt[1]);
									xmPos += 3;
									patSize += 3;
								}
								else
								{
									lowNibble = (command[1] >> 4);
									highNibble = (command[1] & 15);

									if (highNibble != 0)
									{
										Write8B(&xmData[xmPos], 0x9B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], CosCurInt[1]);
										Write8B(&xmData[xmPos + 3], lowNibble);
										Write8B(&xmData[xmPos + 4], highNibble);
										xmPos += 5;
										patSize += 5;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], CosCurInt[1]);
										Write8B(&xmData[xmPos + 3], lowNibble);
										xmPos += 5;
										patSize += 5;
									}
								}

							}
							else if (curNote == 0xFD)
							{
								curNote = 0x61;
								Write8B(&xmData[xmPos], 0x81);
								Write8B(&xmData[xmPos + 1], curNote);
								xmPos += 2;
								patSize += 2;
							}
							else if (curNote == 0xFE)
							{
								if (command[1] == 0)
								{
									Write8B(&xmData[xmPos], 0x80);
									xmPos++;
									patSize++;
								}
								else
								{
									lowNibble = (command[1] >> 4);
									highNibble = (command[1] & 15);

									if (highNibble != 0)
									{
										Write8B(&xmData[xmPos], 0x98);
										Write8B(&xmData[xmPos + 1], lowNibble);
										Write8B(&xmData[xmPos + 2], highNibble);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x88);
										Write8B(&xmData[xmPos + 1], lowNibble);
										xmPos += 2;
										patSize += 2;
									}

								}

							}
						}
						else
						{
							Write8B(&xmData[xmPos], 0x80);
							xmPos++;
							patSize++;
						}
						c2Pos += 3;
					}
					else if (curTrack == 2)
					{
						if (emptySeq[2] == 0)
						{
							command[0] = romData[c3Pos];
							command[1] = romData[c3Pos + 1];
							command[2] = romData[c3Pos + 2];

							curNote = command[2];
							if (curNote >= 0x80 && curNote < 0xFD)
							{
								curNote -= 0x80;
							}
							if (command[0] != 0x00)
							{
								CosCurInt[2] = command[0];
							}

							if (curNote < 0xFD)
							{
								if (command[1] == 0)
								{
									curNote += 25;

									if (isMargo == 1)
									{
										curNote -= 12;
									}
									curNote += transpose[0];
									Write8B(&xmData[xmPos], 0x83);
									Write8B(&xmData[xmPos + 1], curNote);
									Write8B(&xmData[xmPos + 2], CosCurInt[2]);
									xmPos += 3;
									patSize += 3;
								}
								else
								{
									lowNibble = (command[1] >> 4);
									highNibble = (command[1] & 15);

									if (highNibble != 0)
									{
										Write8B(&xmData[xmPos], 0x9B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], CosCurInt[2]);
										Write8B(&xmData[xmPos + 3], lowNibble);
										Write8B(&xmData[xmPos + 4], highNibble);
										xmPos += 5;
										patSize += 5;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], CosCurInt[2]);
										Write8B(&xmData[xmPos + 3], lowNibble);
										xmPos += 5;
										patSize += 5;
									}
								}

							}
							else if (curNote == 0xFD)
							{
								curNote = 0x61;
								Write8B(&xmData[xmPos], 0x81);
								Write8B(&xmData[xmPos + 1], curNote);
								xmPos += 2;
								patSize += 2;
							}
							else if (curNote == 0xFE)
							{
								if (command[1] == 0)
								{
									Write8B(&xmData[xmPos], 0x80);
									xmPos++;
									patSize++;
								}
								else
								{
									lowNibble = (command[1] >> 4);
									highNibble = (command[1] & 15);

									if (highNibble != 0)
									{
										Write8B(&xmData[xmPos], 0x98);
										Write8B(&xmData[xmPos + 1], lowNibble);
										Write8B(&xmData[xmPos + 2], highNibble);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x88);
										Write8B(&xmData[xmPos + 1], lowNibble);
										xmPos += 2;
										patSize += 2;
									}

								}

							}
						}
						else
						{
							Write8B(&xmData[xmPos], 0x80);
							xmPos++;
							patSize++;
						}
						c3Pos += 3;
					}
					else if (curTrack == 3 && emptySeq[3] == 0)
					{
						if (emptySeq[3] == 0)
						{
							command[0] = romData[c4Pos];
							command[1] = romData[c4Pos + 1];
							command[2] = romData[c4Pos + 2];

							curNote = command[2];
							if (curNote >= 0x80 && curNote < 0xFD)
							{
								curNote -= 0x80;
							}
							if (command[0] != 0x00)
							{
								CosCurInt[3] = command[0];
							}

							if (curNote < 0xFD)
							{
								curNote += 25;
								Write8B(&xmData[xmPos], 0x83);
								Write8B(&xmData[xmPos + 1], curNote);
								Write8B(&xmData[xmPos + 2], CosCurInt[3]);
								xmPos += 3;
								patSize += 3;

							}
							else if (curNote == 0xFD)
							{
								curNote = 0x61;
								Write8B(&xmData[xmPos], 0x81);
								Write8B(&xmData[xmPos + 1], curNote);
								xmPos += 2;
								patSize += 2;
							}
							else if (curNote == 0xFE)
							{
								Write8B(&xmData[xmPos], 0x80);
								xmPos++;
								patSize++;
							}
						}
						else
						{
							Write8B(&xmData[xmPos], 0x80);
							xmPos++;
							patSize++;
						}
						c4Pos += 3;
					}

				}
			}
			WriteLE16(&xmData[packPos], patSize);
			romPos += 7;
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
		fclose(xm);
	}
}