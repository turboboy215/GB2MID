/*Factor 5 (early - Probotector 2/Animaniacs)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "FACTOR5.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long seqPtrs[4];
long songPtr;
int numSongs;
int songBank;
long bankAmt;
int curInst;
int F5curInsts[4] = { 0, 0, 0, 0 };
long firstPtr;
int drvVers;
int foundTable;

const char F5MagicBytesA[8] = { 0xE0, 0x24, 0xE0, 0x25, 0xE0, 0x26, 0xC9, 0x21 };
const char F5MagicBytesB[13] = { 0xE0, 0x24, 0xE0, 0x25, 0xE0, 0x26, 0xC9, 0xF5, 0xCD, 0x55, 0x45, 0xF1, 0x21 };

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* multiMidData[4];
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void F5song2mid(int songNum, long ptr);

void F5Proc(int bank)
{
	curInst = 0;
	foundTable = 0;

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

	/*Try to search the bank for song table loader - Method 1: Probotector 2*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], F5MagicBytesA, 8)) && foundTable != 1)
		{
			tablePtrLoc = i + 8;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Method 2: Animaniacs*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], F5MagicBytesB, 13)) && foundTable != 1)
		{
			tablePtrLoc = i + 13;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			F5song2mid(songNum, songPtr);
			i += 2;
			songNum++;
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
void F5song2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	long romPos = 0;
	long seqPos = 0;
	int seqPosM[4];
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int trackSizes[4];
	int onOff[4];
	int transpose = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	int repeat2 = 0;
	long repeat2Pt = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	int firstNotes[4];
	unsigned int midPos = 0;
	unsigned int midPosM[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	long ctrlTrackSize = 0;
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	int ctrlDelays[4];
	long repeatStart;
	int repeatAmt;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	int rowTime = 0;
	int holdNotes[4];
	int songEnd = 0;
	int patEnd = 0;
	long curPat = 0;
	long totalLens[4];
	long shortestLen = 0;

	midLength = 0x10000;
	for (j = 0; j < 4; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < 4; k++)
		{
			multiMidData[k][j] = 0;
		}

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

		/*Process the song patterns*/
		romPos = ptr;
		songEnd = 0;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			firstNotes[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			curNoteLens[curTrack] = 0;
			curDelays[curTrack] = 0;
			seqEnd = 0;
			midTrackBase = midPosM[curTrack];
			F5curInsts[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}


		while (songEnd == 0)
		{
			if (ReadLE16(&romData[romPos]) != 0x00 && romData[romPos + 1] != 0xFF)
			{
				curPat = ReadLE16(&romData[romPos]);
				shortestLen = 0;

				/*Get the "correct" length of the pattern*/
				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					seqPtrs[curTrack] = ReadLE16(&romData[curPat + (2 * curTrack)]);

					if (seqPtrs[curTrack] != 0x0000)
					{
						seqEnd = 0;
						ctrlDelay = 0;
						seqPos = seqPtrs[curTrack];
						repeat1 = -1;

						while (seqEnd == 0)
						{
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];
							command[2] = romData[seqPos + 2];

							/*Play note*/
							if (command[0] <= 0xFC)
							{
								ctrlDelay += ((command[0] + 1) * 5);
								seqPos += 3;
							}

							/*Rest*/
							else if (command[0] == 0xFD)
							{
								ctrlDelay += ((command[1] + 1) * 5);
								seqPos += 3;
							}

							/*Repeat*/
							else if (command[0] == 0xFE)
							{
								if (repeat1 == -1)
								{
									repeat1 = command[1];
									repeat1Pt = seqPos - command[2];
								}

								else if (repeat1 > 0)
								{
									repeat1--;
									seqPos = repeat1Pt;
								}

								else
								{
									seqPos += 3;
									repeat1 = -1;
								}

							}

							/*End of pattern*/
							else if (command[0] == 0xFF)
							{
								if (ctrlDelay < shortestLen || shortestLen == 0)
								{
									if (ctrlDelay != 0)
									{
										shortestLen = ctrlDelay;
									}

								}
								seqEnd = 1;
							}
						}
					}

				}

				/*Now for the real deal...*/
				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (seqPtrs[curTrack] != 0x0000)
					{
						seqEnd = 0;
						seqPos = seqPtrs[curTrack];
						repeat1 = -1;
						ctrlDelays[curTrack] = 0;

						while (seqEnd == 0)
						{
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];
							command[2] = romData[seqPos + 2];

							/*Play note*/
							if (command[0] <= 0xFC)
							{
								curNotes[curTrack] = command[1];
								if (curTrack != 2)
								{
									curNotes[curTrack] += 12;
								}
								curNoteLens[curTrack] = (command[0] + 1) * 5;

								if ((ctrlDelays[curTrack] + curNoteLens[curTrack]) > shortestLen)
								{
									curNoteLens[curTrack] = shortestLen - ctrlDelays[curTrack];
								}

								if (command[1] != F5curInsts[curTrack])
								{
									F5curInsts[curTrack] = command[2];
									firstNotes[curTrack] = 1;
								}
								tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, F5curInsts[curTrack]);
								firstNotes[curTrack] = 0;
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								ctrlDelays[curTrack] += curNoteLens[curTrack];
								seqPos += 3;
							}

							/*Rest*/
							else if (command[0] == 0xFD)
							{
								curNoteLens[curTrack] = (command[1] + 1) * 5;

								if ((ctrlDelays[curTrack] + curNoteLens[curTrack]) > shortestLen)
								{
									curNoteLens[curTrack] = shortestLen - ctrlDelays[curTrack];
								}
								curDelays[curTrack] += curNoteLens[curTrack];
								ctrlDelays[curTrack] += curNoteLens[curTrack];

								seqPos += 3;
							}

							/*Repeat*/
							else if (command[0] == 0xFE)
							{
								if (repeat1 == -1)
								{
									repeat1 = command[1];
									repeat1Pt = seqPos - command[2];
								}

								else if (repeat1 > 0)
								{
									repeat1--;
									seqPos = repeat1Pt;
								}

								else
								{
									seqPos += 3;
									repeat1 = -1;
								}
							}

							/*End of sequence*/
							else if (command[0] == 0xFF)
							{
								seqEnd = 1;
							}
						}
					}
					else if (seqPtrs[curTrack] == 0)
					{
						curDelays[curTrack] += shortestLen;
					}
				}

				romPos += 2;

			}
			else
			{
				songEnd = 1;
			}
		}

		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
			midPosM[curTrack] += 4;
			firstNotes[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
		}

		free(multiMidData[0]);
		fclose(mid);
	}
}