/*Winkysoft*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "WINKYSFT.H"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int drvVers;

const unsigned char WinkyNoteVals[240] = {
0, 0, 2, 4, 5, 7, 9, 11, 0, 0, 2, 4, 5, 7, 9, 11,
0, 12,14,16,17,19,21,23, 0, 24,26,28,29,31,33,35,
0, 36,38,40,41,43,45,47, 0, 48,50,52,53,55,57,59,
0, 60,62,64,65,67,69,71, 0, 1, 3, 4, 6, 8, 10,11,
0, 1, 3, 4, 6, 8, 10,11, 0, 1, 3, 4, 6, 8, 10,11,
0, 13,15,16,18,20,22,23, 0, 25,27,28,30,32,34,35,
0, 37,39,40,42,44,46,47, 0, 49,51,52,54,56,58,59,
0, 61,63,64,66,68,70,71, 0, 1, 3, 5, 6, 8, 10,11,
0, 0, 2, 4, 5, 7, 9, 11, 0, 2, 4, 5, 7, 8, 9, 11,
0, 12,14,16,17,19,21,23, 0, 24,26,28,29,31,33,35,
0, 36,38,40,41,43,45,47, 0, 48,50,52,53,55,57,59,
0, 60,62,64,65,67,69,71, 0, 1, 3, 4, 6, 8, 10,11,
0, 1, 3, 4, 6, 8, 10,11, 0, 1, 3, 4, 6, 8, 10,11,
0, 13,15,16,18,20,22,23, 0, 25,27,28,30,32,34,35,
0, 37,39,40,42,44,46,47, 0, 49,51,52,54,56,58,59
};

const unsigned char WinkyMagicBytes[2] = { 0xF1, 0x21 };

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;
long repeatPts[100][3];

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Winkysong2mid(int songNum, long ptrs[4]);

void WinkyProc(int bank)
{
	foundTable = 0;
	curInst = 0;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	for (i = bankAmt; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], WinkyMagicBytes, 2)) && foundTable != 1)
		{
			tablePtrLoc = i + 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;

			if (romData[tablePtrLoc + 4] == 0xCB)
			{
				drvVers = 2;
			}
			else
			{
				drvVers = 1;
			}

			if (bank > 8 && bank <= 16)
			{
				drvVers = 3;
			}
			else if (bank == 3 || bank > 16)
			{
				drvVers = 4;
			}
		}
	}

	if (foundTable == 1)
	{

		i = tableOffset;
		songNum = 1;

		if (drvVers != 1)
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000 && ReadLE16(&romData[i + 4]) == 0x0000)
			{
				seqPtrs[0] = ReadLE16(&romData[i]);
				seqPtrs[1] = ReadLE16(&romData[i + 2]);
				seqPtrs[2] = ReadLE16(&romData[i + 4]);
				seqPtrs[3] = ReadLE16(&romData[i + 6]);
				printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
				printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
				printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
				printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
				Winkysong2mid(songNum, seqPtrs);
				i += 8;
				songNum++;
			}
		}
		else
		{
			firstPtr = ReadLE16(&romData[i]);
			while (i < firstPtr)
			{
				seqPtrs[0] = ReadLE16(&romData[i]);
				seqPtrs[1] = ReadLE16(&romData[i + 2]);
				seqPtrs[2] = 0x0000;
				seqPtrs[3] = ReadLE16(&romData[i + 4]);
				printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
				printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
				printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[2]);
				Winkysong2mid(songNum, seqPtrs);
				i += 6;
				songNum++;
			}
		}

		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void Winkysong2mid(int songNum, long ptrs[4])
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
	int transpose = 0;
	int repeat1 = -1;
	long repeat1Pos = 0;
	int repeat2 = -1;
	long repeat2Pos = 0;
	int repeat3 = -1;
	long repeat3Pos = 0;
	unsigned char command[2];
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
	int repeat = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int repeatPt = 0;
	int curReptPt = 0;
	int repLevel = 0;
	int repPts[8][3];


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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;
			curDelay = 0;
			ctrlDelay = 0;
			repeat1 = -1;
			repeat2 = -1;
			curReptPt = 0;
			repLevel = 0;

			if (drvVers == 4)
			{
				repeat1Pos = -1;
				repeat2Pos = -1;
			}

			if (drvVers == 4)
			{
				for (k = 0; k < 8; k++)
				{
					repPts[k][0] = 0;
					repPts[k][1] = -1;
					repPts[k][2] = 0;
				}
			}


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

			if (seqPtrs[curTrack] != 0x0000)
			{
				seqPos = seqPtrs[curTrack];
				seqEnd = 0;
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];

				if (seqPos == 0x4D8E)
				{
					seqPos = 0x4D8E;
				}

				/*Play note*/
				if (command[0] <= 0xEF)
				{
					/*Rest*/
					if (command[0] == 0x00)
					{
						curDelay += (command[1] * 5);
						ctrlDelay += (command[1] * 5);
						seqPos += 2;
					}
					else
					{
						curNote = WinkyNoteVals[command[0]] + 24;
						curNoteLen = command[1] * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos += 2;
						ctrlDelay += curNoteLen;

					}
				}

				/*Set note size*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				/*Unknown command F4*/
				else if (command[0] == 0xF4)
				{
					seqPos += 2;
				}

				/*Set duty*/
				else if (command[0] == 0xF5)
				{
					seqPos += 2;
				}

				/*Set envelope*/
				else if (command[0] == 0xF6)
				{
					seqPos += 2;
				}

				/*Unknown command F7*/
				else if (command[0] == 0xF7)
				{
					seqPos += 2;
				}

				/*Unknown command FB*/
				else if (command[0] == 0xFB)
				{
					seqPos += 2;
				}

				/*End of repeat section*/
				else if (command[0] == 0xFC)
				{
					if (drvVers == 2)
					{
						if (repeat2 == -1)
						{
							if (repeat1 > 0)
							{
								seqPos = repeat1Pos;
								repeat1--;
							}
							else if (repeat1 <= 0)
							{
								repeat1 = -1;
								seqPos += 2;
							}
						}
						else if (repeat2 > -1)
						{
							if (repeat2 > 0)
							{
								seqPos = repeat2Pos;
								repeat2--;
							}
							else if (repeat2 <= 0)
							{
								repeat2 = -1;
								seqPos += 2;
							}
						}
					}
					else if (drvVers == 1)
					{
						if (repeat2 == -1)
						{
							if (repeat1 > 1)
							{
								seqPos = repeat1Pos;
								repeat1--;
							}
							else if (repeat1 <= 1)
							{
								repeat1 = -1;
								seqPos += 2;
							}
						}
						else if (repeat2 > -1)
						{
							if (repeat2 > 1)
							{
								seqPos = repeat2Pos;
								repeat2--;
							}
							else if (repeat2 <= 1)
							{
								repeat2 = -1;
								seqPos += 2;
							}
						}
					}
					else if (drvVers == 3)
					{
						if (repeat1 == -1)
						{
							repeat1Pos = seqPos + 2;
							seqPos += 2;
						}
						else
						{
							repeat2Pos = seqPos + 2;
							seqPos += 2;
						}
					}
					else
					{
						if (repPts[repLevel][1] == -1)
						{
							repPts[repLevel][1] = command[1];
						}
						else if (repPts[repLevel][1] > 1)
						{
							repPts[repLevel][1]--;
							seqPos = repPts[repLevel][0];
						}
						else
						{
							repPts[repLevel][1] = -1;
							seqPos += 2;

							if (repLevel > 0)
							{
								repLevel--;
							}
						}

					}
				}

				/*Repeat the following section*/
				else if (command[0] == 0xFD)
				{
					if (drvVers < 3)
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pos = seqPos + 2;
							seqPos += 2;
						}
						else
						{
							repeat2 = command[1];
							repeat2Pos = seqPos + 2;
							seqPos += 2;
						}
					}
					else if (drvVers == 3)
					{
						if (repeat2 == -1)
						{
							if (repeat1 == -1)
							{
								repeat1 = command[1];
							}
							else if (repeat1 > 1)
							{
								seqPos = repeat1Pos;
								repeat1--;
							}
							else
							{
								repeat1 = -1;
								seqPos += 2;
							}
						}
						else if (repeat2 > -1)
						{
							if (repeat2 == -1)
							{
								repeat2 = command[1];
							}
							else if (repeat2 > 1)
							{
								seqPos = repeat2Pos;
								repeat2--;
							}
							else
							{
								repeat2 = -1;
								seqPos += 2;
							}
						}
					}
					else
					{
						repLevel++;
						repPts[repLevel][0] = seqPos + 2;
						seqPos += 2;
					}

				}

				/*End of channel*/
				else if (command[0] == 0xFE)
				{
					seqEnd = 1;
				}

				/*Unknown command*/
				else
				{
					seqPos += 2;
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
		free(midData);
		free(ctrlMidData);
		fclose(mid);

	}
}