/*Distinctive Software*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "DISTINCT.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int foundTable;
int curInst;
long c1Pos;
long c2Pos;
long c3Pos;
long c4Pos;
long bankAmt;
long songPtr;
long seqPtrs[9];

int curVol;
int drvVers;
int songNum;
int numSongs;

unsigned char* romData;
unsigned char* midData;
unsigned char* multiMidData[9];

unsigned char* ctrlMidData;

long midLength;

/*Pointers for Bill Elliott's NASCAR Fast Tracks*/
const long NFPtrs[3] = { 0x66C6, 0x4000, 0x5782 };
/*Pointers for Top Gun: Guts & Glory*/
const long TGPtrs[4] = { 0x4000, 0x4DBA, 0x520C, 0x5382 };
/*Pointers for The Battle of Olympus*/
const long BOPtrs[4] = { 0x6DEA, 0x6EBD, 0x6F2C, 0x65C4 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int DistWriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int DistWriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int DistWriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
int rrca(int a);
void Distsong2mid(int songNum, long songPtr);

unsigned int DistWriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	deltaValue = WriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0xC0 | curChan);
		}
		else
		{
			Write8B(&buffer[pos], 0xC9);
		}

		Write8B(&buffer[pos + 1], inst);
		Write8B(&buffer[pos + 2], 0);

		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			if (drvVers == DIST_VER_NF)
			{
				Write8B(&buffer[pos + 3], 0x99);
			}
			else
			{
				Write8B(&buffer[pos + 3], 0x90 | curChan);
			}
		}
		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	deltaValue = WriteDeltaTime(buffer, pos, length);
	pos += deltaValue;

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}


unsigned int DistWriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	deltaValue = WriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0xC0 | curChan);
		}
		else
		{
			if (drvVers == DIST_VER_NF)
			{
				Write8B(&buffer[pos], 0xC9);
			}
			else
			{
				Write8B(&buffer[pos], 0xC0 | curChan);
			}
		}


		Write8B(&buffer[pos + 1], inst);
		Write8B(&buffer[pos + 2], 0);

		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			if (drvVers == DIST_VER_NF)
			{
				Write8B(&buffer[pos + 3], 0x99);
			}
			else
			{
				Write8B(&buffer[pos + 3], 0x90 | curChan);
			}
		}


		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	return pos;

}

unsigned int DistWriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;

	deltaValue = WriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			if (drvVers == DIST_VER_NF)
			{
				Write8B(&buffer[pos + 3], 0x99);
			}
			else
			{
				Write8B(&buffer[pos + 3], 0x90 | curChan);
			}
		}
		pos++;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}


void DistProc(int bank, char parameters[4][100])
{
	drvVers = DIST_VER_NF;
	curVol = 120;
	curInst = 0;
	foundTable = 0;

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

	drvVers = strtol(parameters[0], NULL, 16);

	if (drvVers < DIST_VER_NF && drvVers >= DIST_VER_WW)
	{
		printf("ERROR: Invalid version number!\n");
		exit(1);
	}

	songNum = 1;

	if (drvVers == DIST_VER_NF)
	{
		numSongs = 3;
		while (songNum <= numSongs)
		{
			songPtr = NFPtrs[songNum - 1];
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Distsong2mid(songNum, songPtr);
			songNum++;
		}
	}

	else if (drvVers == DIST_VER_TG)
	{
		numSongs = 4;
		while (songNum <= numSongs)
		{
			songPtr = TGPtrs[songNum - 1];
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Distsong2mid(songNum, songPtr);
			songNum++;
		}
	}

	else if (drvVers == DIST_VER_BO)
	{
		numSongs = 18;
		i = 0x3FC5;
		while (songNum <= numSongs)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Distsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
		numSongs = 22;
		while (songNum <= numSongs)
		{
			songPtr = BOPtrs[songNum - 19];
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Distsong2mid(songNum, songPtr);
			songNum++;
		}
	}

	else if (drvVers == DIST_VER_WW)
	{
		numSongs = 16;
		i = 0x4012;
		while (songNum <= numSongs)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Distsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void Distsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	int curTrack = 0;
	int curNote = 0;
	unsigned int curNotes[9];
	int curNoteLen = 0;
	int curNoteLens[9];
	int curDelay = 0;
	int curDelays[9];
	int ctrlDelay = 0;
	int masterDelay = 0;
	int masterDelays[9];
	int seqEnd = 0;
	int tracksEnd[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int songEnd = 0;
	unsigned int seqPos = 0;
	unsigned int seqPosM[9];
	unsigned int romPos = 0;
	unsigned int midPos = 0;
	unsigned int midPosM[9];
	long ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	long tempPos = 0;
	int valSize = 0;
	long trackSize = 0;
	long trackSizes[9];
	long ctrlTrackSize = 0;
	int initTempo = 0;
	int tempo = 150;
	int curVol = 0;
	int curVols[9];
	int holdNote = 0;
	int holdNotes[9];
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int firstNote = 0;
	int firstNotes[9];
	long seqTime = 0;
	int curInsts[9];
	int patPtrs[9];
	int transposes[9];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	romPos = songPtr;
	if (drvVers != DIST_VER_NF)
	{
		trackCnt = romData[songPtr];
		romPos++;
	}

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

	switch (drvVers)
	{
	case DIST_VER_NF:
		DIST_STATUS_NOTE_MIN = 0x00;
		DIST_STATUS_NOTE_MAX = 0x74;
		DIST_STATUS_REST = 0x75;
		EventMap[0xD9] = DIST_EVENT_NEXT_PAT;
		EventMap[0xDA] = DIST_EVENT_STOP;
		EventMap[0xDB] = DIST_EVENT_RESTART;
		EventMap[0xDC] = DIST_EVENT_PROG_CHANGE;
		EventMap[0xE0] = DIST_EVENT_DECAY_STAGE;
		break;
	case DIST_VER_TG:
		DIST_STATUS_NOTE_MIN = 0x00;
		DIST_STATUS_NOTE_MAX = 0xD8;
		EventMap[0xD9] = DIST_EVENT_STOP;
		EventMap[0xDA] = DIST_EVENT_STOP;
		EventMap[0xDB] = DIST_EVENT_RESTART;
		EventMap[0xDC] = DIST_EVENT_PROG_CHANGE;
		EventMap[0xDD] = DIST_EVENT_TEMPO;
		EventMap[0xDE] = DIST_EVENT_NOP1;
		EventMap[0xDF] = DIST_EVENT_NOP2;
		EventMap[0xE2] = DIST_EVENT_NOP1;
		EventMap[0xE3] = DIST_EVENT_NOP;
		break;
	case DIST_VER_BO:
		DIST_STATUS_NOTE_MIN = 0x00;
		DIST_STATUS_NOTE_MAX = 0xD8;
		EventMap[0xD9] = DIST_EVENT_RESTART;
		EventMap[0xDA] = DIST_EVENT_STOP;
		EventMap[0xDB] = DIST_EVENT_RESTART;
		EventMap[0xDC] = DIST_EVENT_PROG_CHANGE;
		EventMap[0xDD] = DIST_EVENT_TEMPO;
		EventMap[0xDE] = DIST_EVENT_NOP1;
		EventMap[0xDF] = DIST_EVENT_NOP2;
		EventMap[0xE2] = DIST_EVENT_NOP1;
		EventMap[0xE3] = DIST_EVENT_LOOP_TRACK;
		EventMap[0xEA] = DIST_EVENT_STOP_TRACK;
		break;
	case DIST_VER_WW:
		DIST_STATUS_NOTE_MIN = 0x00;
		DIST_STATUS_NOTE_MAX = 0xD8;
		EventMap[0xD9] = DIST_EVENT_STOP_TRACK;
		EventMap[0xDA] = DIST_EVENT_STOP;
		EventMap[0xDB] = DIST_EVENT_RESTART;
		EventMap[0xDC] = DIST_EVENT_PROG_CHANGE;
		EventMap[0xDD] = DIST_EVENT_TEMPO;
		EventMap[0xDE] = DIST_EVENT_NOP1;
		EventMap[0xDF] = DIST_EVENT_NOP2;
		EventMap[0xE2] = DIST_EVENT_NOP1;
		EventMap[0xE3] = DIST_EVENT_LOOP_TRACK;
		EventMap[0xEA] = DIST_EVENT_STOP_TRACK;
		break;
	default:
		DIST_STATUS_NOTE_MIN = 0x00;
		DIST_STATUS_NOTE_MAX = 0x74;
		DIST_STATUS_REST = 0x75;
		EventMap[0xD9] = DIST_EVENT_NEXT_PAT;
		EventMap[0xDA] = DIST_EVENT_STOP;
		EventMap[0xDB] = DIST_EVENT_RESTART;
		EventMap[0xDC] = DIST_EVENT_PROG_CHANGE;
		EventMap[0xDD] = DIST_EVENT_DECAY_STAGE;
		break;
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

		for (curTrack = 0; curTrack < 9; curTrack++)
		{
			tracksEnd[curTrack] = 1;
		}

		if (drvVers == DIST_VER_NF)
		{
			romPos = ReadLE16(&romData[romPos + 2]);
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				patPtrs[curTrack] = ReadLE16(&romData[romPos]);

				if (patPtrs[curTrack] != 0x0000)
				{
					seqPtrs[curTrack] = ReadLE16(&romData[patPtrs[curTrack]]);
					seqPosM[curTrack] = seqPtrs[curTrack];
					transposes[curTrack] = romData[patPtrs[curTrack] + 2];
					tracksEnd[curTrack] = 0;
				}
				romPos += 2;
			}
		}
		else
		{
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
				seqPosM[curTrack] = seqPtrs[curTrack];
				tracksEnd[curTrack] = 0;
				romPos += 2;
			}
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			curDelays[curTrack] = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			curInsts[curTrack] = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);

			curDelays[curTrack] = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;

		}

		ctrlDelay = 0;
		seqTime = 0;

		if (drvVers == DIST_VER_NF)
		{
			while (songEnd == 0)
			{
				for (curTrack = 0; curTrack < 9; curTrack++)
				{
					if (tracksEnd[curTrack] != 1 && patPtrs[curTrack] == 0x0000)
					{
						tracksEnd[curTrack] = 1;
					}
				}
				if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1 && tracksEnd[3] == 1 && tracksEnd[4] == 1 && tracksEnd[5] == 1 && tracksEnd[6] == 1 && tracksEnd[7] == 1 && tracksEnd[8] == 1)
				{
					songEnd = 1;
				}

				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] == 0)
					{
						command[0] = romData[seqPosM[curTrack]];
						command[1] = romData[seqPosM[curTrack] + 1];

						if (command[0] >= DIST_STATUS_NOTE_MIN && command[0] <= DIST_STATUS_NOTE_MAX)
						{
							if (holdNotes[curTrack] == 1)
							{
								tempPos = DistWriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								holdNotes[curTrack] = 0;
								curDelays[curTrack] = 0;
								midPosM[curTrack] = tempPos;
							}
							curNotes[curTrack] = command[0] - DIST_STATUS_NOTE_MIN + transposes[curTrack];

							if (curTrack == 3)
							{
								curNotes[curTrack] += 24;
							}
							curNoteLens[curTrack] = (command[1] & 0x7F) * 5;
							curInst = curInsts[curTrack];
							tempPos = DistWriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							firstNotes[curTrack] = 0;
							holdNotes[curTrack] = 1;
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = curNoteLens[curTrack];
							masterDelays[curTrack] += curNoteLens[curTrack];
							holdNotes[curTrack] = 1;
							curInsts[curTrack] = 0;
							seqPosM[curTrack] += 2;
						}

						else if (command[0] == DIST_STATUS_REST)
						{
							if (holdNotes[curTrack] == 1)
							{
								tempPos = DistWriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								holdNotes[curTrack] = 0;
								curDelays[curTrack] = 0;
								midPosM[curTrack] = tempPos;
							}
							curNoteLens[curTrack] = (command[1] & 0x7F) * 5;
							curDelays[curTrack] += curNoteLens[curTrack];
							masterDelays[curTrack] += curNoteLens[curTrack];
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == DIST_EVENT_NEXT_PAT)
						{
							for (j = 0; j < 4; j++)
							{
								if (tracksEnd[j] != 1)
								{
									patPtrs[j] += 3;
									seqPtrs[j] = ReadLE16(&romData[patPtrs[j]]);
									seqPosM[j] = seqPtrs[j];
									transposes[j] = romData[patPtrs[j] + 2];
									if (masterDelays[j] > seqTime)
									{
										curDelays[j] -= (masterDelays[j] - seqTime);
									}
									masterDelays[j] = seqTime;
								}
							}
						}

						else if (EventMap[command[0]] == DIST_EVENT_STOP)
						{
							for (j = 0; j < 9; j++)
							{
								tracksEnd[j] = 1;
							}
							songEnd = 1;
						}

						else if (EventMap[command[0]] == DIST_EVENT_RESTART)
						{
							for (j = 0; j < 9; j++)
							{
								tracksEnd[j] = 1;
							}
							songEnd = 1;
						}

						else if (EventMap[command[0]] == DIST_EVENT_PROG_CHANGE)
						{
							curInsts[curTrack] = command[1];
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == DIST_EVENT_DECAY_STAGE)
						{
							seqPosM[curTrack] += 2;
						}

						/*Unknown command*/
						else
						{
							seqPosM[curTrack]++;
						}
					}
				}
				seqTime += 5;
				ctrlDelay += 5;
			}
		}

		else
		{
			while (songEnd == 0)
			{
				for (curTrack = 0; curTrack < 9; curTrack++)
				{
					if (tracksEnd[curTrack] != 1 && seqPtrs[curTrack] == 0x0000)
					{
						tracksEnd[curTrack] = 1;
					}
				}
				if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1 && tracksEnd[3] == 1 && tracksEnd[4] == 1 && tracksEnd[5] == 1 && tracksEnd[6] == 1 && tracksEnd[7] == 1 && tracksEnd[8] == 1)
				{
					songEnd = 1;
				}

				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] == 0)
					{
						command[0] = romData[seqPosM[curTrack]];
						command[1] = romData[seqPosM[curTrack] + 1];
						command[2] = romData[seqPosM[curTrack] + 2];

						/*Delta time*/
						if (command[0] <= 0x7F)
						{
							curDelays[curTrack] += (command[0] * 5);
							masterDelays[curTrack] += (command[0] * 5);
							seqPosM[curTrack]++;
						}
						else
						{
							curDelays[curTrack] += (((rrca(command[0] & 0x7F)) + command[1]) * 5);
							masterDelays[curTrack] += (((rrca(command[0] & 0x7F)) + command[1]) * 5);
							seqPosM[curTrack] += 2;
						}

						if (holdNotes[curTrack] == 1 && curDelays[curTrack] >= curNoteLens[curTrack])
						{
							curDelays[curTrack] -= curNoteLens[curTrack];
							tempPos = DistWriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curNoteLens[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							midPosM[curTrack] = tempPos;
						}

						command[0] = romData[seqPosM[curTrack]];
						command[1] = romData[seqPosM[curTrack] + 1];
						command[2] = romData[seqPosM[curTrack] + 2];

						/*Event*/
						if (command[0] >= DIST_STATUS_NOTE_MIN && command[0] <= DIST_STATUS_NOTE_MAX)
						{
							if (holdNotes[curTrack] == 1)
							{
								tempPos = DistWriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								holdNotes[curTrack] = 0;
								curDelays[curTrack] = 0;
								midPosM[curTrack] = tempPos;
							}

							/*Note*/
							curNotes[curTrack] = romData[seqPosM[curTrack]] & 0x7F;
							if (drvVers == DIST_VER_TG)
							{
								curNotes[curTrack] += 24;
							}
							else if (drvVers == DIST_VER_BO)
							{
								curNotes[curTrack] += 12;
							}
							else if (drvVers == DIST_VER_WW)
							{
								curNotes[curTrack] += 38;
							}
							if ((romData[seqPosM[curTrack]] & 0x80) != 0x00)
							{
								seqPosM[curTrack]++;
							}
							curInst = curInsts[curTrack];
							tempPos = DistWriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							firstNotes[curTrack] = 0;
							holdNotes[curTrack] = 1;
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = 0;
							holdNotes[curTrack] = 1;
							seqPosM[curTrack]++;
							/*Time*/
							if ((romData[seqPosM[curTrack]] & 0x80) != 0x00)
							{
								curNoteLen = (((rrca(romData[seqPosM[curTrack]] & 0x7F)) + romData[seqPosM[curTrack]]) * 5);
								seqPosM[curTrack] += 2;
							}
							else
							{
								curNoteLen = romData[seqPosM[curTrack]];
								seqPosM[curTrack]++;
							}
							curNoteLens[curTrack] = ((curNoteLen + 1) * 4) * 5;
							midPosM[curTrack] = tempPos;
						}

						else if (EventMap[command[0]] == DIST_EVENT_UNKNOWN0 || EventMap[command[0]] == DIST_EVENT_NOP)
						{
							seqPosM[curTrack]++;
						}

						else if (EventMap[command[0]] == DIST_EVENT_UNKNOWN1 || EventMap[command[0]] == DIST_EVENT_NOP1)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == DIST_EVENT_UNKNOWN2 || EventMap[command[0]] == DIST_EVENT_NOP2)
						{
							seqPosM[curTrack] += 3;
						}

						else if (EventMap[command[0]] == DIST_EVENT_STOP)
						{
							for (j = 0; j < 9; j++)
							{
								tracksEnd[j] = 1;
							}
							songEnd = 1;
						}

						else if (EventMap[command[0]] == DIST_EVENT_RESTART)
						{
							for (j = 0; j < 9; j++)
							{
								tracksEnd[j] = 1;
							}
							songEnd = 1;
						}

						else if (EventMap[command[0]] == DIST_EVENT_PROG_CHANGE)
						{
							if (drvVers == DIST_VER_TG)
							{
								curInsts[curTrack] = command[1];
								seqPosM[curTrack] += 2;
							}
							else
							{
								seqPosM[curTrack] += 3;
							}
						}

						else if (EventMap[command[0]] == DIST_EVENT_TEMPO)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == DIST_EVENT_LOOP_TRACK)
						{
							tracksEnd[curTrack] = 1;
						}

						else if (EventMap[command[0]] == DIST_EVENT_STOP_TRACK)
						{
							tracksEnd[curTrack] = 1;
						}

						/*Unknown command*/
						else
						{
							seqPosM[curTrack]++;
						}

					}
				}
				seqTime += 5;
				ctrlDelay += 5;
			}
		}
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (holdNotes[curTrack] == 1)
			{
				tempPos = DistWriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
				holdNotes[curTrack] = 0;
				curDelays[curTrack] = 0;
				midPosM[curTrack] = tempPos;
			}
			/*End of track*/
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
		free(ctrlMidData);
		fclose(mid);
	}
}