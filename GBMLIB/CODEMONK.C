/*The Code Monkeys*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CODEMONK.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int numSongs;
int songBank;
long bankAmt;
int curInst;
int curVol;
int transpose;

int drvVers;
int cfgPtr;
int exitError;
int fileExit;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;

unsigned char* ctrlMidData;

long midLength;

char* argv3;
char string1[100];
char string2[100];
char CodeMonkcheckStrings[5][100] = { "version=", "numSongs=", "bank=", "start=", "transpose=" };

unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void CodeMonksong2mid(int songNum, long ptr);

void CodeMonkProc(char parameters[4][50])
{
	curInst = 0;
	curVol = 100;
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;
	transpose = 0;
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the driver version value*/
		fgets(string1, 9, cfg);
		if (memcmp(string1, CodeMonkcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		drvVers = strtod(string1, NULL);

		fgets(string1, 3, cfg);
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, CodeMonkcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtod(string1, NULL);
		songNum = 1;

		fgets(string1, 3, cfg);

		if (drvVers <= 3)
		{
			printf("Version: %01X\n", drvVers);
			while (songNum <= numSongs)
			{
				/*Skip the first line*/
				fgets(string1, 10, cfg);

				/*Get the song bank*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, CodeMonkcheckStrings[2], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);

				songBank = strtol(string1, NULL, 16);

				fgets(string1, 3, cfg);

				/*Get the start position of the song*/
				fgets(string1, 7, cfg);
				if (memcmp(string1, CodeMonkcheckStrings[3], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);

				songPtr = strtol(string1, NULL, 16);

				fgets(string1, 3, cfg);

				/*Get the song transpose*/
				fgets(string1, 11, cfg);
				if (memcmp(string1, CodeMonkcheckStrings[4], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 3, cfg);

				transpose = (signed char)strtol(string1, NULL, 16);

				printf("Song %i: 0x%04X, bank: %01X, transpose: %i\n", songNum, songPtr, songBank, transpose);


				fgets(string1, 3, cfg);

				fseek(rom, 0, SEEK_SET);
				romData = (unsigned char*)malloc(bankSize * 2);
				fread(romData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(romData + bankSize, 1, bankSize, rom);

				CodeMonksong2mid(songNum, songPtr);


				songNum++;
			}
		}
		else
		{
			printf("ERROR: Invalid version number!\n");
			exit(1);
		}
		printf("The operation was successfully completed!\n");
		exit(0);
	}
}

/*Convert the song data to MIDI*/
void CodeMonksong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
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
	int chanSpeed = 0;
	unsigned char command[4];
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
	int tempByte2 = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long macro4Pos = 0;
	long macro4Ret = 0;
	int transpose2 = 0;

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
		romPos = ptr;
		if (drvVers == 1 || drvVers == 2)
		{
			trackCnt = 3;
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}
		}
		else
		{
			trackCnt = romData[romPos];
			romPos++;
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				seqPtrs[curTrack] = ReadBE16(&romData[romPos]) + ptr + 3;
				romPos += 2;
			}
		}

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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			inMacro = 0;
			transpose2 = 0;
			rest = 0;

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

			if (drvVers == 1)
			{
				seqPos = seqPtrs[curTrack];
				seqEnd = 0;
				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Rest*/
					if (command[0] == 0x00 || command[0] == 0x01)
					{
						curDelay += (command[1] * 5);
						seqPos += 2;
					}

					/*Play note*/
					else if (command[0] >= 0x02 && command[0] <= 0x7F)
					{
						curNote = command[0] + transpose2 + 36;
						curNoteLen = command[1] * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos += 2;
					}

					/*Instrument commands?*/
					else if (command[0] >= 0x80 && command[0] <= 0x93)
					{
						while (romData[seqPos] < 0x40)
						{
							seqPos++;
						}
					}

					/*Go to macro*/
					else if (command[0] == 0x94)
					{
						if (command[2] != 0x00)
						{
							if (inMacro == 0)
							{
								macro1Pos = ReadLE16(&romData[seqPos + 3]);
								macro1Ret = seqPos + 5;
								inMacro = 1;
								transpose2 = (signed char)command[2];
								seqPos = macro1Pos;
							}
							else if (inMacro == 1)
							{
								macro2Pos = ReadLE16(&romData[seqPos + 3]);
								macro2Ret = seqPos + 5;
								inMacro = 2;
								transpose2 = (signed char)command[2];
								seqPos = macro2Pos;
							}
							else if (inMacro == 2)
							{
								macro3Pos = ReadLE16(&romData[seqPos + 3]);
								macro3Ret = seqPos + 5;
								inMacro = 3;
								transpose2 = (signed char)command[2];
								seqPos = macro3Pos;
							}
							else if (inMacro == 3)
							{
								macro4Pos = ReadLE16(&romData[seqPos + 3]);
								macro4Ret = seqPos + 5;
								inMacro = 4;
								transpose2 = (signed char)command[2];
								seqPos = macro4Pos;
							}
							else
							{
								seqEnd = 1;
							}
						}
						else
						{
							seqPos += 5;
						}
					}

					/*Return from macro*/
					else if (command[0] == 0x95)
					{
						transpose2 = 0;
						if (inMacro == 1)
						{
							inMacro = 0;
							seqPos = macro1Ret;
						}
						else if (inMacro == 2)
						{
							inMacro = 1;
							seqPos = macro2Ret;
						}
						else if (inMacro == 3)
						{
							inMacro = 2;
							seqPos = macro3Ret;
						}
						else if (inMacro == 4)
						{
							inMacro = 3;
							seqPos = macro4Ret;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*End of channel (no loop)*/
					else if (command[0] == 0xFE)
					{
						seqEnd = 1;
					}

					/*End of channel (loop)*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}
			}

			else if (drvVers == 2)
			{
				seqPos = seqPtrs[curTrack];
				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Workaround for Asteroids (Asteroids & Missile Command)*/
					if (songNum == 3 && ptr == 0x202E && seqPos >= 0x6F31)
					{
						seqEnd = 1;
					}

					if (seqPos == 0x4D89)
					{
						seqPos = 0x4D89;
					}

					/*Command 00-7C*/
					if (command[0] <= 0x7C && seqEnd == 0)
					{
						curNote = command[0] + transpose;

						if (command[1] < 0x80)
						{
							curNoteLen = command[1] * 5;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;

							seqPos += 2;
						}
						else
						{
							tempByte = romData[seqPos + 1];
							curNoteLen = (tempByte + ((romData[seqPos + 2] / 2) * 0x100)) * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							seqPos += 3;
						}
						curDelay = 0;
					}

					/*End of channel (no loop)*/
					else if (command[0] == 0x7D)
					{
						seqEnd = 1;
					}

					/*End of channel (loop)*/
					else if (command[0] == 0x7E)
					{
						seqEnd = 1;
					}

					/*Rest*/
					else if (command[0] == 0x7F)
					{
						if (command[1] < 0x80)
						{
							curDelay += ((command[1]) * 5);
							seqPos += 2;
						}
						else
						{
							tempByte = (command[1] << 1) & 0xFF;
							tempByte = tempByte >> 1;
							tempByte2 = command[2] % 2;
							if (tempByte2 != 0)
							{
								tempByte = tempByte | 0x80;
							}
							curDelay += ((tempByte + ((command[2] / 2) * 0x100)) * 5);
							seqPos += 3;
						}
					}

					/*Play note*/
					else if (command[0] >= 0x80)
					{
						curNote = command[0] - 0x80 + transpose;

						if (command[1] < 0x80)
						{
							curNoteLen = command[1] * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							seqPos += 2;
						}
						else
						{
							tempByte = romData[seqPos + 1];
							curNoteLen = (tempByte + ((romData[seqPos + 2] / 2) * 0x100)) * 5;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							seqPos += 3;
						}
						if (romData[seqPos] < 0x80)
						{
							curDelay = command[2] * 5;
							seqPos++;
						}
						else
						{
							tempByte = (romData[seqPos] << 1) & 0xFF;
							tempByte = tempByte >> 1;
							tempByte2 = romData[seqPos + 1] % 2;
							if (tempByte2 != 0)
							{
								tempByte = tempByte | 0x80;
							}
							curDelay = (tempByte + ((romData[seqPos + 1] / 2) * 0x100)) * 5;
							seqPos += 2;
						}

					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}
			}

			else
			{
				seqPos = seqPtrs[curTrack];
				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Rest*/
					if (command[0] <= 0x1D)
					{
						curDelay += ((command[0] + 1) * 5);
						seqPos++;
					}

					/*Rest (long)*/
					else if (command[0] == 0x1E)
					{
						if (command[1] < 0x80)
						{
							rest = command[1] * 5;
							curDelay += (command[1] * 5);
							seqPos += 2;
						}
						else
						{
							rest = ((128 * (command[1] - 0x80)) + command[2]) * 5;
							curDelay += (((128 * (command[1] - 0x80)) + command[2]) * 5);
							seqPos += 3;
						}

					}

					/*Repeat rest?*/
					else if (command[0] == 0x1F)
					{
						curDelay += rest;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x20 && command[0] <= 0x83)
					{
						curNote = command[0] - 0x20 + 12;
						if (curTrack != 3)
						{
							curNote += transpose;
						}
						if (command[1] < 0x80)
						{
							curNoteLen = command[1] * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos += 2;
						}
						else
						{
							curNoteLen = ((128 * (command[1] - 0x80)) + command[2]) * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos += 3;
						}

					}

					/*Play note (no new length)*/
					else if (command[0] >= 0x84 && command[0] <= 0xE7)
					{
						curNote = command[0] - 0x84 + 12;
						if (curTrack != 3)
						{
							curNote += transpose;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Command FB*/
					else if (command[0] == 0xFB)
					{
						seqPos += 2;
					}

					/*Unknown command FC*/
					else if (command[0] == 0xFC)
					{
						seqPos++;
					}

					/*End of channel (1)*/
					else if (command[0] == 0xFE)
					{
						seqEnd = 1;
					}

					/*End of channel (2)*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					/*Unknown command*/
					else
					{
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
		free(romData);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}