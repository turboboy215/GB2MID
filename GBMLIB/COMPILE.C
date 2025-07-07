/*Compile*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "COMPILE.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4][4];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int drvVers;
int songBank;

const char CompileMagicBytes[6] = { 0x4F, 0x87, 0x5F, 0x16, 0x00, 0x21 };
const char CompileNoteLens[16] = { 0x01, 0x02, 0x03, 0x04, 0x06, 0x08, 0x0C, 0x10, 0x18, 0x20, 0x30, 0x09, 0x12, 0x1E, 0x24, 0x2A };

unsigned char* romData;
unsigned char* exRomData;
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
void Compilesong2mid(int songNum, long ptr);

void CompileProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	firstPtr = 0;

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

	/*See if the first 2 bytes are pointer to the song table*/
	if (ReadLE16(&romData[0x4000]) >= 0x4000 && ReadLE16(&romData[0x4000]) < 0x8000)
	{
		tablePtrLoc = 0x4000;
		printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
		tableOffset = ReadLE16(&romData[tablePtrLoc]);
		printf("Song table starts at 0x%04x...\n", tableOffset);
		foundTable = 1;
		drvVers = 2;
	}

	else
	{
		/*Try to search the bank for song table loader (Godzilla)*/
		for (i = bankAmt; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], CompileMagicBytes, 6)) && foundTable != 1)
			{
				tablePtrLoc = i + 6;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
				drvVers = 1;
			}
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		if (drvVers == 1)
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
			{
				songBank = bank;

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);

				songPtr = ReadLE16(&romData[i]);
				if (songNum != 14 && songNum != 15 && songNum != 16)
				{
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Compilesong2mid(songNum, songPtr);
				}
				else
				{
					printf("Song %i: 0x%04X (invalid)\n", songNum, songPtr);
				}

				i += 2;
				songNum++;
			}
		}
		else
		{
			if (tableOffset == 0x43BD)
			{
				drvVers = 3;
			}
			while (ReadLE16(&romData[i + 1]) >= bankAmt && ReadLE16(&romData[i + 1]) < 0x8000)
			{
				songBank = romData[i] + 1;

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);

				songPtr = ReadLE16(&romData[i + 1]);
				printf("Song %i: 0x%04X, bank %01X\n", songNum, songPtr, songBank);
				Compilesong2mid(songNum, songPtr);
				i += 3;
				songNum++;
			}
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
void Compilesong2mid(int songNum, long ptr)
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
	int transpose1 = 0;
	int transpose2 = 0;
	int repeat1 = 0;
	long repeat1Pos = 0;
	int repeat2 = 0;
	long repeat2Pos = 0;
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
	int repeat = 0;
	long repeatStart;
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
	int numTracks = 0;
	unsigned int noteVal = 0;
	int firstTrack = 0;

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

		/*Get the channel information*/
		romPos = ptr;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPtrs[curTrack][0] = 0x0000;
		}

		numTracks = exRomData[romPos];
		romPos++;

		for (k = 0; k < numTracks; k++)
		{
			curTrack = exRomData[romPos + 7];
			/*Pointer*/
			seqPtrs[curTrack][0] = ReadLE16(&exRomData[romPos + 8]);
			/*Transpose*/
			seqPtrs[curTrack][1] = (signed char)exRomData[romPos + 5];
			/*Tempo*/
			seqPtrs[curTrack][2] = exRomData[romPos + 6];
			romPos += 12;
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

			if (seqPtrs[curTrack][0] != 0x0000)
			{
				seqEnd = 0;
				romPos += 2;
				seqPos = seqPtrs[curTrack][0];
				transpose1 = seqPtrs[curTrack][1];
				transpose2 = 0;

				/*Set initial tempo*/
				if (firstTrack == 0)
				{

					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					tempo = seqPtrs[curTrack][2] * 1.2;

					if (tempo < 2)
					{
						tempo = 150;
					}

					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;
					firstTrack = 1;
				}
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];

				/*Play note*/
				if (command[0] <= 0x7F)
				{
					noteVal = command[0];

					if (command[1] == 0xDE)
					{
						curNoteLen = command[2] * 10;

						if (noteVal == 0x00)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = noteVal + 23 + transpose1 + transpose2;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos += 3;
					}
					else if (command[1] >= 0xDF)
					{
						curNoteLen = CompileNoteLens[command[1] - 0xDF] * 10;

						if (noteVal == 0x00)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = noteVal + 23 + transpose1 + transpose2;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos += 2;
					}
					else
					{
						if (noteVal == 0x00)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = noteVal + 23 + transpose1 + transpose2;
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

				/*Go to song loop*/
				else if (command[0] == 0x80)
				{
					seqEnd = 1;
				}

				/*End of repeat - go to position*/
				else if (command[0] == 0x81)
				{
					if (repeat2 == -1)
					{
						if (repeat1 > 1)
						{
							seqPos = ReadLE16(&exRomData[seqPos + 2]);
							repeat1--;
						}
						else if (repeat1 <= 1)
						{
							repeat1 = -1;
							seqPos += 4;
						}
					}
					else if (repeat2 > -1)
					{
						if (repeat2 > 1)
						{
							seqPos = ReadLE16(&exRomData[seqPos + 2]);
							repeat2--;
						}
						else if (repeat2 <= 1)
						{
							repeat2 = -1;
							seqPos += 4;
						}
					}
				}

				/*End of channel (no loop)*/
				else if (command[0] == 0x82)
				{
					seqEnd = 1;
				}

				/*Vibrato*/
				else if (command[0] == 0x83)
				{
					seqPos += 2;
				}

				/*Portamento time*/
				else if (command[0] == 0x84)
				{
					seqPos += 2;
				}

				/*Play song ID*/
				else if (command[0] == 0x85)
				{
					seqPos += 2;
				}

				/*Unknown command 86*/
				else if (command[0] == 0x86)
				{
					seqPos++;
				}

				/*Set volume/envelope (v1)*/
				else if (command[0] == 0x87)
				{
					seqPos += 2;
				}

				/*Set volume/envelope (v2)*/
				else if (command[0] == 0x88)
				{
					seqPos += 2;
				}

				/*Set transpose*/
				else if (command[0] == 0x89)
				{
					transpose2 = (signed char)command[1];
					seqPos += 2;
				}

				/*Volume release*/
				else if (command[0] == 0x8A)
				{
					seqPos += 2;
				}

				/*Unknown command 8B*/
				else if (command[0] == 0x8B)
				{
					seqPos += 3;
				}

				/*Unknown command 8C*/
				else if (command[0] == 0x8C)
				{
					seqPos += 2;
				}

				/*Repeat the following section*/
				else if (command[0] == 0x8D)
				{
					if (repeat1 == -1)
					{
						repeat1 = command[2];
						seqPos += 3;
					}
					else
					{
						repeat2 = command[2];
						seqPos += 3;
					}
				}

				/*Unknown command 8E*/
				else if (command[0] == 0x8E)
				{
					seqPos += 2;
				}

				/*Unknown command 8F*/
				else if (command[0] == 0x8F)
				{
					seqPos += 2;
				}

				/*Flags*/
				else if (command[0] == 0x90)
				{
					seqPos += 2;
				}

				/*Unknown command 91*/
				else if (command[0] == 0x91)
				{
					seqPos += 2;
				}

				/*Unknown command 92*/
				else if (command[0] == 0x92)
				{
					seqPos += 2;
				}

				/*Unknown command 93*/
				else if (command[0] == 0x93)
				{
					seqPos += 3;
				}

				/*Unknown command 94*/
				else if (command[0] == 0x94)
				{
					seqPos += 2;
				}

				/*Unknown command 95*/
				else if (command[0] == 0x95)
				{
					seqPos += 2;
				}

				/*Set tempo*/
				else if (command[0] == 0x96)
				{
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					tempo = seqPtrs[curTrack][2] * 1.2;

					if (tempo < 2)
					{
						tempo = 150;
					}

					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;
					firstTrack = 1;
					seqPos += 2;
				}

				/*Set tuning*/
				else if (command[0] == 0x97)
				{
					seqPos += 2;
				}

				/*Unknown command 98*/
				else if (command[0] == 0x98)
				{
					seqPos += 2;
				}

				/*Unknown command 99*/
				else if (command[0] == 0x99)
				{
					seqPos++;
				}

				/*Go to macro*/
				else if (command[0] == 0x9A)
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&exRomData[seqPos + 1]);
						macro1Ret = seqPos + 3;
						seqPos = macro1Pos;
						inMacro = 1;
					}
					else if (inMacro == 1)
					{
						macro2Pos = ReadLE16(&exRomData[seqPos + 1]);
						macro2Ret = seqPos + 3;
						seqPos = macro2Pos;
						inMacro = 2;
					}
					else if (inMacro == 2)
					{
						macro3Pos = ReadLE16(&exRomData[seqPos + 1]);
						macro3Ret = seqPos + 3;
						seqPos = macro3Pos;
						inMacro = 1;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Return from macro*/
				else if (command[0] == 0x9B)
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
					else
					{
						seqEnd = 1;
					}
				}

				/*Unknown command 9C*/
				else if (command[0] == 0x9C)
				{
					seqPos++;
				}

				/*Unknown command 9D*/
				else if (command[0] == 0x9D)
				{
					seqPos += 2;
				}

				/*Unknown command 9E*/
				else if (command[0] == 0x9E)
				{
					seqPos++;
				}

				/*Set ADSR*/
				else if (command[0] == 0x9F)
				{
					seqPos += 2;
				}

				/*Change program?*/
				else if (command[0] == 0xA0)
				{
					seqPos += 2;
				}

				/*Set portamento*/
				else if (command[0] == 0xA1)
				{
					seqPos += 2;
				}

				/*Set waveform*/
				else if (command[0] == 0xA2)
				{
					if (drvVers == 3)
					{
						seqPos++;
					}
					else
					{
						seqPos += 2;
					}

				}

				/*Panpot envelope*/
				else if (command[0] == 0xA3)
				{
					seqPos += 2;
					if (drvVers == 3)
					{
						seqPos--;
					}
				}

				/*Unknown command A4*/
				else if (command[0] == 0xA4)
				{
					seqPos += 2;
				}

				/*Unknown command A5*/
				else if (command[0] == 0xA5)
				{
					seqPos += 2;
				}

				/*Play percussion note*/
				else if (command[0] >= 0xC0 && command[0] <= 0xDD)
				{
					noteVal = command[0] - 0xC0;

					if (command[1] == 0xDE)
					{
						curNoteLen = command[2] * 10;

						if (noteVal == 0x00)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = noteVal + 36 + transpose1 + transpose2;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos += 3;
					}
					else if (command[1] >= 0xDF)
					{
						curNoteLen = CompileNoteLens[command[1] - 0xDF] * 10;

						if (noteVal == 0x00)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = noteVal + 36 + transpose1 + transpose2;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos += 2;
					}
					else
					{
						if (noteVal == 0x00)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = noteVal + 36 + transpose1 + transpose2;
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

				/*Play note with manual length*/
				else if (command[0] == 0xDE)
				{
					curNoteLen = command[1] * 10;

					if (noteVal == 0)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
					}
					else
					{
						curNote = noteVal + 23 + transpose1 + transpose2;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
					}
					seqPos += 2;
				}

				/*Play note*/
				else if (command[0] >= 0xDF)
				{
					curNoteLen = CompileNoteLens[command[0] - 0xDF] * 10;

					if (noteVal == 0)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
					}
					else
					{
						curNote = noteVal + 23 + transpose1 + transpose2;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
					}
					seqPos++;
				}

				/*Unknown command*/
				else
				{
					seqPos += 2;
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
		free(exRomData);
		fclose(mid);
	}
}