/*Imagitec Design*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "IMAGITEC.H"

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
int foundTable;
int curInst;
long firstPtr;
int numSongs;

long bankAmt;

int curVol;
int drvVers;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
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
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Imagitecsong2mid(int songNum, long songPtr);

void ImagitecProc(int bank, char parameters[4][100])
{
	drvVers = IMAGITEC_VER_STD;
	curVol = 120;

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

	if (parameters[0][0] != 0)
	{
		numSongs = strtol(parameters[0], NULL, 16);
	}

	songNum = 1;
	if (bank != 7)
	{
		i = 0x4000;
		while (songNum <= numSongs)
		{
			tableOffset = ReadLE16(&romData[i + 4]);
			songPtr = ReadLE16(&romData[tableOffset]);
			if (songPtr >= bankAmt && songPtr < 0x8000)
			{
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Imagitecsong2mid(songNum, songPtr);
			}
			else
			{
				printf("Song %i: 0x%04X (invalid, skipped)\n", songNum, songPtr);
			}
			i += 8;
			songNum++;
		}

	}
	else
	{
		tableOffset = ReadLE16(&romData[0x7938]);
		songPtr = tableOffset;
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		Imagitecsong2mid(songNum, songPtr);
	}



	free(romData);
}

/*Convert the song data to MIDI*/
void Imagitecsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
	long romPos = 0;
	long romPosM[5];
	long seqPos = 0;
	int seqPosM[5];
	int curTrack = 0;
	int trackCnt = 3;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int seqsEnd[5];
	int curNote = 0;
	unsigned int curNotes[5];
	int curNoteLen = 0;
	int curNoteLens[5];
	int curNoteLenVals[4];
	int transpose = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	int firstNotes[5];
	unsigned int midPos = 0;
	unsigned int midPosM[5];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	long trackSizes[5];
	long ctrlTrackSize = 0;
	int rest = 0;
	int curRepeat[4];
	int repeatTimes[4];
	int repeats[10][4][3];
	int repeatNums[4];
	int tempoVal = 0;
	int tempByte = 0;
	int curDelay = 0;
	int curDelays[5];
	int ctrlDelay = 0;
	int masterDelay = 0;
	long masterDelays[5];
	long tempPos = 0;
	int holdNote = 0;
	int holdNotes[5];
	int trackEnd = 0;
	int tracksEnd[5];
	long seqTime = 0;
	int curInsts[5];
	int inMacros[4];
	int macros[10][4][3];
	int transposes[4];

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

	for (curTrack = 0; curTrack < 4; curTrack++)
	{
		for (j = 0; j < 4; j++)
		{
			repeats[curTrack][j][0] = -1;
			repeats[curTrack][j][1] = 0;
			repeats[curTrack][j][2] = 0;
		}
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

		switch (drvVers)
		{
		case IMAGITEC_VER_STD:
			/*Fall-through*/
		default:
			IMAGITEC_STATUS_NOTE_MIN = 0x00;
			IMAGITEC_STATUS_NOTE_MAX = 0x5F;
			IMAGITEC_STATUS_NOTE_LEN_MIN = 0x70;
			IMAGITEC_STATUS_NOTE_LEN_MAX = 0xBF;
			IMAGITEC_STATUS_PROG_CHANGE_MIN = 0xC0;
			IMAGITEC_STATUS_PROG_CHANGE_MAX = 0xDF;
			IMAGITEC_STATUS_UNKNOWN_MIN = 0xE1;
			IMAGITEC_STATUS_UNKNOWN_MAX = 0xEF;
			IMAGITEC_STATUS_TEMPO_MIN = 0xF0;
			IMAGITEC_STATUS_TEMPO_MAX = 0xFE;

			EventMap[0x60] = IMAGITEC_EVENT_REPEAT_END;
			EventMap[0x61] = IMAGITEC_EVENT_REPEAT_START;
			EventMap[0x62] = IMAGITEC_EVENT_ENV_SEQ;
			EventMap[0x63] = IMAGITEC_EVENT_JUMP;
			EventMap[0x64] = IMAGITEC_EVENT_TRANSPOSE;
			EventMap[0x65] = IMAGITEC_EVENT_VIBRATO_SWITCH;
			EventMap[0x66] = IMAGITEC_EVENT_PAN;
			EventMap[0x67] = IMAGITEC_EVENT_CH3_LEN;
			EventMap[0x68] = IMAGITEC_EVENT_WAVEFORM;
			EventMap[0x69] = IMAGITEC_EVENT_ARP_OFF;
			EventMap[0x6A] = IMAGITEC_EVENT_ARP_ON;
			EventMap[0xE0] = IMAGITEC_EVENT_PARAMS;
			EventMap[0xE1] = IMAGITEC_EVENT_NOTE_LEN;
			EventMap[0xFF] = IMAGITEC_EVENT_STOP;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			curDelays[curTrack] = 0;
			ctrlDelay = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			curNotes[curTrack] = 0;
			curNoteLens[curTrack] = 0;
			curNoteLenVals[curTrack] = 1;
			curInsts[curTrack] = 0;
			tracksEnd[curTrack] = 0;
			curRepeat[curTrack] = 1;
			repeatTimes[curTrack] = -1;
			repeatNums[curTrack] = 0;
			inMacros[curTrack] = 0;
			transposes[curTrack] = 0;

			romPosM[curTrack] = ReadLE16(&romData[songPtr + (curTrack * 2)]);
			seqPosM[curTrack] = ReadLE16(&romData[romPosM[curTrack]]);

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

			if (curTrack == 3)
			{
				tracksEnd[curTrack] = 1;
			}

		}

		seqTime = 0;

		while (trackEnd == 0)
		{
			if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1)
			{
				trackEnd = 1;
			}
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				if (seqPosM[curTrack] >= 0x8000 || seqPosM[curTrack] < bankAmt || midPosM[curTrack] >= 48000)
				{
					tracksEnd[curTrack] = 1;
				}
				while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] == 0 && seqPosM[curTrack] < 0x8000)
				{
					command[0] = romData[seqPosM[curTrack]];
					command[1] = romData[seqPosM[curTrack] + 1];
					command[2] = romData[seqPosM[curTrack] + 2];
					command[3] = romData[seqPosM[curTrack] + 3];

					if (command[0] >= IMAGITEC_STATUS_NOTE_MIN && command[0] <= IMAGITEC_STATUS_NOTE_MAX)
					{
						curNotes[curTrack] = command[0] + transposes[curTrack] + 24;

						if (curTrack < 2)
						{
							curNote += 12;
						}
						curInst = curInsts[curTrack];
						curNoteLens[curTrack] = curNoteLenVals[curTrack] * 40;
						tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
						firstNotes[curTrack] = 0;
						holdNotes[curTrack] = 0;
						midPosM[curTrack] = tempPos;
						curDelays[curTrack] = 0;
						masterDelays[curTrack] += curNoteLens[curTrack];
						seqPosM[curTrack]++;
					}

					else if (command[0] >= IMAGITEC_STATUS_NOTE_LEN_MIN && command[0] <= IMAGITEC_STATUS_NOTE_LEN_MAX)
					{
						curNoteLenVals[curTrack] = command[0] - 0x7F;
						seqPosM[curTrack]++;
					}

					else if (command[0] >= IMAGITEC_STATUS_PROG_CHANGE_MIN && command[0] <= IMAGITEC_STATUS_PROG_CHANGE_MAX)
					{
						curInsts[curTrack] = command[0] - IMAGITEC_STATUS_PROG_CHANGE_MIN;
						firstNotes[curTrack] = 1;
						seqPosM[curTrack]++;
					}

					else if (command[0] >= IMAGITEC_STATUS_UNKNOWN_MIN && command[0] <= IMAGITEC_STATUS_UNKNOWN_MAX)
					{
						seqPosM[curTrack]++;
					}

					else if (command[0] >= IMAGITEC_STATUS_TEMPO_MIN && command[0] <= IMAGITEC_STATUS_TEMPO_MAX)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempoVal = command[0] - 0xEF;
						tempo = 400000 * tempoVal / 7;
						WriteBE24(&ctrlMidData[ctrlMidPos], tempo);
						ctrlMidPos += 2;
						seqPosM[curTrack]++;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_UNKNOWN0)
					{
						seqPosM[curTrack]++;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_UNKNOWN1)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_REPEAT_END)
					{
						if (repeats[curTrack][repeatNums[curTrack]][0] > 1)
						{
							repeats[curTrack][repeatNums[curTrack]][0]--;
							seqPosM[curTrack] = repeats[curTrack][repeatNums[curTrack]][1];
						}
						else
						{
							repeats[curTrack][repeatNums[curTrack]][0] = -1;
							seqPosM[curTrack]++;

							if (repeatNums[curTrack] > 0)
							{
								repeatNums[curTrack]--;
							}


						}
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_REPEAT_START)
					{
						repeatNums[curTrack]++;
						if (repeatNums[curTrack] >= 4)
						{
							tracksEnd[curTrack] = 1;
						}
						else
						{
							repeats[curTrack][repeatNums[curTrack]][0] = command[1];
							repeats[curTrack][repeatNums[curTrack]][1] = seqPosM[curTrack] + 2;
							seqPosM[curTrack] += 2;
						}

					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_ENV_SEQ)
					{
						if (inMacros[curTrack] >= 4)
						{
							tracksEnd[curTrack] = 1;
						}
						else
						{
							inMacros[curTrack]++;
							macros[curTrack][inMacros[curTrack]][0] = ReadLE16(&romData[seqPosM[curTrack]] + 1);
							macros[curTrack][inMacros[curTrack]][1] = seqPosM[curTrack] + 3;
							seqPosM[curTrack] = macros[curTrack][inMacros[curTrack]][0];
						}

					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_JUMP)
					{
						if (ReadLE16(&romData[seqPosM[curTrack] + 1]) > seqPosM[curTrack])
						{
							seqPosM[curTrack] = ReadLE16(&romData[seqPosM[curTrack] + 1]);
						}
						else
						{
							tracksEnd[curTrack] = 1;
						}
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_TRANSPOSE)
					{
						transposes[curTrack] = (signed char)command[1];
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_VIBRATO_SWITCH)
					{
						seqPosM[curTrack]++;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_PAN)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_CH3_LEN)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_WAVEFORM)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_ARP_OFF)
					{
						seqPosM[curTrack]++;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_ARP_ON)
					{
						seqPosM[curTrack]++;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_PARAMS)
					{
						curNoteLens[curTrack] = curNoteLenVals[curTrack] * 40;
						curDelays[curTrack] += curNoteLens[curTrack];
						masterDelays[curTrack] += curNoteLens[curTrack];
						seqPosM[curTrack] += 4;
					}

					else if (EventMap[command[0]] == IMAGITEC_EVENT_STOP)
					{
						if (inMacros[curTrack] == 0)
						{
							romPosM[curTrack] += 2;
							if (ReadLE16(&romData[romPosM[curTrack]]) != 0x0000)
							{
								seqPosM[curTrack] = ReadLE16(&romData[romPosM[curTrack]]);
							}
							else
							{
								tracksEnd[curTrack] = 1;
							}
						}
						else
						{
							seqPosM[curTrack] = macros[curTrack][inMacros[curTrack]][1];
							inMacros[curTrack]--;
						}


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