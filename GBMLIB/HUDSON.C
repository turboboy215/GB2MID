/*Hudson Soft*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "HUDSON.H"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define bankSize 16384

FILE* rom, * mid;
long bank1;
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
int foundTable;
long firstPtr;
int curInst;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char HSMagicBytes[11] = { 0x09, 0x5E, 0x23, 0x66, 0x6B, 0x5E, 0x23, 0x44, 0x4D, 0xCB, 0x1B };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void HSsong2mid(int songNum, long ptr);

void HSProc(int bank)
{
	foundTable = 0;
	firstPtr = 0;
	curInst = 0;

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

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], HSMagicBytes, 11)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		firstPtr = ReadLE16(&romData[i]);
		songNum = 1;

		while ((i + bankAmt) < firstPtr)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);

			if (songPtr != 0x0000 && songPtr >= bankAmt && songPtr < (bankAmt * 2))
			{
				HSsong2mid(songNum, songPtr);
			}
			i += 2;
			songNum++;
		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		free(romData);
		exit(-1);
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void HSsong2mid(int songNum, long ptr)
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
	int octave = 0;
	int transpose = 0;
	int macro1Pos = 0;
	int macro1Ret = 0;
	int macro2Pos = 0;
	int macro2Ret = 0;
	int macro3Pos = 0;
	int macro3Ret = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	int repeat3 = 0;
	long repeat1Pos = -1;
	long repeat2Pos = -1;
	long repeat3Pos = -1;
	int freq = 0;
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
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int inMacro = 0;
	int tie = 0;


	midPos = 0;
	ctrlMidPos = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
		ctrlMidData[j] = 0;
	}

	if (multiBanks == 0)
	{
		sprintf(outfile, "song%d.mid", songNum);
	}
	else
	{
		sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
	}
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

		/*Get the current channel's mask*/
		romPos = ptr - bankAmt;
		mask = romData[romPos];
		maskArray[3] = mask >> 3 & 1;
		maskArray[2] = mask >> 2 & 1;
		maskArray[1] = mask >> 1 & 1;
		maskArray[0] = mask & 1;

		/*Get the channel pointers*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			if (maskArray[curTrack] == 1)
			{
				activeChan[curTrack] = ReadLE16(&romData[romPos + 1 + (curTrack * 2)]);
			}
			else
			{
				activeChan[curTrack] = 0x0000;
			}
		}

		/*Convert the sequence data of each channel*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;
			seqEnd = 0;
			curDelay = 0;
			ctrlDelay = 0;
			repeat1 = -1;
			repeat2 = -1;
			repeat3 = -1;
			inMacro = 0;
			tie = 0;

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

			if (activeChan[curTrack] != 0x0000 && activeChan[curTrack] >= bankAmt && activeChan[curTrack] < (bankAmt * 2))
			{
				seqPos = activeChan[curTrack] - bankAmt;
				seqEnd = 0;

				while (seqEnd == 0 && seqPos < bankSize && midPos < 48000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Rest*/
					if (command[0] < 0x10)
					{
						if (command[0] == 0 && command[1] == 0 && command[2] == 0 && command[3] == 0)
						{
							seqEnd = 1;
						}
						else
						{
							rest = command[0] + 1;
						}

						curDelay += (rest * chanSpeed * 5);
						ctrlDelay += (rest * chanSpeed * 5);
						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x10 && command[0] < 0xD0)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);

						curNote = lowNibble + (octave * 12) - 1;

						if (curTrack != 3)
						{
							curNote += 12;
						}
						else if (curTrack == 3)
						{
							curNote += 24;
						}
						if (tie == 0)
						{
							curNoteLen = (highNibble + 1) * chanSpeed * 5;
							ctrlDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						else
						{
							curNoteLen += (highNibble + 1) * chanSpeed * 5;
							ctrlDelay += curNoteLen;
							curDelay += curNoteLen;
						}

						seqPos++;
					}

					/*Set channel speed*/
					else if (command[0] == 0xD0)
					{
						chanSpeed = command[1];
						seqPos += 2;
					}

					/*Set octave*/
					else if (command[0] >= 0xD1 && command[0] <= 0xD6)
					{
						highNibble = (command[0] & 15);
						octave = highNibble;
						seqPos++;
					}

					/*Increase octave*/
					else if (command[0] == 0xD7)
					{
						octave++;
						seqPos++;
					}

					/*Decrease octave*/
					else if (command[0] == 0xD8)
					{
						octave--;
						seqPos++;
					}

					/*Toggle "tie" effect*/
					else if (command[0] == 0xD9)
					{
						if (tie == 0)
						{
							tie = 0;
						}
						else if (tie == 1)
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							tie = 0;
						}
						seqPos++;
					}

					/*Command DA*/
					else if (command[0] == 0xDA)
					{
						seqPos += 2;
					}

					/*Set stereo panning*/
					else if (command[0] == 0xDC)
					{
						seqPos += 2;
					}

					/*Song loop point*/
					else if (command[0] == 0xDD)
					{
						seqPos++;
					}

					/*Go to song loop point*/
					else if (command[0] == 0xDE)
					{
						seqEnd = 1;
					}

					/*Repeat section*/
					else if (command[0] == 0xDF)
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pos = seqPos += 2;
							seqPos = repeat1Pos;
						}
						else if (repeat2 == -1)
						{
							repeat2 = command[1];
							repeat2Pos = seqPos += 2;
							seqPos = repeat2Pos;
						}
						else
						{
							repeat3 = command[1];
							repeat3Pos = seqPos += 2;
							seqPos = repeat3Pos;
						}

					}

					/*End of repeat section*/
					else if (command[0] == 0xE0)
					{
						if (repeat3 > -1)
						{
							if (repeat3 > 1)
							{
								seqPos = repeat3Pos;
								repeat3--;
							}
							else if (repeat3 <= 1)
							{
								repeat3 = -1;
								seqPos++;
							}
						}
						else if (repeat2 == -1)
						{
							if (repeat1 > 1)
							{
								seqPos = repeat1Pos;
								repeat1--;
							}
							else if (repeat1 <= 1)
							{
								repeat1 = -1;
								seqPos++;
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
								seqPos++;
							}
						}
					}

					/*Command E1*/
					else if (command[0] == 0xE1)
					{
						seqPos += 2;
					}

					/*Go to macro*/
					else if (command[0] == 0xE2)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro1Ret = seqPos += 3;
							seqPos = macro1Pos;
							inMacro = 1;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro2Ret = seqPos += 3;
							seqPos = macro2Pos;
							inMacro = 2;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro3Ret = seqPos += 3;
							seqPos = macro3Pos;
							inMacro = 3;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0xE3)
					{
						if (inMacro == 1)
						{
							seqPos = macro1Ret;
							inMacro = 0;
						}
						else if (inMacro == 2)
						{
							seqPos = macro2Ret;
							inMacro = 1;
						}
						else if (inMacro == 3)
						{
							seqPos = macro3Ret;
							inMacro = 2;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Set frequency*/
					else if (command[0] == 0xE4)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0xE5)
					{
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0xE6)
					{
						seqPos += 2;
					}

					/*Set volume envelope*/
					else if (command[0] == 0xE7)
					{
						seqPos += 2;
					}

					/*Set note cut-off*/
					else if (command[0] == 0xE8)
					{
						seqPos += 2;
					}

					/*Set echo effect*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Set vibrato type*/
					else if (command[0] == 0xEA)
					{
						seqPos += 2;
					}

					/*Set vibrato delay*/
					else if (command[0] == 0xEB)
					{
						seqPos += 2;
					}

					/*Set pitch offset*/
					else if (command[0] == 0xEC)
					{
						seqPos += 2;
					}

					/*Adjust pitch offset*/
					else if (command[0] == 0xED)
					{
						seqPos += 2;
					}

					/*Command EE*/
					else if (command[0] == 0xEE)
					{
						seqPos += 2;
					}

					/*Command EF*/
					else if (command[0] == 0xEF)
					{
						seqPos += 2;
					}

					/*Preset*/
					else if (command[0] == 0xF0)
					{
						seqPos += 2;
					}

					/*Command F1*/
					else if (command[0] == 0xF1)
					{
						seqPos += 2;
					}

					/*Command F2*/
					else if (command[0] == 0xF2)
					{
						seqPos++;
					}

					/*Command F3*/
					else if (command[0] == 0xF3)
					{
						seqPos++;
					}

					/*Command F4*/
					else if (command[0] == 0xF4)
					{
						seqPos++;
					}

					/*Command FA*/
					else if (command[0] == 0xFA)
					{
						seqPos++;
					}

					/*Command FB*/
					else if (command[0] == 0xFB)
					{
						seqPos++;
					}

					/*Command FC*/
					else if (command[0] == 0xFC)
					{
						seqPos++;
					}

					/*End of track*/
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

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}
