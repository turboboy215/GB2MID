/*Lucky Chicken Games*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "CHICKEN.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;

int i, j;
char outfile[1000000];
int songNum;
long songPtr;
int songBank;
long bankAmt;
int curVol;
int curInst;
int drvVers;
int foundTable;
int progBank;
int verOverride;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* multiMidData[4];
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
void Chickensong2mid(int songNum, long songPtr);

void ChickenProc(int bank, char parameters[4][100])
{
	drvVers = CHICKEN_VER_STD;
	curVol = 120;
	curInst = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	if (parameters[0][0] != 0)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	songNum = 1;
	i = tableOffset;

	printf("Song table: 0x%04X\n", tableOffset);

	while (ReadLE16(&romData[i]) != 0xFFFF)
	{
		songPtr = ReadLE16(&romData[i]);
		songBank = ReadLE16(&romData[i + 2]);
		printf("Song %i: 0x%04X, bank %02X\n", songNum, songPtr, songBank);
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, (songBank * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
		Chickensong2mid(songNum, songPtr);
		free(exRomData);
		i += 4;
		songNum++;
	}
	free(romData);
}

/*Convert the song data to MIDI*/
void Chickensong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	long romPos = 0;
	int curDelay[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int curNote[4];
	int curNoteLen[4];
	int rowTime = 0;
	int songEnd = 0;
	int curSeq = 0;
	long seqPtr = 0;
	int seqEnd = 0;
	int k = 0;
	int curTrack = 0;
	unsigned int command[3];
	int curInsts[4];
	int firstNote[16];
	int midPos[4];
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	long tempPos = 0;
	int trackSize[4];
	int ctrlTrackSize = 0;
	int onOff[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int valSize = 0;
	long songStart = 0;
	long patOffset = 0;
	int numSeqs = 0;
	int ctrlDelay = 0;
	int curVols[4];
	long curFreq;

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

	for (k = 0; k < 4; k++)
	{
		onOff[k] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		seqPos = songPtr;
		seqEnd = 0;

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

		switch (drvVers)
		{
		case CHICKEN_VER_STD:
			/*Fall-through*/
		default:
			EventMap[0x01] = CHICKEN_EVENT_NOTE_ON;
			EventMap[0x02] = CHICKEN_EVENT_NOTE_OFF;
			EventMap[0x03] = CHICKEN_EVENT_PROG_CHANGE;
			EventMap[0x04] = CHICKEN_EVENT_PAN;
			EventMap[0xFE] = CHICKEN_EVENT_SPECIAL;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPos[curTrack] = 0;
			firstNote[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPos[curTrack]], 0x4D54726B);
			midPos[curTrack] += 8;
			curNoteLen[curTrack] = 0;
			curDelay[curTrack] = 0;
			curVols[curTrack] = 120;
			seqEnd = 0;
			midTrackBase = midPos[curTrack];

			/*Add track header*/
			valSize = WriteDeltaTime(multiMidData[curTrack], midPos[curTrack], 0);
			midPos[curTrack] += valSize;
			WriteBE16(&multiMidData[curTrack][midPos[curTrack]], 0xFF03);
			midPos[curTrack] += 2;
			Write8B(&multiMidData[curTrack][midPos[curTrack]], strlen(TRK_NAMES[curTrack]));
			midPos[curTrack]++;
			sprintf((char*)&multiMidData[curTrack][midPos[curTrack]], TRK_NAMES[curTrack]);
			midPos[curTrack] += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSize[curTrack]);

			curInsts[curTrack] = 0;
		}

		seqEnd = 0;

		while (seqEnd == 0 && midPos[0] < 48000 && ctrlDelay < 110000)
		{
			command[0] = exRomData[seqPos];
			command[1] = exRomData[seqPos + 1];
			command[2] = exRomData[seqPos + 2];

			highNibble = (command[0] & 15);
			lowNibble = (command[0] >> 4);

			if (EventMap[highNibble] == CHICKEN_EVENT_NOTE_ON)
			{
				curTrack = lowNibble - 1;

				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
					curDelay[curTrack] = 0;
					midPos[curTrack] = tempPos;
				}
				curNote[curTrack] = command[1] + 24;
				if (curTrack < 2)
				{
					curNote[curTrack] += 12;
				}
				onOff[curTrack] = 1;


				tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
				curDelay[curTrack] = 0;
				firstNote[curTrack] = 0;
				midPos[curTrack] = tempPos;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (command[2] != 0x00)
					{
						curDelay[curTrack] += (command[2] * 5);
					}
				}

				seqPos += 3;
			}

			else if (EventMap[highNibble] == CHICKEN_EVENT_NOTE_OFF)
			{
				curTrack = lowNibble - 1;
				if (onOff[curTrack] == 1)
				{
					onOff[curTrack] = 0;
					tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
					curDelay[curTrack] = 0;
					midPos[curTrack] = tempPos;
				}
				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					curDelay[curTrack] += (command[1] * 5);
				}
				seqPos += 2;
			}

			else if (EventMap[highNibble] == CHICKEN_EVENT_PROG_CHANGE)
			{
				curTrack = lowNibble - 1;
				curInsts[curTrack] = command[1];
				firstNote[curTrack] = 1;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					curDelay[curTrack] += (command[2] * 5);
				}
				seqPos += 3;
			}

			else if (EventMap[highNibble] == CHICKEN_EVENT_PAN)
			{
				curTrack = lowNibble - 1;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					curDelay[curTrack] += (command[2] * 5);
				}
				seqPos += 3;
			}

			else if (EventMap[command[0]] == CHICKEN_EVENT_SPECIAL)
			{
				/*Play sound effect*/
				if (command[1] <= 0xEF)
				{
					seqPos += 3;
				}
				/*Go to loop*/
				else
				{
					seqEnd = 1;
				}

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					curDelay[curTrack] += (command[2] * 5);
				}
			}

			/*End of song*/
			else
			{
				seqEnd = 1;
			}
		}



		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (onOff[curTrack] == 1)
			{
				tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
				curDelay[curTrack] = 0;
				midPos[curTrack] = tempPos;
			}
			/*WriteDeltaTime(DE1multiMidData[curTrack], midPos[curTrack], 0);
			midPos[curTrack]++;*/
			WriteBE32(&multiMidData[curTrack][midPos[curTrack]], 0xFF2F00);
			midPos[curTrack] += 4;
			firstNote[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSize[curTrack]);
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
			fwrite(multiMidData[curTrack], midPos[curTrack], 1, mid);
		}

		for (k = 0; k < trackCnt; k++)
		{
			free(multiMidData[k]);
		}
		free(ctrlMidData);
		fclose(mid);
	}
}