/*Carillon Player (Aleski Eeben)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CARILLON.H"

#define bankSize 16384

FILE* rom, * xm, * data;
long bank;
long offset;
long blockList;
int i, j;
char outfile[1000000];
int songNum;
long songPtr;
long blockPtr;
int chanMask;
long bankAmt;
int foundTable;
int curInst;
int firstPtr;
int multiBanks;
int curBank;

char folderName[100];

unsigned static char* romData;
unsigned static char* xmData;
unsigned static char* endData;
long xmLength;

const unsigned int SongLocs[8] = { 0x00, 0x80, 0x40, 0xC0, 0x20, 0x60, 0xA0, 0xE0 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void Carillonsong2xm(int songNum, long ptr);

void CarillonProc(int bank)
{
	foundTable = 0;
	blockList = 0x4F00;
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

	songNum = 1;

	/*Setup for Hugo songs*/
	if (romData[blockList - bankAmt] == 0x7F && romData[blockList + 1 - bankAmt] == 0x00)
	{
		firstPtr = ReadLE16(&romData[blockList + 2 - bankAmt]) + blockList;
		i = blockList + 1 - bankAmt;

		while ((i + bankAmt) < firstPtr)
		{
			if ((i + bankAmt) < firstPtr)
			{
				songPtr = blockList + ReadBE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Carillonsong2xm(songNum, songPtr);
				songNum++;
				i += 2;
			}

		}
	}
	else
	{
		while (songNum <= 8)
		{
			songPtr = blockList + SongLocs[songNum - 1];

			if (romData[songPtr - bankAmt] != 0x00)
			{
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Carillonsong2xm(songNum, songPtr);
			}
			songNum++;
		}
	}
}

/*Convert the song data to XM*/
void Carillonsong2xm(int songNum, long ptr)
{
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int curBlock = 0;
	int songEnd = 0;
	int seqEnd = 0;
	int curPatVal = 0;
	int curNote = 0;
	long xmPos = 0;
	int channels = 4;
	int defTicks = 7;
	int tempo = 0;
	int bpm = 300;
	int rows = 32;
	int patRows = 32;
	int curRow = 0;
	int effect = 0;
	int curByte = 0;
	long packPos = 0;
	long tempPos = 0;
	int rowsLeft = 0;
	long patSize = 0;
	int numBlocks = 0;
	int parameter = 0;
	int vibWidth = 0;
	int vibRate = 0;
	int vibChan = 0;
	int vibrato = 0;
	int slideUp = 0;
	int slideDown = 0;
	int slideChan = 0;
	int slide = 0;
	int tempoCtrl = 0;
	int patBreak = 0;
	unsigned char row[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

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

		/*Get the total number of blocks*/
		romPos = ptr - bankAmt;
		while (romData[romPos] != 0x00)
		{
			if (romData[romPos] != 0x00)
			{
				numBlocks++;
				romPos++;
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
		WriteLE16(&xmData[xmPos], numBlocks);
		xmPos += 2;

		/*Restart position*/
		WriteLE16(&xmData[xmPos], 0);
		xmPos += 2;

		/*Number of channels*/
		WriteLE16(&xmData[xmPos], channels);
		xmPos += 2;

		/*Number of patterns*/
		WriteLE16(&xmData[xmPos], numBlocks);
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
		for (l = 0; l < numBlocks; l++)
		{
			Write8B(&xmData[xmPos], l);
			xmPos++;
		}
		xmPos += (256 - l);

		romPos = ptr - bankAmt;

		for (curBlock = 0; curBlock < numBlocks; curBlock++)
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

			/*Get the current block*/
			blockPtr = romData[romPos] * 0x100;

			seqPos = blockPtr - bankAmt;

			for (rowsLeft = patRows; rowsLeft > 0; rowsLeft--)
			{
				vibChan = 0;
				slideChan = 0;
				tempoCtrl = 0;
				patBreak = 0;
				/*Get the row information*/
				for (curByte = 0; curByte < 8; curByte++)
				{
					row[curByte] = romData[seqPos + curByte];
				}

				/*Effect/parameter*/
				if (row[7] != 0)
				{
					lowNibble = (row[7] >> 4);
					highNibble = (row[7] & 15);
					parameter = highNibble;

					/*Modulate channel (vibrato)*/
					if (lowNibble == 1)
					{
						if (highNibble == 0)
						{
							vibChan = 0;
						}
						else if (highNibble == 1)
						{
							vibChan = 1;
						}
						else if (highNibble == 2)
						{
							vibChan = 2;
						}
						else if (highNibble == 3)
						{
							vibChan = 3;
						}
						else
						{
							vibChan = 1;
						}
					}

					/*Slide channel*/
					else if (lowNibble == 2)
					{
						if (highNibble == 0)
						{
							slideChan = 0;
						}
						else if (highNibble == 1)
						{
							slideChan = 1;
						}
						else if (highNibble == 2)
						{
							slideChan = 2;
						}
						else if (highNibble == 3)
						{
							slideChan = 3;
						}
						else
						{
							slideChan = 1;
						}
					}

					/*Set vibrato width*/
					else if (lowNibble == 3)
					{
						vibWidth = highNibble;
					}

					/*Set vibrato rate*/
					else if (lowNibble == 4)
					{
						vibRate = highNibble;
					}

					/*Init slide up*/
					else if (lowNibble == 5)
					{
						slideUp = highNibble;
					}

					/*Init slide down*/
					else if (lowNibble == 6)
					{
						slideDown = highNibble;
					}

					/*Set tempo*/
					else if (lowNibble == 7)
					{
						tempoCtrl = 1;
						tempo = highNibble;
					}

					/*Pattern break*/
					else if (lowNibble == 8)
					{
						patBreak = 1;
					}

				}

				/*Channel 1*/
				if (row[0] != 0)
				{
					curNote = (row[0] / 2) + 13;
					lowNibble = (row[1] >> 4);
					curInst = lowNibble + 1;
					if (vibChan == 1)
					{
						vibrato = (vibRate << 4) | vibWidth;

						if (vibrato != 0)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x04);
							Write8B(&xmData[xmPos + 4], vibrato);
							xmPos += 5;
							patSize += 5;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x8B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x04);
							xmPos += 4;
							patSize += 5;
						}
					}
					else if (slideChan == 1)
					{
						if (slideUp > 0)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x01);
							Write8B(&xmData[xmPos + 4], slideUp);
							xmPos += 5;
							patSize += 5;
						}
						else if (slideDown > 1)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x02);
							Write8B(&xmData[xmPos + 4], slideDown);
							xmPos += 5;
							patSize += 5;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x8B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x01);
							xmPos += 4;
							patSize += 4;
						}
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
					if (vibChan == 1)
					{
						vibrato = (vibRate << 4) | vibWidth;
						if (vibrato != 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x04);
							Write8B(&xmData[xmPos + 2], vibrato);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x04);
							xmPos += 2;
							patSize += 2;
						}

					}
					else if (slideChan == 1)
					{
						if (slideUp > 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x01);
							Write8B(&xmData[xmPos + 2], slideUp);
							xmPos += 3;
							patSize += 3;
						}
						else if (slideDown > 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x02);
							Write8B(&xmData[xmPos + 2], slideDown);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x01);
							xmPos += 2;
							patSize += 2;
						}
					}
					else
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}

				}

				/*Channel 2*/
				if (row[2] != 0)
				{
					curNote = (row[2] / 2) + 13;
					lowNibble = (row[3] >> 4);
					curInst = lowNibble + 1;
					if (vibChan == 2)
					{
						vibrato = (vibRate << 4) | vibWidth;

						if (vibrato != 0)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x04);
							Write8B(&xmData[xmPos + 4], vibrato);
							xmPos += 5;
							patSize += 5;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x8B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x04);
							xmPos += 4;
							patSize += 5;
						}
					}
					else if (slideChan == 2)
					{
						if (slideUp > 0)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x01);
							Write8B(&xmData[xmPos + 4], slideUp);
							xmPos += 5;
							patSize += 5;
						}
						else if (slideDown > 1)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x02);
							Write8B(&xmData[xmPos + 4], slideDown);
							xmPos += 5;
							patSize += 5;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x8B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x01);
							xmPos += 4;
							patSize += 4;
						}
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
					if (vibChan == 2)
					{
						vibrato = (vibRate << 4) | vibWidth;
						if (vibrato != 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x04);
							Write8B(&xmData[xmPos + 2], vibrato);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x04);
							xmPos += 2;
							patSize += 2;
						}

					}
					else if (slideChan == 2)
					{
						if (slideUp > 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x01);
							Write8B(&xmData[xmPos + 2], slideUp);
							xmPos += 3;
							patSize += 3;
						}
						else if (slideDown > 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x02);
							Write8B(&xmData[xmPos + 2], slideDown);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x01);
							xmPos += 2;
							patSize += 2;
						}
					}
					else
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
				}

				/*Channel 3*/
				if (row[4] != 0)
				{
					if (row[4] == 0xFF)
					{
						curNote = 25;
					}
					else
					{
						curNote = (row[4] / 2) + 13;
					}

					lowNibble = (row[5] >> 4);
					curInst = lowNibble + 1;
					if (vibChan == 3)
					{
						vibrato = (vibRate << 4) | vibWidth;

						if (vibrato != 0)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x04);
							Write8B(&xmData[xmPos + 4], vibrato);
							xmPos += 5;
							patSize += 5;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x8B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x04);
							xmPos += 4;
							patSize += 5;
						}
					}
					else if (slideChan == 3)
					{
						if (slideUp > 0)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x01);
							Write8B(&xmData[xmPos + 4], slideUp);
							xmPos += 5;
							patSize += 5;
						}
						else if (slideDown > 1)
						{
							Write8B(&xmData[xmPos], 0x9B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x02);
							Write8B(&xmData[xmPos + 4], slideDown);
							xmPos += 5;
							patSize += 5;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x8B);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);
							Write8B(&xmData[xmPos + 3], 0x01);
							xmPos += 4;
							patSize += 4;
						}
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
					if (vibChan == 3)
					{
						vibrato = (vibRate << 4) | vibWidth;
						if (vibrato != 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x04);
							Write8B(&xmData[xmPos + 2], vibrato);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x04);
							xmPos += 2;
							patSize += 2;
						}

					}
					else if (slideChan == 3)
					{
						if (slideUp > 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x01);
							Write8B(&xmData[xmPos + 2], slideUp);
							xmPos += 3;
							patSize += 3;
						}
						else if (slideDown > 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x02);
							Write8B(&xmData[xmPos + 2], slideDown);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x01);
							xmPos += 2;
							patSize += 2;
						}
					}
					else
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
				}

				/*Channel 4*/
				if (row[6] != 0)
				{
					lowNibble = (row[6] >> 4);
					curInst = lowNibble + 1;
					if (tempoCtrl == 1)
					{
						if (tempo == 0)
						{
							Write8B(&xmData[xmPos], 0x8A);
							Write8B(&xmData[xmPos + 1], curInst);
							Write8B(&xmData[xmPos + 2], 0x0F);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x9A);
							Write8B(&xmData[xmPos + 1], curInst);
							Write8B(&xmData[xmPos + 2], 0x0F);
							Write8B(&xmData[xmPos + 3], tempo);
							xmPos += 4;
							patSize += 4;
						}
					}
					else if (patBreak == 1)
					{
						Write8B(&xmData[xmPos], 0x8A);
						Write8B(&xmData[xmPos + 1], curInst);
						Write8B(&xmData[xmPos + 2], 0x0D);
						xmPos += 3;
						patSize += 3;
					}
					else
					{
						Write8B(&xmData[xmPos], 0x82);
						Write8B(&xmData[xmPos + 1], curInst);
						xmPos += 2;
						patSize += 2;
					}

				}
				else
				{
					if (tempoCtrl == 1)
					{
						if (tempo != 0)
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x0F);
							Write8B(&xmData[xmPos + 2], tempo);
							xmPos += 3;
							patSize += 3;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x0F);
							xmPos += 2;
							patSize += 2;
						}

					}
					else if (patBreak == 1)
					{
						Write8B(&xmData[xmPos], 0x88);
						Write8B(&xmData[xmPos + 1], 0x0D);
						xmPos += 2;
						patSize += 2;
					}
					else
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}

				}
				seqPos += 8;
			}
			WriteLE16(&xmData[packPos], patSize);
			romPos++;
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