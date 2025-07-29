/*Saffire*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "SAFFIRE.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long offset;
long tablePtrLoc;
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
int curInst;
long firstPtr;
int drvVers;
int cfgPtr;
int exitError;
int fileExit;
int curVols[4];
int songTempo;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* multiMidData[4];
unsigned char* ctrlMidData;

long midLength;

char* argv3;
char string1[100];
char string2[100];
char SaffirecheckStrings[5][100] = { "numSongs=", "bank=", "start=", "tempo=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
void Saffiresong2mid(int songNum, long ptr, int tempo);

void SaffireProc(int bank, char parameters[4][100])
{
	curInst = 0;
	firstPtr = 0;
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, SaffirecheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtol(string1, NULL, 16);

		printf("Number of songs: %i\n", numSongs);
		fgets(string1, 3, cfg);

		/*Process each song*/
		songNum = 1;

		while (songNum <= numSongs)
		{
			/*Skip the first line*/
			fgets(string1, 10, cfg);

			/*Get the song bank*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, SaffirecheckStrings[1], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songBank = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get the start position of the song*/
			fgets(string1, 7, cfg);
			if (memcmp(string1, SaffirecheckStrings[2], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songPtr = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get the song tempo*/
			fgets(string1, 7, cfg);
			if (memcmp(string1, SaffirecheckStrings[3], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songTempo = strtol(string1, NULL, 16);

			printf("Song %i: 0x%04X, bank: %02X, tempo: %02X\n", songNum, songPtr, songBank, songTempo);

			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);

			fgets(string1, 3, cfg);

			Saffiresong2mid(songNum, songPtr, songTempo);

			songNum++;
		}
	}
}

void Saffiresong2mid(int songNum, long ptr, int tempo)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int curTempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int trackSizes[4];
	int onOff[4];
	int curNoteSize = 0;
	long totalTime = 0;
	long nextTime = 0;
	int transpose[4];
	unsigned char command[6];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	int firstNotes[4];
	unsigned int midPos = 0;
	unsigned int midPosM[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	long ctrlTrackSize = 0;
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	long tempPos = 0;
	int holdNote = 0;
	int rowTime = 0;
	int holdNotes[4];
	int delayTime[4];
	int prevNotes[4][2];
	int curTimes[4];
	int curNotesVal[4];
	int offFlag = 0;

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	for (j = 0; j < 4; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < 4; k++)
		{
			multiMidData[k][j] = 0;
		}

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

		curTempo = tempo * 30;
		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / curTempo);
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
			midPosM[curTrack] = 0;
			firstNotes[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);

			midPosM[curTrack] += 8;
			curNoteLens[curTrack] = 0;
			curDelays[curTrack] = 0;
			seqEnd = 0;
			midTrackBase = midPosM[curTrack];

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);

			onOff[curTrack] = 0;
			holdNotes[curTrack] = 0;
			delayTime[curTrack] = 0;
			prevNotes[curTrack][0] = 0;
			prevNotes[curTrack][1] = 0;

			seqEnd = 0;
			seqPos = songPtr;
			totalTime = 0;
			offFlag = 0;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			curVols[curTrack] = 100;
			curTimes[curTrack] = 0;
			curDelays[curTrack] = 0;
			onOff[curTrack] = 0;
			curNotesVal[curTrack] = 0;
		}

		while (seqEnd == 0 && romData[seqPos + 2] < 4)
		{
			command[0] = romData[seqPos];
			command[1] = romData[seqPos + 1];
			command[2] = romData[seqPos + 2];
			command[3] = romData[seqPos + 3];
			command[4] = romData[seqPos + 4];
			command[5] = romData[seqPos + 5];

			if (ReadBE16(&romData[seqPos]) < totalTime || ReadBE16(&romData[seqPos]) == 0xFFFF)
			{
				seqEnd = 1;
				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (onOff[curTrack] == 1)
					{
						curDelays[curTrack] = totalTime - curTimes[curTrack];
						tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
						midPosM[curTrack] = tempPos;
						curDelays[curTrack] = 0;
						onOff[curTrack] = 0;
					}
				}
			}
			else
			{
				totalTime = ReadBE16(&romData[seqPos]);

				curTrack = command[2];

				if (curNotesVal[curTrack] == 0x0000)
				{
					curDelays[curTrack] += ReadBE16(&romData[seqPos]) - curTimes[curTrack];
				}
				else
				{
					curDelays[curTrack] = ReadBE16(&romData[seqPos]) - curTimes[curTrack];
				}


				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
					midPosM[curTrack] = tempPos;
					curDelays[curTrack] = 0;
					onOff[curTrack] = 0;
				}

				curTimes[curTrack] = ReadBE16(&romData[seqPos]);

				curNotesVal[curTrack] = ReadBE16(&romData[seqPos + 4]);

				if (curNotesVal[curTrack] == 0x0000)
				{
					;
				}
				else
				{
					offFlag = 0;
					curVol = (command[3] & 0xF0) * 0.5;

					if (curVol == 0)
					{
						curVol = 1;
					}
					if (curNotesVal[curTrack] <= 0x0020)
					{
						curNotes[curTrack] = 12;
					}
					else if (curNotesVal[curTrack] >= 0x0020 && curNotesVal[curTrack] < 0x0095)
					{
						curNotes[curTrack] = 12;
					}
					else if (curNotesVal[curTrack] >= 0x0095 && curNotesVal[curTrack] < 0x00FD)
					{
						curNotes[curTrack] = 13;
					}
					else if (curNotesVal[curTrack] >= 0x00FD && curNotesVal[curTrack] < 0x016B)
					{
						curNotes[curTrack] = 14;
					}
					else if (curNotesVal[curTrack] >= 0x0160 && curNotesVal[curTrack] < 0x01C0)
					{
						curNotes[curTrack] = 15;
					}
					else if (curNotesVal[curTrack] >= 0x01C0 && curNotesVal[curTrack] < 0x021E)
					{
						curNotes[curTrack] = 16;
					}
					else if (curNotesVal[curTrack] >= 0x021E && curNotesVal[curTrack] < 0x0270)
					{
						curNotes[curTrack] = 17;
					}
					else if (curNotesVal[curTrack] >= 0x0270 && curNotesVal[curTrack] < 0x02C0)
					{
						curNotes[curTrack] = 18;
					}
					else if (curNotesVal[curTrack] >= 0x02C0 && curNotesVal[curTrack] < 0x0310)
					{
						curNotes[curTrack] = 19;
					}
					else if (curNotesVal[curTrack] >= 0x0310 && curNotesVal[curTrack] < 0x0350)
					{
						curNotes[curTrack] = 20;
					}
					else if (curNotesVal[curTrack] >= 0x0350 && curNotesVal[curTrack] < 0x0390)
					{
						curNotes[curTrack] = 21;
					}
					else if (curNotesVal[curTrack] >= 0x0390 && curNotesVal[curTrack] < 0x03D0)
					{
						curNotes[curTrack] = 22;
					}
					else if (curNotesVal[curTrack] >= 0x03D0 && curNotesVal[curTrack] < 0x0410)
					{
						curNotes[curTrack] = 23;
					}
					else if (curNotesVal[curTrack] >= 0x0410 && curNotesVal[curTrack] < 0x0440)
					{
						curNotes[curTrack] = 24;
					}
					else if (curNotesVal[curTrack] >= 0x0440 && curNotesVal[curTrack] < 0x0480)
					{
						curNotes[curTrack] = 25;
					}
					else if (curNotesVal[curTrack] >= 0x0480 && curNotesVal[curTrack] < 0x04B5)
					{
						curNotes[curTrack] = 26;
					}
					else if (curNotesVal[curTrack] >= 0x04B0 && curNotesVal[curTrack] < 0x04E0)
					{
						curNotes[curTrack] = 27;
					}
					else if (curNotesVal[curTrack] >= 0x04E0 && curNotesVal[curTrack] < 0x0510)
					{
						curNotes[curTrack] = 28;
					}
					else if (curNotesVal[curTrack] >= 0x0510 && curNotesVal[curTrack] < 0x053B)
					{
						curNotes[curTrack] = 29;
					}
					else if (curNotesVal[curTrack] >= 0x053B && curNotesVal[curTrack] < 0x0563)
					{
						curNotes[curTrack] = 30;
					}
					else if (curNotesVal[curTrack] >= 0x0563 && curNotesVal[curTrack] < 0x0589)
					{
						curNotes[curTrack] = 31;
					}
					else if (curNotesVal[curTrack] >= 0x0589 && curNotesVal[curTrack] < 0x05AC)
					{
						curNotes[curTrack] = 32;
					}
					else if (curNotesVal[curTrack] >= 0x05AC && curNotesVal[curTrack] < 0x05CE)
					{
						curNotes[curTrack] = 33;
					}
					else if (curNotesVal[curTrack] >= 0x05CE && curNotesVal[curTrack] < 0x05ED)
					{
						curNotes[curTrack] = 34;
					}
					else if (curNotesVal[curTrack] >= 0x05ED && curNotesVal[curTrack] < 0x060B)
					{
						curNotes[curTrack] = 35;
					}
					else if (curNotesVal[curTrack] >= 0x060B && curNotesVal[curTrack] < 0x0627)
					{
						curNotes[curTrack] = 36;
					}
					else if (curNotesVal[curTrack] >= 0x0627 && curNotesVal[curTrack] < 0x0642)
					{
						curNotes[curTrack] = 37;
					}
					else if (curNotesVal[curTrack] >= 0x0642 && curNotesVal[curTrack] < 0x065B)
					{
						curNotes[curTrack] = 38;
					}
					else if (curNotesVal[curTrack] >= 0x065B && curNotesVal[curTrack] < 0x0672)
					{
						curNotes[curTrack] = 39;
					}
					else if (curNotesVal[curTrack] >= 0x0672 && curNotesVal[curTrack] < 0x0689)
					{
						curNotes[curTrack] = 40;
					}
					else if (curNotesVal[curTrack] >= 0x0689 && curNotesVal[curTrack] < 0x069E)
					{
						curNotes[curTrack] = 41;
					}
					else if (curNotesVal[curTrack] >= 0x069E && curNotesVal[curTrack] < 0x06B2)
					{
						curNotes[curTrack] = 42;
					}
					else if (curNotesVal[curTrack] >= 0x06B2 && curNotesVal[curTrack] < 0x06C4)
					{
						curNotes[curTrack] = 43;
					}
					else if (curNotesVal[curTrack] >= 0x06C4 && curNotesVal[curTrack] < 0x06D6)
					{
						curNotes[curTrack] = 44;
					}
					else if (curNotesVal[curTrack] >= 0x06D6 && curNotesVal[curTrack] < 0x06E7)
					{
						curNotes[curTrack] = 45;
					}
					else if (curNotesVal[curTrack] >= 0x06E7 && curNotesVal[curTrack] < 0x06F7)
					{
						curNotes[curTrack] = 46;
					}
					else if (curNotesVal[curTrack] >= 0x06F7 && curNotesVal[curTrack] < 0x0706)
					{
						curNotes[curTrack] = 47;
					}
					else if (curNotesVal[curTrack] >= 0x0706 && curNotesVal[curTrack] < 0x0714)
					{
						curNotes[curTrack] = 48;
					}
					else if (curNotesVal[curTrack] >= 0x0714 && curNotesVal[curTrack] < 0x0721)
					{
						curNotes[curTrack] = 49;
					}
					else if (curNotesVal[curTrack] >= 0x0721 && curNotesVal[curTrack] < 0x072D)
					{
						curNotes[curTrack] = 50;
					}
					else if (curNotesVal[curTrack] >= 0x072D && curNotesVal[curTrack] < 0x0739)
					{
						curNotes[curTrack] = 51;
					}
					else if (curNotesVal[curTrack] >= 0x0739 && curNotesVal[curTrack] < 0x0744)
					{
						curNotes[curTrack] = 52;
					}
					else if (curNotesVal[curTrack] >= 0x0744 && curNotesVal[curTrack] < 0x074F)
					{
						curNotes[curTrack] = 53;
					}
					else if (curNotesVal[curTrack] >= 0x074F && curNotesVal[curTrack] < 0x0759)
					{
						curNotes[curTrack] = 54;
					}
					else if (curNotesVal[curTrack] >= 0x0759 && curNotesVal[curTrack] < 0x0762)
					{
						curNotes[curTrack] = 55;
					}
					else if (curNotesVal[curTrack] >= 0x0762 && curNotesVal[curTrack] < 0x076B)
					{
						curNotes[curTrack] = 56;
					}
					else if (curNotesVal[curTrack] >= 0x076B && curNotesVal[curTrack] < 0x0773)
					{
						curNotes[curTrack] = 57;
					}
					else if (curNotesVal[curTrack] >= 0x0773 && curNotesVal[curTrack] < 0x077B)
					{
						curNotes[curTrack] = 58;
					}
					else if (curNotesVal[curTrack] >= 0x077B && curNotesVal[curTrack] < 0x0783)
					{
						curNotes[curTrack] = 59;
					}
					else if (curNotesVal[curTrack] >= 0x0783 && curNotesVal[curTrack] < 0x078A)
					{
						curNotes[curTrack] = 60;
					}
					else if (curNotesVal[curTrack] >= 0x078A && curNotesVal[curTrack] < 0x0790)
					{
						curNotes[curTrack] = 61;
					}
					else if (curNotesVal[curTrack] >= 0x0790 && curNotesVal[curTrack] < 0x0797)
					{
						curNotes[curTrack] = 62;
					}
					else if (curNotesVal[curTrack] >= 0x0797 && curNotesVal[curTrack] < 0x079D)
					{
						curNotes[curTrack] = 63;
					}
					else if (curNotesVal[curTrack] >= 0x079D && curNotesVal[curTrack] < 0x07A2)
					{
						curNotes[curTrack] = 64;
					}
					else if (curNotesVal[curTrack] >= 0x07A2 && curNotesVal[curTrack] < 0x07A7)
					{
						curNotes[curTrack] = 65;
					}
					else if (curNotesVal[curTrack] >= 0x07A7 && curNotesVal[curTrack] < 0x07AC)
					{
						curNotes[curTrack] = 66;
					}
					else if (curNotesVal[curTrack] >= 0x07AC && curNotesVal[curTrack] < 0x07B1)
					{
						curNotes[curTrack] = 67;
					}
					else if (curNotesVal[curTrack] >= 0x07B1 && curNotesVal[curTrack] < 0x07B6)
					{
						curNotes[curTrack] = 68;
					}
					else if (curNotesVal[curTrack] >= 0x07B6 && curNotesVal[curTrack] < 0x07BA)
					{
						curNotes[curTrack] = 69;
					}
					else if (curNotesVal[curTrack] >= 0x07BA && curNotesVal[curTrack] < 0x07BE)
					{
						curNotes[curTrack] = 70;
					}
					else if (curNotesVal[curTrack] >= 0x07BE && curNotesVal[curTrack] < 0x07C1)
					{
						curNotes[curTrack] = 71;
					}
					else if (curNotesVal[curTrack] >= 0x07C1 && curNotesVal[curTrack] < 0x07C5)
					{
						curNotes[curTrack] = 72;
					}
					else if (curNotesVal[curTrack] >= 0x07C5 && curNotesVal[curTrack] < 0x07C8)
					{
						curNotes[curTrack] = 73;
					}
					else if (curNotesVal[curTrack] >= 0x07C8 && curNotesVal[curTrack] < 0x07CB)
					{
						curNotes[curTrack] = 74;
					}
					else if (curNotesVal[curTrack] >= 0x07CB && curNotesVal[curTrack] < 0x07CE)
					{
						curNotes[curTrack] = 75;
					}
					else if (curNotesVal[curTrack] >= 0x07CE && curNotesVal[curTrack] < 0x07D1)
					{
						curNotes[curTrack] = 76;
					}
					else if (curNotesVal[curTrack] >= 0x07D1 && curNotesVal[curTrack] < 0x07D4)
					{
						curNotes[curTrack] = 77;
					}
					else if (curNotesVal[curTrack] >= 0x07D4 && curNotesVal[curTrack] < 0x07D6)
					{
						curNotes[curTrack] = 78;
					}
					else if (curNotesVal[curTrack] >= 0x07D6 && curNotesVal[curTrack] < 0x07D9)
					{
						curNotes[curTrack] = 79;
					}
					else if (curNotesVal[curTrack] >= 0x07D9 && curNotesVal[curTrack] < 0x07DB)
					{
						curNotes[curTrack] = 80;
					}
					else if (curNotesVal[curTrack] >= 0x07DB && curNotesVal[curTrack] < 0x07DD)
					{
						curNotes[curTrack] = 81;
					}
					else if (curNotesVal[curTrack] >= 0x07DD && curNotesVal[curTrack] < 0x07DF)
					{
						curNotes[curTrack] = 82;
					}
					else if (curNotesVal[curTrack] >= 0x07DF && curNotesVal[curTrack] < 0x07E1)
					{
						curNotes[curTrack] = 83;
					}
					else if (curNotesVal[curTrack] >= 0x07E1 && curNotesVal[curTrack] < 0x07E2)
					{
						curNotes[curTrack] = 84;
					}
					else if (curNotesVal[curTrack] >= 0x07E2 && curNotesVal[curTrack] < 0x07E4)
					{
						curNotes[curTrack] = 85;
					}
					else if (curNotesVal[curTrack] >= 0x07E4 && curNotesVal[curTrack] < 0x07E6)
					{
						curNotes[curTrack] = 86;
					}
					else if (curNotesVal[curTrack] >= 0x07E6 && curNotesVal[curTrack] < 0x07E7)
					{
						curNotes[curTrack] = 87;
					}
					else if (curNotesVal[curTrack] >= 0x07E7 && curNotesVal[curTrack] < 0x07E9)
					{
						curNotes[curTrack] = 88;
					}
					else if (curNotesVal[curTrack] >= 0x07E9 && curNotesVal[curTrack] < 0x07EA)
					{
						curNotes[curTrack] = 89;
					}
					else if (curNotesVal[curTrack] >= 0x07EA && curNotesVal[curTrack] < 0x07EB)
					{
						curNotes[curTrack] = 90;
					}
					else if (curNotesVal[curTrack] >= 0x07EB && curNotesVal[curTrack] < 0x07EC)
					{
						curNotes[curTrack] = 91;
					}
					else if (curNotesVal[curTrack] >= 0x07EC && curNotesVal[curTrack] < 0x07ED)
					{
						curNotes[curTrack] = 92;
					}
					else if (curNotesVal[curTrack] >= 0x07ED && curNotesVal[curTrack] < 0x07EE)
					{
						curNotes[curTrack] = 93;
					}
					else if (curNotesVal[curTrack] >= 0x07EE && curNotesVal[curTrack] < 0x07EF)
					{
						curNotes[curTrack] = 94;
					}
					else if (curNotesVal[curTrack] >= 0x07EF && curNotesVal[curTrack] < 0x07F0)
					{
						curNotes[curTrack] = 95;
					}
					else if (curNotesVal[curTrack] >= 0x07F0 && curNotesVal[curTrack] < 0x07F1)
					{
						curNotes[curTrack] = 96;
					}
					else if (curNotesVal[curTrack] >= 0x07F1 && curNotesVal[curTrack] < 0x07F2)
					{
						curNotes[curTrack] = 97;
					}
					else if (curNotesVal[curTrack] >= 0x07F2 && curNotesVal[curTrack] < 0x07F2)
					{
						curNotes[curTrack] = 98;
					}
					else if (curNotesVal[curTrack] >= 0x07F3 && curNotesVal[curTrack] < 0x07F4)
					{
						curNotes[curTrack] = 99;
					}
					else if (curNotesVal[curTrack] >= 0x07F4 && curNotesVal[curTrack] < 0x07F5)
					{
						curNotes[curTrack] = 100;
					}
					else if (curNotesVal[curTrack] >= 0x07F4 && curNotesVal[curTrack] < 0x07F5)
					{
						curNotes[curTrack] = 101;
					}
					else if (curNotesVal[curTrack] >= 0x07F5 && curNotesVal[curTrack] < 0x07F6)
					{
						curNotes[curTrack] = 102;
					}
					else
					{
						curNotes[curTrack] = 103;
					}

					curNotes[curTrack] += 12;

					if (curTrack == 3)
					{
						curNotes[curTrack] = command[4];
					}

					tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
					firstNotes[curTrack] = 0;
					midPosM[curTrack] = tempPos;
					onOff[curTrack] = 1;
				}

				seqPos += 6;

			}

		}

		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
			midPosM[curTrack] += 4;
			firstNotes[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
		}

		free(multiMidData[0]);
		free(romData);
		fclose(mid);


	}
}