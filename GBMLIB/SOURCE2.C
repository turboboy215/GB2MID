/*Source #2 (Workboy/Double Dragon 3)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "SOURCE2.H"

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
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int drvVers;
int curInst;
int progBank;
int curVol;
long noteLensBase;

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
void Source2mid(int songNum, long songPtrs[4]);

void Source2Proc(int bank)
{
	curInst = 0;
	foundTable = 0;
	drvVers = SOURCE2_VER_WB;
	progBank = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}
	else if (bank == 0x04)
	{
		drvVers = SOURCE2_VER_WB;
		tableOffset = 0x388D;
	}
	else
	{
		drvVers = SOURCE2_VER_DD3;
		tableOffset = 0x3AD2;
		noteLensBase = 0x3B2A;
	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	i = tableOffset;
	songNum = 1;

	if (drvVers == SOURCE2_VER_WB)
	{
		while (songNum <= 16)
		{
			songPtrs[0] = ReadLE16(&romData[i + 2]);
			songPtrs[1] = ReadLE16(&romData[i]);
			songPtrs[2] = 0x0000;
			songPtrs[3] = 0x0000;
			printf("Song %i channel 1: %04X\n", songNum, songPtrs[0]);
			printf("Song %i channel 2: %04X\n", songNum, songPtrs[1]);
			Source2mid(songNum, songPtrs);
			i += 4;
			songNum++;
		}
	}
	else
	{
		while (songNum <= 7)
		{
			songPtrs[0] = ReadLE16(&romData[i + 2]);
			songPtrs[1] = ReadLE16(&romData[i]);
			songPtrs[2] = 0x0000;
			songPtrs[3] = ReadLE16(&romData[i + 4]);
			printf("Song %i channel 1: %04X\n", songNum, songPtrs[0]);
			printf("Song %i channel 2: %04X\n", songNum, songPtrs[1]);
			printf("Song %i channel 3: %04X\n", songNum, songPtrs[2]);
			Source2mid(songNum, songPtrs);
			i += 6;
			songNum++;
		}
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void Source2mid(int songNum, long songPtrs[4])
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
		case SOURCE2_VER_WB:
			/*Fall-through*/
		case SOURCE2_VER_DD3:
		default:
			SOURCE2_STATUS_REST = 0x00;
			SOURCE2_STATUS_NOTE_MIN = 0x01;
			SOURCE2_STATUS_NOTE_MAX = 0xEF;
			SOURCE2_STATUS_RESTART = 0xFE;
			SOURCE2_STATUS_STOP = 0xFF;
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

					if (command[0] == SOURCE2_STATUS_REST)
					{
						if (drvVers == SOURCE2_VER_WB)
						{
							curNoteLen = command[1] * 5;
						}
						else
						{
							curNoteLen = romData[noteLensBase + (4 * 8) + command[1]] * 5;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos += 2;
					}

					else if (command[0] >= SOURCE2_STATUS_NOTE_MIN && command[0] <= SOURCE2_STATUS_NOTE_MAX)
					{
						if (drvVers == SOURCE2_VER_WB)
						{
							curNote = command[0] - 48 + 24;
							curNoteLen = command[1] * 5;
						}
						else
						{
							curNote = command[0] - 36 + 24;
							curNoteLen = romData[noteLensBase + (4 * 8) + command[1]] * 5;
						}
						if (curTrack < 2)
						{
							curNote += 12;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos += 2;
					}

					else if (command[0] == SOURCE2_STATUS_RESTART)
					{
						seqEnd = 1;
					}

					else if (command[0] == SOURCE2_STATUS_STOP)
					{
						seqEnd = 1;
					}

					/*Unknown or non-functional command*/
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);

		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}
