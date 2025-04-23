/*Sheep - Manabu Namiki (Doki X Doki Sasete)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "SHEEP.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[8];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
int multiBanks;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char SheepMagicBytes[5] = { 0x06, 0x00, 0xCB, 0x10, 0x21 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Sheepsong2mid(int songNum, long ptr);

void SheepProc(int bank)
{
	foundTable = 0;
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
		if ((!memcmp(&romData[i], SheepMagicBytes, 5)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 5;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;
		while (ReadLE16(&romData[i]) != 0xFFFF)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Sheepsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}


	free(romData);
}

/*Convert the song data to MIDI*/
void Sheepsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[8] = { "Square 1", "Square 2", "Wave", "Noise", "Empty", "Empty", "Empty", "Empty" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int tempo = 150;
	int tempoVal = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long jumpPos = 0;

	int repeat1 = 0;
	int repeat1Pt = 0;
	int repeat2 = 0;
	int repeat2Pt = 0;
	int repeat3 = 0;
	int repeat3Pt = 0;
	int rest = 0;

	unsigned char command[4];
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int repNote = 0;
	int repNoteLen = 0;
	int trackCnt = 8;
	int ticks = 120;
	int transpose = 0;

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


		/*Workarounds for tempos NOT set first*/
		if (bank == 0 && songNum == 2)
		{
			tempo = 224;
		}
		else if (bank == 2 && songNum == 2)
		{
			tempo = 264;
		}

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

		romPos = ptr - bankAmt;

		for (curTrack = 0; curTrack < 8; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
		}

		for (curTrack = 0; curTrack < 8; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			seqEnd = 0;

			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;

			curNote = 0;
			curNoteLen = 0;
			curDelay = 0;
			ctrlDelay = 0;
			inMacro = 0;
			macro1Pos = 0;
			macro1Ret = 0;
			macro2Pos = 0;
			macro2Ret = 0;
			macro3Pos = 0;
			macro3Ret = 0;
			repeat1 = -1;
			repeat2 = -1;
			repeat3 = -1;
			transpose = 0;

			seqPos = seqPtrs[curTrack] - bankAmt;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (seqPtrs[curTrack] >= bankAmt && seqPtrs[curTrack] < 0x8000)
			{
				seqPos = seqPtrs[curTrack] - bankAmt;
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note*/
					if (command[0] <= 0x4F)
					{
						curNote = command[0] + 24;
						curNote += transpose;
						curNoteLen = command[1] * 5;
						tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Set tempo*/
					else if (command[0] == 0x50)
					{
						tempo = command[1] * 2;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set panning?*/
					else if (command[0] == 0x51)
					{
						seqPos += 2;
					}

					/*Set length/envelope 1*/
					else if (command[0] == 0x52)
					{
						seqPos += 2;
					}

					/*Set length/envelope 2*/
					else if (command[0] == 0x53)
					{
						seqPos += 2;
					}

					/*Set instrument*/
					else if (command[0] == 0x54)
					{
						curInst = command[1];
						if (curInst >= 0x80)
						{
							curInst = 0;
						}
						firstNote = 1;
						seqPos += 2;
					}

					/*Set volume*/
					else if (command[0] == 0x55)
					{
						seqPos += 2;
					}

					/*Set arpeggio*/
					else if (command[0] == 0x56)
					{
						seqPos += 2;
					}

					/*Set distortion*/
					else if (command[0] == 0x57)
					{
						seqPos += 2;
					}

					/*Set vibrato*/
					else if (command[0] == 0x58)
					{
						seqPos += 2;
					}

					/*Set tuning*/
					else if (command[0] == 0x59)
					{
						transpose = (signed char)command[1];

						transpose /= 8;
						seqPos += 2;
					}

					/*Restart song after point?*/
					else if (command[0] == 0x5A || command[0] == 0x78)
					{
						seqPos += 2;
					}

					/*Invalid command*/
					else if (command[0] >= 0x5B && command[0] <= 0x77)
					{
						seqPos += 2;
					}

					/*Unknown command 79*/
					else if (command[0] == 0x79)
					{
						seqPos += 2;
					}

					/*Go to macro*/
					else if (command[0] == 0x7A)
					{
						if (inMacro == 0)
						{
							macro1Ret = ReadLE16(&romData[seqPos + 1]);
							macro1Pos = (((seqPos + bankAmt) + macro1Ret) & 0xFFFF) - bankAmt;

							macro1Ret = seqPos + 3;
							inMacro = 1;
							seqPos = macro1Pos;
						}

						else if (inMacro == 1)
						{
							macro2Ret = ReadLE16(&romData[seqPos + 1]);
							macro2Pos = (((seqPos + bankAmt) + macro2Ret) & 0xFFFF) - bankAmt;
							macro2Ret = seqPos + 3;
							inMacro = 2;
							seqPos = macro2Pos;
						}

						else if (inMacro == 2)
						{
							macro3Ret = ReadLE16(&romData[seqPos + 1]);
							macro3Pos = (((seqPos + bankAmt) + macro3Ret) & 0xFFFF) - bankAmt;
							macro3Ret = seqPos + 3;
							inMacro = 3;
							seqPos = macro3Pos;
						}

						else
						{
							seqEnd = 1;
						}
					}

					/*Return from macro*/
					else if (command[0] == 0x7B)
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

					/*Repeat the following section*/
					else if (command[0] == 0x7C)
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							seqPos += 2;
						}
						else
						{
							repeat2 = command[1];
							seqPos += 2;
						}
					}

					/*End of repeat/jump to position*/
					else if (command[0] == 0x7D)
					{
						if (repeat2 == -1)
						{
							if (repeat1 > 1)
							{
								jumpPos = ReadLE16(&romData[seqPos + 1]);
								repeat1Pt = (((seqPos + bankAmt) + jumpPos) & 0xFFFF) - bankAmt;
								seqPos = repeat1Pt;
								repeat1--;
							}
							else
							{
								repeat1 = -1;
								seqPos += 3;
							}

						}

						else if (repeat2 > -1)
						{
							if (repeat2 > 1)
							{
								jumpPos = ReadLE16(&romData[seqPos + 1]);
								repeat2Pt = (((seqPos + bankAmt) + jumpPos) & 0xFFFF) - bankAmt;
								seqPos = repeat2Pt;
								repeat2--;
							}
							else
							{
								repeat2 = -1;
								seqPos += 3;
							}
						}
					}

					/*Jump to position on final repeat*/
					else if (command[0] == 0x7E)
					{
						if (repeat2 == -1)
						{
							if (repeat1 == 1)
							{
								jumpPos = ReadLE16(&romData[seqPos + 1]);
								seqPos = (((seqPos + bankAmt) + jumpPos) & 0xFFFF) - bankAmt;
							}
							else
							{
								seqPos += 3;
							}
						}
						else if (repeat2 >= -1)
						{
							if (repeat2 == 1)
							{
								jumpPos = ReadLE16(&romData[seqPos + 1]);
								seqPos = (((seqPos + bankAmt) + jumpPos) & 0xFFFF) - bankAmt;
							}
							else
							{
								seqPos += 3;
							}
						}

					}

					/*Go to loop point*/
					else if (command[0] == 0x7F)
					{
						seqEnd = 1;
					}

					/*Rest/delay*/
					else if (command[0] >= 0x80)
					{
						rest = (command[0] - 0x7F) * 5;
						curDelay += rest;
						ctrlDelay += rest;
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