/*Karma Studios*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "KARMA.H"

#define bankSize 16384

FILE* rom, * xm, * data;
long bank;
long offset;
long headerOffset;
int i, j;
char outfile[1000000];
long bankAmt;
int numSongs;
int songNum;
int patRows;
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
void Karmasong2xm(int songNum, long songPtr);

void KarmaProc(int bank)
{
	if (bank < 2)
	{
		bank = 2;
	}
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);
	songNum = 1;
	offset = 0x4000;
	Karmasong2xm(songNum, offset);

	free(romData);
}

/*Convert the song data to XM*/
void Karmasong2xm(int songNum, long songPtr)
{
	int curPat = 0;
	long pattern[16];
	long curPos = 0;
	int index = 0;
	long romPos = 0;
	long xmPos = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	int songLen = 0;
	int numPats = 0;
	int numInst = 0;

	long packPos = 0;
	long tempPos = 0;
	int rowsLeft = 0;
	int curChan = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long patSize = 0;
	int curNote;
	int curVol;
	int curInst;
	int effect;
	int effectAmt;

	int l = 0;

	patRows = 64;

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
		/*Get basic information about the song*/
		romPos = songPtr;
		songLen = romData[romPos];
		numPats = romData[romPos + 1];
		numInst = romData[romPos + 2];
		defTicks = romData[romPos + 3];
		bpm = (0xFFFF / romData[romPos + 4]) / 100;

		romPos += 5;

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
		WriteLE16(&xmData[xmPos], songLen);
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
		WriteLE16(&xmData[xmPos], defTicks);
		xmPos += 2;

		/*Default tempo (BPM), always the same for our case*/
		WriteLE16(&xmData[xmPos], bpm);
		xmPos += 2;

		/*Pattern table*/
		for (l = 0; l < songLen; l++)
		{
			Write8B(&xmData[xmPos], romData[romPos]);
			xmPos++;
			romPos++;
		}
		xmPos += (256 - l);

		/*Advance past instruments*/
		for (l = 0; l < numInst; l++)
		{
			if (romData[romPos] == 0x02)
			{
				romPos += 3;
			}
			else if (romData[romPos] == 0x03)
			{
				romPos += 17;
			}
			else if (romData[romPos] == 0x04)
			{
				romPos += 2;
			}
			else
			{
				romPos += 3;
			}
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
			rowsLeft = patRows;
			patSize = 0;

			for (rowsLeft = patRows; rowsLeft > 0; rowsLeft--)
			{
				for (curChan = 0; curChan < 4; curChan++)
				{
					pattern[0] = romData[romPos];
					pattern[1] = romData[romPos + 1];
					pattern[2] = romData[romPos + 2];
					pattern[3] = romData[romPos + 3];

					curNote = pattern[0];
					if (curNote != 0xFF)
					{
						curNote += 24;

						if (curChan == 2)
						{
							curNote -= 12;
						}
					}

					curVol = pattern[1] * 0.6;

					if (curVol >= 80)
					{
						curVol = 80;
					}
					curInst = pattern[2] & 0x0F;
					effect = (pattern[2] >> 4);

					effectAmt = pattern[3];

					/*Note*/
					if (curNote == 0xFF)
					{
						if (curVol == 0x00)
						{
							if (curInst == 0x00)
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x80);
										xmPos++;
										patSize++;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x88);
										Write8B(&xmData[xmPos + 1], effect);
										xmPos += 2;
										patSize += 2;
									}

								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x90);
										Write8B(&xmData[xmPos + 1], effectAmt);
										xmPos += 2;
										patSize += 2;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x9A);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], effect);
										Write8B(&xmData[xmPos + 3], effectAmt);
										xmPos += 4;
										patSize += 4;
									}

								}
							}
							else
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x82);
										Write8B(&xmData[xmPos + 1], curInst);
										xmPos += 2;
										patSize += 2;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8A);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], effect);
										xmPos += 3;
										patSize += 3;
									}

								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x92);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], effectAmt);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x9A);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], effect);
										Write8B(&xmData[xmPos + 3], effectAmt);
										xmPos += 4;
										patSize += 4;
									}

								}
							}
						}
						else
						{
							if (curInst == 0x00)
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x84);
										Write8B(&xmData[xmPos + 1], curVol);
										xmPos += 2;
										patSize += 2;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8C);
										Write8B(&xmData[xmPos + 1], curVol);
										Write8B(&xmData[xmPos + 2], effect);
										xmPos += 3;
										patSize += 3;
									}

								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x94);
										Write8B(&xmData[xmPos + 1], curVol);
										Write8B(&xmData[xmPos + 2], effectAmt);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x9C);
										Write8B(&xmData[xmPos + 1], curVol);
										Write8B(&xmData[xmPos + 2], effect);
										Write8B(&xmData[xmPos + 3], effectAmt);
										xmPos += 4;
										patSize += 4;
									}

								}
							}
							else
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x86);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], curVol);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8E);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], curVol);
										Write8B(&xmData[xmPos + 3], effect);
										xmPos += 4;
										patSize += 4;
									}

								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x96);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], curVol);
										Write8B(&xmData[xmPos + 3], effectAmt);
										xmPos += 4;
										patSize += 4;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x9E);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], curVol);
										Write8B(&xmData[xmPos + 3], effect);
										Write8B(&xmData[xmPos + 4], effectAmt);
										xmPos += 5;
										patSize += 5;
									}

								}
							}
						}
					}
					else
					{
						if (curVol == 0x00)
						{
							if (curInst == 0x00)
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x81);
										Write8B(&xmData[xmPos + 1], curNote);
										xmPos += 2;
										patSize += 2;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x89);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], effect);
										xmPos += 3;
										patSize += 3;
									}

								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x91);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], effectAmt);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x99);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], effect);
										Write8B(&xmData[xmPos + 3], effectAmt);
										xmPos += 4;
										patSize += 4;
									}

								}
							}
							else
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x83);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInst);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInst);
										Write8B(&xmData[xmPos + 3], effect);
										xmPos += 4;
										patSize += 4;
									}

								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x93);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInst);
										Write8B(&xmData[xmPos + 3], effectAmt);
										xmPos += 4;
										patSize += 4;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x9B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInst);
										Write8B(&xmData[xmPos + 3], effect);
										Write8B(&xmData[xmPos + 4], effectAmt);
										xmPos += 5;
										patSize += 5;
									}

								}
							}
						}
						else
						{
							if (curInst == 0x00)
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x85);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curVol);
										xmPos += 3;
										patSize += 3;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8D);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curVol);
										Write8B(&xmData[xmPos + 3], effect);
										xmPos += 4;
										patSize += 4;
									}

								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x95);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curVol);
										Write8B(&xmData[xmPos + 3], effectAmt);
										xmPos += 4;
										patSize += 4;
									}
									else
									{
										Write8B(&xmData[xmPos], 0x9D);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curVol);
										Write8B(&xmData[xmPos + 3], effect);
										Write8B(&xmData[xmPos + 4], effectAmt);
										xmPos += 5;
										patSize += 5;
									}

								}
							}
							else
							{
								if (effectAmt == 0x00)
								{
									if (effect == 0x00)
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
										Write8B(&xmData[xmPos], 0x8F);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInst);
										Write8B(&xmData[xmPos + 3], curVol);
										Write8B(&xmData[xmPos + 4], effect);
										xmPos += 5;
										patSize += 5;
									}
								}
								else
								{
									if (effect == 0x00)
									{
										Write8B(&xmData[xmPos], 0x97);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInst);
										Write8B(&xmData[xmPos + 3], curVol);
										Write8B(&xmData[xmPos + 4], effectAmt);
										xmPos += 5;
										patSize += 5;
									}
									else
									{
										Write8B(&xmData[xmPos], curNote);
										Write8B(&xmData[xmPos + 1], curInst);
										Write8B(&xmData[xmPos + 2], curVol);
										Write8B(&xmData[xmPos + 3], effect);
										Write8B(&xmData[xmPos + 4], effectAmt);
										xmPos += 5;
										patSize += 5;
									}

								}
							}
						}
					}

					romPos += 4;
				}
				WriteLE16(&xmData[packPos], patSize);
			}
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

		free(xmData);
		fclose(xm);
	}
}