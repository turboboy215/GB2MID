/*Data East*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DATAEAST.H"

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
int sysMode;
char string1[4];
int usePALTempo;

const char DEastMagicBytes[8] = { 0x4F, 0x06, 0x00, 0xCB, 0x21, 0xCB, 0x10, 0x09 };

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
void DEastsong2mid(int songNum, long ptr);

void DEastProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	sysMode = 2;
	usePALTempo = 0;

	bankAmt = 0x4000;
	if (bank < 2)
	{
		bank = 2;
	}
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = 0; (i < bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], DEastMagicBytes, 8)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset + 2;
		songNum = 1;

		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			DEastsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
		free(romData);
	}
	else
	{
		free(romData);
		printf("ERROR: Magic bytes not found!\n");
		fclose(rom);
		exit(2);
	}

}

/*Convert the song data to MIDI*/
void DEastsong2mid(int songNum, long ptr)
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
	int transpose1 = 0;
	int transpose2 = 0;
	long jumpPos1 = 0;
	long jumpPosRet1 = 0;
	long jumpPos2 = 0;
	long jumpPosRet2 = 0;
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
	int inMacro = 0;
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
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int masterDelay = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	int repeat3 = 0;
	long repeat1Pos = -1;
	long repeat2Pos = -1;
	long repeat3Pos = -1;
	int repeats[5][3];
	int repeatNum;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	unsigned int noteStore[4];
	int noteStoreLen = 0;

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

		/*Get the channel pointers*/
		romPos = songPtr + 3;

		seqPtrs[0] = ReadLE16(&romData[romPos + 6]);
		seqPtrs[1] = ReadLE16(&romData[romPos + 4]);
		seqPtrs[2] = ReadLE16(&romData[romPos + 2]);
		seqPtrs[3] = ReadLE16(&romData[romPos]);

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
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;

			transpose1 = 0;
			transpose2 = 0;

			inMacro = 0;
			jumpPos1 = 0;
			jumpPosRet1 = 0;
			jumpPos2 = 0;
			jumpPosRet2 = 0;
			repeat1 = -1;
			repeat2 = -1;
			repeat3 = -1;
			holdNote = 0;

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

			if (sysMode == 1)
			{
				if (seqPtrs[curTrack] >= bankAmt && seqPtrs[curTrack] < 0xC000)
				{
					seqEnd = 0;
					seqPos = seqPtrs[curTrack] - bankAmt;
				}
				else
				{
					seqEnd = 1;
				}
			}
			else
			{
				if (seqPtrs[curTrack] >= bankAmt && seqPtrs[curTrack] < 0x8000)
				{
					seqEnd = 0;
					seqPos = seqPtrs[curTrack];
				}
				else
				{
					seqEnd = 1;
				}
			}


			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 50000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				if (songNum == 35 && curTrack == 0)
				{
					songNum = 35;
				}

				/*Set note length*/
				if (command[0] <= 0x7F)
				{
					curNoteLen = command[0] * 5;
					seqPos++;
				}

				/*Play note*/
				else if (command[0] >= 0x80 && command[0] <= 0x8F)
				{
					if (curTrack == 3)
					{
						transpose1 = 0;
						transpose2 = 0;
					}
					curNote = (command[0] - 0x80) + transpose1 + transpose2 + 24;

					if (curTrack == 2)
					{
						curNote -= 12;
					}
					else if (curTrack == 3)
					{
						curNote += 24;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				/*Rest*/
				else if (command[0] == 0x90)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				/*Octave up*/
				else if (command[0] == 0x91)
				{
					transpose1 += 12;
					seqPos++;
				}

				/*Octave down*/
				else if (command[0] == 0x92)
				{
					transpose1 -= 12;
					seqPos++;
				}

				/*Set channel transpose*/
				else if (command[0] == 0x93)
				{
					transpose1 = (signed char)command[1];
					seqPos += 2;
				}

				/*Set noise envelope*/
				else if (command[0] == 0x94)
				{
					seqPos += 2;
				}

				/*Tie (slur) the following note*/
				else if (command[0] == 0x95)
				{
					seqPos++;
				}

				/*Set duty*/
				else if (command[0] == 0x96)
				{
					seqPos += 2;
				}

				/*Go to macro with base note*/
				else if (command[0] == 0x97)
				{

					if (sysMode == 1)
					{
						seqPos += 2;
					}
					else
					{
						transpose1 = (signed char)command[1];

						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&romData[seqPos + 2]);
							if (sysMode == 1)
							{
								macro1Pos -= bankAmt;
							}
							macro1Ret = seqPos + 4;
							seqPos = macro1Pos;
							inMacro = 1;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&romData[seqPos + 2]);
							if (sysMode == 1)
							{
								macro2Pos -= bankAmt;
							}
							macro2Ret = seqPos + 4;
							seqPos = macro2Pos;
							inMacro = 2;
						}
						else if (inMacro == 2)
						{
							macro3Pos = ReadLE16(&romData[seqPos + 2]);
							if (sysMode == 1)
							{
								macro3Pos -= bankAmt;
							}
							macro3Ret = seqPos + 4;
							seqPos = macro3Pos;
							inMacro = 3;
						}
						else if (inMacro == 3)
						{
							macro4Pos = ReadLE16(&romData[seqPos + 2]);
							if (sysMode == 1)
							{
								macro4Pos -= bankAmt;
							}
							macro4Ret = seqPos + 4;
							seqPos = macro4Pos;
							inMacro = 4;
						}
						else if (inMacro == 4)
						{
							macro5Pos = ReadLE16(&romData[seqPos + 2]);
							if (sysMode == 1)
							{
								macro5Pos -= bankAmt;
							}
							macro5Ret = seqPos + 4;
							seqPos = macro5Pos;
							inMacro = 5;
						}
					}
				}

				/*Command 98 (Invalid)*/
				else if (command[0] == 0x98)
				{
					seqPos++;
				}

				/*Set panning (left)*/
				else if (command[0] == 0x99)
				{
					seqPos += 2;
				}

				/*Set panning (right)*/
				else if (command[0] == 0x9A)
				{
					seqPos += 2;
				}

				/*Set channel envelope?*/
				else if (command[0] == 0x9B)
				{
					seqPos += 2;
				}

				/*Set channel transpose (v2)*/
				else if (command[0] == 0x9C)
				{
					transpose2 = (signed char)command[1];
					seqPos += 2;
				}

				/*Command 9D (Invalid)*/
				else if (command[0] == 0x9D)
				{
					seqPos++;
				}

				/*Command 9E (No effect)*/
				else if (command[0] == 0x9E)
				{
					seqPos++;
				}

				/*Command 9F (Invalid)*/
				else if (command[0] == 0x9F)
				{
					seqPos++;
				}

				/*Command A0 (Invalid)*/
				else if (command[0] == 0xA0)
				{
					seqPos++;
				}

				/*Jump to position*/
				else if (command[0] == 0xA1)
				{
					jumpPos1 = ReadLE16(&romData[seqPos + 1]);

					if (sysMode == 1)
					{
						jumpPos1 -= bankAmt;
					}

					if (sysMode == 1)
					{
						if (jumpPos1 >= 0xB000)
						{
							if (jumpPos1 == seqPtrs[curTrack])
							{
								seqEnd = 1;
							}
							else
							{
								seqPos = jumpPos1;
							}
						}

						else
						{
							seqEnd = 1;
						}
					}
					else
					{
						if (jumpPos1 >= 0x7000)
						{
							if (jumpPos1 == seqPtrs[curTrack])
							{
								seqEnd = 1;
							}
							else
							{
								seqPos = jumpPos1;
							}
						}

						else
						{
							seqEnd = 1;
						}
					}
				}

				/*End of channel*/
				else if (command[0] == 0xA2)
				{
					seqEnd = 1;
				}

				/*Repeat the following section (SFX)*/
				else if (command[0] == 0xA3)
				{
					if (repeat1 == -1)
					{
						repeat1 = command[1];
						repeat1Pos = seqPos + 2;
						seqPos = repeat1Pos;
					}
					else if (repeat2 == -1)
					{
						repeat2 = command[1];
						repeat2Pos = seqPos + 2;
						seqPos = repeat2Pos;
					}
					else
					{
						repeat3 = command[1];
						repeat3Pos = seqPos + 2;
						seqPos = repeat3Pos;
					}
				}

				/*End of repeat section (SFX)*/
				else if (command[0] == 0xA4)
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

				/*Command A5 (Invalid)*/
				else if (command[0] == 0xA5)
				{
					seqPos++;
				}

				/*Command A6 (Invalid)*/
				else if (command[0] == 0xA6)
				{
					seqPos++;
				}

				/*Set note length (SFX)*/
				else if (command[0] == 0xA7)
				{
					seqPos += 2;
				}

				/*Set frequency (SFX)*/
				else if (command[0] == 0xA8)
				{
					seqPos += 3;
				}

				/*Set duty (SFX)*/
				else if (command[0] == 0xA9)
				{
					seqPos += 2;
				}

				/*Command AA (Invalid)*/
				else if (command[0] == 0xAA)
				{
					seqPos++;
				}

				/*Unknown command AB*/
				else if (command[0] == 0xAB)
				{
					seqPos += 2;
				}

				/*Effect 1*/
				else if (command[0] == 0xAC)
				{
					seqPos += 2;
				}

				/*Command AD (Invalid)*/
				else if (command[0] == 0xAD)
				{
					seqPos++;
				}

				/*Command AE (Invalid)*/
				else if (command[0] == 0xAE)
				{
					seqPos++;
				}

				/*Command AF (Invalid)*/
				else if (command[0] == 0xAF)
				{
					seqPos++;
				}

				/*Effect 2*/
				else if (command[0] == 0xB0)
				{
					seqPos += 2;
				}

				/*Slide down effect*/
				else if (command[0] == 0xB1)
				{
					seqPos += 2;
				}

				/*Command B2 (Invalid)*/
				else if (command[0] == 0xB2)
				{
					seqPos++;
				}

				/*Command B3 (Invalid)*/
				else if (command[0] == 0xB3)
				{
					seqPos++;
				}

				/*Command B4 (Invalid)*/
				else if (command[0] == 0xB4)
				{
					seqPos++;
				}

				/*Command B5 (Invalid)*/
				else if (command[0] == 0xB5)
				{
					seqPos++;
				}

				/*End of channel (SFX)*/
				else if (command[0] == 0xB6)
				{
					seqEnd = 1;
				}

				/*Command B7 (Invalid)*/
				else if (command[0] == 0xB7)
				{
					seqPos++;
				}

				/*Command B8 (Invalid)*/
				else if (command[0] == 0xB8)
				{
					seqPos++;
				}

				/*Command B9 (Invalid)*/
				else if (command[0] == 0xB9)
				{
					seqPos++;
				}

				/*Command BA (Invalid)*/
				else if (command[0] == 0xBA)
				{
					seqPos++;
				}

				/*Set tempo*/
				else if (command[0] == 0xBB)
				{
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					tempo = command[2] * 0.6;
					if (usePALTempo == 1 && sysMode == 1)
					{
						tempo *= 0.83;
					}

					if (tempo < 2)
					{
						tempo = 150;
					}
					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;
					seqPos += 3;
				}

				/*Repeat the following section*/
				else if (command[0] == 0xBC)
				{
					if (repeat1 == -1)
					{
						repeat1 = command[1];
						repeat1Pos = seqPos + 2;
						seqPos = repeat1Pos;
					}
					else if (repeat2 == -1)
					{
						repeat2 = command[1];
						repeat2Pos = seqPos + 2;
						seqPos = repeat2Pos;
					}
					else
					{
						repeat3 = command[1];
						repeat3Pos = seqPos + 2;
						seqPos = repeat3Pos;
					}
				}

				/*End of repeat section*/
				else if (command[0] == 0xBD)
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

				/*Go to macro*/
				else if (command[0] == 0xBE)
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&romData[seqPos + 1]);
						if (sysMode == 1)
						{
							macro1Pos -= bankAmt;
						}
						macro1Ret = seqPos + 3;
						seqPos = macro1Pos;
						inMacro = 1;
					}
					else if (inMacro == 1)
					{
						macro2Pos = ReadLE16(&romData[seqPos + 1]);
						if (sysMode == 1)
						{
							macro2Pos -= bankAmt;
						}
						macro2Ret = seqPos + 3;
						seqPos = macro2Pos;
						inMacro = 2;
					}
					else if (inMacro == 2)
					{
						macro3Pos = ReadLE16(&romData[seqPos + 1]);
						if (sysMode == 1)
						{
							macro3Pos -= bankAmt;
						}
						macro3Ret = seqPos + 3;
						seqPos = macro3Pos;
						inMacro = 3;
					}
					else if (inMacro == 3)
					{
						macro4Pos = ReadLE16(&romData[seqPos + 1]);
						if (sysMode == 1)
						{
							macro4Pos -= bankAmt;
						}
						macro4Ret = seqPos + 3;
						seqPos = macro4Pos;
						inMacro = 4;
					}
					else if (inMacro == 4)
					{
						macro5Pos = ReadLE16(&romData[seqPos + 1]);
						if (sysMode == 1)
						{
							macro5Pos -= bankAmt;
						}
						macro5Ret = seqPos + 3;
						seqPos = macro5Pos;
						inMacro = 5;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Return from macro*/
				else if (command[0] == 0xBF)
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
					else
					{
						seqEnd = 1;
					}
				}

				/*Command C0 (Invalid)*/
				else if (command[0] == 0xC0)
				{
					seqPos++;
				}

				/*Set vibrato*/
				else if (command[0] == 0xC1)
				{
					seqPos += 2;
				}

				/*Command C2 (Invalid)*/
				else if (command[0] == 0xC2)
				{
					seqPos++;
				}

				/*Command C3 (Invalid)*/
				else if (command[0] == 0xC3)
				{
					seqPos++;
				}

				/*Set sweep 1*/
				else if (command[0] == 0xC4)
				{
					seqPos += 2;
				}

				/*Set sweep 2*/
				else if (command[0] == 0xC5)
				{
					seqPos += 2;
				}

				/*Set decay*/
				else if (command[0] == 0xC6)
				{
					seqPos += 2;
				}

				/*Set noise frequency*/
				else if (command[0] == 0xC7)
				{
					seqPos += 2;
				}

				/*Unknown command C8*/
				else if (command[0] == 0xC8)
				{
					seqPos += 2;
				}

				/*Hold note*/
				else if (command[0] == 0xC9)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				/*Set noise envelope effect*/
				else if (command[0] == 0xCA)
				{
					seqPos += 2;
				}

				/*Set volume*/
				else if (command[0] == 0xCB)
				{
					seqPos += 2;
				}

				/*Set waveform*/
				else if (command[0] == 0xCC)
				{
					seqPos += 17;
				}

				/*Command CD (No effect)*/
				else if (command[0] == 0xCD)
				{
					seqPos++;
				}

				/*Set noise sweep*/
				else if (command[0] == 0xCE)
				{
					seqPos += 2;
				}

				/*Set noise size*/
				else if (command[0] == 0xCF)
				{
					seqPos += 2;
				}

				/*Set note size*/
				else if (command[0] >= 0xD0 && command[0] <= 0xDF)
				{
					seqPos++;
				}

				/*Unknown command E0*/
				else if (command[0] == 0xE0)
				{
					seqPos += 2;
				}

				/*Unknown command E1*/
				else if (command[0] == 0xE1)
				{
					seqPos += 2;
				}

				/*Store note*/
				else if (command[0] >= 0xE2 && command[0] <= 0xE5)
				{
					noteStore[command[0] - 0xE2] = command[1] - 0x80;
					seqPos += 2;
				}

				/*Play stored note*/
				else if (command[0] >= 0xE6 && command[0] <= 0xE9)
				{
					if (curTrack == 3)
					{
						transpose1 = 0;
						transpose2 = 0;
					}
					curNote = noteStore[command[0] - 0xE6] + transpose1 + transpose2 + 24;

					if (curTrack == 2)
					{
						curNote -= 12;
					}
					else if (curTrack == 3)
					{
						curNote += 24;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				/*Command EA-EF (Invalid)*/
				else if (command[0] >= 0xEA && command[0] <= 0xEF)
				{
					seqPos++;
				}

				/*Unknown command*/
				else
				{
					seqPos++;
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