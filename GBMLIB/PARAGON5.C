/*Paragon 5*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "PARAGON5.H"

#define bankSize 16384

FILE* rom, * xm, * data;
long bank;
long tablePtrLoc;
long tableOffset;
long patOffset;
int i, j;
char outfile[1000000];
int songNum;
long bankAmt;
long insPtr;
long songPtr;
int songBank;
int insBank;
int foundTable;
long patSize;
int rowNum;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* xmData;
unsigned char* endData;
unsigned char* songBankData;
unsigned char* patBankData;
long xmLength;

const char TableFind[4] = { 0xCE, 0x00, 0x67, 0x2A };
const char TableFindS11[9] = { 0xE0, 0x79, 0x6F, 0x26, 0x00, 0x29, 0x29, 0x29, 0x11 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void P5song2xm(int songNum, long ptr);
int P5row2xm(int pos, long rowData[20]);
int P5checkRows(int songNum, long ptr);
void P5song2xmMB(int songNum, long ptr, int bank);
void P5Proc(int bank);

void P5Proc(int bank)
{
	bankAmt = bankSize;

	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	if (bank > 1)
	{
		/*Try to search the bank for song pattern table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], TableFind, 4))
			{
				tablePtrLoc = bankAmt + i - 4;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = romData[tablePtrLoc - bankAmt] + (romData[tablePtrLoc + 3 - bankAmt] * 0x100);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				break;
			}
		}

		if (foundTable == 1)
		{
			i = tableOffset - bankAmt;
			songNum = 1;

			while ((ReadLE16(&romData[i]) > bankAmt) && (ReadLE16(&romData[i + 2]) > bankAmt))
			{
				insPtr = ReadLE16(&romData[i]);
				songPtr = ReadLE16(&romData[i + 2]);
				printf("Song %i instruments: 0x%04X\n", songNum, insPtr);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				rowNum = P5checkRows(songNum, songPtr);

				/*Fix for "Mean Mr. Mustard" homebrew*/
				if (bank == 3 && insPtr == 0x57A8 && songPtr == 0x5164 && songNum == 1)
				{
					rowNum = 96;
				}
				P5song2xm(songNum, songPtr);
				free(xmData);
				i += 4;
				songNum++;
			}
			free(romData);
		}

		else
		{
			printf("ERROR: Magic bytes not found!\n");
			exit(-1);
		}
	}

	else if (bank == 1)
	{
		/*Try to search the bank for song pattern table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], TableFindS11, 9))
			{
				tablePtrLoc = i + 9;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				break;
			}
		}

		if (foundTable == 1)
		{
			i = tableOffset;
			songNum = 1;

			while ((ReadLE16(&romData[i + 2]) >= bankAmt) && (ReadLE16(&romData[i + 6]) >= bankAmt))
			{
				insBank = romData[i];
				insPtr = ReadLE16(&romData[i + 2]);
				songBank = romData[i + 4];
				songPtr = ReadLE16(&romData[i + 6]);
				printf("Song %i instruments bank: %01X\n", songNum, insBank);
				printf("Song %i instruments: 0x%04X\n", songNum, insPtr);
				printf("Song %i bank: %01X\n", songNum, songBank);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				P5song2xmMB(songNum, songPtr, songBank);
				free(xmData);
				printf("\n");
				i += 8;
				songNum++;
			}
			free(romData);
		}

		else
		{
			printf("ERROR: Magic bytes not found!\n");
			exit(-1);
		}
	}
}

void P5song2xm(int songNum, long ptr)
{
	int songPos = 0;
	int patPos = 0;
	long curPat = 0;
	long curBank1 = 0;
	int patNum = 1;
	int patEnd = 0;
	long command[3];
	long row[20];
	int curNote = 0;
	int curChan = 0;
	int curSpeed = 0;
	int curIns = 0;
	int tempVal = 0;
	int breakRows = 0;
	int numPats = 0;
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	long xmPos = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	int rowsLeft = 0;
	int rows = 64;
	int l;
	int curPatNum = 0;
	int patWrite = 1;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	unsigned char mask = 0;
	int rowCount = 0;
	int emptyRows = 0;
	long packPos = 0;

	l = 0;
	patSize = 0;

	xmLength = 0x10000;
	xmData = ((unsigned char*)malloc(xmLength));

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	for (l = 0; l < xmLength; l++)
	{
		xmData[l] = 0;
	}

	for (l = 0; l < 20; l++)
	{
		row[l] = 0;
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
		/*Determine number of patterns*/
		songPos = ptr - bankAmt;
		numPats = 0;
		while (ReadLE16(&romData[songPos]) > bankAmt)
		{
			numPats++;
			songPos += 4;
		}

		numPats--;


		/*Write the header*/
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


		/*Copy the pattern data*/
		songPos = ptr - bankAmt;

		for (curPatNum = 0; curPatNum < numPats; curPatNum++)
		{
			curPat = ReadLE16(&romData[songPos]);
			curBank1 = ReadLE16(&romData[songPos + 2]);

			patPos = curPat - bankAmt;
			patEnd = 0;
			rowsLeft = 64;
			patSize = 0;
			rows = rowNum;

			for (rowCount = 0; rowCount < 20; rowCount++)
			{
				row[rowCount] = 0;
			}

			/*First, pattern header*/
			/*Pattern header length*/
			WriteLE32(&xmData[xmPos], 9);
			xmPos += 4;

			/*Packing type = 0*/
			Write8B(&xmData[xmPos], 0);
			xmPos++;

			/*Number of rows*/
			WriteLE16(&xmData[xmPos], rows);
			xmPos += 2;

			/*Packed pattern data - fill in later*/
			packPos = xmPos;
			WriteLE16(&xmData[xmPos], 0);
			xmPos += 2;

			/*Now the actual pattern data...*/
			while (patEnd == 0)
			{
				/*Copy data from current row into array*/
				command[0] = romData[patPos];
				command[1] = romData[patPos + 1];
				command[2] = romData[patPos + 2];

				/*Break pattern OR following rows are empty*/
				if (command[0] == 0x00)
				{

					if (command[2] == 0x07)
					{
						xmPos = P5row2xm(xmPos, row);
						breakRows = ((command[1] + 1) / curSpeed) - 1;
						if (breakRows > 0)
						{
							for (l = 0; l < breakRows; l++)
							{
								Write8B(&xmData[xmPos], 0x80);
								Write8B(&xmData[xmPos + 1], 0x80);
								Write8B(&xmData[xmPos + 2], 0x80);
								Write8B(&xmData[xmPos + 3], 0x80);
								xmPos += 4;
								patSize += 4;
							}
							row[18] = 0x0D;
							row[19] = 0x00;
							xmPos = P5row2xm(xmPos, row);
						}
						else
						{
							row[18] = 0x0D;
							row[19] = 0x00;
							xmPos = P5row2xm(xmPos, row);
						}

					}

					else if (command[1] < (curSpeed * 4))
					{
						row[18] = 0x0F;
						row[19] = command[1];
						xmPos = P5row2xm(xmPos, row);
					}

					else
					{
						xmPos = P5row2xm(xmPos, row);
						breakRows = ((command[1] + 1) / curSpeed) - 1;
						for (l = 0; l < breakRows; l++)
						{
							Write8B(&xmData[xmPos], 0x80);
							Write8B(&xmData[xmPos + 1], 0x80);
							Write8B(&xmData[xmPos + 2], 0x80);
							Write8B(&xmData[xmPos + 3], 0x80);
							xmPos += 4;
							patSize += 4;
						}
					}
					patPos += 2;
				}

				/*Play note, instrument*/
				else if (command[0] == 0x01)
				{
					curNote = command[1] + 0x19;

					if (((command[2] - 4) % 4 == 0))
					{
						curIns = (command[2] - 4) / 4;
						curChan = 1;
						curIns++;
						row[0] = curNote;
						row[1] = curIns;
					}

					else if (((command[2] - 5) % 4 == 0))
					{
						curIns = (command[2] - 5) / 4;
						curChan = 2;
						curIns++;
						row[5] = curNote;
						row[6] = curIns;
					}

					else if (((command[2] - 6) % 4 == 0))
					{
						curIns = (command[2] - 6) / 4;
						curChan = 3;
						curIns++;
						row[10] = curNote;
						row[11] = curIns;
					}

					else if (((command[2] - 3) % 4 == 0))
					{
						curIns = (command[2] - 3) / 4;
						curChan = 4;
						curIns++;
						row[15] = curNote;
						row[16] = curIns;
					}

					patPos += 3;
				}


				/*Play instrument OR note end*/
				else if (command[0] == 0x02)
				{
					if (command[1] < 0x04)
					{
						curChan = command[1] + 1;
						if (curChan == 1)
						{
							row[0] = 0x61;
						}
						else if (curChan == 2)
						{
							row[5] = 0x61;
						}
						else if (curChan == 3)
						{
							row[10] = 0x61;
						}
						else if (curChan == 4)
						{
							row[15] = 0x61;
						}
					}

					else if (command[1] >= 0x04)
					{
						if (((command[1] - 4) % 4 == 0))
						{
							curIns = (command[1] - 4) / 4;
							curChan = 1;
							curIns++;
							row[1] = curIns;
						}

						else if (((command[1] - 5) % 4 == 0))
						{
							curIns = (command[1] - 5) / 4;
							curChan = 2;
							curIns++;
							row[6] = curIns;
						}

						else if (((command[1] - 6) % 4 == 0))
						{
							curIns = (command[1] - 6) / 4;
							curChan = 3;
							curIns++;
							row[11] = curIns;
						}

						else if (((command[1] - 3) % 4 == 0))
						{
							curIns = (command[1] - 3) / 4;
							curChan = 4;
							curIns++;
							row[16] = curIns;
						}
					}

					patPos += 2;
				}

				/*Set starting volume of envelope*/
				else if (command[0] == 0x03)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;

					if (curChan == 1)
					{
						row[2] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[7] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[12] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[17] = lowNibble;
					}
					patPos += 2;
				}

				/*Vibrato*/
				else if (command[0] == 0x04)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;

					if (curChan == 1)
					{
						row[3] = 0x04;
						row[4] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[8] = 0x04;
						row[9] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[13] = 0x04;
						row[14] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[18] = 0x04;
						row[19] = lowNibble;
					}

					patPos += 2;
				}

				/*Effects off*/
				else if (command[0] == 0x05)
				{
					curChan = command[1] + 1;
					patPos += 2;
				}

				/*Flag command (for game-specific data)*/
				else if (command[0] == 0x06)
				{
					patPos += 2;
				}

				/*End of pattern*/
				else if (command[0] == 0x07)
				{
					patEnd = 1;
				}

				/*End loop data*/
				else if (command[0] == 0x08)
				{
					patPos += 3;
				}

				/*Go to order*/
				else if (command[0] == 0x09)
				{
					patEnd = 1;
				}

				/*Pattern speed*/
				else if (command[0] == 0x0A)
				{
					curSpeed = command[1];
					row[18] = 0x0F;
					row[19] = curSpeed;
					patPos += 2;
				}

				/*End of row*/
				else if (command[0] == 0x0B)
				{
					xmPos = P5row2xm(xmPos, row);
					patPos++;
				}

				/*Next row is empty*/
				else if (command[0] == 0x0C)
				{
					xmPos = P5row2xm(xmPos, row);

					for (emptyRows = 0; emptyRows < 4; emptyRows++)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
					patPos++;
				}

				/*Next 2 rows are empty*/
				else if (command[0] == 0x0D)
				{
					xmPos = P5row2xm(xmPos, row);

					for (emptyRows = 0; emptyRows < 8; emptyRows++)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
					patPos++;
				}

				/*Next 3 rows are empty*/
				else if (command[0] == 0x0E)
				{
					xmPos = P5row2xm(xmPos, row);

					for (emptyRows = 0; emptyRows < 12; emptyRows++)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
					patPos++;
				}

				/*Pitch slide down*/
				else if (command[0] == 0x0F)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;
					if (curChan == 1)
					{
						row[3] = 0x02;
						row[4] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[8] = 0x02;
						row[9] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[13] = 0x02;
						row[14] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[18] = 0x02;
						row[19] = lowNibble;
					}
					patPos += 2;
				}

				/*Pitch slide up*/
				else if (command[0] == 0x10)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;
					if (curChan == 1)
					{
						row[3] = 0x01;
						row[4] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[8] = 0x01;
						row[9] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[13] = 0x01;
						row[14] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[18] = 0x01;
						row[19] = lowNibble;
					}
					patPos += 2;
				}


				/*Arpeggio with halftones X and Y*/
				else if (command[0] == 0x11)
				{
					curChan = command[2] + 1;
					if (curChan == 1)
					{
						row[3] = 0x00;
						row[4] = command[1];
					}
					else if (curChan == 2)
					{
						row[8] = 0x00;
						row[9] = command[1];
					}
					else if (curChan == 3)
					{
						row[13] = 0x00;
						row[14] = command[1];
					}
					else if (curChan == 4)
					{
						row[18] = 0x00;
						row[19] = command[1];
					}
					patPos += 3;
				}

				/*Set pulse width OR set volume (channel 3)*/
				else if (command[0] == 0x12)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;

					if (curChan < 3)
					{
						;
					}

					else if (curChan == 3)
					{
						if (lowNibble == 0)
						{
							row[12] = 0x00;
						}

						else if (lowNibble == 1)
						{
							row[12] = 0x10;
						}

						else if (lowNibble == 2)
						{
							row[12] = 0x25;
						}

						else if (lowNibble == 3)
						{
							row[12] = 0x30;
						}

						else if (lowNibble == 4)
						{
							row[12] = 0x50;
						}
						row[12] = lowNibble;
					}

					patPos += 2;
				}

				else if (command[0] == 0xFF)
				{
					patEnd = 1;
				}

				/*Unknown command*/
				else
				{
					patPos += 2;
				}

			}

			songPos += 4;
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
		fclose(xm);
	}
}

int P5row2xm(int pos, long rowData[20])
{
	unsigned char mask = 0x80;
	int maskList[5];
	int maskIndex = 0;
	int chan = 0;
	int clearVal = 0;
	int newPos = 0;

	/*Get channel "mask"*/
	/*0 = Note, 1 = Instrument, 2 = Volume, 3 = Effect type, 4 = Effect amount*/

	for (chan = 0; chan < 4; chan++)
	{
		mask = 0x80;
		for (maskIndex = 0; maskIndex < 5; maskIndex++)
		{
			maskList[maskIndex] = 0;
		}

		for (maskIndex = 0; maskIndex < 5; maskIndex++)
		{
			newPos = maskIndex + (chan * 5);
			if (rowData[maskIndex + (chan * 5)] != 0)
			{
				maskList[maskIndex] = 1;
			}
		}

		for (maskIndex = 0; maskIndex < 5; maskIndex++)
		{
			if (maskList[maskIndex] != 0)
			{
				if (maskIndex == 0)
				{
					mask = mask | 0x01;
				}
				else if (maskIndex == 1)
				{
					mask = mask | 0x02;
				}
				else if (maskIndex == 2)
				{
					mask = mask | 0x04;
				}
				else if (maskIndex == 3)
				{
					mask = mask | 0x08;
				}
				else if (maskIndex == 4)
				{
					mask = mask | 0x10;
				}
			}
		}

		/*Write the event "mask"*/
		if (mask != 0x9F)
		{
			Write8B(&xmData[pos], mask);
			pos++;
			patSize++;
		}

		/*Play note*/
		if (maskList[0] != 0x00)
		{
			Write8B(&xmData[pos], rowData[0 + (chan * 5)]);
			pos++;
			patSize++;
		}

		/*Instrument*/
		if (maskList[1] != 0x00)
		{
			Write8B(&xmData[pos], rowData[1 + (chan * 5)]);
			pos++;
			patSize++;
		}

		/*Volume*/
		if (maskList[2] != 0x00)
		{
			Write8B(&xmData[pos], rowData[2 + (chan * 5)]);
			pos++;
			patSize++;
		}

		/*Effect*/
		if (maskList[3] != 0x00 || maskList[4] != 0x00)
		{
			if (rowData[3 + (chan * 5)] != 0x00 && rowData[4 + (chan * 5)] != 0x00)
			{
				Write8B(&xmData[pos], rowData[3 + (chan * 5)]);
				Write8B(&xmData[pos + 1], rowData[4 + (chan * 5)]);
				pos += 2;
				patSize += 2;
			}
			else if (rowData[3 + (chan * 5)] == 0x00 && rowData[4 + (chan * 5)] != 0x00)
			{
				Write8B(&xmData[pos], rowData[4 + (chan * 5)]);
				pos++;
				patSize++;
			}
			else if (rowData[3 + (chan * 5)] != 0x00 && rowData[4 + (chan * 5)] == 0x00)
			{
				Write8B(&xmData[pos], rowData[3 + (chan * 5)]);
				pos++;
				patSize++;
			}

		}
	}

	/*Clear the row data*/
	for (clearVal = 0; clearVal < 20; clearVal++)
	{
		rowData[clearVal] = 0;
	}

	return pos;
}


/*Check to see if the pattern size is over 64*/
int P5checkRows(int songNum, long ptr)
{
	int songPos = 0;
	int curPat = 0;
	int curBank1 = 0;
	long command[3];
	int rowNum = 0;
	int patEnd = 0;
	int patPos = 0;
	int breakRows = 0;
	int curSpeed = 0;

	songPos = ptr - bankAmt;
	curPat = ReadLE16(&romData[songPos]);
	curBank1 = ReadLE16(&romData[songPos + 2]);
	patPos = curPat - bankAmt;

	while (patEnd == 0)
	{
		command[0] = romData[patPos];
		command[1] = romData[patPos + 1];
		command[2] = romData[patPos + 2];
		/*Break pattern OR following rows are empty*/
		if (command[0] == 0x00)
		{

			if (command[2] == 0x07)
			{
				breakRows = ((command[1] + 1) / curSpeed) - 1;
				if (breakRows > 0)
				{
					rowNum += breakRows;
					patEnd = 1;
				}
				else
				{
					patEnd = 0;
				}

			}

			else if (command[1] < (curSpeed * 4))
			{
				;
			}

			else
			{
				rowNum += breakRows;
			}
			patPos += 2;
		}

		/*Play note, instrument*/
		else if (command[0] == 0x01)
		{
			patPos += 3;
		}

		/*Play instrument OR note end*/
		else if (command[0] == 0x02)
		{
			patPos += 2;
		}

		/*Set starting volume of envelope*/
		else if (command[0] == 0x03)
		{
			patPos += 2;
		}

		/*Vibrato*/
		else if (command[0] == 0x04)
		{
			patPos += 2;
		}

		/*Effects off*/
		else if (command[0] == 0x05)
		{
			patPos += 2;
		}

		/*Flag command (for game-specific data)*/
		else if (command[0] == 0x06)
		{
			patPos += 2;
		}

		/*End of pattern*/
		else if (command[0] == 0x07)
		{
			patEnd = 1;
		}

		/*End loop data*/
		else if (command[0] == 0x08)
		{
			patPos += 3;
		}

		/*Go to order*/
		else if (command[0] == 0x09)
		{
			patEnd = 1;
		}

		/*Pattern speed*/
		else if (command[0] == 0x0A)
		{
			curSpeed = command[1];
			patPos += 2;
		}

		/*End of row*/
		else if (command[0] == 0x0B)
		{
			rowNum++;
			patPos++;
		}

		/*Next row is empty*/
		else if (command[0] == 0x0C)
		{
			rowNum += 2;
			patPos++;
		}

		/*Next 2 rows are empty*/
		else if (command[0] == 0x0D)
		{
			rowNum += 3;
			patPos++;
		}

		/*Next 3 rows are empty*/
		else if (command[0] == 0x0E)
		{
			rowNum += 4;
			patPos++;
		}

		/*Pitch slide down*/
		else if (command[0] == 0x0F)
		{
			patPos += 2;
		}

		/*Pitch slide up*/
		else if (command[0] == 0x10)
		{
			patPos += 2;
		}

		/*Arpeggio with halftones X and Y*/
		else if (command[0] == 0x11)
		{
			patPos += 3;
		}

		/*Set pulse width OR set volume (channel 3)*/
		else if (command[0] == 0x12)
		{
			patPos += 2;
		}

		else if (command[0] == 0xFF)
		{
			patEnd = 1;
		}

		/*Unknown command*/
		else
		{
			patPos += 2;
		}
	}

	if (rowNum <= 65)
	{
		return 64;
	}
	else if (rowNum > 65 && rowNum < 127)
	{
		return 96;
	}
	else if (rowNum >= 127)
	{
		return 128;
	}
}

void P5song2xmMB(int songNum, long ptr, int bank)
{
	int songPos = 0;
	int patPos = 0;
	long curPat = 0;
	long patBank = 0;
	long curBank1 = 0;
	int patNum = 1;
	int patEnd = 0;
	long command[3];
	long row[20];
	int curNote = 0;
	int curChan = 0;
	int curSpeed = 0;
	int curIns = 0;
	int tempVal = 0;
	int breakRows = 0;
	int numPats = 0;
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	long xmPos = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	int rowsLeft = 0;
	int rows = 64;
	int l;
	int curPatNum = 0;
	int patWrite = 1;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	unsigned char mask = 0;
	int rowCount = 0;
	int emptyRows = 0;
	long packPos = 0;

	l = 0;
	patSize = 0;

	xmLength = 0x10000;
	xmData = ((unsigned char*)malloc(xmLength));

	for (l = 0; l < xmLength; l++)
	{
		xmData[l] = 0;
	}

	for (l = 0; l < 20; l++)
	{
		row[l] = 0;
	}


	sprintf(outfile, "song%d.xm", songNum);
	if ((xm = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.xm!\n", songNum);
		exit(2);
	}
	else
	{
		patBank = songBank;
		/*Get the current bank*/
		fseek(rom, ((patBank)*bankSize), SEEK_SET);
		songBankData = (unsigned char*)malloc(bankSize);
		fread(songBankData, 1, bankSize, rom);

		/*Determine number of patterns*/
		songPos = ptr - bankAmt;
		numPats = 0;
		while (ReadLE16(&songBankData[songPos]) >= bankAmt)
		{
			numPats++;
			songPos += 4;
		}

		numPats--;


		/*Write the header*/
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


		/*Copy the pattern data*/
		songPos = ptr - bankAmt;

		for (curPatNum = 0; curPatNum < numPats; curPatNum++)
		{
			curPat = ReadLE16(&songBankData[songPos]);
			curBank1 = ReadLE16(&songBankData[songPos + 2]);

			/*Copy pattern data over to memory from set bank*/
			fseek(rom, ((curBank1)*bankSize), SEEK_SET);
			patBankData = (unsigned char*)malloc(bankSize);
			fread(patBankData, 1, bankSize, rom);

			patPos = curPat - bankAmt;
			patEnd = 0;
			patSize = 0;
			rows = rowNum;

			for (rowCount = 0; rowCount < 20; rowCount++)
			{
				row[rowCount] = 0;
			}

			/*First, pattern header*/
			/*Pattern header length*/
			WriteLE32(&xmData[xmPos], 9);
			xmPos += 4;

			/*Packing type = 0*/
			Write8B(&xmData[xmPos], 0);
			xmPos++;

			/*Number of rows*/
			WriteLE16(&xmData[xmPos], rows);
			xmPos += 2;

			/*Packed pattern data - fill in later*/
			packPos = xmPos;
			WriteLE16(&xmData[xmPos], 0);
			xmPos += 2;

			/*Now the actual pattern data...*/
			while (patEnd == 0)
			{
				/*Copy data from current row into array*/
				command[0] = patBankData[patPos];
				command[1] = patBankData[patPos + 1];
				command[2] = patBankData[patPos + 2];

				/*Break pattern OR following rows are empty*/
				if (command[0] == 0x00)
				{

					if (command[2] == 0x07)
					{
						xmPos = P5row2xm(xmPos, row);
						breakRows = ((command[1] + 1) / curSpeed) - 1;
						if (breakRows > 0)
						{
							for (l = 0; l < breakRows; l++)
							{
								Write8B(&xmData[xmPos], 0x80);
								Write8B(&xmData[xmPos + 1], 0x80);
								Write8B(&xmData[xmPos + 2], 0x80);
								Write8B(&xmData[xmPos + 3], 0x80);
								xmPos += 4;
								patSize += 4;
							}
							row[18] = 0x0D;
							row[19] = 0x00;
							xmPos = P5row2xm(xmPos, row);
						}
						else
						{
							row[18] = 0x0D;
							row[19] = 0x00;
							xmPos = P5row2xm(xmPos, row);
						}

					}

					else if (command[1] < (curSpeed * 4))
					{
						row[18] = 0x0F;
						row[19] = command[1];
						xmPos = P5row2xm(xmPos, row);
					}

					else
					{
						xmPos = P5row2xm(xmPos, row);
						breakRows = ((command[1] + 1) / curSpeed) - 1;
						for (l = 0; l < breakRows; l++)
						{
							Write8B(&xmData[xmPos], 0x80);
							Write8B(&xmData[xmPos + 1], 0x80);
							Write8B(&xmData[xmPos + 2], 0x80);
							Write8B(&xmData[xmPos + 3], 0x80);
							xmPos += 4;
							patSize += 4;
						}
					}
					patPos += 2;
				}

				/*Play note, instrument*/
				else if (command[0] == 0x01)
				{
					curNote = command[1] + 0x19;

					if (((command[2] - 4) % 4 == 0))
					{
						curIns = (command[2] - 4) / 4;
						curChan = 1;
						curIns++;
						row[0] = curNote;
						row[1] = curIns;
					}

					else if (((command[2] - 5) % 4 == 0))
					{
						curIns = (command[2] - 5) / 4;
						curChan = 2;
						curIns++;
						row[5] = curNote;
						row[6] = curIns;
					}

					else if (((command[2] - 6) % 4 == 0))
					{
						curIns = (command[2] - 6) / 4;
						curChan = 3;
						curIns++;
						row[10] = curNote;
						row[11] = curIns;
					}

					else if (((command[2] - 3) % 4 == 0))
					{
						curIns = (command[2] - 3) / 4;
						curChan = 4;
						curIns++;
						row[15] = curNote;
						row[16] = curIns;
					}

					patPos += 3;
				}


				/*Play instrument OR note end*/
				else if (command[0] == 0x02)
				{
					if (command[1] < 0x04)
					{
						curChan = command[1] + 1;
						if (curChan == 1)
						{
							row[0] = 0x61;
						}
						else if (curChan == 2)
						{
							row[5] = 0x61;
						}
						else if (curChan == 3)
						{
							row[10] = 0x61;
						}
						else if (curChan == 4)
						{
							row[15] = 0x61;
						}
					}

					else if (command[1] >= 0x04)
					{
						if (((command[1] - 4) % 4 == 0))
						{
							curIns = (command[1] - 4) / 4;
							curChan = 1;
							curIns++;
							row[1] = curIns;
						}

						else if (((command[1] - 5) % 4 == 0))
						{
							curIns = (command[1] - 5) / 4;
							curChan = 2;
							curIns++;
							row[6] = curIns;
						}

						else if (((command[1] - 6) % 4 == 0))
						{
							curIns = (command[1] - 6) / 4;
							curChan = 3;
							curIns++;
							row[11] = curIns;
						}

						else if (((command[1] - 3) % 4 == 0))
						{
							curIns = (command[1] - 3) / 4;
							curChan = 4;
							curIns++;
							row[16] = curIns;
						}
					}

					patPos += 2;
				}

				/*Set starting volume of envelope*/
				else if (command[0] == 0x03)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;

					if (curChan == 1)
					{
						row[2] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[7] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[12] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[17] = lowNibble;
					}
					patPos += 2;
				}

				/*Vibrato*/
				else if (command[0] == 0x04)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;

					if (curChan == 1)
					{
						row[3] = 0x04;
						row[4] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[8] = 0x04;
						row[9] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[13] = 0x04;
						row[14] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[18] = 0x04;
						row[19] = lowNibble;
					}

					patPos += 2;
				}

				/*Effects off*/
				else if (command[0] == 0x05)
				{
					curChan = command[1] + 1;
					patPos += 2;
				}

				/*Flag command (for game-specific data)*/
				else if (command[0] == 0x06)
				{
					patPos += 2;
				}

				/*End of pattern*/
				else if (command[0] == 0x07)
				{
					patEnd = 1;
				}

				/*End loop data*/
				else if (command[0] == 0x08)
				{
					patPos += 3;
				}

				/*Go to order*/
				else if (command[0] == 0x09)
				{
					patEnd = 1;
				}

				/*Pattern speed*/
				else if (command[0] == 0x0A)
				{
					curSpeed = command[1];
					row[18] = 0x0F;
					row[19] = curSpeed;
					patPos += 2;
				}

				/*End of row*/
				else if (command[0] == 0x0B)
				{
					xmPos = P5row2xm(xmPos, row);
					patPos++;
				}

				/*Next row is empty*/
				else if (command[0] == 0x0C)
				{
					xmPos = P5row2xm(xmPos, row);

					for (emptyRows = 0; emptyRows < 4; emptyRows++)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
					patPos++;
				}

				/*Next 2 rows are empty*/
				else if (command[0] == 0x0D)
				{
					xmPos = P5row2xm(xmPos, row);

					for (emptyRows = 0; emptyRows < 8; emptyRows++)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
					patPos++;
				}

				/*Next 3 rows are empty*/
				else if (command[0] == 0x0E)
				{
					xmPos = P5row2xm(xmPos, row);

					for (emptyRows = 0; emptyRows < 12; emptyRows++)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
					}
					patPos++;
				}

				/*Pitch slide down*/
				else if (command[0] == 0x0F)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;
					if (curChan == 1)
					{
						row[3] = 0x02;
						row[4] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[8] = 0x02;
						row[9] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[13] = 0x02;
						row[14] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[18] = 0x02;
						row[19] = lowNibble;
					}
					patPos += 2;
				}

				/*Pitch slide up*/
				else if (command[0] == 0x10)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;
					if (curChan == 1)
					{
						row[3] = 0x01;
						row[4] = lowNibble;
					}
					else if (curChan == 2)
					{
						row[8] = 0x01;
						row[9] = lowNibble;
					}
					else if (curChan == 3)
					{
						row[13] = 0x01;
						row[14] = lowNibble;
					}
					else if (curChan == 4)
					{
						row[18] = 0x01;
						row[19] = lowNibble;
					}
					patPos += 2;
				}


				/*Arpeggio with halftones X and Y*/
				else if (command[0] == 0x11)
				{
					curChan = command[2] + 1;
					if (curChan == 1)
					{
						row[3] = 0x00;
						row[4] = command[1];
					}
					else if (curChan == 2)
					{
						row[8] = 0x00;
						row[9] = command[1];
					}
					else if (curChan == 3)
					{
						row[13] = 0x00;
						row[14] = command[1];
					}
					else if (curChan == 4)
					{
						row[18] = 0x00;
						row[19] = command[1];
					}
					patPos += 3;
				}

				/*Set pulse width OR set volume (channel 3)*/
				else if (command[0] == 0x12)
				{
					lowNibble = (command[1] >> 4);
					highNibble = (command[1] & 15);
					curChan = highNibble + 1;

					if (curChan < 3)
					{
						;
					}

					else if (curChan == 3)
					{
						if (lowNibble == 0)
						{
							row[12] = 0x00;
						}

						else if (lowNibble == 1)
						{
							row[12] = 0x10;
						}

						else if (lowNibble == 2)
						{
							row[12] = 0x25;
						}

						else if (lowNibble == 3)
						{
							row[12] = 0x30;
						}

						else if (lowNibble == 4)
						{
							row[12] = 0x50;
						}
						row[12] = lowNibble;
					}

					patPos += 2;
				}

				else if (command[0] == 0xFF)
				{
					patEnd = 1;
				}

				/*Unknown command*/
				else
				{
					patPos += 2;
				}

			}

			songPos += 4;
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
		fclose(xm);
	}
}