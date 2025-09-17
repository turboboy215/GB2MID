/*Culture Brain*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CULTRBRN.H"

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
int drvVers;
int stopCvt;
int ptrOverride;
int multiBanks;
int curBank;

char folderName[100];

const char CBMagicBytesA[7] = { 0xCB, 0x27, 0xCB, 0x27, 0xCB, 0x27, 0x11 };
const char CBMagicBytesB[7] = { 0xCB, 0x14, 0xCB, 0x25, 0xCB, 0x14, 0x11 };

const char NB1LenTables[48] = { 0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x40, 0x18, 0x02, 0x0E, 0x04, 0x08, 0x10, 0x20, 0x40,
0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x02, 0x04, 0x00, 0x03, 0x02, 0x03, 0x04, 0x05, 0x06 };

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
void CBsong2mid(int songNum, long ptrs[4]);

void CBProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	curInst = 0;
	drvVers = 1;
	stopCvt = 0;
	ptrOverride = 0;

	if (bank < 2)
	{
		bank = 2;
	}

	/*Special case for Super Chinese Land 1 in compilation*/
	if (bank == 0x11)
	{
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((bank)*bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);
	}
	else
	{
		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);
	}

	if (parameters[1][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);

		if (drvVers != 0 && drvVers != 1)
		{
			printf("ERROR: Invalid version number!\n");
			exit(1);
		}
		tableOffset = strtol(parameters[1], NULL, 16);
		ptrOverride = 1;
		foundTable = 1;
	}

	else if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);

		if (drvVers != 0 && drvVers != 1)
		{
			printf("ERROR: Invalid version number!\n");
			exit(1);
		}
	}

	if (ptrOverride == 0)
	{
		/*Try to search the bank for song table loader - Method 1 - Common version*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], CBMagicBytesA, 7)) && foundTable != 1)
			{
				tablePtrLoc = i + 7;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}

		/*Method 2 - Ninja Boy 2*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], CBMagicBytesB, 7)) && foundTable != 1)
			{
				tablePtrLoc = i + 7;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		if (drvVers == 0)
		{
			while (songNum <= 18)
			{
				seqPtrs[0] = ReadLE16(&romData[i]);
				seqPtrs[1] = ReadLE16(&romData[i + 2]);
				seqPtrs[2] = ReadLE16(&romData[i + 4]);
				seqPtrs[3] = ReadLE16(&romData[i + 6]);
				printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
				printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
				printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
				printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
				CBsong2mid(songNum, seqPtrs);
				i += 8;
				songNum++;
			}
		}
		else
		{
			if (ReadLE16(&romData[i]) == 0xEA08)
			{
				i += 8;
			}
			while (stopCvt != 1)
			{
				seqPtrs[0] = ReadLE16(&romData[i]);
				seqPtrs[1] = ReadLE16(&romData[i + 2]);
				seqPtrs[2] = ReadLE16(&romData[i + 4]);
				seqPtrs[3] = ReadLE16(&romData[i + 6]);

				for (j = 0; j < 4; j++)
				{
					if (seqPtrs[j] > 0x0000 && seqPtrs[j] < 0x4000)
					{
						stopCvt = 1;
					}
					if (seqPtrs[j] >= 0x8000)
					{
						stopCvt = 1;
					}
				}

				if (stopCvt != 1)
				{
					printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
					printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
					printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
					printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
					CBsong2mid(songNum, seqPtrs);
					i += 8;
					songNum++;
				}

			}
		}

		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}
}

/*Convert the song data to MIDI*/
void CBsong2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned int mask = 0;
	unsigned int mask2 = 0;
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
	int baseNote = 0;
	int transpose = 0;
	int repeats[500][2];
	int repeatNum;
	unsigned char command[5];
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
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	int lenBase = 0;
	long startPos = 0;
	long macro1Pos = 0;
	long macro1End = 0;
	long macro2Pos = 0;
	long macro2End = 0;
	long macro3Pos = 0;
	long macro3End = 0;
	long macro4Pos = 0;
	long macro4End = 0;
	int inMacro = 0;
	long jumpAmt = 0;

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

	for (repeatNum = 0; repeatNum < 16; repeatNum++)
	{
		repeats[repeatNum][0] = -1;
		repeats[repeatNum][1] = 0;
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
			baseNote = 0;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			transpose = 0;
			inMacro = 0;
			repeatNum = 0;

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
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];
			}
			else
			{
				seqEnd = 1;
			}

			/*Early driver (Ninja Boy)*/
			if (drvVers == 0)
			{
				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*End of channel*/
					if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					else
					{
						mask = command[0] & 0x1F;
						{
							/*Unknown flag*/
							if (mask == 0x1E)
							{
								seqPos++;
							}
							/*Command*/
							else if (mask == 0x1F)
							{
								mask2 = command[0] & 0xE0;

								/*Set base note*/
								if (mask2 == 0x00)
								{
									baseNote = command[1] * 4;
									seqPos += 2;
								}
								/*Set channel parameters*/
								else if (mask2 == 0x20)
								{
									lenBase = command[3] * 8;
									seqPos += 5;
								}
								/*Set tempo*/
								else if (mask2 == 0x40)
								{
									ctrlMidPos++;
									valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
									ctrlDelay = 0;
									ctrlMidPos += valSize;
									WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
									ctrlMidPos += 3;
									tempo = command[1] * 0.6;
									WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
									ctrlMidPos += 2;
									seqPos += 2;
								}
								/*Jump to position (loop)*/
								else if (mask2 == 0x60)
								{
									seqEnd = 1;
								}
								/*Go to repeat point*/
								else if (mask2 == 0x80)
								{
									repeatNum = (command[1] & 0xF0) >> 4;

									if (repeats[repeatNum][0] == -1)
									{
										repeats[repeatNum][0] = command[1] & 0x0F;
										repeats[repeatNum][1] = ReadLE16(&romData[seqPos + 2]);
									}
									else if (repeats[repeatNum][0] > 0)
									{
										repeats[repeatNum][0]--;
										seqPos = repeats[repeatNum][1];
									}
									else
									{
										repeats[repeatNum][0] = -1;
										seqPos += 4;
									}
								}
								/*Command A0*/
								else if (mask2 == 0xA0)
								{
									seqPos++;
								}
								/*Unknown command*/
								else
								{
									seqPos++;
								}
							}
							/*Play note*/
							else
							{
								/*Rest*/
								if ((command[0] & 0x1F) == 0x00)
								{
									curNoteLen = (NB1LenTables[((command[0] & 0xE0) / 32) + lenBase]) * 5;
									curDelay += curNoteLen;
								}
								else
								{
									curNote = baseNote + (command[0] & 0x1F) + 24;
									if (curTrack == 2)
									{
										curNote -= 12;
									}
									curNoteLen = (NB1LenTables[((command[0] & 0xE0) / 32) + lenBase]) * 5;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
								}

								seqPos++;
							}
						}
					}
				}
			}
			else
			{
				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note with automatic length*/
					if (command[0] <= 0x3F)
					{
						curNote = command[0] + 21 + transpose;
						if (curTrack == 3 && curNote >= 36)
						{
							curNote -= 36;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Play note with volume*/
					else if (command[0] >= 0x40 && command[0] <= 0x7F)
					{
						curNote = command[0] - 0x40 + 21 + transpose;
						if (curTrack == 3 && curNote >= 36)
						{
							curNote -= 36;
						}
						if (command[1] >= 0x80)
						{
							curNoteLen = command[2] * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							seqPos += 3;
						}
						else
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							seqPos += 2;
						}
					}

					/*Play note with manual length*/
					else if (command[0] >= 0x80 && command[0] <= 0xBF)
					{
						curNote = command[0] - 0x80 + 21 + transpose;
						if (curTrack == 3 && curNote >= 36)
						{
							curNote -= 36;
						}
						curNoteLen = command[1] * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Set envelope*/
					else if (command[0] == 0xC0)
					{
						seqPos += 2;
					}

					/*Set volume*/
					else if (command[0] == 0xC1)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0xC2)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Set tempo*/
					else if (command[0] == 0xC3)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 0.6;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0xC4)
					{
						seqPos += 2;
					}

					/*Go to macro*/
					else if (command[0] == 0xC5)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&romData[seqPos + 1]);
							macro1End = seqPos + 3;
							inMacro = 1;
							seqPos = macro1Pos;
						}
						else if (inMacro == 1)
						{
							if (ReadLE16(&romData[seqPos + 1]) != macro1Pos)
							{
								macro2Pos = ReadLE16(&romData[seqPos + 1]);
								macro2End = seqPos + 3;
								inMacro = 2;
								seqPos = macro2Pos;
							}
							else
							{
								seqEnd = 1;
							}

						}
						else if (inMacro == 2)
						{
							if (ReadLE16(&romData[seqPos + 1]) != macro2Pos)
							{
								macro3Pos = ReadLE16(&romData[seqPos + 1]);
								macro3End = seqPos + 3;
								inMacro = 3;
								seqPos = macro3Pos;
							}
							else
							{
								seqEnd = 1;
							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Return from macro*/
					else if (command[0] == 0xC6)
					{
						if (inMacro == 1)
						{
							inMacro = 0;
							seqPos = macro1End;
						}
						else if (inMacro == 2)
						{
							inMacro = 1;
							seqPos = macro2End;
						}
						else if (inMacro == 3)
						{
							inMacro = 2;
							seqPos = macro3End;
						}
						else
						{
							seqPos++;
						}
					}

					/*Repeat position*/
					else if (command[0] == 0xC7)
					{
						repeatNum = (command[1] & 0xF0) >> 4;

						if (repeats[repeatNum][0] == -1)
						{
							repeats[repeatNum][0] = command[1] & 0x0F;
							repeats[repeatNum][1] = ReadLE16(&romData[seqPos + 2]);
						}
						else if (repeats[repeatNum][0] > 0)
						{
							repeats[repeatNum][0]--;
							seqPos = repeats[repeatNum][1];
						}
						else
						{
							repeats[repeatNum][0] = -1;
							seqPos += 4;
						}
					}

					/*Command C8 (no effect)*/
					else if (command[0] == 0xC8)
					{
						seqPos++;
					}

					/*Command C9*/
					else if (command[0] == 0xC9)
					{
						seqPos += 2;
					}

					/*Command CA (set bit of C9 variable)*/
					else if (command[0] == 0xCA)
					{
						seqPos++;
					}

					/*Command CB (reset bit of C9 variable)*/
					else if (command[0] == 0xCB)
					{
						seqPos++;
					}

					/*Reset envelope*/
					else if (command[0] == 0xCC)
					{
						seqPos++;
					}

					/*Set envelope - short*/
					else if (command[0] == 0xCD)
					{
						seqPos++;
					}

					/*Set envelope - long*/
					else if (command[0] == 0xCE)
					{
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0xCF)
					{
						curDelay += (command[1] * 5);
						ctrlDelay += (command[1] * 5);
						seqPos += 2;
					}

					/*Add to transpose*/
					else if (command[0] == 0xD0)
					{
						transpose += (signed char)command[1];
						seqPos += 2;
					}

					/*Command D1*/
					else if (command[0] == 0xD1)
					{
						seqPos++;
					}

					/*Set waveform length?*/
					else if (command[0] == 0xD2)
					{
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0xD3)
					{
						seqPos += 2;
					}

					/*Set envelope decay*/
					else if (command[0] == 0xD4)
					{
						seqPos += 2;
					}

					/*Reset envelope decay?*/
					else if (command[0] == 0xD5)
					{
						seqPos++;
					}

					/*Invert envelope decay?*/
					else if (command[0] == 0xD6)
					{
						seqPos++;
					}

					/*Command D7 (only functional in Ninja Boy 2?)*/
					else if (command[0] == 0xD7)
					{
						seqPos++;
					}

					/*Command D8 (only functional in Ninja Boy 2?)*/
					else if (command[0] == 0xD8)
					{
						seqPos++;
					}

					/*Command D9*/
					else if (command[0] == 0xD9)
					{
						seqPos += 2;
					}

					/*Command DA*/
					else if (command[0] == 0xDA)
					{
						seqPos++;
					}

					/*Command DB*/
					else if (command[0] == 0xDB)
					{
						seqPos++;
					}

					/*Set channel parameters*/
					else if (command[0] == 0xDC)
					{
						transpose = (signed char)command[3];
						seqPos += 5;
					}

					/*End of channel*/
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