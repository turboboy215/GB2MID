/*NMK (Rolan's Curse 1/2/Ninja Taro)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "ROLAN.H"

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
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int curInst;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

const char RCMagicBytes[5] = { 0x21, 0x31, 0xDD, 0x34, 0x21 };

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void RCsong2mid(int songNum, long ptr);

void RCProc(int bank)
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
		if ((!memcmp(&romData[i], RCMagicBytes, 5)) && foundTable != 1)
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
		while (ReadLE16(&romData[i]) > bankAmt && ReadLE16(&romData[i]) < (bankAmt * 2))
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0)
			{
				RCsong2mid(songNum, songPtr);
			}
			i += 2;
			songNum++;
		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		free(romData);
		exit(1);
	}
}

void RCsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long channels[4] = { 0, 0, 0, 0 };
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
	int octave = 0;
	int key = 0;
	float chanSpeed = 0;
	int transpose = 0;
	int jumpPos = 0;
	int repeat = 0;
	int repeatPos = 0;
	int loopPos = 0;
	int headerEnd = 0;
	int frequency = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long macro4Pos = 0;
	long macro4Ret = 0;
	long macro5Pos = 0;
	long macro5Ret = 0;
	unsigned char command[3];
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
	int isSFX = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int inMacro = 0;
	float noteNum = 0;

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

		romPos = ptr - bankAmt;

		/*Check to see if it is music or sound effects*/
		if (romData[romPos] < 0x04)
		{
			isSFX = 1;
		}
		else
		{
			isSFX = 0;
		}
		while (headerEnd == 0)
		{
			switch (romData[romPos])
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
			case 0xFF:
				headerEnd = 1;
				break;
			default:
				break;
			}

			if (romData[romPos] != 0xFF)
			{
				channels[curTrack] = ReadLE16(&romData[romPos + 1]);
				romPos += 3;
			}
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
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
			frequency = 0;
			transpose = 0;
			inMacro = 0;

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
				seqEnd = 0;
				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];

					/*Play note*/
					if (command[0] < 0xC0)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);

						switch (lowNibble)
						{
						case 0x00:
							noteNum = 1;
							break;
						case 0x01:
							noteNum = 192;
							break;
						case 0x02:
							noteNum = 144;
							break;
						case 0x03:
							noteNum = 96;
							break;
						case 0x04:
							noteNum = 72;
							break;
						case 0x05:
							noteNum = 48;
							break;
						case 0x06:
							noteNum = 36;
							break;
						case 0x07:
							noteNum = 24;
							break;
						case 0x08:
							noteNum = 12;
							break;
						case 0x09:
							noteNum = 18;
							break;
						case 0x0A:
							noteNum = 6;
							break;
						case 0x0B:
							noteNum = 6;
							break;
						case 0x0C:
							noteNum = 1;
							break;
						}

						/*Fix for RC1 song 7*/
						if (songNum == 7 && songPtr == 0x6E8B)
						{
							if (noteNum == 18)
							{
								noteNum = 7;
							}
						}

						/*Fix for RC2 song 12*/
						if (songNum == 12)
						{
							if (noteNum == 18)
							{
								noteNum = 12;
							}
							if (lowNibble == 0x0B)
							{
								noteNum = 0;
							}
						}

						/*Fix for Ninja Taro song 8*/
						if (songNum == 8 && songPtr == 0x7151)
						{
							if (noteNum == 18)
							{
								noteNum = 8;
							}
						}
						curNoteLen = noteNum * chanSpeed;
						curNote = highNibble + frequency + transpose;

						if (highNibble == 0x0F)
						{
							curDelay += curNoteLen;
							seqPos++;
						}
						else
						{

							if (curTrack == 3)
							{
								curNote += 24;
							}

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}
					}

					/*Select envelope*/
					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						seqPos++;
					}

					/*Set volume/envelope*/
					else if (command[0] >= 0xD0 && command[0] < 0xE0)
					{
						seqPos++;
					}

					/*Set instrument*/
					else if (command[0] == 0xE0)
					{
						seqPos += 3;
					}

					/*Set sweep*/
					else if (command[0] == 0xE1)
					{
						seqPos += 2;
					}

					/*Set frequency*/
					else if (command[0] == 0xE2)
					{
						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);

						octave = (lowNibble * 12) + 12;
						key = highNibble;
						frequency = octave + key;
						seqPos += 2;
					}

					/*Set envelope bit values*/
					else if (command[0] >= 0xE3 && command[0] <= 0xE8)
					{
						seqPos++;
					}

					/*Set noise tone*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0xEA)
					{
						if (command[1] < 0x80)
						{
							transpose = command[1];
						}
						else if (command[1] >= 0x80)
						{
							transpose = (command[1] - 0x80) * -1;
						}
						seqPos += 2;
					}

					/*Set transpose +1 octave*/
					else if (command[0] == 0xEB)
					{
						transpose += 12;
						seqPos++;
					}

					/*Set transpose -1 octave*/
					else if (command[0] == 0xEC)
					{
						transpose -= 12;
						seqPos++;
					}

					/*Reset channels 1-4?*/
					else if (command[0] >= 0xED && command[0] <= 0xEF)
					{
						seqPos++;
					}

					/*Set panning options?*/
					else if (command[0] >= 0xF0 && command[0] <= 0xF3)
					{
						seqPos++;
					}

					/*Set waveform*/
					else if (command[0] == 0xF4)
					{
						seqPos += 2;
					}

					/*Set channel speed value: 00*/
					else if (command[0] == 0xF5)
					{
						chanSpeed = 4;
						seqPos++;
					}

					/*Set channel speed value: 0C*/
					else if (command[0] == 0xF6)
					{
						chanSpeed = 3.5;
						seqPos++;
					}

					/*Set channel speed value: 18*/
					else if (command[0] == 0xF7)
					{
						chanSpeed = 3;
						seqPos++;
					}

					/*Set channel speed value: 24*/
					else if (command[0] == 0xF8)
					{
						chanSpeed = 2.5;
						seqPos++;
					}

					/*Go to loop point*/
					else if (command[0] == 0xF9)
					{
						seqEnd = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0xFA)
					{
						if (repeat == 0)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							seqPos = jumpPos;
						}
						else
						{
							seqPos += 3;
						}

					}

					/*Set repeat of section*/
					else if (command[0] == 0xFB)
					{
						if (repeat < 1)
						{
							repeat = command[1];
						}
						seqPos += 2;
					}

					/*Go to repeat point*/
					else if (command[0] == 0xFC)
					{
						repeat--;
						repeatPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
						if (repeat > 0)
						{
							/*Workaround for Ninja Taro crash*/
							if (repeatPos == 0x2D29 && songNum == 3)
							{
								seqPos += 3;
							}
							else
							{
								seqPos = repeatPos;
							}

						}
						else
						{
							seqPos += 3;
						}
					}

					/*Go to macro*/
					else if (command[0] == 0xFD)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro1Ret = seqPos + 3;
							inMacro = 1;
							seqPos = macro1Pos;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro2Ret = seqPos + 3;
							inMacro = 2;
							seqPos = macro2Pos;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro3Ret = seqPos + 3;
							inMacro = 3;
							seqPos = macro3Pos;
						}
						else if (inMacro == 3)
						{
							macro4Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro4Ret = seqPos + 3;
							inMacro = 4;
							seqPos = macro4Pos;
						}
						else if (inMacro == 4)
						{
							macro5Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro5Ret = seqPos + 3;
							inMacro = 5;
							seqPos = macro5Pos;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0xFE)
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
						else if (inMacro == 4)
						{
							seqPos = macro4Ret;
							inMacro = 3;
						}
						else if (inMacro == 5)
						{
							seqPos = macro5Ret;
							inMacro = 4;
						}

					}

					/*End of sequence*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

				}
				/*End of track*/
				WriteBE32(&midData[midPos], 0xFF2F00);
				midPos += 4;

				/*Calculate MIDI channel size*/
				trackSize = midPos - midTrackBase;
				WriteBE16(&midData[midTrackBase - 2], trackSize);
			}
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