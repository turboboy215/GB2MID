/*Torus (1st driver)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <math.h>
#include "SHARED.H"
#include "TORUS1.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
int totalSongs;
int songBank;
long songPtr;
long bankAmt;
int curInst;
int drvVers;
int curVol;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* multiMidData[8];
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
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
void Torus1song2mid(int songNum, long songPtr);

int gbFreq2Note(unsigned int freq);

void Torus1Proc(int bank, char parameters[4][100])
{
	drvVers = TORUS1_VER_STD;
	curVol = 120;

	if (bank < 2)
	{
		bank = 2;
	}

	tableOffset = strtol(parameters[0], NULL, 16);

	totalSongs = strtol(parameters[1], NULL, 16);

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	i = tableOffset;
	songNum = 1;

	while (songNum <= totalSongs)
	{
		songBank = romData[i];
		songPtr = ReadLE16(&romData[i + 1]);
		printf("Song %i: 0x%04X, bank %02X\n", songNum, songPtr, songBank);

		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, (songBank * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
		Torus1song2mid(songNum, songPtr);
		free(exRomData);
		i += 3;
		songNum++;
	}

	free(romData);

}

/*Convert the song data to MIDI*/
void Torus1song2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	unsigned int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	int masterDelay = 0;
	int masterDelays[4];
	int curInst = 0;
	int songEnd = 0;
	int rowsLeft = 0;
	unsigned int seqPos = 0;
	unsigned int seqPosM[4];
	unsigned int romPos = 0;
	unsigned int midPos = 0;
	unsigned int midPosM[4];
	long ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	long tempPos = 0;
	int valSize = 0;
	long trackSize = 0;
	long trackSizes[4];
	long ctrlTrackSize = 0;
	int curVols[4];
	int holdNote = 0;
	int holdNotes[4];
	int firstNote = 0;
	int firstNotes[4];
	int rowTime;
	int mask;
	int maskArray[8];
	int curFreq;
	int onOff[4];
	int activeChan[4];
	int startDelay = 0;
	unsigned int tempByte;

	long seqTime = 0;
	int curInsts[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	for (curTrack = 0; curTrack < trackCnt; curTrack++)
	{
		midPosM[curTrack] = 0;
	}

	midLength = 0x10000;

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < trackCnt; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < trackCnt; k++)
		{
			multiMidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < trackCnt; k++)
		{
			multiMidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
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
			midPosM[curTrack] = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			curNoteLens[curTrack] = 0;
			curDelays[curTrack] = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			onOff[curTrack] = 0;
			seqEnd = 0;
			midTrackBase = midPosM[curTrack];
			curInsts[curTrack] = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
			midPosM[curTrack] += valSize;
			WriteBE16(&multiMidData[curTrack][midPosM[curTrack]], 0xFF03);
			midPosM[curTrack] += 2;
			Write8B(&multiMidData[curTrack][midPosM[curTrack]], strlen(TRK_NAMES[curTrack]));
			midPosM[curTrack]++;
			sprintf((char*)&multiMidData[curTrack][midPosM[curTrack]], TRK_NAMES[curTrack]);
			midPosM[curTrack] += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);

		}

		/*Process song header*/
		rowsLeft = ReadLE16(&exRomData[songPtr + 1]);
		startDelay = exRomData[songPtr + 3];
		if (startDelay != 0)
		{
			startDelay++;
		}
		seqPos = songPtr + 4;

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			curDelays[curTrack] = (startDelay * 5);
		}

		while (rowsLeft > 0)
		{
			mask = exRomData[seqPos];

			for (j = 0; j < 8; j++)
			{
				maskArray[j] = 0;
			}

			/*Channel 1*/
			if ((mask & 0x01) != 0x00)
			{
				maskArray[0] = 1;
			}
			/*Channel 2*/
			if ((mask & 0x02) != 0x00)
			{
				maskArray[1] = 1;
			}
			/*Channel 3*/
			if ((mask & 0x04) != 0x00)
			{
				maskArray[2] = 1;
			}
			/*Channel 4*/
			if ((mask & 0x08) != 0x00)
			{
				maskArray[3] = 1;
			}
			/*NR51 - channel 1*/
			if ((mask & 0x10) != 0x00)
			{
				maskArray[4] = 1;
			}
			/*NR51 - channel 2*/
			if ((mask & 0x20) != 0x00)
			{
				maskArray[5] = 1;
			}
			/*NR51 - channel 3*/
			if ((mask & 0x40) != 0x00)
			{
				maskArray[6] = 1;
			}
			/*NR51 - channel 4*/
			if ((mask & 0x80) != 0x00)
			{
				maskArray[7] = 1;
			}
			seqPos++;

			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				if (maskArray[curTrack + 4] == 0 && holdNotes[curTrack] == 1)
				{
					tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
					holdNotes[curTrack] = 0;
					midPosM[curTrack] = tempPos;
					curDelays[curTrack] = 0;
				}
				if (maskArray[curTrack] != 0)
				{
					if (holdNotes[curTrack] == 1)
					{
						tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
						holdNotes[curTrack] = 0;
						midPosM[curTrack] = tempPos;
						curDelays[curTrack] = 0;
					}
					curFreq = ReadLE16(&exRomData[seqPos]) & 0x07FF;

					if (curTrack != 3)
					{
						curNotes[curTrack] = gbFreq2Note(curFreq) - 1;
						if (curTrack == 2)
						{
							curNotes[curTrack] -= 12;
						}
						onOff[curTrack] = 1;
						seqPos += 2;
					}
					else
					{
						curNotes[curTrack] = exRomData[seqPos];
						seqPos++;
					}

					tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
					holdNotes[curTrack] = 1;
					firstNotes[curTrack] = 0;
					midPosM[curTrack] = tempPos;
					curDelays[curTrack] = 0;
					onOff[curTrack] = 0;
				}
			}

			rowTime = exRomData[seqPos];
			if (rowTime != 0)
			{
				rowTime++;
			}
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				curDelays[curTrack] += (rowTime * 5);
			}
			rowsLeft--;
			seqPos++;
		}

		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (holdNotes[curTrack] == 1)
			{
				tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
				holdNotes[curTrack] = 0;
				midPosM[curTrack] = tempPos;
				curDelays[curTrack] = 0;
			}

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

		for (k = 0; k < trackCnt; k++)
		{
			free(multiMidData[k]);
		}
		free(ctrlMidData);
		fclose(mid);

	}

}