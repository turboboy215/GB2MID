/*Atlus (Tsukasa Masuko)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "ATLUS.H"

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
long lenTable;
int multiBanks;
int curBank;

char folderName[100];

const unsigned char AtlusMagicBytesA[8] = { 0x26, 0x00, 0x6F, 0x19, 0x5E, 0x23, 0x56, 0x26 };
const unsigned char AtlusMagicBytesB[7] = { 0x07, 0xED, 0x07, 0xEE, 0x07, 0xEF, 0x07 };
const unsigned char AtlusMagicBytesLT[6] = { 0xCB, 0xBF, 0x4F, 0x06, 0x00, 0x21 };

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
unsigned int WriteNoteEvent(unsigned static char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned static char* buffer, unsigned int pos, unsigned int value);
void Atlussong2mid(int songNum, long ptr);

void AtlusProc(int bank)
{
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

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize * 2; i++)
	{
		if ((!memcmp(&romData[i], AtlusMagicBytesA, 8)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*If that fails, then try to search the bank for end of frequency table*/
	for (i = 0; i < bankSize * 2; i++)
	{
		if ((!memcmp(&romData[i], AtlusMagicBytesB, 7)) && foundTable != 1)
		{
			tableOffset = i + 7;
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		/*Try to search the bank for the note length table*/
		for (i = 0; i < bankSize * 2; i++)
		{
			if ((!memcmp(&romData[i], AtlusMagicBytesLT, 6)) && foundTable != 2)
			{
				lenTable = ReadLE16(&romData[i + 6]);
				printf("Note length table starts at 0x%04X...\n", lenTable);
				foundTable = 2;
			}
		}

		songNum = 1;
		/*Skip empty song*/
		i = tableOffset + 2;

		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Atlussong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
		free(romData);
	}
	else
	{
		fclose(rom);
		free(romData);
		printf("ERROR: Magic bytes not found!\n");
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void Atlussong2mid(int songNum, long ptr)
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
	int transpose = 0;
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
	long songLoopPt = 0;
	int songLoopAmt = 0;
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

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = 0x0000;
		}

		/*Get the pointer information from the header*/
		romPos = ptr;

		while (romData[romPos] != 0xFF && romPos < 0x7FF0)
		{
			tempByte = romData[romPos];

			switch (tempByte)
			{
			case 0x00:
				curTrack = 0;
				break;
			case 0x01:
				curTrack = 1;
				break;
			case 0x02:
				curTrack = 2;
				break;
			case 0x03:
				curTrack = 3;
				break;
			case 0x04:
				curTrack = 0;
				break;
			case 0x05:
				curTrack = 1;
				break;
			case 0x06:
				curTrack = 2;
				break;
			case 0x07:
				curTrack = 3;
				break;
			default:
				curTrack = 0;
				break;
			}

			seqPtrs[curTrack] = ReadLE16(&romData[romPos + 1]);
			romPos += 3;
		}

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
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			repeat3 = -1;
			transpose = 0;
			inMacro = 0;
			songLoopAmt = 0;
			songLoopPt = 0;


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
				seqPos = seqPtrs[curTrack] + 1;

				if (seqPos < bankAmt)
				{
					seqEnd = 1;
				}

				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note*/
					if (command[0] <= 0x7F)
					{
						/*Rest*/
						if (command[0] == 0x54)
						{
							curDelay += (chanSpeed * 5);
							ctrlDelay += (chanSpeed * 5);
						}

						else
						{
							curNote = command[0] + 24 + transpose;
							if (command[0] > 0x54)
							{
								curNote -= 0x54;
							}
							curNoteLen = chanSpeed * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;

						}
						seqPos++;
					}

					/*Change note length*/
					else if (command[0] >= 0x80 && command[0] <= 0xBF)
					{
						chanSpeed = romData[lenTable + (command[0] - 0x80)];
						seqPos++;
					}

					/*Set volume?*/
					else if (command[0] >= 0xC0 && command[0] <= 0xEF)
					{
						seqPos++;
					}

					/*Set note size*/
					else if (command[0] == 0xF0)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0xF1)
					{
						seqPos += 2;
					}

					/*Set envelope*/
					else if (command[0] == 0xF2)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0xF3)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Repeat the following section*/
					else if (command[0] == 0xF4)
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
					else if (command[0] == 0xF5)
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

					/*Go to macro*/
					else if (command[0] == 0xF6)
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

					/*Return from macro*/
					else if (command[0] == 0xF7)
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

					/*Go to song loop*/
					else if (command[0] == 0xF8)
					{
						seqEnd = 1;
					}

					/*Set tuning*/
					else if (command[0] == 0xF9)
					{
						seqPos += 2;
					}

					/*Set sweep effect*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Turn off sweep?*/
					else if (command[0] == 0xFB)
					{
						seqPos++;
					}

					/*Set waveform*/
					else if (command[0] == 0xFC)
					{
						seqPos += 3;
					}

					/*Unknown command FD*/
					else if (command[0] == 0xFD)
					{
						seqPos++;
					}

					/*Unknown command FE*/
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