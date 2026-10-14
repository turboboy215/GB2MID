/*Taro Bando (Mole Mania/Moguranya)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <direct.h>
#include "SHARED.H"
#include "MOLE.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long sfxTablePtrLoc;
long sfxTableOffset;
long lenTablesStart;
int i, j;
char outfile[1000000];
int songNum;
long curPtr;
long seqPtrs[4];
long songPtr;
long bankAmt;
int curVol;
int drvVers;
int foundTable;
int songTempo;
int curInst;
int progBank;

int multiBanks;
int curBank;

char folderName[100];

unsigned const char MoleMagicBytes[7] = { 0xDC, 0x3D, 0x07, 0x4F, 0x06, 0x00, 0x21 };
unsigned const char MoleMagicBytesSFX[7] = { 0xF5, 0x3D, 0x07, 0x4F, 0x06, 0x00, 0x21 };

unsigned const char MoleMagicBytesLen[9] = { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x12, 0x24, 0x48 };

unsigned char* romData;
unsigned char* multiMidData[8];
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
void Molesong2mid(int songNum, long ptr);

void MoleProc(int bank)
{
	drvVers = MOLE_VER_STD;
	curInst = 0;
	curVol = 120;
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

	/*Try to search the bank for song table loader*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MoleMagicBytes, 7)))
		{
			if (foundTable == 0)
			{
				tablePtrLoc = i + 7;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
			}
		}
	}

	/*Now search for SFX table loader*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MoleMagicBytesSFX, 7)))
		{
			if (foundTable == 1)
			{
				sfxTablePtrLoc = i + 7;
				printf("Found pointer to SFX table at address 0x%04X!\n", sfxTablePtrLoc);
				sfxTableOffset = ReadLE16(&romData[sfxTablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", sfxTableOffset);
				foundTable = 2;
			}
		}
	}

	/*Try to search the bank for note length tables*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MoleMagicBytesLen, 9)))
		{
			lenTablesStart = i;
			printf("Found pointer to note length table at address 0x%04X!\n", lenTablesStart);
		}
	}

	if (foundTable == 2)
	{
		i = tableOffset;
		songNum = 1;

		while (i != sfxTableOffset)
		{
			songPtr = ReadBE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Molesong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
		free(romData);
	}

}

/*Convert the song data to MIDI*/
void Molesong2mid(int songNum, long ptr)
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
	int curVol = 0;
	int curVols[4];
	int holdNote = 0;
	int holdNotes[4];
	int transpose = 0;
	int transposes[4];
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int firstNote = 0;
	int firstNotes[4];
	long seqTime = 0;
	int curInsts[4];
	int prevNotes[4];
	int tie;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long framePtr;

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
		case MOLE_VER_STD:
			/*Fall-through*/
		default:
			EventMap[0x00] = MOLE_EVENT_END;
			EventMap[0x01] = MOLE_EVENT_LEN_TABLE;
			EventMap[0x02] = MOLE_EVENT_WAVEFORM;
			EventMap[0x03] = MOLE_EVENT_VIBRATO_ON;
			EventMap[0x04] = MOLE_EVENT_VIBRATO_OFF;
			EventMap[0x05] = MOLE_EVENT_RELEASE_ON;
			EventMap[0x06] = MOLE_EVENT_RELEASE_OFF;
			EventMap[0x07] = MOLE_EVENT_PITCH_SLIDE_ON;
			EventMap[0x08] = MOLE_EVENT_ABS_NOTE_LEN;
			EventMap[0x09] = MOLE_EVENT_TRANSPOSE;
			EventMap[0x0A] = MOLE_EVENT_PAN_LR;
			EventMap[0x0B] = MOLE_EVENT_PAN_R;
			EventMap[0x0C] = MOLE_EVENT_PAN_L;
			EventMap[0x0D] = MOLE_EVENT_ABS_NOTE_LEN;
			EventMap[0x0E] = MOLE_EVENT_CH3_DECAY;
			EventMap[0x0F] = MOLE_EVENT_SCOOP_ON;
			EventMap[0x10] = MOLE_EVENT_DUTY_SEQ;
			EventMap[0x11] = MOLE_EVENT_ENV_SEQ;
			EventMap[0x12] = MOLE_EVENT_PITCH_SLIDE_OFF;
			EventMap[0x13] = MOLE_EVENT_SCOOP_OFF;
			MOLE_STATUS_NOTE_LEN_MIN = 0x20;
			MOLE_STATUS_NOTE_LEN_MAX = 0x2B;
			MOLE_STATUS_CH3_VOL_MIN = 0x2C;
			MOLE_STATUS_CH3_VOL_MAX = 0x2F;
			MOLE_STATUS_NOTE_DUR_MIN = 0x30;
			MOLE_STATUS_NOTE_DUR_MAX = 0x6F;
			MOLE_STATUS_REST = 0x70;
			MOLE_STATUS_NOTE_MIN = 0x71;
			MOLE_STATUS_NOTE_MAX = 0xB9;
			MOLE_STATUS_TIE = 0xBA;
			MOLE_STATUS_CH4_NOTE_MIN = 0xBC;
			MOLE_STATUS_CH4_NOTE_MAX = 0xFF;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			curDelays[curTrack] = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			curNotes[curTrack] = 0;
			curNoteLens[curTrack] = 0;
			curInsts[curTrack] = 0;
			tracksEnd[curTrack] = 0;
			transposes[curTrack] = 0;

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


		ctrlDelay = 0;
		seqTime = 0;

		patPos = songPtr;
		curNoteLenTab = lenTablesStart;
		tie = 0;

		while (songEnd == 0)
		{
			/*Get current frame pointers*/
			if (romData[patPos] == 0x00)
			{
				songEnd = 1;
			}
			else
			{
				framePtr = ReadBE16(&romData[patPos]);

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					seqEnd = 0;
					seqPosM[curTrack] = ReadBE16(&romData[framePtr + (curTrack * 2)]);
				}

				while (seqEnd == 0)
				{
					for (curTrack = 0; curTrack < trackCnt; curTrack++)
					{
						if (seqPosM[curTrack] != 0x0000)
						{
							while (seqTime >= masterDelays[curTrack] && seqEnd == 0)
							{
								command[0] = romData[seqPosM[curTrack]];
								command[1] = romData[seqPosM[curTrack] + 1];
								command[2] = romData[seqPosM[curTrack] + 2];
								command[3] = romData[seqPosM[curTrack] + 3];

								if (EventMap[command[0]] == MOLE_EVENT_END)
								{
									seqEnd = 1;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_LEN_TABLE)
								{
									curNoteLenTab = lenTablesStart + ((command[1]) * 11);
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_WAVEFORM)
								{
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_VIBRATO_ON)
								{
									seqPosM[curTrack] += 4;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_VIBRATO_OFF)
								{
									seqPosM[curTrack]++;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_RELEASE_ON)
								{
									seqPosM[curTrack] += 3;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_RELEASE_OFF)
								{
									seqPosM[curTrack]++;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_PITCH_SLIDE_ON)
								{
									seqPosM[curTrack] += 4;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_ABS_NOTE_LEN)
								{
									curNoteLens[curTrack] = command[1] * 5;
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_TRANSPOSE)
								{
									transposes[curTrack] = (signed char)command[1];
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_PAN_LR)
								{
									seqPosM[curTrack]++;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_PAN_R)
								{
									seqPosM[curTrack]++;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_PAN_L)
								{
									seqPosM[curTrack]++;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_CH3_DECAY)
								{
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_SCOOP_ON)
								{
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_DUTY_SEQ)
								{
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_ENV_SEQ)
								{
									seqPosM[curTrack] += 2;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_PITCH_SLIDE_OFF)
								{
									seqPosM[curTrack]++;
								}

								else if (EventMap[command[0]] == MOLE_EVENT_SCOOP_OFF)
								{
									seqPosM[curTrack]++;
								}

								else if (command[0] >= MOLE_STATUS_NOTE_LEN_MIN && command[0] <= MOLE_STATUS_NOTE_LEN_MAX)
								{
									curNoteLens[curTrack] = romData[curNoteLenTab + (command[0] - MOLE_STATUS_NOTE_LEN_MIN)] * 5;
									seqPosM[curTrack]++;
								}

								else if (command[0] >= MOLE_STATUS_CH3_VOL_MIN && command[0] <= MOLE_STATUS_CH3_VOL_MAX)
								{
									curVols[curTrack] = command[0] - MOLE_STATUS_CH3_VOL_MIN;
									if (curVols[curTrack] == 0)
									{
										curVols[curTrack] = 1;
									}
									else if (curVols[curTrack] == 1)
									{
										curVols[curTrack] = 30;
									}
									else if (curVols[curTrack] == 2)
									{
										curVols[curTrack] = 60;
									}
									else
									{
										curVols[curTrack] = 120;
									}
									seqPosM[curTrack]++;
								}

								else if (command[0] >= MOLE_STATUS_NOTE_DUR_MIN && command[0] <= MOLE_STATUS_NOTE_DUR_MAX)
								{
									seqPosM[curTrack]++;
								}

								else if (command[0] == MOLE_STATUS_REST)
								{
									if (holdNotes[curTrack] == 1)
									{
										tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										holdNotes[curTrack] = 0;
										curDelays[curTrack] = 0;
										midPosM[curTrack] = tempPos;
									}
									curDelays[curTrack] += curNoteLens[curTrack];
									masterDelays[curTrack] += curNoteLens[curTrack];
									seqPosM[curTrack]++;
								}

								else if (command[0] >= MOLE_STATUS_NOTE_MIN && command[0] <= MOLE_STATUS_NOTE_MAX)
								{
									if (holdNotes[curTrack] == 1)
									{
										tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										holdNotes[curTrack] = 0;
										curDelays[curTrack] = 0;
										midPosM[curTrack] = tempPos;
									}
									curNotes[curTrack] = command[0] - MOLE_STATUS_NOTE_MIN + 24 + transposes[curTrack];

									if (curTrack != 2)
									{
										curNotes[curTrack] += 12;
									}
									curVol = curVols[curTrack];
									tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									firstNotes[curTrack] = 0;
									holdNotes[curTrack] = 1;
									midPosM[curTrack] = tempPos;
									curDelays[curTrack] = curNoteLens[curTrack];
									masterDelays[curTrack] += curNoteLens[curTrack];
									holdNotes[curTrack] = 1;
									tie = 0;
									seqPosM[curTrack]++;
								}

								else if (command[0] == MOLE_STATUS_TIE)
								{
									tie = 1;
									seqPosM[curTrack]++;
								}

								else if (command[0] >= MOLE_STATUS_CH4_NOTE_MIN && command[0] <= MOLE_STATUS_CH4_NOTE_MAX)
								{
									if (holdNotes[curTrack] == 1)
									{
										tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										holdNotes[curTrack] = 0;
										curDelays[curTrack] = 0;
										midPosM[curTrack] = tempPos;
									}
									curVol = curVols[curTrack];
									curNotes[curTrack] = command[0] - MOLE_STATUS_CH4_NOTE_MIN + 36;
									tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									firstNotes[curTrack] = 0;
									holdNotes[curTrack] = 1;
									midPosM[curTrack] = tempPos;
									curDelays[curTrack] = curNoteLens[curTrack];
									masterDelays[curTrack] += curNoteLens[curTrack];
									holdNotes[curTrack] = 1;
									seqPosM[curTrack]++;
								}

								/*Unknown command*/
								else
								{
									seqPosM[curTrack]++;
								}
							}
						}
						else
						{
							curDelays[curTrack] += 5;
							masterDelays[curTrack] += 5;
						}

					}
					seqTime += 5;
					ctrlDelay += 5;
				}
				for (j = 0; j < 4; j++)
				{
					if (masterDelays[j] > seqTime)
					{
						curDelays[j] -= (masterDelays[j] - seqTime);
					}
					masterDelays[j] = seqTime;
				}
				patPos += 2;
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