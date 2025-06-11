/*Now Production/Music Worx*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "NOWPRO.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long seqPtrs[4];
long songPtr;
int numSongs;
int masterBank;
int songBank;
long bankAmt;
long sizeBank;
int curInst;
int curInsts[4];
long firstPtr;
int drvVers;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

char* argv3;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Nowsong2mid(int songNum, long ptr);

void NowProc(int bank, char parameters[4][50])
{
	tableOffset = strtol(parameters[0], NULL, 16);
	numSongs = strtol(parameters[1], NULL, 16);
	drvVers = strtol(parameters[2], NULL, 16);

	if (drvVers == 1)
	{
		if (bank != 1)
		{
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);
		}
		else if (bank == 1)
		{
			fseek(rom, ((bank - 1) * bankSize * 2), SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize * 2, rom);
		}

		i = tableOffset;

		for (songNum = 1; songNum <= numSongs; songNum++)
		{
			songPtr = ReadLE16(&exRomData[i]);
			if (songPtr != 0x0000)
			{
				if (exRomData[songPtr] != 0x00)
				{
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Nowsong2mid(songNum, songPtr);
				}
				else if (exRomData[songPtr] == 0x00)
				{
					printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
				}

			}
			else if (songPtr == 0x0000)
			{
				printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
			}
			i += 2;
		}
		free(exRomData);
	}
	else
	{
		if (bank != 1)
		{
			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);
		}
		else if (bank == 1)
		{
			fseek(rom, ((bank - 1) * bankSize * 2), SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize * 2, rom);
		}

		i = tableOffset;

		for (songNum = 1; songNum <= numSongs; songNum++)
		{
			songBank = romData[i];
			songPtr = ReadLE16(&romData[i + 1]);
			if (songPtr == 0xDF70)
			{
				i++;
				songBank = romData[i];
				songPtr = ReadLE16(&romData[i + 1]);
			}

			if (songBank != 0)
			{
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, (songBank * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
			}
			else if (songBank == 0)
			{
				fseek(rom, (songBank * bankSize * 2), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize * 2, rom);
			}
			if (songPtr != 0x0000)
			{
				if (exRomData[songPtr] != 0x00)
				{
					printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, songBank);
					Nowsong2mid(songNum, songPtr);
				}
				else if (exRomData[songPtr] == 0x00)
				{
					printf("Song %i: 0x%04X (bank %02X) (empty, skipped)\n", songNum, songPtr, songBank);
				}

			}
			else if (songPtr == 0x0000)
			{
				printf("Song %i: 0x%04X (bank %02X) (empty, skipped)\n", songNum, songPtr, songBank);
			}
			free(exRomData);
			i += 3;
		}
		free(romData);
	}
}

/*Convert the song data to MIDI*/
void Nowsong2mid(int songNum, long ptr)
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
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long macro4Pos = 0;
	long macro4Ret = 0;
	int inMacro = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	int repeat2 = 0;
	long repeat2Pt = 0;
	int repeat3 = 0;
	long repeat3Pt = 0;
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
	int repeat = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	long jumpPos = 0;


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

		romPos = ptr;
		mask = exRomData[romPos];

		/*Try to get active channels*/
		maskArray[3] = mask >> 3 & 1;
		maskArray[2] = mask >> 2 & 1;
		maskArray[1] = mask >> 1 & 1;
		maskArray[0] = mask & 1;

		romPos++;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (maskArray[curTrack] != 0)
			{
				seqPtrs[curTrack] = ReadLE16(&exRomData[romPos]);
				romPos += 2;
			}
		}

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

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			inMacro = 0;
			transpose = 0;
			repeat1 = -1;
			repeat2 = -1;
			repeat3 = -1;
			ctrlDelay = 0;

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

			if (maskArray[curTrack] != 0)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];
				inMacro = 0;
			}
			else
			{
				seqEnd = 1;
			}

			if (drvVers == 1)
			{
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];
					command[5] = exRomData[seqPos + 5];
					command[6] = exRomData[seqPos + 6];
					command[7] = exRomData[seqPos + 7];

					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);

					/*Rest*/
					if (command[0] <= 0x0F)
					{
						curNoteLen = highNibble + 1;
						curDelay += (chanSpeed * curNoteLen * 5);
						ctrlDelay += (chanSpeed * curNoteLen * 5);
						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x10 && command[0] <= 0xCF)
					{
						curNote = (lowNibble + (12 * octave)) + 23 + transpose;
						curNoteLen = (highNibble + 1) * chanSpeed * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Set octave*/
					else if (command[0] >= 0xD0 && command[0] <= 0xD5)
					{
						octave = highNibble;
						seqPos++;
					}

					/*Raise octave*/
					else if (command[0] == 0xD6)
					{
						octave++;
						seqPos++;
					}

					/*Lower octave*/
					else if (command[0] == 0xD7)
					{
						octave--;
						seqPos++;
					}

					/*Set decay?*/
					else if (command[0] == 0xD8)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0xD9)
					{
						seqPos += 2;
					}

					/*Set panning*/
					else if (command[0] == 0xDA)
					{
						seqPos += 2;
					}

					/*Set channel speed*/
					else if (command[0] == 0xDB)
					{
						chanSpeed = command[1] + 1;
						seqPos += 2;
					}

					/*Set channel effect?*/
					else if (command[0] == 0xDC)
					{
						seqPos += 2;
					}

					/*Set envelope (absolute)*/
					else if (command[0] == 0xDD)
					{
						seqPos += 2;
					}

					/*Set envelope (relative)*/
					else if (command[0] == 0xDE)
					{
						seqPos += 2;
					}

					/*Set tuning (absolute)*/
					else if (command[0] == 0xDF)
					{
						seqPos += 2;
					}

					/*Set tuning (relative)*/
					else if (command[0] == 0xE0)
					{
						seqPos += 2;
					}

					/*Repeat the following section*/
					else if (command[0] == 0xE1)
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pt = seqPos + 2;
							seqPos += 2;
						}
						else if (repeat2 == -1)
						{
							repeat2 = command[1];
							repeat2Pt = seqPos + 2;
							seqPos += 2;
						}
						else
						{
							repeat3 = command[1];
							repeat3Pt = seqPos + 2;
							seqPos += 2;
						}
					}

					/*End of repeat*/
					else if (command[0] == 0xE2)
					{
						if (repeat3 > -1)
						{
							if (repeat3 > 1)
							{
								seqPos = repeat3Pt;
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
								seqPos = repeat1Pt;
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
								seqPos = repeat2Pt;
								repeat2--;
							}
							else if (repeat2 <= 1)
							{
								repeat2 = -1;
								seqPos++;
							}
						}

						else
						{
							seqEnd = 1;
						}

					}

					/*Jump to position on final repeat*/
					else if (command[0] == 0xE3)
					{
						if (repeat2 == -1)
						{
							if (repeat1 == 1)
							{
								jumpPos = ReadLE16(&exRomData[seqPos + 1]);
								seqPos = jumpPos;
								repeat1 = -1;
							}
							else
							{
								seqPos += 3;
							}

						}
						else if (repeat2 > -1)
						{
							if (repeat2 == 1)
							{
								jumpPos = ReadLE16(&exRomData[seqPos + 1]);
								seqPos = jumpPos;
								repeat2 = -1;
							}
							else
							{
								seqPos += 3;
							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Go to song loop*/
					else if (command[0] == 0xE4)
					{
						seqEnd = 1;
					}

					/*Go to macro*/
					else if (command[0] == 0xE5)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro1Ret = seqPos + 3;
							inMacro = 1;
							seqPos = macro1Pos;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro2Ret = seqPos + 3;
							inMacro = 2;
							seqPos = macro2Pos;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro3Ret = seqPos + 3;
							inMacro = 3;
							seqPos = macro3Pos;
						}
						else if (inMacro == 3)
						{
							macro4Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro4Ret = seqPos + 3;
							inMacro = 4;
							seqPos = macro4Pos;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Return from macro*/
					else if (command[0] == 0xE6)
					{
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

					/*Unknown command E7*/
					else if (command[0] == 0xE7)
					{
						seqPos += 2;
					}

					/*Set transpose (absolute)*/
					else if (command[0] == 0xE8)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (relative)*/
					else if (command[0] == 0xE9)
					{
						transpose += (signed char)command[1];
						seqPos += 2;
					}

					/*Unknown command EA*/
					else if (command[0] == 0xEA)
					{
						seqPos += 2;
					}

					/*Set note size*/
					else if (command[0] == 0xEB)
					{
						seqPos += 2;
					}

					/*Set arpeggio effect*/
					else if (command[0] == 0xEC)
					{
						seqPos += 2;
					}

					/*Unknown command ED*/
					else if (command[0] == 0xED)
					{
						seqPos += 2;
					}

					/*Set vibrato*/
					else if (command[0] == 0xEE)
					{
						seqPos += 3;
					}

					/*Set channel unknown*/
					else if (command[0] == 0xEF)
					{
						seqPos += 2;
					}

					/*Invalid commands*/
					else if (command[0] >= 0xF0 && command[0] <= 0xFD)
					{
						seqPos++;
					}

					/*Tie to next note*/
					else if (command[0] == 0xFE)
					{
						seqPos++;
					}

					/*End of channel (no loop)*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}
				}
			}
			else
			{
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];
					command[5] = exRomData[seqPos + 5];
					command[6] = exRomData[seqPos + 6];
					command[7] = exRomData[seqPos + 7];

					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);

					/*Rest*/
					if (command[0] == 0x00)
					{
						curNoteLen = (command[1] + 1) * 5;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Play note*/
					else if (command[0] >= 0x01 && command[0] <= 0x7F)
					{
						curNote = command[0] + 23 + transpose;
						curNoteLen = (command[1] + 1) * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Set note size*/
					else if (command[0] >= 0x80 && command[0] <= 0x8F)
					{
						seqPos++;
					}

					/*Set envelope decay*/
					else if (command[0] == 0x90)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] >= 0x91 && command[0] <= 0x94)
					{
						seqPos++;
					}

					/*Set waveform*/
					else if (command[0] == 0x95)
					{
						seqPos += 2;
					}

					/*Set panning: left only*/
					else if (command[0] == 0x96)
					{
						seqPos++;
					}

					/*Set panning: left and right*/
					else if (command[0] == 0x97)
					{
						seqPos++;
					}

					/*Set panning: right only*/
					else if (command[0] == 0x98)
					{
						seqPos++;
					}

					/*Set noise type 1*/
					else if (command[0] == 0x99)
					{
						seqPos++;
					}

					/*Set noise type 2*/
					else if (command[0] == 0x9A)
					{
						seqPos++;
					}

					/*Set envelope volume (absolute)*/
					else if (command[0] == 0x9B)
					{
						seqPos += 2;
					}

					/*Set envelope volume (relative)*/
					else if (command[0] == 0x9C)
					{
						seqPos += 2;
					}

					/*Set tuning (absolute)*/
					else if (command[0] == 0x9D)
					{
						seqPos += 2;
					}

					/*Set tuning (relative)*/
					else if (command[0] == 0x9E)
					{
						seqPos += 2;
					}

					/*Repeat the following section*/
					else if (command[0] == 0x9F)
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pt = seqPos + 2;
							seqPos += 2;
						}
						else if (repeat2 == -1)
						{
							repeat2 = command[1];
							repeat2Pt = seqPos + 2;
							seqPos += 2;
						}
						else
						{
							repeat3 = command[1];
							repeat3Pt = seqPos + 2;
							seqPos += 2;
						}
					}

					/*End of repeat section*/
					else if (command[0] == 0xA0)
					{
						if (repeat3 > -1)
						{
							if (repeat3 > 1)
							{
								seqPos = repeat3Pt;
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
								seqPos = repeat1Pt;
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
								seqPos = repeat2Pt;
								repeat2--;
							}
							else if (repeat2 <= 1)
							{
								repeat2 = -1;
								seqPos++;
							}
						}

						else
						{
							seqEnd = 1;
						}
					}

					/*Jump to position on final repeat*/
					else if (command[0] == 0xA1)
					{
						if (repeat2 == -1)
						{
							if (repeat1 == 1)
							{
								jumpPos = ReadLE16(&exRomData[seqPos + 1]);
								seqPos = jumpPos;
								repeat1 = -1;
							}
							else
							{
								seqPos += 3;
							}

						}
						else if (repeat2 > -1)
						{
							if (repeat2 == 1)
							{
								jumpPos = ReadLE16(&exRomData[seqPos + 1]);
								seqPos = jumpPos;
								repeat2 = -1;
							}
							else
							{
								seqPos += 3;
							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Go to song loop*/
					else if (command[0] == 0xA2)
					{
						seqEnd = 1;
					}

					/*Go to macro*/
					else if (command[0] == 0xA3)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro1Ret = seqPos + 3;
							inMacro = 1;
							seqPos = macro1Pos;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro2Ret = seqPos + 3;
							inMacro = 2;
							seqPos = macro2Pos;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro3Ret = seqPos + 3;
							inMacro = 3;
							seqPos = macro3Pos;
						}
						else if (inMacro == 3)
						{
							macro4Pos = ReadLE16(&exRomData[seqPos + 1]);
							macro4Ret = seqPos + 3;
							inMacro = 4;
							seqPos = macro4Pos;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Return from macro*/
					else if (command[0] == 0xA4)
					{
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

					/*Unknown command A5*/
					else if (command[0] == 0xA5)
					{
						seqPos += 2;
					}

					/*Set transpose (absolute)*/
					else if (command[0] == 0xA6)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (relative)*/
					else if (command[0] == 0xA7)
					{
						transpose += (signed char)command[1];
						seqPos += 2;
					}

					/*Set pitch bend (up)*/
					else if (command[0] == 0xA8)
					{
						seqPos += 2;
					}

					/*Set pitch bend (down)*/
					else if (command[0] == 0xA9)
					{
						seqPos += 2;
					}

					/*Set channel pitch effect*/
					else if (command[0] == 0xAA)
					{
						seqPos += 3;
					}

					/*Unknown command AB*/
					else if (command[0] == 0xAB)
					{
						seqPos += 2;
					}

					/*End of channel (no loop)*/
					else if (command[0] == 0xAC)
					{
						seqEnd = 1;
					}

					/*Unknown command AD*/
					else if (command[0] == 0xAD)
					{
						seqPos += 2;
					}

					/*Set global volume*/
					else if (command[0] == 0xAE)
					{
						seqPos += 2;
					}

					/*Tie to next note*/
					else if (command[0] == 0xFF)
					{
						seqPos++;
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