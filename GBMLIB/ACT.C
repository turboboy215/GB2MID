/*ACT Japan*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "ACT.H"

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
long notePtrLoc;
long notesOffset;
int drvVers;
int format;
char string1[4];
char string2[9];
int multiBanks;
int curBank;

char folderName[100];

const char ACTMagicBytes[7] = { 0xCB, 0xFF, 0x22, 0x23, 0xCB, 0xBE, 0x21 };
const char ACTNoteLoc[7] = { 0x09, 0x44, 0x4D, 0xE1, 0xC5, 0x0E, 0x04 };
const char ACTNoteLens[16] = { 0x60, 0x48, 0x40, 0x30, 0x24, 0x20, 0x18, 0x12, 0x10, 0x0C, 0x09, 0x08, 0x06, 0x04, 0x03, 0x02 };
const char ACTMagicBytesGG[7] = { 0xC0, 0xF6, 0x80, 0x32, 0xD2, 0xDE, 0x21 };
const char ACTNoteLocGG[5] = { 0x5F, 0x16, 0x00, 0xE5, 0x21 };

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
void ACTsong2mid(int songNum, long ptr);
void ACTsong2midGG(int songNum, long ptr);

void ACTProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	firstPtr = 0;
	drvVers = 1;
	format = 1;

	/*Check for GG ROM header*/
	fseek(rom, 0x7FF0, SEEK_SET);
	fgets(string2, 9, rom);
	if (!memcmp(string2, "TMR SEGA", 1))
	{
		format = 2;
	}

	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);

	if (format != 2)
	{
		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);

		if (bank != 1)
		{
			bankAmt = bankSize;
		}
		else
		{
			bankAmt = 0;
		}
	}
	else
	{
		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 4);
		fread(romData, 1, (bankSize * 4), rom);

		bankAmt = 0x8000;
	}

	if (format != 2)
	{
		/*Try to search the bank for song table loader*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], ACTMagicBytes, 7)) && foundTable != 1)
			{
				tablePtrLoc = i + 7;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}

		/*Now search for note lengths start offset (for earlier games)*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], ACTNoteLoc, 7)) && foundTable != 2)
			{
				notePtrLoc = i - 2;
				printf("Found pointer to note length tables at address 0x%04x!\n", notePtrLoc);
				notesOffset = ReadLE16(&romData[notePtrLoc]);
				printf("Note lengths start at 0x%04x...\n", notesOffset);
				foundTable = 2;
				drvVers = 1;
			}
		}
	}
	else
	{
		/*Try to search the bank for song table loader*/
		for (i = 0x8000; i < (bankSize * 4); i++)
		{
			if ((!memcmp(&romData[i], ACTMagicBytesGG, 7)) && foundTable != 1)
			{
				tablePtrLoc = i + 7;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}

		/*Now search for note lengths start offset (for earlier games)*/
		for (i = 0x8000; i < (bankSize * 4); i++)
		{
			if ((!memcmp(&romData[i], ACTNoteLocGG, 5)) && foundTable != 2)
			{
				notePtrLoc = i + 5;
				printf("Found pointer to note length tables at address 0x%04x!\n", notePtrLoc);
				notesOffset = ReadLE16(&romData[notePtrLoc]);
				printf("Note lengths start at 0x%04x...\n", notesOffset);
				foundTable = 2;
				drvVers = 1;
			}
		}
	}




	if (foundTable >= 1)
	{
		if (foundTable == 1)
		{
			drvVers = 2;
		}

		i = tableOffset;
		songNum = 1;

		if (format != 2)
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				ACTsong2mid(songNum, songPtr);
				i += 2;
				songNum++;
			}
		}
		else
		{
			while (ReadLE16(&romData[i]) >= 0x8000 && ReadLE16(&romData[i]) < 0xC000)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				ACTsong2midGG(songNum, songPtr);
				i += 2;
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
void ACTsong2mid(int songNum, long ptr)
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
	int repeat1 = 0;
	long repeat1Start = 0;
	int repeat2 = 0;
	long repeat2Start = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
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
	int speedAmt = 0;
	int prevNote = 0;
	int noteOne = 0;
	int playedNote = 0;

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
		romPos = ptr;
		if (drvVers == 1)
		{
			speedAmt = (romData[romPos] & 15) * 0x10;
		}
		else
		{
			tempo = romData[romPos] * 2;
			if (tempo < 2)
			{
				tempo = 150;
			}
		}
		romPos++;

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

		/*Get channel pointers*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
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
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			transpose = 0;
			inMacro = 0;
			songLoopAmt = 0;
			songLoopPt = 0;
			noteOne = 1;
			prevNote = 0;
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

			if (seqPtrs[curTrack] >= bankAmt && seqPtrs[curTrack] < 0x8000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				/*Set note*/
				if (command[0] <= 0x4F)
				{
					if (playedNote == 1 && curNoteLen != 0)
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						curNoteLen = 0;
					}
					curNote = command[0] + 24 + transpose;
					seqPos++;
				}

				/*Set decay/release*/
				else if (command[0] >= 0x50 && command[0] <= 0x5F)
				{
					seqPos++;
				}

				/*Set noise type*/
				else if (command[0] >= 0x60 && command[0] <= 0x6F)
				{
					curNote = 36 + command[0] - 0x60;
					seqPos++;
				}

				/*Set duty/volume*/
				else if (command[0] >= 0x70 && command[0] <= 0x7F)
				{
					seqPos++;
				}

				/*Add to note length*/
				else if (command[0] >= 0x80 && command[0] <= 0x8F)
				{
					if (curTrack == 1 && songNum == 3)
					{
						curTrack = 1;
					}
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0x80)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0x80]) * 5;
					}
					holdNote = 0;
					playedNote = 1;
					seqPos++;
				}

				/*Play note with length*/
				else if (command[0] >= 0x90 && command[0] <= 0x9F)
				{
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0x90)] * 5);
						ctrlDelay += (romData[notesOffset + speedAmt + (command[0] - 0x90)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0x90]) * 5;
						ctrlDelay += (ACTNoteLens[command[0] - 0x90]) * 5;
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					curNoteLen = 0;
					noteOne = 0;
					playedNote = 0;
					seqPos++;
				}

				/*Rest/quiet note*/
				else if (command[0] >= 0xA0 && command[0] <= 0xAF)
				{
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0xA0)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0xA0]) * 5;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					curNoteLen = 0;
					noteOne = 0;
					playedNote = 0;
					seqPos++;
				}

				/*Cut off note/rest?*/
				else if (command[0] >= 0xB0 && command[0] <= 0xBF)
				{
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0xB0)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0xB0]) * 5;
					}
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					curNoteLen = 0;
					seqPos++;
				}

				/*End of channel*/
				else if (command[0] == 0xC0)
				{
					if (playedNote == 1 && curNoteLen != 0)
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						curNoteLen = 0;
					}
					seqEnd = 1;
				}

				/*Repeat the following section (1)*/
				else if (command[0] == 0xC2)
				{
					repeat1 = command[1];
					seqPos += 2;
				}

				/*Repeat the following section (2)*/
				else if (command[0] == 0xC4)
				{
					repeat2 = command[1];
					seqPos += 2;
				}

				/*End of repeat section (1)*/
				else if (command[0] == 0xC6)
				{
					if (repeat1 > 1)
					{
						repeat1--;
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqPos += 3;
					}
				}

				/*End of repeat section (2)*/
				else if (command[0] == 0xC8)
				{
					if (repeat2 > 1)
					{
						repeat2--;
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqPos += 3;
					}
				}

				/*Go to macro*/
				else if (command[0] == 0xCA)
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&romData[seqPos + 1]);
						macro1Ret = seqPos + 3;
						seqPos = macro1Pos;
						inMacro = 1;
					}
					else if (inMacro == 1)
					{
						macro2Pos = ReadLE16(&romData[seqPos + 1]);
						macro2Ret = seqPos + 3;
						seqPos = macro2Pos;
						inMacro = 2;
					}
					else if (inMacro == 2)
					{
						macro3Pos = ReadLE16(&romData[seqPos + 1]);
						macro3Ret = seqPos + 3;
						seqPos = macro3Pos;
						inMacro = 3;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Go to song loop*/
				else if (command[0] == 0xCC)
				{
					if (playedNote == 1 && curNoteLen != 0)
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						curNoteLen = 0;
					}
					seqEnd = 1;
				}

				/*Return from macro*/
				else if (command[0] == 0xCE)
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

				/*Set pitch sweep speed*/
				else if (command[0] == 0xD0)
				{
					seqPos += 2;
				}

				/*Set tuning*/
				else if (command[0] == 0xD2)
				{
					seqPos += 2;
				}

				/*Set tuning to 0*/
				else if (command[0] == 0xD4)
				{
					seqPos++;
				}

				/*Set vibrato*/
				else if (command[0] == 0xD6)
				{
					seqPos += 2;
				}

				/*Reset tuning*/
				else if (command[0] == 0xD8)
				{
					transpose = 0;
					seqPos++;
				}

				/*Set transpose and delay*/
				else if (command[0] == 0xDA)
				{
					transpose = ReadLE16(&romData[seqPos + 1]);
					if (transpose >= 0x8000)
					{
						transpose -= 0x10000;
					}
					transpose /= 16;
					curNoteLen += (command[3] * 5);
					seqPos += 4;
				}

				/*Set envelope*/
				else if (command[0] == 0xDC)
				{
					seqPos += 2;
				}

				/*Set global volume?*/
				else if (command[0] == 0xDE)
				{
					seqPos += 2;
				}

				/*Set envelope (v2)*/
				else if (command[0] == 0xE0)
				{
					seqPos += 2;
				}

				/*Reset envelope?*/
				else if (command[0] == 0xE2)
				{
					seqPos++;
				}

				/*Set envelope (v3)*/
				else if (command[0] == 0xE4)
				{
					seqPos += 2;
				}

				/*Set pitch bend effect amount*/
				else if (command[0] == 0xE6)
				{
					seqPos += 3;
				}

				/*Sweep effect*/
				else if (command[0] == 0xE8)
				{
					seqPos += 2;
				}

				/*Set noise frequency?*/
				else if (command[0] == 0xEA)
				{
					seqPos += 2;
				}

				/*Noise effect?*/
				else if (command[0] == 0xEC)
				{
					seqPos += 2;
				}

				/*Set pitch effect?*/
				else if (command[0] == 0xEE)
				{
					seqPos += 2;
				}

				/*Set effect volume?*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				/*Unknown command F2*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				/*Unknown command F4*/
				else if (command[0] == 0xF4)
				{
					seqPos += 2;
				}

				/*Unknown command F6*/
				else if (command[0] == 0xF6)
				{
					seqPos += 2;
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

/*Convert the song data to MIDI*/
void ACTsong2midGG(int songNum, long ptr)
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
	int repeat1 = 0;
	long repeat1Start = 0;
	int repeat2 = 0;
	long repeat2Start = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
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
	int speedAmt = 0;
	int prevNote = 0;
	int noteOne = 0;
	int playedNote = 0;

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
		romPos = ptr;
		if (drvVers == 1)
		{
			speedAmt = (romData[romPos] & 15) * 0x10;
		}
		else
		{
			tempo = romData[romPos] * 2;
			if (tempo < 2)
			{
				tempo = 150;
			}
		}
		romPos++;

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

		/*Get channel pointers*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
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
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			transpose = 0;
			inMacro = 0;
			songLoopAmt = 0;
			songLoopPt = 0;
			noteOne = 1;
			prevNote = 0;
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

			if (seqPtrs[curTrack] >= 0x8000 && seqPtrs[curTrack] < 0xC000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0xC000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				/*Set note*/
				if (command[0] <= 0x4F)
				{
					if (playedNote == 1 && curNoteLen != 0)
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						curNoteLen = 0;
					}
					curNote = command[0] + 24 + transpose;
					seqPos++;
				}

				/*Set decay/release*/
				else if (command[0] >= 0x50 && command[0] <= 0x5F)
				{
					seqPos++;
				}

				/*Set noise type*/
				else if (command[0] >= 0x60 && command[0] <= 0x6F)
				{
					curNote = 36 + command[0] - 0x60;
					seqPos++;
				}

				/*Set duty/volume*/
				else if (command[0] >= 0x70 && command[0] <= 0x7F)
				{
					seqPos++;
				}

				/*Add to note length*/
				else if (command[0] >= 0x80 && command[0] <= 0x8F)
				{
					if (curTrack == 1 && songNum == 3)
					{
						curTrack = 1;
					}
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0x80)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0x80]) * 5;
					}
					holdNote = 0;
					playedNote = 1;
					seqPos++;
				}

				/*Play note with length*/
				else if (command[0] >= 0x90 && command[0] <= 0x9F)
				{
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0x90)] * 5);
						ctrlDelay += (romData[notesOffset + speedAmt + (command[0] - 0x90)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0x90]) * 5;
						ctrlDelay += (ACTNoteLens[command[0] - 0x90]) * 5;
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					curNoteLen = 0;
					noteOne = 0;
					playedNote = 0;
					seqPos++;
				}

				/*Rest/quiet note*/
				else if (command[0] >= 0xA0 && command[0] <= 0xAF)
				{
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0xA0)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0xA0]) * 5;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					curNoteLen = 0;
					noteOne = 0;
					playedNote = 0;
					seqPos++;
				}

				/*Cut off note/rest?*/
				else if (command[0] >= 0xB0 && command[0] <= 0xBF)
				{
					if (drvVers == 1)
					{
						curNoteLen += (romData[notesOffset + speedAmt + (command[0] - 0xB0)] * 5);
					}
					else
					{
						curNoteLen += (ACTNoteLens[command[0] - 0xB0]) * 5;
					}
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					curNoteLen = 0;
					seqPos++;
				}

				/*End of channel*/
				else if (command[0] == 0xC0)
				{
					if (playedNote == 1 && curNoteLen != 0)
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						curNoteLen = 0;
					}
					seqEnd = 1;
				}

				/*Repeat the following section (1)*/
				else if (command[0] == 0xC2)
				{
					repeat1 = command[1];
					seqPos += 2;
				}

				/*Repeat the following section (2)*/
				else if (command[0] == 0xC4)
				{
					repeat2 = command[1];
					seqPos += 2;
				}

				/*End of repeat section (1)*/
				else if (command[0] == 0xC6)
				{
					if (repeat1 > 1)
					{
						repeat1--;
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqPos += 3;
					}
				}

				/*End of repeat section (2)*/
				else if (command[0] == 0xC8)
				{
					if (repeat2 > 1)
					{
						repeat2--;
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqPos += 3;
					}
				}

				/*Go to macro*/
				else if (command[0] == 0xCA)
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&romData[seqPos + 1]);
						macro1Ret = seqPos + 3;
						seqPos = macro1Pos;
						inMacro = 1;
					}
					else if (inMacro == 1)
					{
						macro2Pos = ReadLE16(&romData[seqPos + 1]);
						macro2Ret = seqPos + 3;
						seqPos = macro2Pos;
						inMacro = 2;
					}
					else if (inMacro == 2)
					{
						macro3Pos = ReadLE16(&romData[seqPos + 1]);
						macro3Ret = seqPos + 3;
						seqPos = macro3Pos;
						inMacro = 3;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Go to song loop*/
				else if (command[0] == 0xCC)
				{
					if (playedNote == 1 && curNoteLen != 0)
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						curNoteLen = 0;
					}
					seqEnd = 1;
				}

				/*Return from macro*/
				else if (command[0] == 0xCE)
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

				/*Set pitch sweep speed*/
				else if (command[0] == 0xD0)
				{
					seqPos += 2;
				}

				/*Set tuning*/
				else if (command[0] == 0xD2)
				{
					seqPos += 2;
				}

				/*Set tuning to 0*/
				else if (command[0] == 0xD4)
				{
					seqPos++;
				}

				/*Set vibrato*/
				else if (command[0] == 0xD6)
				{
					seqPos += 2;
				}

				/*Reset tuning*/
				else if (command[0] == 0xD8)
				{
					transpose = 0;
					seqPos++;
				}

				/*Set transpose and delay*/
				else if (command[0] == 0xDA)
				{
					transpose = ReadLE16(&romData[seqPos + 1]);
					if (transpose >= 0x8000)
					{
						transpose -= 0x10000;
					}
					transpose /= 16;
					curNoteLen += (command[3] * 5);
					seqPos += 4;
				}

				/*Set envelope*/
				else if (command[0] == 0xDC)
				{
					seqPos += 2;
				}

				/*Set global volume?*/
				else if (command[0] == 0xDE)
				{
					seqPos += 2;
				}

				/*Set envelope (v2)*/
				else if (command[0] == 0xE0)
				{
					seqPos += 2;
				}

				/*Reset envelope?*/
				else if (command[0] == 0xE2)
				{
					seqPos++;
				}

				/*Set envelope (v3)*/
				else if (command[0] == 0xE4)
				{
					seqPos += 2;
				}

				/*Set pitch bend effect amount*/
				else if (command[0] == 0xE6)
				{
					seqPos += 3;
				}

				/*Sweep effect*/
				else if (command[0] == 0xE8)
				{
					seqPos += 2;
				}

				/*Set noise frequency?*/
				else if (command[0] == 0xEA)
				{
					seqPos += 2;
				}

				/*Noise effect?*/
				else if (command[0] == 0xEC)
				{
					seqPos += 2;
				}

				/*Set pitch effect?*/
				else if (command[0] == 0xEE)
				{
					seqPos += 2;
				}

				/*Set effect volume?*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				/*Unknown command F2*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				/*Unknown command F4*/
				else if (command[0] == 0xF4)
				{
					seqPos += 2;
				}

				/*Unknown command F6*/
				else if (command[0] == 0xF6)
				{
					seqPos += 2;
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