/*Fatass (by Jeremy Evers/Runnyfun) (Animorphs)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "ANIMORPH.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long bankMapTable;
long seqPtrLoc;
long seqOffset;
int i, j;
char outfile[1000000];
int songNum;
int songPtr;
long seqPtrs[4];
long songTempo;
long nextPtr;
long endPtr;
long bankAmt;
int curVol;
int curInst;
int drvVers;
int foundTable;
int trackFmt;
int songBank;
int progBank;

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
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Animorphsong2mid(int songNum, long songPtr);
int AnimorphCmdArgLen(unsigned char cmd, int curTrack);
int AnimorphPatRows(long pos, long patTab, int curTrack);
int AnimorphRowHasNote(long pos, long patTab, int curTrack);
int AnimorphNextPattern(long pos);
int AnimorphRowUpdates(int speed, int depth, int phase, int* acc);

void AnimorphProc(int bank)
{
	drvVers = ANIMORPH_VER_STD;
	curVol = 120;
	curInst = 0;

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

	tableOffset = 0x7480;
	bankMapTable = 0x74A8;
	songNum = 1;

	while (songNum <= 20)
	{
		songPtr = ReadLE16(&romData[tableOffset + ((songNum - 1) * 2)]);
		songBank = romData[bankMapTable + (songNum - 1)];
		bankAmt = bankSize;
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, (songBank * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
		printf("Song %i: 0x%04X, bank %02X\n", songNum, songPtr, songBank);
		Animorphsong2mid(songNum, songPtr);
		free(exRomData);
		songNum++;
	}

	free(romData);

}

/*Number of argument bytes the driver reads (or skips) after a command byte*/
int AnimorphCmdArgLen(unsigned char cmd, int curTrack)
{
	switch (cmd)
	{
	case 0xD3: return 2;
	case 0xD4: return (curTrack < 2) ? 2 : 1;	/*driver skips only 1 byte on CH3/CH4*/
	case 0xD5: return 2;
	case 0xD6: return (curTrack == 2) ? 1 : 2;	/*driver skips 2 bytes off CH3*/
	case 0xD7: return 2;
	case 0xDA: case 0xDB: case 0xDC: return 1;
	case 0xDD: return 3;
	case 0xDE: case 0xDF: case 0xE0: return 1;
	case 0xE1: return 2;
	case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7: case 0xE8: case 0xE9: case 0xEA: return 1;
	case 0xFB: return 1;
	case 0xFC: return 2;
	case 0xFD: case 0xFE: return 1;
	default: return 0;
	}
}

/*Rows in one channel stream up to its FF (a pattern lasts as long as its shortest stream)*/
int AnimorphPatRows(long pos, long patTab, int curTrack)
{
	int rows = 0;
	long ret = 0;
	unsigned char b;

	while (rows < 100000)
	{
		b = exRomData[pos];
		if (b < 0x80)
		{
			rows++;
			pos++;
		}
		else if (b < 0xC0)
		{
			rows += (b & 0x3F) + 1;
			pos++;
		}
		else if (b == 0xFF)
		{
			if (ret == 0)
			{
				break;
			}
			pos = ret;
			ret = 0;
		}
		else if (b == 0xD3 && ret == 0)
		{
			ret = pos + 3;
			pos = patTab + ReadLE16(&exRomData[pos + 1]);
		}
		else
		{
			pos += 1 + AnimorphCmdArgLen(b, curTrack);
		}
	}
	return rows;
}

/*Does the row starting at pos contain a note (before its terminator)?*/
int AnimorphRowHasNote(long pos, long patTab, int curTrack)
{
	unsigned char b;

	for (;;)
	{
		b = exRomData[pos];
		if (b == 0x00 || (b >= 0x80 && b < 0xC0) || b == 0xFF)
		{
			return 0;
		}
		if (b < 0x80)
		{
			return (curTrack == 3 || b <= 0x48);
		}
		if (b == 0xD3)
		{
			pos = patTab + ReadLE16(&exRomData[pos + 1]);
			continue;
		}
		pos += 1 + AnimorphCmdArgLen(b, curTrack);
	}
}

/*Next pattern number in the sequence after pos (-1 = the sequence loops)*/
int AnimorphNextPattern(long pos)
{
	unsigned char b;

	for (;;)
	{
		b = exRomData[pos];
		if (b <= 0xFA)
		{
			return b;
		}
		if (b == 0xFF)
		{
			return -1;
		}
		pos += (b == 0xFC) ? 3 : 2;
	}
}

/*Row clock (bank 9 $691C): every update adds (speed*2 * ShuffleTable[depth][column]) >> 5
  to an 8-bit accumulator; a row happens when it overflows. Returns how many
  updates the current row lasts. ShuffleTable is at $5040 in the driver bank.*/
int AnimorphRowUpdates(int speed, int depth, int phase, int* acc)
{
	int updates = 0;
	int mult = romData[0x5040 + (depth * 16) + (phase >> 8)];
	int inc = ((((speed * 2) & 0xFF) * mult) >> 5) & 0xFF;

	if (inc == 0)
	{
		inc = 1;
	}
	do
	{
		*acc += inc;
		updates++;
	} while (*acc < 0x100);
	*acc &= 0xFF;
	return updates;
}

/*Convert the song data to MIDI*/
void Animorphsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[4];
	int curTrack = 0;
	int curNote = 0;
	int tempNote = 0;
	unsigned int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int curNoteLenTab;
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	int masterDelay = 0;
	int masterDelays[4];
	int octave = 4;
	int octaves[4];
	int seqEnd = 0;
	int tracksEnd[4] = { 0, 0, 0, 0 };
	int songEnd = 0;
	unsigned int seqPos = 0;
	unsigned int seqPosM[4];
	unsigned int patPos;
	unsigned int patPosM[4];
	unsigned int noteLensPtr;
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
	int initTempo = 0;
	int tempo = 150;
	long jumpPos = 0;
	long jumpPosEnd = 0;
	int curVols[4];
	int curSpeed = 0x30;
	int curSpeeds[4];
	int holdNote = 0;
	int holdNotes[4];
	int transpose = 0;
	int transposes[4];
	int trackCnt = 4;
	int ticks = 60;
	int k = 0;
	int firstNote = 0;
	int firstNotes[4];
	long seqTime = 0;
	int curInsts[4];
	int macros[4][2];
	int inMacroM[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long framePtr;
	int numChan;
	int envVals[4];
	int pitchBend[4];
	long seqOffset;
	long patTab;
	int numPat;
	int curPat;
	int nextPat;
	long patEnd;
	int endRow;
	int chanEnd[4];
	int skipNote[4];
	int rowCnt;
	int shufPhase = 0;
	int shufRate = 0;
	int shufDepth = 0;
	int tickAcc = 0;
	int rowUpd = 0;
	int lastRowUpd = -1;
	int firstRow = 1;
	long rowTempo;
	char title[64];

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

		switch (drvVers)
		{
		case ANIMORPH_VER_STD:
			/*Fall-through*/
		default:
			ANIMORPH_STATUS_PATTERN_MIN = 0x00;
			ANIMORPH_STATUS_PATTERN_MAX = 0xFA;
			ANIMORPH_STATUS_NOTE_OFF = 0x00;
			ANIMORPH_STATUS_NOTE_MIN = 0x01;
			ANIMORPH_STATUS_NOTE_MAX = 0x48;
			ANIMORPH_STATUS_EMPTY_ROW_MIN = 0x49;
			ANIMORPH_STATUS_EMPTY_ROW_MAX = 0x7F;
			ANIMORPH_STATUS_WAIT_MIN = 0x80;
			ANIMORPH_STATUS_WAIT_MAX = 0xBF;
			for (j = 0xC0; j <= 0xD2; j++)
			{
				EventMap[j] = ANIMORPH_EVENT_NOP;
			}
			EventMap[0xD3] = ANIMORPH_EVENT_CALL;
			EventMap[0xD4] = ANIMORPH_EVENT_CYCLE_DUTY;
			EventMap[0xD5] = ANIMORPH_EVENT_CYCLE_PAN;
			EventMap[0xD6] = ANIMORPH_EVENT_WAVEFORM_DISTORT;
			EventMap[0xD7] = ANIMORPH_EVENT_WAVEFORM_MIX;
			EventMap[0xD8] = ANIMORPH_EVENT_STOP_SAMPLE;
			EventMap[0xD9] = ANIMORPH_EVENT_RESET_VIBRATO;
			EventMap[0xDA] = ANIMORPH_EVENT_WAVEFORM_PRESET;
			EventMap[0xDB] = ANIMORPH_EVENT_TUNING;
			EventMap[0xDC] = ANIMORPH_EVENT_PLAY_SAMPLE;
			EventMap[0xDD] = ANIMORPH_EVENT_ARPEGGIO;
			EventMap[0xDE] = ANIMORPH_EVENT_GLISSANDO;
			EventMap[0xDF] = ANIMORPH_EVENT_PORTAMENTO_SOURCE;
			EventMap[0xE0] = ANIMORPH_EVENT_PORTAMENTO_RATE;
			EventMap[0xE1] = ANIMORPH_EVENT_VIBRATO;
			EventMap[0xE2] = ANIMORPH_EVENT_RESTART_ENV;
			EventMap[0xE3] = ANIMORPH_EVENT_RETRIGGER_DELAY;
			EventMap[0xE4] = ANIMORPH_EVENT_TRIGGER_DELAY;
			EventMap[0xE5] = ANIMORPH_EVENT_USE_DUR;
			EventMap[0xE6] = ANIMORPH_EVENT_SWEEP;
			EventMap[0xE7] = ANIMORPH_EVENT_ENV_VOL;
			EventMap[0xE8] = ANIMORPH_EVENT_ENV_RATE;
			EventMap[0xE9] = ANIMORPH_EVENT_DUR;
			EventMap[0xEA] = ANIMORPH_EVENT_DUTY;
			for (j = 0xEB; j <= 0xFA; j++)
			{
				EventMap[j] = ANIMORPH_EVENT_NOP;
			}
			EventMap[0xFB] = ANIMORPH_EVENT_PAN;
			EventMap[0xFC] = ANIMORPH_EVENT_SHUFFLE;
			EventMap[0xFD] = ANIMORPH_EVENT_TRANSPOSE;
			EventMap[0xFE] = ANIMORPH_EVENT_SPEED;
			EventMap[0xFF] = ANIMORPH_EVENT_STOP;
		}

		/*Process song header*/
		seqOffset = songPtr + ReadLE16(&exRomData[songPtr]);
		patTab = songPtr + ReadLE16(&exRomData[songPtr + 2]);

		for (k = 0; k < 63; k++)
		{
			unsigned char b = exRomData[songPtr + 7 + k];
			if (b == 0x00 || b == 0xFF)
			{
				break;
			}
			title[k] = b;
		}
		title[k] = 0;
		while (k > 0 && title[k - 1] == ' ')
		{
			title[--k] = 0;
		}
		printf("  title: %s\n", title);

		/*Now process the sequence*/
		romPos = seqOffset;
		songEnd = 0;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			curDelays[curTrack] = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			pitchBend[curTrack] = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			curNotes[curTrack] = 0;
			curNoteLens[curTrack] = 0;
			curInsts[curTrack] = 0;
			tracksEnd[curTrack] = 0;
			transposes[curTrack] = 0;
			transpose = 0;
			curSpeeds[curTrack] = 0;
			envVals[curTrack] = 0xFF;
			inMacroM[curTrack] = 0;

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

		/*Add song title*/
		ctrlMidPos++;
		valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos += valSize;
		WriteBE16(&ctrlMidData[ctrlMidPos], 0xFF03);
		ctrlMidPos += 2;
		Write8B(&ctrlMidData[ctrlMidPos], strlen(title));
		ctrlMidPos++;
		sprintf((char*)&ctrlMidData[ctrlMidPos], title);
		ctrlMidPos += strlen(title) - 1;

		while (songEnd == 0)
		{
			if (exRomData[romPos] >= ANIMORPH_STATUS_PATTERN_MIN && exRomData[romPos] <= ANIMORPH_STATUS_PATTERN_MAX)
			{
				seqEnd = 0;
				curPat = exRomData[romPos] - ANIMORPH_STATUS_PATTERN_MIN;
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					seqPosM[curTrack] = patTab + ReadLE16(&exRomData[patTab + (curPat * 8) + (curTrack * 2)]);
				}

				/*The pattern ends when its SHORTEST stream reaches FF, and the next
				  pattern starts in that same update. On that last row every channel
				  still reads its events (transpose etc. apply), and a note there only
				  sounds if the next pattern's first row has no note of its own.*/
				patEnd = 0x7FFFFFFF;
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					rowCnt = AnimorphPatRows(seqPosM[curTrack], patTab, curTrack);
					if (seqTime + rowCnt * 5 < patEnd)
					{
						patEnd = seqTime + rowCnt * 5;
					}
					chanEnd[curTrack] = 0;
					inMacroM[curTrack] = 0;
				}
				nextPat = AnimorphNextPattern(romPos + 1);
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					skipNote[curTrack] = 1;
					if (nextPat >= 0)
					{
						skipNote[curTrack] = AnimorphRowHasNote(patTab + ReadLE16(&exRomData[patTab + (nextPat * 8) + (curTrack * 2)]), patTab, curTrack);
					}
				}
				endRow = 0;

				/*Song start: the driver backs the shuffle phase up one step and runs
				  until the accumulator first overflows (the first row). That lead-in is
				  not written; only the accumulator remainder it leaves matters.*/
				if (firstRow == 1)
				{
					AnimorphRowUpdates(curSpeed, shufDepth, (shufPhase - ReadLE16(&romData[0x5140 + (shufRate * 2)])) & 0xFFF, &tickAcc);
					firstRow = 0;
				}

				while (seqEnd == 0)
				{
					endRow = (seqTime >= patEnd);
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						while (seqTime >= masterDelays[curTrack] && chanEnd[curTrack] == 0)
						{
							command[0] = exRomData[seqPosM[curTrack]];
							command[1] = exRomData[seqPosM[curTrack] + 1];
							command[2] = exRomData[seqPosM[curTrack] + 2];

							if (command[0] == ANIMORPH_STATUS_NOTE_OFF)
							{
								if (holdNotes[curTrack] == 1)
								{
									tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									holdNotes[curTrack] = 0;
									curDelays[curTrack] = 0;
									midPosM[curTrack] = tempPos;
								}
								curDelays[curTrack] += 5;
								masterDelays[curTrack] += 5;
								seqPosM[curTrack]++;
							}

							else if ((command[0] >= ANIMORPH_STATUS_NOTE_MIN && command[0] <= ANIMORPH_STATUS_NOTE_MAX) && curTrack != 3)
							{
								if (endRow && skipNote[curTrack])
								{
									/*replaced by the next pattern's first note in the same update*/
									masterDelays[curTrack] += 5;
									curDelays[curTrack] += 5;
									seqPosM[curTrack]++;
									continue;
								}
								if (holdNotes[curTrack] == 1)
								{
									tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									holdNotes[curTrack] = 0;
									curDelays[curTrack] = 0;
									midPosM[curTrack] = tempPos;
								}
								curNotes[curTrack] = command[0] - ANIMORPH_STATUS_NOTE_MIN + transpose + transposes[curTrack] + 24;
								if (curTrack != 2)
								{
									curNotes[curTrack] += 12;
								}
								curVol = curVols[curTrack];
								tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								firstNotes[curTrack] = 0;
								holdNotes[curTrack] = 1;
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 5;
								masterDelays[curTrack] += 5;
								holdNotes[curTrack] = 1;
								seqPosM[curTrack]++;
							}

							else if ((command[0] >= ANIMORPH_STATUS_NOTE_MIN && command[0] <= ANIMORPH_STATUS_EMPTY_ROW_MAX) && curTrack == 3)
							{
								if (endRow && skipNote[curTrack])
								{
									/*replaced by the next pattern's first note in the same update*/
									masterDelays[curTrack] += 5;
									curDelays[curTrack] += 5;
									seqPosM[curTrack]++;
									continue;
								}
								if (holdNotes[curTrack] == 1)
								{
									tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									holdNotes[curTrack] = 0;
									curDelays[curTrack] = 0;
									midPosM[curTrack] = tempPos;
								}
								curNotes[curTrack] = command[0] - ANIMORPH_STATUS_NOTE_MIN + transpose + transposes[curTrack];
								curVol = curVols[curTrack];
								tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								firstNotes[curTrack] = 0;
								holdNotes[curTrack] = 1;
								midPosM[curTrack] = tempPos;

								if (pitchBend[curTrack] == 1 && firstNotes[curTrack] == 0)
								{
									tempPos = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
									midPosM[curTrack] += tempPos;
									Write8B(&multiMidData[curTrack][midPosM[curTrack]], (0xE0 | curTrack));
									Write8B(&multiMidData[curTrack][midPosM[curTrack] + 1], 0);
									Write8B(&multiMidData[curTrack][midPosM[curTrack] + 2], 0x40);
									Write8B(&multiMidData[curTrack][midPosM[curTrack] + 3], 0);
									midPosM[curTrack] += 3;
									pitchBend[curTrack] = 0;
									firstNotes[curTrack] = 1;
								}

								curDelays[curTrack] = 5;
								masterDelays[curTrack] += 5;
								holdNotes[curTrack] = 1;
								seqPosM[curTrack]++;
							}

							else if (command[0] >= ANIMORPH_STATUS_EMPTY_ROW_MIN && command[0] <= ANIMORPH_STATUS_EMPTY_ROW_MAX)
							{
								curDelays[curTrack] += 5;
								masterDelays[curTrack] += 5;
								seqPosM[curTrack]++;
							}

							else if (command[0] >= ANIMORPH_STATUS_WAIT_MIN && command[0] <= ANIMORPH_STATUS_WAIT_MAX)
							{
								curDelays[curTrack] += (5 * ((command[0] & 0x3F) + 1));
								masterDelays[curTrack] += (5 * ((command[0] & 0x3F) + 1));
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_NOP)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_CALL)
							{
								if (inMacroM[curTrack] == 0)
								{
									inMacroM[curTrack] = 1;
									macros[curTrack][0] = ReadLE16(&exRomData[seqPosM[curTrack] + 1]);
									macros[curTrack][1] = seqPosM[curTrack] + 3;
									seqPosM[curTrack] = macros[curTrack][0];
								}
								else
								{
									seqPosM[curTrack] += 3;
								}
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_CYCLE_DUTY)
							{
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_CYCLE_PAN)
							{
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_WAVEFORM)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_WAVEFORM_DISTORT)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_WAVEFORM_MIX)
							{
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_STOP_SAMPLE)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_RESET_VIBRATO)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_WAVEFORM_PRESET)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_TUNING)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_PLAY_SAMPLE)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_ARPEGGIO)
							{
								seqPosM[curTrack] += 4;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_GLISSANDO)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_PORTAMENTO_SOURCE)
							{
								pitchBend[curTrack] = 1;
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_PORTAMENTO_RATE)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_VIBRATO)
							{
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_RESTART_ENV)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_RETRIGGER_DELAY)
							{
								seqPosM[curTrack] += 2;
							}
							else if (EventMap[command[0]] == ANIMORPH_EVENT_TRIGGER_DELAY)
							{
								seqPosM[curTrack] += 2;
							}
							else if (EventMap[command[0]] == ANIMORPH_EVENT_USE_DUR)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_SWEEP)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_ENV_VOL)
							{
								envVals[curTrack] = command[1];
								if (curTrack != 2)
								{
									/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
									if ((envVals[curTrack] & 0x08) == 0x00)
									{
										curVols[curTrack] = (envVals[curTrack] & 0xF0) / 2;
									}
									else
									{
										if (((envVals[curTrack] & 0xF0) / 2) == curVols[curTrack])
										{
											curVols[curTrack] = (envVals[curTrack] & 0xF0) / 2;
										}
										else
										{
											curVols[curTrack] = 120;
										}

									}

									if (curVols[curTrack] == 0)
									{
										curVols[curTrack] = 1;
									}

								}
								else if (curTrack == 2)
								{
									switch ((envVals[curTrack] & 0x7F))
									{
									case 0x20:
										curVols[curTrack] = 120;
										break;
									case 0x40:
										curVols[curTrack] = 60;
										break;
									case 0x60:
										curVols[curTrack] = 30;
										break;
									case 0x00:
										curVols[curTrack] = 1;
										break;
									default:
										curVols[curTrack] = 120;
										break;
									}
								}
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_ENV_RATE)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_DUR)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_DUTY)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_PAN)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_SHUFFLE)
							{
								/*FC phase, depth<<4 | rate*/
								shufPhase = (command[1] << 4) & 0xFFF;
								shufRate = command[2] & 0x0F;
								shufDepth = command[2] >> 4;
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_TRANSPOSE)
							{
								transposes[curTrack] = (signed char)command[1];
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == ANIMORPH_EVENT_SPEED)
							{
								/*the row clock below writes the tempo*/
								curSpeed = command[1];
								seqPosM[curTrack] += 2;
							}
							else if (EventMap[command[0]] == ANIMORPH_EVENT_STOP)
							{
								if (inMacroM[curTrack] == 1)
								{
									inMacroM[curTrack] = 0;
									seqPosM[curTrack] = macros[curTrack][1];
								}
								else
								{
									chanEnd[curTrack] = 1;
								}

							}

							/*Unknown command*/
							else
							{
								seqPosM[curTrack]++;
							}
						}
					}

					if (endRow)
					{
						break;
					}

					/*Row clock: length of this row in driver updates (speed and shuffle),
					  written as a tempo change whenever it differs from the last row.
					  5 ticks per row, 60 ticks per quarter = 12 rows per quarter
					  (one update = 33485.6 us; rows of up to 41 updates fit the tempo field).*/
					rowUpd = AnimorphRowUpdates(curSpeed, shufDepth, shufPhase, &tickAcc);
					shufPhase = (shufPhase + ReadLE16(&romData[0x5140 + (shufRate * 2)])) & 0xFFF;
					if (rowUpd != lastRowUpd)
					{
						rowTempo = (long)(12.0 * rowUpd * 33485.6 + 0.5);
						if (rowTempo > 0xFFFFFF)
						{
							rowTempo = 0xFFFFFF;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], rowTempo);
						ctrlMidPos += 2;
						lastRowUpd = rowUpd;
					}

					seqTime += 5;
					ctrlDelay += 5;
				}

				/*Cut every channel back to the pattern end (longer streams, or ones in
				  the middle of a wait, stop here)*/
				for (j = 0; j < 4; j++)
				{
					if (masterDelays[j] > seqTime)
					{
						curDelays[j] -= (masterDelays[j] - seqTime);
					}
					masterDelays[j] = seqTime;
				}
				romPos++;
			}

			else if (EventMap[exRomData[romPos]] == ANIMORPH_EVENT_PAN)
			{
				romPos += 2;
			}

			else if (EventMap[exRomData[romPos]] == ANIMORPH_EVENT_SHUFFLE)
			{
				/*FC phase, depth<<4 | rate*/
				shufPhase = (exRomData[romPos + 1] << 4) & 0xFFF;
				shufRate = exRomData[romPos + 2] & 0x0F;
				shufDepth = exRomData[romPos + 2] >> 4;
				romPos += 3;
			}

			else if (EventMap[exRomData[romPos]] == ANIMORPH_EVENT_TRANSPOSE)
			{
				transpose = (signed char)(exRomData[romPos + 1]);
				romPos += 2;
			}

			else if (EventMap[exRomData[romPos]] == ANIMORPH_EVENT_SPEED)
			{
				/*the row clock writes the tempo*/
				curSpeed = exRomData[romPos + 1];
				romPos += 2;
			}

			else if (EventMap[exRomData[romPos]] == ANIMORPH_EVENT_STOP)
			{
				songEnd = 1;
			}
		}
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (holdNotes[curTrack] == 1)
			{
				tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
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
			free(multiMidData[curTrack]);
		}

		free(ctrlMidData);
		fclose(mid);

	}
}