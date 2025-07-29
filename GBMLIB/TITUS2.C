/*Titus (Elmar Krieger)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "TITUSDEC.H"
#include "SQZDEC.H"

#define bankSize 16384

FILE* rom, * xm, * mid, * cfg, * data;
long masterBank;
long songBank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int chanMask;
long bankAmt;
int drvVers;
int numSongs;
long songStart;
long songEnd;
long songBase;
long songLength;
int compressed;
long compLength;
int fileExit;
int exitError;
int curInst;
int curVol;
int noteFlag;
int noteSize;

unsigned char* romData;
unsigned char* songData;
unsigned char* compData;
unsigned char* midData;
unsigned char* ctrlMidData;
unsigned char* xmData;
unsigned char* endData;

long midLength;
long xmLength;

char* argv3;

/*Strings to check in CFG*/
char string1[100];
char string2[100];
char Tit2checkStrings[6][100] = { "numSongs=", "bank=", "base=", "start=", "end=", "comp=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void copyData(unsigned char* source, unsigned char* dest, long base, long dataStart, long dataEnd);
void Tit2song12xm(int songNum);
void Tit2song22mid(int songNum, long songPtr);

void copyData(unsigned char* source, unsigned char* dest, long base, long dataStart, long dataEnd)
{
	int k = dataStart;
	int l = 0;

	if (compressed == 0)
	{
		songLength = dataEnd - dataStart;
		songData = (unsigned char*)malloc(songLength);

		while (k <= dataEnd)
		{
			songData[l] = romData[k - 0x4000];
			k++;
			l++;
		}
	}
	else
	{
		compLength = dataEnd - dataStart;
		compData = (unsigned char*)malloc(compLength);
		songData = (unsigned char*)malloc(bankSize);

		while (k <= dataEnd)
		{
			compData[l] = romData[k - 0x4000];
			k++;
			l++;
		}
	}
}
void Tit2Proc(int bank, char parameters[4][100])
{
	drvVers = 0;
	fileExit = 0;
	exitError = 0;
	noteFlag = 0;
	drvVers = strtoul(parameters[0], NULL, 16);
	masterBank = bank;

	if (drvVers == 1)
	{
		if ((cfg = fopen(parameters[1], "r")) == NULL)
		{
			printf("ERROR: Unable to open configuration file %s!\n", parameters[1]);
			exit(1);
		}
		else
		{
			fileExit = 0;
			exitError = 0;
			songNum = 1;

			/*Get the total number of songs*/
			fgets(string1, 10, cfg);

			if (memcmp(string1, Tit2checkStrings[0], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 3, cfg);
			numSongs = strtod(string1, NULL);
			printf("Total # of songs: %i\n", numSongs);

			/*Repeat for every song*/
			while (fileExit == 0 && exitError == 0)
			{
				if (songNum > numSongs)
				{
					fileExit = 1;
				}

				if (fileExit == 0)
				{
					/*Skip new line*/
					fgets(string1, 2, cfg);
					/*Skip the first line*/
					fgets(string1, 10, cfg);

					/*Get the bank of each song*/
					fgets(string1, 6, cfg);
					if (memcmp(string1, Tit2checkStrings[1], 1))
					{
						exitError = 1;
					}
					fgets(string1, 5, cfg);
					{
						songBank = strtol(string1, NULL, 16);
					}

					printf("Song %i, Bank: %01X\n", songNum, songBank);

					/*Copy the ROM's bank data into RAM*/
					if (songBank != 1)
					{
						bankAmt = bankSize;
						fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
						romData = (unsigned char*)malloc(bankSize);
						fread(romData, 1, bankSize, rom);
					}

					else
					{
						bankAmt = 0;
						fseek(rom, ((songBank - 1) * bankSize * 2), SEEK_SET);
						romData = (unsigned char*)malloc(bankSize * 2);
						fread(romData, 1, bankSize * 2, rom);
					}

					/*Skip new line*/
					fgets(string1, 2, cfg);

					/*Now look for the "base"*/
					fgets(string1, 6, cfg);
					if (memcmp(string1, Tit2checkStrings[2], 1))
					{
						exitError = 1;
					}
					fgets(string1, 5, cfg);
					songBase = strtol(string1, NULL, 16);
					printf("Song %i, Base: 0x%04X\n", songNum, songBase);

					/*Skip new line*/
					fgets(string1, 2, cfg);

					/*Get the "start" of the module*/
					fgets(string1, 7, cfg);
					if (memcmp(string1, Tit2checkStrings[3], 1))
					{
						exitError = 1;
					}
					fgets(string1, 5, cfg);
					songStart = strtol(string1, NULL, 16);
					printf("Song %i, Start: 0x%04X\n", songNum, songStart);

					/*Skip new line*/
					fgets(string1, 2, cfg);

					/*Get the "end" of the module*/
					fgets(string1, 5, cfg);
					if (memcmp(string1, Tit2checkStrings[4], 1))
					{
						exitError = 1;
					}
					fgets(string1, 5, cfg);
					songEnd = strtol(string1, NULL, 16);
					printf("Song %i, End: 0x%04X\n", songNum, songEnd);

					/*Skip new line*/
					fgets(string1, 2, cfg);

					/*Check if the module is compressed*/
					fgets(string1, 6, cfg);
					if (memcmp(string1, Tit2checkStrings[5], 1))
					{
						exitError = 1;
					}
					fgets(string1, 5, cfg);
					compressed = strtol(string1, NULL, 16);

					if (compressed == 0)
					{
						printf("Song %i is uncompressed\n", songNum);
						copyData(romData, songData, songBase, songStart, songEnd);
					}

					else if (compressed == 1)
					{
						printf("Song %i is compressed - SQZ\n", songNum);
						copyData(romData, compData, songBase, songStart, songEnd);
						UnSQZ(compData, songData, compLength);
					}

					else if (compressed == 2)
					{
						printf("Song %i is compressed - Titus LZ\n", songNum);
						copyData(romData, compData, songBase, songStart, songEnd);
						TitusDecomp(compData, songData, compLength);
					}

					else
					{
						exitError = 1;
					}

					Tit2song12xm(songNum);


					songNum++;
				}
			}

		}
	}
	else if (drvVers == 2)
	{
		if (masterBank != 1)
		{
			bankAmt = bankSize;
		}
		else
		{
			bankAmt = 0;
		}
		fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);

		tableOffset = 0x000C;
		songNum = 1;
		i = tableOffset;

		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) <= (bankSize * 2))
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Tit2song22mid(songNum, songPtr);
			i += 2;
			songNum++;
		}

		/*Convert addditional compressed songs in Superman*/
		if (songNum == 12 && ReadLE16(&romData[i]) == 0xD000)
		{
			if (parameters[1][0] != 0)
			{
				if ((cfg = fopen(parameters[1], "r")) == NULL)
				{
					printf("ERROR: Unable to open configuration file %s!\n", parameters[1]);
					exit(1);
				}
				else
				{
					fileExit = 0;
					exitError = 0;
					/*Get the total number of (additional) songs*/
					fgets(string1, 10, cfg);

					if (memcmp(string1, Tit2checkStrings[0], 1))
					{
						printf("ERROR: Invalid CFG data!\n");
						exit(1);

					}
					fgets(string1, 3, cfg);
					numSongs = strtod(string1, NULL);
					/*Repeat for every song*/
					while (fileExit == 0 && exitError == 0)
					{
						/*Skip new line*/
						fgets(string1, 2, cfg);
						/*Skip the first line*/
						fgets(string1, 10, cfg);

						/*Get the bank of each song*/
						fgets(string1, 6, cfg);
						if (memcmp(string1, Tit2checkStrings[1], 1))
						{
							exitError = 1;
						}
						fgets(string1, 5, cfg);
						{
							songBank = strtol(string1, NULL, 16);
						}

						printf("Song %i, Bank: %01X\n", songNum, songBank);

						/*Copy the ROM's bank data into RAM*/
						if (songBank != 1)
						{
							bankAmt = bankSize;
							fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
							romData = (unsigned char*)malloc(bankSize);
							fread(romData, 1, bankSize, rom);
						}

						else
						{
							bankAmt = 0;
							fseek(rom, ((songBank - 1) * bankSize * 2), SEEK_SET);
							romData = (unsigned char*)malloc(bankSize * 2);
							fread(romData, 1, bankSize * 2, rom);
						}

						/*Skip new line*/
						fgets(string1, 2, cfg);

						/*Now look for the "base"*/
						fgets(string1, 6, cfg);
						if (memcmp(string1, Tit2checkStrings[2], 1))
						{
							exitError = 1;
						}
						fgets(string1, 5, cfg);
						songBase = strtol(string1, NULL, 16);
						printf("Song %i, Base: 0x%04X\n", songNum, songBase);

						/*Skip new line*/
						fgets(string1, 2, cfg);

						/*Get the "start" of the module*/
						fgets(string1, 7, cfg);
						if (memcmp(string1, Tit2checkStrings[3], 1))
						{
							exitError = 1;
						}
						fgets(string1, 5, cfg);
						songStart = strtol(string1, NULL, 16);
						printf("Song %i, Start: 0x%04X\n", songNum, songStart);

						/*Skip new line*/
						fgets(string1, 2, cfg);

						/*Get the "end" of the module*/
						fgets(string1, 5, cfg);
						if (memcmp(string1, Tit2checkStrings[4], 1))
						{
							exitError = 1;
						}
						fgets(string1, 5, cfg);
						songEnd = strtol(string1, NULL, 16);
						printf("Song %i, End: 0x%04X\n", songNum, songEnd);

						/*Skip new line*/
						fgets(string1, 2, cfg);

						/*Check if the module is compressed*/
						fgets(string1, 6, cfg);
						if (memcmp(string1, Tit2checkStrings[5], 1))
						{
							exitError = 1;
						}
						fgets(string1, 5, cfg);
						compressed = strtol(string1, NULL, 16);

						if (compressed == 0)
						{
							printf("Song %i is uncompressed\n", songNum);
							copyData(romData, songData, songBase, songStart, songEnd);
						}

						else if (compressed == 1)
						{
							printf("Song %i is compressed - SQZ\n", songNum);
							copyData(romData, compData, songBase, songStart, songEnd);
							UnSQZ(compData, songData, compLength);
						}

						else if (compressed == 2)
						{
							printf("Song %i is compressed - Titus LZ\n", songNum);
							copyData(romData, compData, songBase, songStart, songEnd);
							TitusDecomp(compData, songData, compLength);
						}

						else
						{
							exitError = 1;
						}

						bankAmt = 0xD000;
						songPtr = 0xD000;

						Tit2song22mid(songNum, songPtr);

						if (songNum < 15)
						{
							songNum++;
						}
						else
						{
							fileExit = 1;
						}


					}
				}
			}

		}

	}
	else
	{
		printf("ERROR: Invalid driver mode!\n");
		exit(2);
	}

	free(romData);
}

void Tit2song12xm(int songNum)
{
	int curPat = 0;
	long pattern[3];
	unsigned char command[4];
	long curPos = 0;
	int index = 0;
	int curSeq = 0;
	signed int skipRows[3] = { 0, 0, 0 };
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	long romPos = 0;
	long xmPos = 0;
	int channels = 3;
	int defTicks = 6;
	int bpm = 120;
	long packPos = 0;
	long tempPos = 0;
	int rowsLeft = 0;
	int curChan = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long patSize = 0;
	long patRows = 0;
	int curNote;
	int numPat = 0;
	int loopPos = 0;
	int curTrack = 0;
	int highestPat = 0;
	int addRows = 0;

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

		/*Get information about the song data*/
		for (curTrack = 0; curTrack < 3; curTrack++)
		{
			seqPtrs[curTrack] = 0;
		}
		numPat = songData[0x78];
		loopPos = songData[0x79];
		defTicks = songData[0x7A];
		patRows = songData[0x7B];

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
		WriteLE16(&xmData[xmPos], numPat);
		xmPos += 2;

		/*Restart position*/
		WriteLE16(&xmData[xmPos], 0);
		xmPos += 2;

		/*Number of channels*/
		WriteLE16(&xmData[xmPos], channels);
		xmPos += 2;

		/*Number of patterns*/
		WriteLE16(&xmData[xmPos], numPat);
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

		highestPat = 0;

		/*Pattern table*/
		for (l = 0; l < numPat; l++)
		{
			curPat = songData[0x30 + l];
			if (curPat >= highestPat)
			{
				highestPat = curPat;
			}
			Write8B(&xmData[xmPos], curPat);
			xmPos++;
		}
		xmPos += (256 - l);

		romPos = songPtr - songBase;

		romPos = 0x100;

		/*Convert the pattern data*/
		for (curPat = 0; curPat <= highestPat; curPat++)
		{
			/*Get each channel's pattern*/
			for (curTrack = 0; curTrack < 3; curTrack++)
			{
				pattern[curTrack] = ReadLE16(&songData[romPos + (curTrack * 2)]);
			}

			c1Pos = pattern[0] - songBase;
			c2Pos = pattern[1] - songBase;
			c3Pos = pattern[2] - songBase;

			romPos += 6;

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
				for (curTrack = 0; curTrack < 3; curTrack++)
				{
					if (curTrack == 0)
					{
						command[0] = songData[c1Pos];
						command[1] = songData[c1Pos + 1];
						command[2] = songData[c1Pos + 2];
						command[3] = songData[c1Pos + 3];
					}
					else if (curTrack == 1)
					{
						command[0] = songData[c2Pos];
						command[1] = songData[c2Pos + 1];
						command[2] = songData[c2Pos + 2];
						command[3] = songData[c2Pos + 3];
					}
					else if (curTrack == 2)
					{
						command[0] = songData[c3Pos];
						command[1] = songData[c3Pos + 1];
						command[2] = songData[c3Pos + 2];
						command[3] = songData[c3Pos + 3];
					}

					if (skipRows[curTrack] > 0)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
						skipRows[curTrack]--;
					}
					else
					{
						/*Play note*/
						if (command[0] < 0x7F)
						{
							curNote = command[0] + 24;

							if (curNote >= 0x60)
							{
								curNote = command[0];
							}

							lowNibble = (command[1] >> 4);
							highNibble = (command[1] & 15);

							curInst = lowNibble + 1;

							Write8B(&xmData[xmPos], 0x83);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);

							if (highNibble != 0)
							{
								if (curTrack == 0)
								{
									c1Pos += 3;
								}
								else if (curTrack == 1)
								{
									c2Pos += 3;
								}
								else if (curTrack == 2)
								{
									c3Pos += 3;
								}
							}
							else if (highNibble == 0)
							{
								if (curTrack == 0)
								{
									c1Pos += 2;
								}
								else if (curTrack == 1)
								{
									c2Pos += 2;
								}
								else if (curTrack == 2)
								{
									c3Pos += 2;
								}
							}
							xmPos += 3;
							patSize += 3;
						}

						/*Set volume*/
						else if (command[0] == 0x7F)
						{
							lowNibble = (command[1] >> 4);
							highNibble = (command[1] & 15);
							curVol = lowNibble;

							if (curVol == 0)
							{
								Write8B(&xmData[xmPos], 0x88);
								Write8B(&xmData[xmPos + 1], 0x0C);
								xmPos += 2;
								patSize += 2;
							}
							else
							{
								Write8B(&xmData[xmPos], 0x98);
								Write8B(&xmData[xmPos + 1], 0x0C);
								Write8B(&xmData[xmPos + 2], curVol);
								xmPos += 3;
								patSize += 3;
							}

							if (curTrack == 0)
							{
								c1Pos += 2;
							}
							else if (curTrack == 1)
							{
								c2Pos += 2;
							}
							else if (curTrack == 2)
							{
								c3Pos += 2;
							}

						}

						/*Skip rows*/
						else if (command[0] >= 0x80)
						{
							skipRows[curTrack] = command[0] - 0x80;
							Write8B(&xmData[xmPos], 0x80);
							xmPos++;
							patSize++;

							if (curTrack == 0)
							{
								c1Pos++;
							}
							else if (curTrack == 1)
							{
								c2Pos++;
							}
							else if (curTrack == 2)
							{
								c3Pos++;
							}
						}
					}

				}
			}
			WriteLE16(&xmData[packPos], patSize);
		}


		/*Add empty patterns if some are repeated*/

		if (highestPat < (numPat - 1))
		{
			addRows = (numPat - 1) - highestPat;
			for (l = 0; l < addRows; l++)
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

void Tit2song22mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int curPat = 0;
	int patNum = 1;
	int noteFlag = 0;


	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
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

		romPos = songPtr - bankAmt;
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			if (compressed == 0)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos + (2 * curTrack)]);
			}
			else
			{
				seqPtrs[curTrack] = ReadLE16(&songData[romPos + (2 * curTrack)]);
			}

		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNote = 1;
			curNote = 36;
			curDelay = 0;
			romPos = seqPtrs[curTrack] - bankAmt;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			while ((compressed == 0) && (ReadLE16(&romData[romPos]) != 0x0000) || (compressed != 0) && (ReadLE16(&songData[romPos]) != 0x0000))
			{
				if (compressed == 0)
				{
					curPat = ReadLE16(&romData[romPos]);
				}
				else
				{
					curPat = ReadLE16(&songData[romPos]);
				}

				seqEnd = 0;
				seqPos = curPat - bankAmt;
				noteFlag = 0;

				while (seqEnd == 0)
				{


					if (curNote >= 0x80)
					{
						curNote = 0x7F;
					}

					if (compressed == 0)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];
						command[3] = romData[seqPos + 3];
						command[4] = romData[seqPos + 4];
						command[5] = romData[seqPos + 5];
						command[6] = romData[seqPos + 6];
						command[7] = romData[seqPos + 8];
					}
					else
					{
						command[0] = songData[seqPos];
						command[1] = songData[seqPos + 1];
						command[2] = songData[seqPos + 2];
						command[3] = songData[seqPos + 3];
						command[4] = songData[seqPos + 4];
						command[5] = songData[seqPos + 5];
						command[6] = songData[seqPos + 6];
						command[7] = songData[seqPos + 8];
					}


					if (command[0] < 0x80)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);

						if (highNibble == 0x0F)
						{
							noteSize = 5;
						}

						else if (highNibble == 0x0E)
						{
							noteSize = 10;
						}

						else if (highNibble == 0x0D)
						{
							noteSize = 15;
						}

						else if (highNibble == 0x0C)
						{
							noteSize = 20;
						}

						else if (highNibble == 0x0B)
						{
							noteSize = 30;
						}

						else if (highNibble == 0x0A)
						{
							noteSize = 40;
						}

						else if (highNibble == 0x09)
						{
							noteSize = 50;
						}

						else if (highNibble == 0x08)
						{
							noteSize = 80;
						}

						else if (highNibble == 0x07)
						{
							noteSize = 90;
						}

						else if (highNibble == 0x06)
						{
							noteSize = 100;
						}

						else if (highNibble == 0x05)
						{
							noteSize = 120;
						}

						else if (highNibble == 0x04)
						{
							noteSize = 150;
						}

						else if (highNibble == 0x03)
						{
							noteSize = 200;
						}

						else if (highNibble == 0x02)
						{
							noteSize = 250;
						}

						else if (highNibble == 0x01)
						{
							noteSize = 300;
						}

						else
						{
							noteSize = 150;
						}

						/*Play note*/
						if (lowNibble == 0)
						{

							if (noteFlag == 1)
							{
								curNote = 36;
								curNoteLen = (command[1] - 0x80) * 5;
								seqPos += 2;
							}
							else
							{
								if (command[1] < 0x80)
								{
									curNote = command[1];
									curNoteLen = (command[2] - 0x80) * 5;
									seqPos += 3;
								}
								else
								{
									if (command[2] < 0x80)
									{
										curNote = command[2];
										curNoteLen = (command[3] - 0x80) * 5;
										seqPos += 4;
									}
									else
									{
										if (command[3] < 0x80)
										{
											curNote = command[3];
											curNoteLen = (command[4] - 0x80) * 5;
											seqPos += 5;
										}
										else
										{
											curNote = command[4];
											curNoteLen = (command[5] - 0x80) * 5;
											seqPos += 6;
										}

									}

								}
							}

							if (curNoteLen < 0)
							{
								curNoteLen = 0;
							}

							if (highNibble == 0)
							{
								noteSize = curNoteLen;
							}

							if (curNoteLen >= noteSize)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
								curDelay = curNoteLen - noteSize;
							}
							else
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
							}
							firstNote = 0;
							midPos = tempPos;
						}

						/*Play note with volume?*/
						else if (lowNibble == 1)
						{
							if (noteFlag == 1)
							{
								curNote = 36;
								curNoteLen = (command[1] - 0x80) * 5;
								seqPos += 2;
							}
							else
							{
								if (command[3] < 0x80)
								{
									curNote = command[3];
									curNoteLen = (command[4] - 0x80) * 5;
									seqPos += 5;
								}
								else
								{
									if (command[4] < 0x80)
									{
										curNote = command[4];
										curNoteLen = (command[5] - 0x80) * 5;
										seqPos += 6;
									}
									else
									{
										curNote = command[5];
										curNoteLen = (command[6] - 0x80) * 5;
										seqPos += 7;
									}

								}
							}

							if (curNoteLen < 0)
							{
								curNoteLen = 0;
							}
							if (highNibble == 0)
							{
								noteSize = curNoteLen;
							}

							if (curNoteLen >= noteSize)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
								curDelay = curNoteLen - noteSize;
							}
							else
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
							}
							firstNote = 0;
							midPos = tempPos;
						}

						/*Play note and change instrument?*/
						else if (lowNibble == 2)
						{
							if (command[1] < 0x60)
							{
								noteFlag = 0;
							}
							else
							{
								noteFlag = 1;
							}
							if (noteFlag == 1)
							{
								curNote = 36;
								curNoteLen = (command[2] - 0x80) * 5;
								seqPos += 3;
							}
							else
							{
								curInst = command[1];
								firstNote = 1;
								if (command[2] >= 0x80)
								{
									if (command[3] >= 0x80)
									{
										curNote = command[4];
										curNoteLen = (command[5] - 0x80) * 5;
										seqPos += 6;
									}
									else
									{
										curNote = command[3];
										curNoteLen = (command[4] - 0x80) * 5;
										seqPos += 5;
									}

								}
								else
								{
									curNote = command[2];
									curNoteLen = (command[3] - 0x80) * 5;
									seqPos += 4;
								}

							}

							if (curNoteLen < 0)
							{
								curNoteLen = 0;
							}
							if (highNibble == 0)
							{
								noteSize = curNoteLen;
							}

							if (curNoteLen >= noteSize)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
								curDelay = curNoteLen - noteSize;
							}
							else
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
							}
							firstNote = 0;
							midPos = tempPos;
						}

						/*Fade effect note?*/
						else if (lowNibble == 3)
						{

							if (noteFlag == 1)
							{
								curNote = 36;
								curNoteLen = (command[4] - 0x80) * 5;
								seqPos += 5;
							}
							else
							{
								if (command[4] < 0x80)
								{
									curNote = command[4];
									curNoteLen = (command[5] - 0x80) * 5;
									seqPos += 6;
								}
								else
								{
									if (command[5] < 0x80)
									{
										curNote = command[5];
										curNoteLen = (command[6] - 0x80) * 5;
										seqPos += 7;
									}
									else
									{
										curNote = command[6];
										curNoteLen = (command[7] - 0x80) * 5;
										seqPos += 8;
									}

								}

							}
							if (curNoteLen < 0)
							{
								curNoteLen = 0;
							}
							if (highNibble == 0)
							{
								noteSize = curNoteLen;
							}

							if (curNoteLen >= noteSize)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
								curDelay = curNoteLen - noteSize;
							}
							else
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
							}
							firstNote = 0;
							midPos = tempPos;
						}

						/*End of channel*/
						else if (lowNibble == 4)
						{
							seqEnd = 1;
							romPos += 2;
						}

						/*Sweep?*/
						else if (lowNibble == 5)
						{
							seqPos += 2;
						}

						/*Unknown command*/
						else
						{
							seqPos++;
						}
					}

					/*Delay*/
					else if (command[0] >= 0x80)
					{
						curDelay += ((command[0] - 0x80) * 5);
						seqPos++;
					}
				}
			}

			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);
		}
		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		trackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], trackSize);

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		if (compData != 0)
		{
			free(songData);
		}
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}