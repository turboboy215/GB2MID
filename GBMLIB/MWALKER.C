/*Martin Walker*/
/*For Metal Walker, see TOSE.*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MWALKER.H"

#define bankSize 16384

FILE* rom, * mid;
int bank;
long offset;
long addTable;
long baseValue;
long tablePtrLoc;
long seqPtrList;
long seqPtrList2;
long seqData;
long songList;
long patData;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long curSpeed;
long nextPtr;
long endPtr;
long bankAmt;
long nextBase;
int addValues[7];
int multiBanks;
int curBank;
int usePALTempo;

char folderName[100];

long switchPoint[400][2];
int switchNum;

int sysMode;

int drvVers;

char* argv3;

char SMSGGString[9];

int curVol;
int curVols[5];
int volTrack;

const unsigned char MWMagicBytes[12] = { 0x3E, 0x77, 0xE0, 0x24, 0x3E, 0xFF, 0xE0, 0x25, 0x3E, 0x8F, 0xE0, 0x26 };
const unsigned char MWMagicBytesGGA[9] = { 0x3E, 0x0D, 0x32, 0x04, 0xC5, 0x3E, 0x0F, 0x32, 0x03 };
const unsigned char MWMagicBytesGGB[9] = { 0x3E, 0x0D, 0x32, 0x04, 0xDE, 0x3E, 0x0F, 0x32, 0x03 };
const unsigned char MWMagicBytesGGC[9] = { 0x3E, 0x0D, 0x32, 0x04, 0xC7, 0x3E, 0x0F, 0x32, 0x03 };
const unsigned char MWMagicBytesGGD[9] = { 0x3E, 0x0D, 0x32, 0xF4, 0xDE, 0x3E, 0x0F, 0x32, 0xF3 };

unsigned long seqList[500];
unsigned long chanPts[3];
int totalSeqs;

unsigned char* romData;
unsigned char* midData;
unsigned char* multiMidData[8];
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void MWsong2mid(int songNum, long ptrs[4]);

void MWProc(int bank)
{
	drvVers = MWALKER_VER_STD;
	sysMode = MWALKER_SYSTEM_GB;
	curVol = 120;
	usePALTempo = 0;

	if (bank < 0x02)
	{
		bankAmt = 0x0000;
		bank = 0x02;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);


	/*Try to search the bank for base table*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], MWMagicBytes, 12))
		{
			if ((romData[i + 13]) == 0x21)
			{
				/*Standard driver version (version 2.x or later)*/
				drvVers = MWALKER_VER_STD;
			}
			else if ((romData[i + 13]) == 0xCB)
			{
				/*Early driver - Speedball 2 (version 1.x)*/
				drvVers = MWALKER_VER1;
			}

			tablePtrLoc = i + 13;
			printf("Found base table values at at address 0x%04X!\n", tablePtrLoc);
			if (drvVers == MWALKER_VER1)
			{
				printf("Detected old driver version (V1.x).\n");
			}

			if (drvVers == MWALKER_VER_STD)
			{			
				addTable = ReadLE16(&romData[tablePtrLoc + 1]);
				baseValue = ReadLE16(&romData[tablePtrLoc + 4]);
			}
			else if (drvVers == MWALKER_VER1)
			{
				baseValue = ReadLE16(&romData[tablePtrLoc + 3]);
				addTable = ReadLE16(&romData[tablePtrLoc + 6]);
			}

			printf("Base value: 0x%04X\nAdd table: 0x%04X\n", baseValue, addTable);
			break;
		}
	}

	/*Get pointers to sequence and song data*/
	if (addTable != 0 && baseValue != 0)
	{
		if (drvVers == MWALKER_VER_STD)
		{
			for (j = 0; j < 7; j++)
			{
				addValues[j] = ReadLE16(&romData[(addTable) + (2 * j)]);
			}
			seqPtrList = baseValue + addValues[0] + addValues[1] + addValues[2];
			printf("Sequence pointer list: 0x%04X\n", seqPtrList);
			seqData = seqPtrList + addValues[3];
			printf("Sequence data: 0x%04X\n", seqData);
			songList = seqData + addValues[4];
			printf("Song pointer list: 0x%04X\n", songList);
			patData = songList + addValues[5];
			printf("Song track data: 0x%04X\n", patData);

			i = songList;
			songNum = 1;
			/*Get song information*/
			while (i != patData)
			{
				songPtrs[0] = ReadLE16(&romData[i]) + patData;
				printf("Song %i channel 1 pointer: 0x%04X\n", songNum, songPtrs[0]);
				songPtrs[1] = ReadLE16(&romData[i + 2]) + patData;
				printf("Song %i channel 2 pointer: 0x%04X\n", songNum, songPtrs[1]);
				songPtrs[2] = ReadLE16(&romData[i + 4]) + patData;
				printf("Song %i channel 3 pointer: 0x%04X\n", songNum, songPtrs[2]);
				songPtrs[3] = ReadLE16(&romData[i + 6]);
				MWsong2mid(songNum, songPtrs);
				i += 8;
				songNum++;
			}
		}
		else if (drvVers == MWALKER_VER1)
		{
			for (j = 0; j < 6; j++)
			{
				addValues[j] = ReadLE16(&romData[(addTable)+(2 * j)]);
			}
			seqPtrList = baseValue + addValues[0] + addValues[1] + addValues[1];
			printf("Sequence pointer (low values) list: 0x%04X\n", seqPtrList);
			seqPtrList2 = seqPtrList + addValues[2];
			printf("Sequence pointer (high values) list: 0x%04X\n", seqPtrList2);
			seqData = seqPtrList + addValues[2] + addValues[2];
			printf("Sequence data: 0x%04X\n", seqData);
			songList = seqData + addValues[3];
			printf("Song pointer list: 0x%04X\n", songList);
			patData = songList + addValues[4];
			printf("Song track data: 0x%04X\n", patData);

			i = songList;
			songNum = 1;
			/*Get song information*/
			songPtrs[0] = patData + (romData[i] + (romData[i + 3] * 0x100));
			printf("Song %i channel 1 pointer: 0x%04X\n", songNum, songPtrs[0]);
			songPtrs[1] = patData + (romData[i + 1] + (romData[i + 4] * 0x100));
			printf("Song %i channel 2 pointer: 0x%04X\n", songNum, songPtrs[1]);
			songPtrs[2] = patData + (romData[i + 2] + (romData[i + 5] * 0x100));
			printf("Song %i channel 3 pointer: 0x%04X\n", songNum, songPtrs[2]);
			MWsong2mid(songNum, songPtrs);

			songNum++;

			/*Now do the rest of the music*/
			baseValue = ReadLE16(&romData[tablePtrLoc + 11]);
			addTable = ReadLE16(&romData[tablePtrLoc + 14]);
			printf("Base value: 0x%04X\nAdd table: 0x%04X\n", baseValue, addTable);

			for (j = 0; j < 6; j++)
			{
				addValues[j] = ReadLE16(&romData[(addTable)+(2 * j)]);
			}
			seqPtrList = baseValue + addValues[0] + addValues[1] + addValues[1];
			printf("Sequence pointer (low values) list: 0x%04X\n", seqPtrList);
			seqPtrList2 = seqPtrList + addValues[2];
			printf("Sequence pointer (high values) list: 0x%04X\n", seqPtrList2);
			seqData = seqPtrList + addValues[2] + addValues[2];
			printf("Sequence data: 0x%04X\n", seqData);
			songList = seqData + addValues[3];
			printf("Song pointer list: 0x%04X\n", songList);
			patData = songList + addValues[4];
			printf("Song track data: 0x%04X\n", patData);

			i = songList;
			/*Get song information*/
			while (i != patData)
			{
				songPtrs[0] = patData + (romData[i] + (romData[i + 3] * 0x100));
				printf("Song %i channel 1 pointer: 0x%04X\n", songNum, songPtrs[0]);
				songPtrs[1] = patData + (romData[i + 1] + (romData[i + 4] * 0x100));
				printf("Song %i channel 2 pointer: 0x%04X\n", songNum, songPtrs[1]);
				songPtrs[2] = patData + (romData[i + 2] + (romData[i + 5] * 0x100));
				printf("Song %i channel 3 pointer: 0x%04X\n", songNum, songPtrs[2]);
				MWsong2mid(songNum, songPtrs);
				i += 16;
				songNum++;
			}

		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
}

/*Convert the song data to MIDI*/
void MWsong2mid(int songNum, long ptrs[3])
{
	static const char* TRK_NAMES_GB[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	static const char* TRK_NAMES_GG[4] = { "Square 1", "Square 2", "Square 3", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	unsigned int midPosM[5];
	int trackCnt = 3;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int curDelays[5];
	int prevDelays[5];
	int midChan = 0;
	int seqEnd = 0;
	int seqsEnd[5];
	int tracksEnd[5];
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;
	long startPos = 0;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	long trackSize = 0;
	long trackSizes[5];
	long ctrlTrackSize = 0;

	unsigned int curNote = 0;
	unsigned int curNotes[5];
	unsigned int prevNotes[5];
	int curNoteLen = 0;
	int curNoteLens[5];
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 140;
	float fadeTempo;

	int curInst = 0;
	int curInsts[5];

	int inSeq = 0;
	int inSeqM[5];

	int playTimes = 1;

	unsigned long patPos = 0;
	unsigned long seqPos = 0;
	unsigned long patPosM[5];

	unsigned char command[4];
	unsigned char patCommand[4];

	signed int transpose = 0;
	signed int transposes[5];

	int firstNote = 1;
	int firstNotes[5];

	int timeVal = 0;

	int holdNote = 0;
	int holdNotes[5];

	long ctrlDelay = 0;
	long masterDelay = 0;
	long masterDelays[5];

	int tie[4];
	int instChange[4];

	int curPhrase;
	int phraseEnd[4] = { 0, 0, 0, 0 };

	int inMacro = 0;
	int inMacroM[5];
	int macros[5][4];

	int playTrack = 0;

	int seqPosM[5];
	int startSeqPosM[5];

	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	int valSize = 0;

	int seqTime = 0;

	int trackLoops[5][2];

	int seqRepeat[5];

	int hasLooped[4] = { 0, 0, 0, 0 };

	trackCnt = 3;

	tempo = 120;

	for (curTrack = 0; curTrack < trackCnt; curTrack++)
	{
		midPosM[curTrack] = 0;
	}

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

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

		/*Now retrieve version information...*/

		switch (drvVers)
		{
		case MWALKER_VER1:
			/*Fall-through*/
		case MWALKER_VER_STD:
		default:
			MWALKER_STATUS_SEQ_MIN = 0x00;
			MWALKER_STATUS_SEQ_MAX = 0x3F;
			MWALKER_STATUS_LOOP_STOP = 0x40;
			MWALKER_STATUS_LOOP_START_MIN = 0x41;
			MWALKER_STATUS_LOOP_START_MAX = 0xFC;
			MWALKER_STATUS_FADE = 0xFD;
			MWALKER_STATUS_STOP = 0xFE;
			MWALKER_STATUS_JUMP = 0xFF;
			MWALKER_STATUS_NOTE_MIN = 0x00;
			MWALKER_STATUS_NOTE_MAX = 0x7F;
			EventMap[0x80] = MWALKER_EVENT_PROG_CHANGE;
			EventMap[0x81] = MWALKER_EVENT_REST;
			EventMap[0x82] = MWALKER_EVENT_BEND;
			EventMap[0x83] = MWALKER_EVENT_TEMPO;
			EventMap[0x84] = MWALKER_EVENT_TIE;
			EventMap[0x85] = MWALKER_EVENT_GATE;
			EventMap[0x86] = MWALKER_EVENT_GLOBAL_TRANSPOSE;
			EventMap[0xFF] = MWALKER_EVENT_RETURN;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			transposes[curTrack] = 0;
			curDelays[curTrack] = 0;
			ctrlDelay = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			seqsEnd[curTrack] = 1;
			seqRepeat[curTrack] = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			curNotes[curTrack] = 0;
			prevNotes[curTrack] = 0;
			curNoteLens[curTrack] = 0;
			curInsts[curTrack] = 0;
			transposes[curTrack] = 0;
			tracksEnd[curTrack] = 0;
			inMacroM[curTrack] = 0;
			macros[curTrack][1] = 0;
			macros[curTrack][3] = 0;
			patPosM[curTrack] = ptrs[curTrack];
			phraseEnd[curTrack] = 1;
			hasLooped[curTrack] = 0;
			instChange[curTrack] = 0;
			tie[curTrack] = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
			midPosM[curTrack] += valSize;
			WriteBE16(&multiMidData[curTrack][midPosM[curTrack]], 0xFF03);
			midPosM[curTrack] += 2;
			Write8B(&multiMidData[curTrack][midPosM[curTrack]], strlen(TRK_NAMES_GB[curTrack]));
			midPosM[curTrack]++;
			sprintf((char*)&multiMidData[curTrack][midPosM[curTrack]], TRK_NAMES_GB[curTrack]);
			midPosM[curTrack] += strlen(TRK_NAMES_GB[curTrack]);

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		seqTime = 0;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (ptrs[curTrack] == 0)
			{
				trackEnd = 1;
			}
			else
			{
				trackEnd = 0;
			}
		}

		while (trackEnd == 0)
		{
			if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1)
			{
				trackEnd = 1;
			}
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] == 0)
				{
					if (seqsEnd[curTrack] == 1)
					{
						command[0] = romData[patPosM[curTrack]];
						command[1] = romData[patPosM[curTrack] + 1];
						command[2] = romData[patPosM[curTrack] + 2];
						command[3] = romData[patPosM[curTrack] + 3];

						if (command[0] >= MWALKER_STATUS_SEQ_MIN && command[0] <= MWALKER_STATUS_SEQ_MAX)
						{
							curSeq = command[1] - MWALKER_STATUS_SEQ_MIN;
							seqRepeat[curTrack] = command[0];
							seqsEnd[curTrack] = 0;
							if (drvVers == MWALKER_VER1)
							{
								startSeqPosM[curTrack] = romData[seqPtrList + curSeq] + (romData[seqPtrList2 + curSeq] * 0x100) + seqData;
							}
							else
							{
								startSeqPosM[curTrack] = ReadLE16(&romData[seqPtrList + (curSeq * 2)]) + seqData;
							}

							seqPosM[curTrack] = startSeqPosM[curTrack];

						}
						else if (command[0] == MWALKER_STATUS_LOOP_STOP)
						{
							if (trackLoops[curTrack][0] > 1)
							{
								patPosM[curTrack] = trackLoops[curTrack][1];
								trackLoops[curTrack][0]--;
							}
							else
							{
								patPosM[curTrack]++;
							}
						}

						else if (command[0] >= MWALKER_STATUS_LOOP_START_MIN && command[0] <= MWALKER_STATUS_LOOP_START_MAX)
						{
							trackLoops[curTrack][0] = command[0] - MWALKER_STATUS_LOOP_START_MIN + 1;
							trackLoops[curTrack][1] = patPosM[curTrack] + 1;
							patPosM[curTrack]++;
						}

						else if (command[0] == MWALKER_STATUS_FADE)
						{
							patPosM[curTrack] += 2;
						}

						else if (command[0] == MWALKER_STATUS_STOP)
						{
							tracksEnd[curTrack] = 1;
						}

						else if (command[0] == MWALKER_STATUS_JUMP)
						{
							tracksEnd[curTrack] = 1;
						}
					}
					else
					{
						command[0] = romData[seqPosM[curTrack]];
						command[1] = romData[seqPosM[curTrack] + 1];
						command[2] = romData[seqPosM[curTrack] + 2];
						command[3] = romData[seqPosM[curTrack] + 3];

						if (drvVers == MWALKER_VER1)
						{
							if (EventMap[command[0]] == MWALKER_EVENT_RETURN)
							{
								if (seqRepeat[curTrack] > 1)
								{
									seqPosM[curTrack] = startSeqPosM[curTrack];
									seqRepeat[curTrack]--;
								}
								else
								{
									seqsEnd[curTrack] = 1;
									patPosM[curTrack] += 2;
								}
							}
							else
							{
								/*Rest*/
								if ((command[0] & 0x40) != 0x00)
								{
									if (holdNotes[curTrack] == 1)
									{
										tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										holdNotes[curTrack] = 0;
										curDelays[curTrack] = 0;
										midPosM[curTrack] = tempPos;
										if (instChange[curTrack] == 1)
										{
											firstNotes[curTrack] = 1;
											instChange[curTrack] = 0;
										}
									}
									curNoteLens[curTrack] = (command[0] & 0x3F) * 10;
									curDelays[curTrack] += curNoteLens[curTrack];
									masterDelays[curTrack] += curNoteLens[curTrack];
									seqPosM[curTrack]++;
								}
								/*Set voice OR pitch bend and play note*/
								else if ((command[0] & 0x80) != 0x00)
								{
									if (holdNotes[curTrack] == 1)
									{
										tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										holdNotes[curTrack] = 0;
										curDelays[curTrack] = 0;
										midPosM[curTrack] = tempPos;
										if (instChange[curTrack] == 1)
										{
											firstNotes[curTrack] = 1;
											instChange[curTrack] = 0;
										}
									}
									/*Voice change*/
									if ((command[1] & 0x80) == 0x00)
									{
										curInsts[curTrack] = command[1];
										firstNotes[curTrack] = 1;
									}
									/*Pitch bend*/
									else
									{
										tempPos = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], curDelays[curTrack]);
										midPosM[curTrack] += tempPos;
										Write8B(&multiMidData[curTrack][midPosM[curTrack]], (0xE0 | curTrack));
										Write8B(&multiMidData[curTrack][midPosM[curTrack] + 1], 0);
										Write8B(&multiMidData[curTrack][midPosM[curTrack] + 2], 0x40);
										Write8B(&multiMidData[curTrack][midPosM[curTrack] + 3], 0);
										curDelays[curTrack] = 0;
										firstNotes[curTrack] = 1;
										midPosM[curTrack] += 3;
									}
									curNotes[curTrack] = (command[2] & 0x7F) + 12;
									curNoteLens[curTrack] = (command[0] & 0x3F) * 10;
									tempPos = WriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									firstNotes[curTrack] = 0;
									midPosM[curTrack] = tempPos;
									curDelays[curTrack] = curNoteLens[curTrack];
									holdNotes[curTrack] = 1;
									masterDelays[curTrack] += curNoteLens[curTrack];
									seqPosM[curTrack] += 3;
								}
								/*Play note*/
								else
								{
									if (holdNotes[curTrack] == 1)
									{
										tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										holdNotes[curTrack] = 0;
										curDelays[curTrack] = 0;
										midPosM[curTrack] = tempPos;
										if (instChange[curTrack] == 1)
										{
											firstNotes[curTrack] = 1;
											instChange[curTrack] = 0;
										}
									}
									curNotes[curTrack] = (command[1] & 0x7F) + 12;
									curNoteLens[curTrack] = (command[0] & 0x3F) * 10;
									tempPos = WriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									firstNotes[curTrack] = 0;
									midPosM[curTrack] = tempPos;
									curDelays[curTrack] = curNoteLens[curTrack];
									holdNotes[curTrack] = 1;
									masterDelays[curTrack] += curNoteLens[curTrack];
									seqPosM[curTrack] += 2;
								}
							}
						}
						else
						{
							if (command[0] >= MWALKER_STATUS_NOTE_MIN && command[0] <= MWALKER_STATUS_NOTE_MAX)
							{
								if (holdNotes[curTrack] == 1)
								{
									tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									holdNotes[curTrack] = 0;
									curDelays[curTrack] = 0;
									midPosM[curTrack] = tempPos;
									if (instChange[curTrack] == 1)
									{
										firstNotes[curTrack] = 1;
										instChange[curTrack] = 0;
									}
									tie[curTrack] = 0;
								}
								curNotes[curTrack] = command[0] + transpose;
								curNoteLens[curTrack] = command[1] * 10;
								tempPos = WriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								firstNotes[curTrack] = 0;
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = curNoteLens[curTrack];
								holdNotes[curTrack] = 1;
								masterDelays[curTrack] += curNoteLens[curTrack];
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_PROG_CHANGE)
							{
								curInsts[curTrack] = command[1];
								instChange[curTrack] = 1;
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_REST)
							{
								if (holdNotes[curTrack] == 1)
								{
									tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									holdNotes[curTrack] = 0;
									curDelays[curTrack] = 0;
									midPosM[curTrack] = tempPos;
									if (instChange[curTrack] == 1)
									{
										firstNotes[curTrack] = 1;
										instChange[curTrack] = 0;
									}
								}
								curNoteLens[curTrack] = command[1] * 10;
								curDelays[curTrack] += curNoteLens[curTrack];
								masterDelays[curTrack] += curNoteLens[curTrack];
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_BEND)
							{
								if (holdNotes[curTrack] == 1)
								{
									tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									holdNotes[curTrack] = 0;
									curDelays[curTrack] = 0;
									midPosM[curTrack] = tempPos;
									if (instChange[curTrack] == 1)
									{
										firstNotes[curTrack] = 1;
										instChange[curTrack] = 0;
									}
								}
								tempPos = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], curDelays[curTrack]);
								midPosM[curTrack] += tempPos;
								Write8B(&multiMidData[curTrack][midPosM[curTrack]], (0xE0 | curTrack));
								Write8B(&multiMidData[curTrack][midPosM[curTrack] + 1], 0);
								Write8B(&multiMidData[curTrack][midPosM[curTrack] + 2], 0x40);
								Write8B(&multiMidData[curTrack][midPosM[curTrack] + 3], 0);
								curDelays[curTrack] = 0;
								firstNotes[curTrack] = 1;
								midPosM[curTrack] += 3;
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_TEMPO)
							{
								tempByte = command[1] * 1.2;
								if (usePALTempo == 1)
								{
									tempByte = tempByte * 0.83;
								}
								if (tempo != tempByte)
								{
									ctrlMidPos++;
									valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
									ctrlDelay = 0;
									ctrlMidPos += valSize;
									WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
									ctrlMidPos += 3;
									tempo = command[1] * 1.2;
									if (usePALTempo == 1)
									{
										tempo = tempo * 0.83;
									}
									WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
									ctrlMidPos += 2;
								}
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_TIE)
							{
								tie[curTrack] = 1;
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_GATE)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_GLOBAL_TRANSPOSE)
							{
								transpose = (signed char)command[1];
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == MWALKER_EVENT_RETURN)
							{
								if (seqRepeat[curTrack] > 1)
								{
									seqPosM[curTrack] = startSeqPosM[curTrack];
									seqRepeat[curTrack]--;
								}
								else
								{
									seqsEnd[curTrack] = 1;
									patPosM[curTrack] += 2;
								}

							}

							/*Unknown command*/
							else
							{
								seqPosM[curTrack]++;
							}
						}
					}
				}
			}
			seqTime += 5;
			ctrlDelay += 5;
		}
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (holdNotes[curTrack] == 1)
			{
				tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
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

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
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