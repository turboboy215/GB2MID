/*Source #1 (Pyramids of Ra)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "SOURCE1.H"

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
int songTempo;
int numBars;
int transposes[2];
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int drvVers;
int curInst;
int progBank;
int curVol;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Source1song2mid(int songNum, long songPtrs[4]);

void Source1Proc(int bank)
{
	curInst = 0;
	foundTable = 0;
	firstPtr = 0;
	drvVers = SOURCE1_VER_STD;
	progBank = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	tableOffset = 0x4068;
	songNum = 1;
	i = tableOffset;

	while (songNum <= 5)
	{
		songPtrs[3] = ReadLE16(&romData[i]);
		songPtrs[1] = ReadLE16(&romData[i + 2]);
		songPtrs[0] = ReadLE16(&romData[i + 4]);
		songPtrs[2] = ReadLE16(&romData[i + 6]);
		songTempo = romData[i + 8];
		numBars = romData[i + 9];
		transposes[0] = (signed char)romData[i + 10];
		transposes[1] = (signed char)romData[i + 11];
		printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
		printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
		printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
		printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
		printf("Song %i tempo: %i\n", songNum, songTempo);
		printf("Song %i bars: %i\n", songNum, numBars);
		printf("Song %i channel 1 transpose: %i\n", songNum, transposes[0]);
		printf("Song %i channel 2 transpose: %i\n", songNum, transposes[1]);
		Source1song2mid(songNum, songPtrs);
		i += 12;
		songNum++;
	}
	free(romData);
}

/*Convert the song data to MIDI*/
void Source1song2mid(int songNum, long songPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	int curTrack = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int octave = 0;
	long jumpPos = 0;
	long jumpPosRet = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
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
	int masterDelay = 0;
	int holdNote = 0;
	long tempPos = 0;
	int curBar = 0;
	int curTime = 0;

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

		tempo = 0xFFFF / songTempo / 30;
		if (tempo < 2)
		{
			tempo = 2;
		}

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

		switch (drvVers)
		{
		case SOURCE1_VER_STD:
			/*Fall-through*/
		default:
			SOURCE1_STATUS_STOP_TRACK = 0x00;
			SOURCE1_STATUS_STOP = 0x01;
			SOURCE1_STATUS_PARK = 0x74;
			SOURCE1_STATUS_NOTE_MIN = 0x90;
			SOURCE1_STATUS_NOTE_MAX = 0xBF;
			SOURCE1_STATUS_PROG_CHANGE_MIN = 0xC0;
			SOURCE1_STATUS_PROG_CHANGE_MAX = 0xFF;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			jumpPos = 0;
			jumpPosRet = 0;
			holdNote = 0;
			curVol = 120;

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

			if (songPtrs[curTrack] != 0x0000)
			{
				seqEnd = 0;
				seqPos = songPtrs[curTrack];
				curTime = 0;

				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
					command[4] = romData[seqPos + 4];

					curBar = ((ReadBE16(&romData[seqPos])) & 0xFFF0) >> 4;
					curDelay = ((curBar * 0x2FC) + (ReadBE16(&romData[seqPos + 1]) & 0x0FFF) - curTime);
					curTime = (curBar * 0x2FC) + (ReadBE16(&romData[seqPos + 1]) & 0x0FFF);

					if (curBar >= numBars)
					{
						seqEnd = 1;
					}
					else
					{
						if (command[3] == SOURCE1_STATUS_STOP)
						{
							seqEnd = 1;
						}

						else if (command[3] == SOURCE1_STATUS_STOP_TRACK)
						{
							seqEnd = 1;
						}

						else if (command[3] == SOURCE1_STATUS_PARK)
						{
							seqEnd = 1;
						}

						else if (command[3] >= SOURCE1_STATUS_NOTE_MIN && command[3] <= SOURCE1_STATUS_NOTE_MAX)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							curNote = command[4];
							/*
							if (curTrack > 2)
							{
								curNote += transposes[curTrack];
							}
							*/
							midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 1;
							seqPos += 5;
						}

						else if (command[3] >= SOURCE1_STATUS_PROG_CHANGE_MIN && command[3] <= SOURCE1_STATUS_PROG_CHANGE_MAX)
						{
							curInst = command[3] - SOURCE1_STATUS_PROG_CHANGE_MIN;
							seqPos += 5;
						}

						/*Unknown command*/
						else
						{
							seqPos += 5;
						}
					}
				}
			}
			if (holdNote == 1)
			{
				tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
				curDelay = 0;
				holdNote = 0;
				midPos = tempPos;
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