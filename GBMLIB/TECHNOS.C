/*Technos Japan*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "TECHNOS.H"

#define bankSize 16384

const unsigned char TJMagicBytesA[9] = { 0x85, 0x6F, 0x3E, 0x00, 0x8C, 0x67, 0x5E, 0x23, 0x56 };
const unsigned char TJMagicBytesB[5] = { 0xCE, 0x00, 0x67, 0x5E, 0x23 };
const unsigned char TJMagicBytesC[9] = { 0xCD, 0xDD, 0x5F, 0xC3, 0xC1, 0x5F, 0x3D, 0x87, 0x21 };

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

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void TJsong2mid(int songNum, long ptr);

void TJProc(int bank)
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

	/*Try to search the bank for song table loader - Method 1: Double Dragon 1*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], TJMagicBytesA, 9)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Method 2: Nintendo World Cup/Double Dragon 2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], TJMagicBytesB, 5)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc - bankAmt] + (romData[tablePtrLoc + 3 - bankAmt] * 0x100);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Method 3: Zen-Nippon Pro Wrestling Jet*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], TJMagicBytesC, 9)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 9;
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

		/*Special implementation for Gyouten Ningen Batseelor*/
		if ((bank == 0x0C || bank == 0x0D) && tableOffset == 0x4B31)
		{
			while (ReadLE16(&romData[i]) > bankAmt)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				if (songPtr != 0 && songPtr < 0x8000)
				{
					TJsong2mid(songNum, songPtr);
				}
				i += 2;
				songNum++;
			}
		}

		/*Normal*/
		else
		{
			while (ReadLE16(&romData[i]) > bankAmt)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				if (songPtr != 0)
				{
					TJsong2mid(songNum, songPtr);
				}
				i += 2;
				songNum++;
			}
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
void TJsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char mask = 0;
	long channels[4] = { 0, 0, 0, 0 };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	long jumpPos = 0;
	long jumpPosRet = 0;
	long macroPos = 0;
	long macroRet = 0;
	int inMacro = 0;
	int repeat1Times = 0;
	int repeat1Pos = 0;
	int repeat2Times = 0;
	int repeat2Pos = 0;
	int transpose = 0;
	int ticks = 120;
	int tempo = 150;
	long insPtr = 0;
	long loopPt = 0;
	int repNote = 0;
	int repNoteMode = 0;
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
	int k = 0;
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

		romPos = ptr - bankAmt;

		/*Get the channel mask*/
		mask = romData[romPos];
		romPos++;

		while (romData[romPos] != 0xFF)
		{
			switch (romData[romPos])
			{
			case 0x00:
				channels[0] = ReadLE16(&romData[romPos + 1]);
				break;
			case 0x01:
				channels[1] = ReadLE16(&romData[romPos + 1]);
				break;
			case 0x02:
				channels[2] = ReadLE16(&romData[romPos + 1]);
				break;
			case 0x03:
				channels[3] = ReadLE16(&romData[romPos + 1]);
				break;
			case 0x04:
				channels[0] = ReadLE16(&romData[romPos + 1]);
				break;
			case 0x05:
				channels[1] = ReadLE16(&romData[romPos + 1]);
				break;
			case 0x06:
				channels[2] = ReadLE16(&romData[romPos + 1]);
				break;
			case 0x07:
				channels[3] = ReadLE16(&romData[romPos + 1]);
				break;
			default:
				channels[0] = ReadLE16(&romData[romPos + 1]);
				break;
			}
			romPos += 3;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNote = 1;
			inMacro = 0;
			transpose = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1Times = -1;
			repeat2Times = -1;
			repNoteMode = 0;
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
			if (channels[curTrack] >= bankAmt)
			{
				seqPos = channels[curTrack] - bankAmt;

				while (seqEnd == 0 && seqPos < (bankAmt * 2) && midPos < 16000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
					/*Set note length*/
					if (command[0] < 0x40)
					{
						curNoteLen = command[0] * 10;
						seqPos++;
					}


					/*Play note*/
					else if (command[0] >= 0x40 && command[0] < 0x80)
					{
						if (repNoteMode == 0)
						{
							if (curTrack != 3)
							{
								curNote = command[0] - 0x40 + 21 + transpose;
							}

							if (curTrack == 2)
							{
								curNote -= 12;
							}
							else if (curTrack == 3)
							{
								curNote = command[0] + 12;
							}

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							seqPos++;
						}
						else if (repNoteMode == 1)
						{
							for (k = 0; k < repNote; k++)
							{
								if (curTrack != 3)
								{
									curNote = command[0] - 0x40 + 21 + transpose;
								}

								if (curTrack == 2)
								{
									curNote -= 12;
								}
								else if (curTrack == 3)
								{
									curNote = command[0] + 12;
								}
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
							}
							seqPos++;
							repNoteMode = 0;
						}

					}

					/*Play note with custom size?*/
					else if (command[0] >= 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 21 + transpose;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Set noise type*/
					else if (command[0] == 0xC0)
					{
						seqPos++;
					}

					/*Sweep effect*/
					else if (command[0] == 0xC1)
					{
						seqPos += 2;
					}

					/*Set tuning*/
					else if (command[0] == 0xC2)
					{
						seqPos += 2;
					}

					/*Portamento effect*/
					else if (command[0] == 0xC3)
					{
						seqPos += 2;
					}

					/*Set volume/envelope*/
					else if (command[0] == 0xC4)
					{
						seqPos += 2;
					}

					/*Turn on "buzz" effect?*/
					else if (command[0] == 0xC5)
					{
						seqPos++;
					}

					/*Turn off "buzz" effect?*/
					else if (command[0] == 0xC6)
					{
						seqPos++;
					}

					/*Set note size*/
					else if (command[0] == 0xC7)
					{
						seqPos += 2;
					}

					/*Wave setting?*/
					else if (command[0] == 0xC8)
					{
						seqPos += 2;
					}

					/*Rest*/
					else if (command[0] == 0xC9)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Turn on effect mode*/
					else if (command[0] == 0xCA)
					{
						seqPos++;
					}

					/*Turn off effect mode*/
					else if (command[0] == 0xCB)
					{
						seqPos++;
					}

					/*Set tempo*/
					else if (command[0] == 0xCC)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 1.1;

						if (tempo != 0)
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						}
						else
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / 150);
						}
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set instrument (pointer)*/
					else if (command[0] == 0xCD)
					{
						seqPos += 3;
					}

					/*Repeat section # times (v1)*/
					else if (command[0] == 0xCE)
					{
						if (repeat1Times == -1)
						{
							repeat1Times = command[1];
							repeat1Pos = ReadLE16(&romData[seqPos + 2]);

						}
						else if (repeat1Times > 1)
						{
							seqPos = repeat1Pos - bankAmt;
							repeat1Times--;
						}

						else if (repeat1Times == 1)
						{
							seqPos += 4;
							repeat1Times = -1;
						}

					}

					/*Repeat section # times (v2)*/
					else if (command[0] == 0xCF)
					{
						if (curTrack == 3 && songNum == 2 && ptr == 0x4FA1)
						{
							seqPos += 4;
						}
						if (repeat2Times == -1)
						{
							repeat2Times = command[1];
							repeat2Pos = ReadLE16(&romData[seqPos + 2]);

						}
						else if (repeat2Times > 1)
						{
							seqPos = repeat2Pos - bankAmt;
							repeat2Times--;
						}

						else if (repeat2Times == 1)
						{
							seqPos += 4;
							repeat2Times = -1;
						}

					}

					/*Go to loop point*/
					else if (command[0] == 0xD0)
					{
						seqEnd = 1;
					}

					/*Go to macro*/
					else if (command[0] == 0xD1)
					{
						macroPos = ReadLE16(&romData[seqPos + 1]);
						macroRet = seqPos + 3;
						seqPos = macroPos - bankAmt;
					}

					/*Return from macro*/
					else if (command[0] == 0xD2)
					{
						seqPos = macroRet;
					}

					/*Stop channel*/
					else if (command[0] == 0xD3)
					{
						seqEnd = 1;
					}

					/*Tom tom effect*/
					else if (command[0] == 0xD4)
					{
						seqPos += 2;
					}

					/*Disable tom tom?*/
					else if (command[0] == 0xD5)
					{
						seqPos++;
					}

					/*Turn on "distortion pitch"*/
					else if (command[0] == 0xD6)
					{
						seqPos++;
					}

					/*Turn off "distortion pitch"*/
					else if (command[0] == 0xD7)
					{
						seqPos++;
					}

					/*Set transpose*/
					else if (command[0] == 0xD8)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Play drum?*/
					else if (command[0] == 0xD9)
					{
						seqPos += 2;
					}

					/*Repeat the following note %i times*/
					else if (command[0] == 0xDA)
					{
						repNote = command[1];
						repNoteMode = 1;
						seqPos += 2;
					}

					/*Set vibrato*/
					else if (command[0] == 0xDB)
					{
						seqPos += 4;
					}

					/*Enable vibrato?*/
					else if (command[0] == 0xDC)
					{
						seqPos++;
					}

					/*Disable vibrato?*/
					else if (command[0] == 0xDD)
					{
						seqPos++;
					}


					/*Unknown (used in Nekketsu game)*/
					else if (command[0] == 0xDE)
					{
						seqPos += 2;
					}

					/*Unknown (used in Ultraman Ball)*/
					else if (command[0] == 0xDF)
					{
						seqPos += 2;
					}

					/*Unknown (used in Nekketsu game)*/
					else if (command[0] == 0xE0)
					{
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