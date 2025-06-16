/*DSEQ (Toshiyuki Ueno)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DSEQ.H"

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
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int drvVers;
int verOverride;
int multiBanks;
int curBank;

char folderName[100];

const char DSEQMagicBytesA[4] = { 0xFE, 0xFF, 0x20, 0x07 };
const char DSEQMagicBytesB[4] = { 0x21, 0x00, 0xDD, 0xCD };
const char DSEQNoteLens1[16] = { 0x01, 0x02, 0x03, 0x04, 0x06, 0x08, 0x09, 0x0C, 0x10, 0x12, 0x18, 0x20, 0x24, 0x30, 0x40, 0x48 };
const char DSEQNoteLens3[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x0C, 0x10, 0x12, 0x18, 0x20, 0x24, 0x30, 0x48, 0x60 };

unsigned char* romData;
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
void DSEQsong2mid(int songNum, long songPtrs[4]);

void DSEQProc(int bank, char parameters[4][50])
{
	foundTable = 0;
	curInst = 0;
	drvVers = 1;
	verOverride = 0;

	if (parameters[0][0] != 0)
	{
		verOverride = 1;
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers > 4)
		{
			printf("ERROR: Invalid version number!\n");
			exit(2);
		}
	}
	else
	{
		drvVers = 1;
	}

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

	/*Try to search the bank for song table loader - Version 1*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], DSEQMagicBytesA, 4)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			if (verOverride == 0)
			{
				drvVers = 1;
			}
		}
	}

	/*Try to search the bank for song table loader - Version 2/3*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], DSEQMagicBytesB, 4)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			if (verOverride == 0)
			{
				drvVers = 2;
			}
		}
	}

	if (foundTable == 1)
	{
		/*Skip empty song*/
		i = tableOffset + 8;
		songNum = 1;

		while (ReadLE16(&romData[i]) >= bankSize && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtrs[0] = ReadLE16(&romData[i]);
			songPtrs[1] = ReadLE16(&romData[i + 2]);
			songPtrs[2] = ReadLE16(&romData[i + 4]);
			songPtrs[3] = ReadLE16(&romData[i + 6]);
			printf("Song %i, channel 1: 0x%04X\n", songNum, songPtrs[0]);
			printf("Song %i, channel 2: 0x%04X\n", songNum, songPtrs[1]);
			printf("Song %i, channel 3: 0x%04X\n", songNum, songPtrs[2]);
			printf("Song %i, channel 4: 0x%04X\n", songNum, songPtrs[3]);
			DSEQsong2mid(songNum, songPtrs);
			i += 8;
			songNum++;
		}
		free(romData);
	}
}

/*Convert the song data to MIDI*/
void DSEQsong2mid(int songNum, long songPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
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
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long macro4Pos = 0;
	long macro4Ret = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	int repeat2 = 0;
	long repeat2Pt = 0;
	int repeat3 = 0;
	long repeat3Pt = 0;
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
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			repeat3 = -1;
			transpose = 0;
			inMacro = 0;

			seqPos = songPtrs[curTrack];

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
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note*/
					if (command[0] <= 0xDF)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);


						/*Hold note*/
						if (lowNibble == 0x0C)
						{
							if (holdNote == 1)
							{
								curNoteLen += (DSEQNoteLens1[highNibble] * 5);
							}
							else
							{
								curNoteLen = (DSEQNoteLens1[highNibble] * 5);
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

						}
						/*Rest*/
						else if (lowNibble == 0x0D)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;

							}
							curNoteLen = DSEQNoteLens1[highNibble] * 5;
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;

							}
							curNoteLen = DSEQNoteLens1[highNibble] * 5;
							curNote = (octave * 12) + lowNibble + transpose + 24;
							holdNote = 1;
						}
						seqPos++;
					}

					/*Set octave*/
					else if (command[0] >= 0xE0 && command[0] <= 0xE8)
					{
						octave = command[0] - 0xE0;
						seqPos++;
					}

					/*Octave up*/
					else if (command[0] == 0xE9)
					{
						octave++;
						seqPos++;
					}

					/*Octave down*/
					else if (command[0] == 0xEA)
					{
						octave--;
						seqPos++;
					}

					/*Command EB-EC (invalid)*/
					else if (command[0] == 0xEB || command[0] == 0xEC)
					{
						seqPos++;
					}

					/*Unknown command ED*/
					else if (command[0] == 0xED)
					{
						seqPos += 2;
					}

					/*Jump to position*/
					else if (command[0] == 0xEE)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;

						}
						seqEnd = 1;
					}

					/*End of song or return from macro*/
					else if (command[0] == 0xEF)
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
							if (holdNote == 1)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;

							}
							seqEnd = 1;
						}
					}

					/*Set pulse/wave parameters*/
					else if (command[0] == 0xF0)
					{
						seqPos += 4;
					}

					/*Repeat the following section*/
					else if (command[0] == 0xF1)
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
					else if (command[0] == 0xF2)
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

					/*Command F3 (invalid)*/
					else if (command[0] == 0xF3)
					{
						seqPos++;
					}

					/*Set tuning (v1)*/
					else if (command[0] == 0xF4)
					{
						seqPos += 2;
					}

					/*Set vibrato*/
					else if (command[0] == 0xF5)
					{
						seqPos += 4;
					}

					/*Set tuning (v2)*/
					else if (command[0] == 0xF6)
					{
						seqPos += 2;
					}

					/*Set volume*/
					else if (command[0] == 0xF7)
					{
						seqPos += 2;
					}

					/*Command F8 (invalid)*/
					else if (command[0] == 0xF8)
					{
						seqPos++;
					}

					/*Set tempo*/
					else if (command[0] == 0xF9)
					{
						if (curTrack == 0)
						{
							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							tempo = command[1];
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
						}
						seqPos += 2;
					}

					/*Set note type*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Set panning*/
					else if (command[0] == 0xFB)
					{
						seqPos += 2;
					}

					/*Set noise*/
					else if (command[0] == 0xFC)
					{
						seqPos += 2;
					}

					/*Command FD (invalid)*/
					else if (command[0] == 0xFD)
					{
						seqPos++;
					}

					/*Go to macro*/
					else if (command[0] == 0xFE)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&romData[seqPos + 1]);
							macro1Ret = seqPos + 3;
							inMacro = 1;
							seqPos = macro1Pos;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&romData[seqPos + 1]);
							macro2Ret = seqPos + 3;
							inMacro = 2;
							seqPos = macro2Pos;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&romData[seqPos + 1]);
							macro3Ret = seqPos + 3;
							inMacro = 3;
							seqPos = macro3Pos;
						}
						else if (inMacro == 3)
						{
							macro4Pos = ReadLE16(&romData[seqPos + 1]);
							macro4Ret = seqPos + 3;
							inMacro = 4;
							seqPos = macro4Pos;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Command FF (invalid)*/
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
			else if (drvVers == 2 || drvVers == 3)
			{
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					if (curTrack == 1 && songNum == 2)
					{
						curTrack = 1;
					}

					/*Play note*/
					if (command[0] <= 0x7F)
					{

						/*Hold note*/
						if (command[0] == 0x6C)
						{
							if (holdNote == 1)
							{
								curNoteLen += (command[1] * 5);
							}
							else
							{
								curNoteLen = command[1] * 5;
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}
						}
						/*Rest*/
						else if (command[0] == 0x6D)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;

							}
							curNoteLen = command[1] * 5;
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;

							}
							curNoteLen = command[1] * 5;
							curNote = command[0] + 24 + transpose;
							holdNote = 1;
						}
						seqPos += 2;

					}

					/*End of channel*/
					else if (command[0] == 0x80)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;

						}
						seqEnd = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x81)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;

						}
						seqEnd = 1;
					}

					/*Set tempo*/
					else if (command[0] >= 0x82 && command[0] <= 0x84)
					{
						if (drvVers == 2)
						{
							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							tempo = command[1];
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
							seqPos += 2;
						}
						else
						{
							if (command[0] == 0x82)
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
							else if (command[0] == 0x83)
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
							else
							{
								ctrlMidPos++;
								valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
								ctrlDelay = 0;
								ctrlMidPos += valSize;
								WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
								ctrlMidPos += 3;
								tempo = command[1];
								WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
								ctrlMidPos += 2;
								seqPos += 2;
							}
						}

					}

					/*Set panning - left*/
					else if (command[0] == 0x85)
					{
						seqPos++;
					}

					/*Set panning - middle*/
					else if (command[0] == 0x86)
					{
						seqPos++;
					}

					/*Set panning - right*/
					else if (command[0] == 0x87)
					{
						seqPos++;
					}

					/*Set envelope*/
					else if (command[0] == 0x88 || command[0] == 0x89)
					{
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0x8A || command[0] == 0x8B)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0x8C)
					{
						seqPos += 2;
					}

					/*Set wave length*/
					else if (command[0] == 0x8D)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0x8E)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Set vibrato?*/
					else if (command[0] == 0x8F)
					{
						seqPos += 3;
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
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note*/
					if (command[0] <= 0xDF)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);


						/*Hold note*/
						if (lowNibble == 0x0C)
						{
							if (holdNote == 1)
							{
								curNoteLen += (DSEQNoteLens3[highNibble] * 5);
							}
							else
							{
								curNoteLen = (DSEQNoteLens3[highNibble] * 5);
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

						}
						/*Rest*/
						else if (lowNibble == 0x0D)
						{
							if (curTrack == 1 && songNum == 32)
							{
								curTrack = 1;
							}
							if (holdNote == 1)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;

							}
							curNoteLen = DSEQNoteLens3[highNibble] * 5;
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;

							}
							curNoteLen = DSEQNoteLens3[highNibble] * 5;
							curNote = (octave * 12) + lowNibble + transpose + 24;
							holdNote = 1;
						}
						seqPos++;
					}

					/*Set channel speed*/
					else if (command[0] >= 0xE0 && command[0] <= 0xEF)
					{
						octave = command[0] - 0xE0;
						seqPos++;
					}

					/*End of channel*/
					else if (command[0] == 0xF0)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;

						}
						seqEnd = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0xF1)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;

						}
						seqEnd = 1;
					}

					/*Repeat the following section*/
					else if (command[0] == 0xF2)
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
					else if (command[0] == 0xF3)
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

					/*Set tempo*/
					else if (command[0] == 0xF4)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1];
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set panning - left*/
					else if (command[0] == 0xF5)
					{
						seqPos++;
					}

					/*Set panning - middle*/
					else if (command[0] == 0xF6)
					{
						seqPos++;
					}

					/*Set panning - right*/
					else if (command[0] == 0xF7)
					{
						seqPos++;
					}

					/*Set envelope*/
					else if (command[0] == 0xF8 || command[0] == 0xF9)
					{
						seqPos += 2;
					}

					/*Unknown command FA*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0xFB)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0xFC)
					{
						seqPos += 2;
					}

					/*Set wave length*/
					else if (command[0] == 0xFD)
					{
						seqPos += 2;
					}

					/*Unknown command FE*/
					else if (command[0] == 0xFE)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0xFF)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
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