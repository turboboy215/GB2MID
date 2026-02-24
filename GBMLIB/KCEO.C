/*Konami (KCE Osaka)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "KCEO.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long firstPtr;
long bankAmt;
int foundTable;
int curInst;
int drvVers;
int songBank;
int tableBank;
int curVol;
int spawnFix;

const unsigned char KCEOMagicBytes[5] = { 0x29, 0x29, 0x29, 0x19, 0x11 };

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void KCEOsong2mid(int songNum, long songPtrs[4]);

void KCEOProc(int bank)
{
	curInst = 0;
	drvVers = KCEO_VER_STD;
	bankAmt = bankSize;
	spawnFix = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	/*Fix Spawn tempo*/
	if (bank == 0x7D)
	{
		spawnFix = 1;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KCEOMagicBytes, 5)) && foundTable != 1)
		{
			tablePtrLoc = i + 5;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		while (ReadLE16(&romData[i + 1]) >= bankAmt && ReadLE16(&romData[i + 1]) < (bankSize * 2) && romData[i] < 0x80)
		{
			songBank = romData[i];
			printf("Song %i bank: %02X\n", songNum, songBank);
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, (songBank * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);
			seqPtrs[0] = ReadLE16(&romData[i + 1]);
			printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
			seqPtrs[1] = ReadLE16(&romData[i + 3]);
			printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
			seqPtrs[2] = ReadLE16(&romData[i + 5]);
			printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
			seqPtrs[3] = ReadLE16(&romData[i + 7]);
			printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
			KCEOsong2mid(songNum, seqPtrs);
			free(exRomData);
			i += 9;
			songNum++;
		}
		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}
}

/*Convert the song data to MIDI*/
void KCEOsong2mid(int songNum, long songPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int maskArray[8];
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int tempoVal = 0;
	int chanSpeed;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int autoLen = 0;
	int transpose = 0;
	unsigned char command[4];
	int firstNote = 1;
	int holdNote = 0;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int masterDelay = 0;
	long tempPos = 0;
	int songLoop = 0;
	int inMacro = 0;
	long macroPos = 0;
	long macroRet = 0;
	int repeat1 = 0;
	int repeat1Times = 0;
	long repeat1Pos = 0;
	int repeat2 = 0;
	int repeat2Times = 0;
	long repeat2Pos = 0;
	int hasSize = 0;
	int hasLen = 0;
	int newNote = 0;
	int condJump = 0;
	int condJumpStart = 0;
	int condJumpStop = 0;
	int pitchBend = 0;

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
		case KCEO_VER_STD:
			/*Fall-through*/
		default:
			KCEO_STATUS_NOTE_MIN = 0x00;
			KCEO_STATUS_NOTE_MAX = 0x7F;

			EventMap[0x80] = KCEO_EVENT_DUTY0;
			EventMap[0x81] = KCEO_EVENT_DUTY1;
			EventMap[0x82] = KCEO_EVENT_DUTY2;
			EventMap[0x83] = KCEO_EVENT_DUTY3;
			EventMap[0x84] = KCEO_EVENT_WAVE;
			EventMap[0x85] = KCEO_EVENT_WAVE;
			EventMap[0x86] = KCEO_EVENT_WAVE;
			EventMap[0x87] = KCEO_EVENT_WAVE;
			EventMap[0x88] = KCEO_EVENT_WAVE;
			EventMap[0x89] = KCEO_EVENT_WAVE;
			EventMap[0x8A] = KCEO_EVENT_WAVE;
			EventMap[0x8B] = KCEO_EVENT_WAVE;
			EventMap[0x8C] = KCEO_EVENT_WAVE;
			EventMap[0x8D] = KCEO_EVENT_WAVE;
			EventMap[0x8E] = KCEO_EVENT_WAVE;
			EventMap[0x8F] = KCEO_EVENT_WAVE;
			EventMap[0x90] = KCEO_EVENT_WAVE;
			EventMap[0x91] = KCEO_EVENT_WAVE;
			EventMap[0x92] = KCEO_EVENT_WAVE;
			EventMap[0x93] = KCEO_EVENT_WAVE;
			EventMap[0x94] = KCEO_EVENT_NOISE0;
			EventMap[0x95] = KCEO_EVENT_NOISE1;
			EventMap[0x96] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x97] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x98] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x99] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x9A] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x9B] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x9C] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x9D] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x9E] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0x9F] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0xA0] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0xA1] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0xA2] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0xA3] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0xA4] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0xA5] = KCEO_EVENT_NOTE_SIZE;
			EventMap[0xA6] = KCEO_EVENT_DECAY;
			EventMap[0xA7] = KCEO_EVENT_DECAY;
			EventMap[0xA8] = KCEO_EVENT_DECAY;
			EventMap[0xA9] = KCEO_EVENT_DECAY;
			EventMap[0xAA] = KCEO_EVENT_DECAY;
			EventMap[0xAB] = KCEO_EVENT_DECAY;
			EventMap[0xAC] = KCEO_EVENT_DECAY;
			EventMap[0xAD] = KCEO_EVENT_DECAY;
			EventMap[0xAE] = KCEO_EVENT_PAN_L;
			EventMap[0xAF] = KCEO_EVENT_PAN_R;
			EventMap[0xB0] = KCEO_EVENT_PAN_LR;
			EventMap[0xB1] = KCEO_EVENT_STOP;
			EventMap[0xB2] = KCEO_EVENT_STOP;
			EventMap[0xB3] = KCEO_EVENT_STOP;
			EventMap[0xB4] = KCEO_EVENT_STOP;
			EventMap[0xB5] = KCEO_EVENT_STOP;
			EventMap[0xB6] = KCEO_EVENT_STOP;
			EventMap[0xB7] = KCEO_EVENT_STOP;
			EventMap[0xB8] = KCEO_EVENT_STOP;
			EventMap[0xB9] = KCEO_EVENT_STOP;
			EventMap[0xBA] = KCEO_EVENT_STOP;
			EventMap[0xBB] = KCEO_EVENT_STOP;
			EventMap[0xBC] = KCEO_EVENT_STOP;
			EventMap[0xBD] = KCEO_EVENT_STOP;
			EventMap[0xBE] = KCEO_EVENT_STOP;
			EventMap[0xBF] = KCEO_EVENT_STOP;
			EventMap[0xC0] = KCEO_EVENT_STOP;
			EventMap[0xC1] = KCEO_EVENT_REPEAT1_START;
			EventMap[0xC2] = KCEO_EVENT_REPEAT2_START;
			EventMap[0xC3] = KCEO_EVENT_SONG_LOOP;
			EventMap[0xC4] = KCEO_EVENT_CONDITIONAL_MARKER;
			EventMap[0xC5] = KCEO_EVENT_CONDITIONAL_JUMP;
			EventMap[0xC6] = KCEO_EVENT_GOTO_LOOP;
			EventMap[0xC7] = KCEO_EVENT_STOP;
			EventMap[0xC8] = KCEO_EVENT_STOP;
			EventMap[0xC9] = KCEO_EVENT_STOP;
			EventMap[0xCA] = KCEO_EVENT_STOP;
			EventMap[0xCB] = KCEO_EVENT_STOP;
			EventMap[0xCC] = KCEO_EVENT_STOP;
			EventMap[0xCD] = KCEO_EVENT_STOP;
			EventMap[0xCE] = KCEO_EVENT_STOP;
			EventMap[0xCF] = KCEO_EVENT_STOP;
			EventMap[0xD0] = KCEO_EVENT_TEMPO;
			EventMap[0xD1] = KCEO_EVENT_UNKNOWN1;
			EventMap[0xD2] = KCEO_EVENT_NOTE_SIZE_ABS;
			EventMap[0xD3] = KCEO_EVENT_UNKNOWN1;
			EventMap[0xD4] = KCEO_EVENT_TRANSPOSE;
			EventMap[0xD5] = KCEO_EVENT_TUNING;
			EventMap[0xD6] = KCEO_EVENT_DECAY;
			EventMap[0xD7] = KCEO_EVENT_NOP;
			EventMap[0xD8] = KCEO_EVENT_NOP;
			EventMap[0xD9] = KCEO_EVENT_REST;
			EventMap[0xDA] = KCEO_EVENT_ENV_GAIN;
			EventMap[0xDB] = KCEO_EVENT_RESET;
			EventMap[0xDC] = KCEO_EVENT_STOP;
			EventMap[0xDD] = KCEO_EVENT_STOP;
			EventMap[0xDE] = KCEO_EVENT_STOP;
			EventMap[0xDF] = KCEO_EVENT_STOP;
			EventMap[0xE0] = KCEO_EVENT_UNKNOWN1;
			EventMap[0xE1] = KCEO_EVENT_PITCH_BEND;
			EventMap[0xE2] = KCEO_EVENT_FADE;
			EventMap[0xE3] = KCEO_EVENT_TIE;
			EventMap[0xE4] = KCEO_EVENT_GOTO;
			EventMap[0xE5] = KCEO_EVENT_STOP;
			EventMap[0xE6] = KCEO_EVENT_STOP;
			EventMap[0xE7] = KCEO_EVENT_STOP;
			EventMap[0xE8] = KCEO_EVENT_STOP;
			EventMap[0xE9] = KCEO_EVENT_STOP;
			EventMap[0xEA] = KCEO_EVENT_STOP;
			EventMap[0xEB] = KCEO_EVENT_STOP;
			EventMap[0xEC] = KCEO_EVENT_STOP;
			EventMap[0xED] = KCEO_EVENT_STOP;
			EventMap[0xEE] = KCEO_EVENT_STOP;
			EventMap[0xEF] = KCEO_EVENT_STOP;
			EventMap[0xF0] = KCEO_EVENT_UNKNOWN0;
			EventMap[0xF1] = KCEO_EVENT_DISTORT;
			EventMap[0xF2] = KCEO_EVENT_UNKNOWN3;
			EventMap[0xF3] = KCEO_EVENT_REPEAT1_END;
			EventMap[0xF4] = KCEO_EVENT_REPEAT2_END;
			EventMap[0xF5] = KCEO_EVENT_UNKNOWN3;
			EventMap[0xF6] = KCEO_EVENT_UNKNOWN3;


		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			repeat1 = -1;
			repeat2 = -1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			condJump = 0;
			curVol = 100;
			holdNote = 0;
			pitchBend = 0;

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

			if (seqPtrs[curTrack] >= bankSize && seqPtrs[curTrack] < (bankSize * 2))
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && masterDelay < 110000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];

				if (command[0] >= KCEO_STATUS_NOTE_MIN && command[0] <= KCEO_STATUS_NOTE_MAX)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;

						if (pitchBend == 1)
						{
							tempPos = WriteDeltaTime(midData, midPos, curDelay);
							midPos += tempPos;
							Write8B(&midData[midPos], (0xE0 | curTrack));
							Write8B(&midData[midPos + 1], 0);
							Write8B(&midData[midPos + 2], 0x40);
							Write8B(&midData[midPos + 3], 0);
							firstNote = 1;
							midPos += 3;
							pitchBend = 0;
						}
					}

					if ((command[0] & 0x10) != 0x00)
					{
						hasSize = 0;
					}
					else
					{
						hasSize = 1;
					}

					if ((command[0] & 0x20) != 0x00)
					{
						hasLen = 0;
					}
					else
					{
						hasLen = 1;
					}

					if ((command[0] & 0x40) != 0x00)
					{
						newNote = 0;
					}
					else
					{
						newNote = 1;
					}

					seqPos++;

					if (hasSize == 1)
					{
						seqPos++;
					}

					if (hasLen == 1)
					{
						curNoteLen = exRomData[seqPos] * 8;
						seqPos++;
					}

					if (newNote == 1)
					{
						if (curTrack != 3)
						{
							curNote = exRomData[seqPos] + 24 + transpose;

							if (curTrack == 2)
							{
								curNote -= 12;
							}
						}
						else if (curTrack == 3)
						{
							if (curNote >= 24)
							{
								curNote = exRomData[seqPos] - 24;
							}
							else
							{
								curNote = exRomData[seqPos];
							}
						}
						seqPos++;
					}

					tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 1;
					midPos = tempPos;
					curDelay = curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;


				}

				else if (EventMap[command[0]] == KCEO_EVENT_NOP || EventMap[command[0]] == KCEO_EVENT_UNKNOWN0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_UNKNOWN1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_UNKNOWN2)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_UNKNOWN3)
				{
					seqPos += 4;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_DUTY0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_DUTY1)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_DUTY2)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_DUTY3)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_WAVE)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_NOISE0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_NOISE1)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_NOTE_SIZE)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_DECAY)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_PAN_L)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_PAN_R)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_PAN_LR)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_STOP)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;

						if (pitchBend == 1)
						{
							tempPos = WriteDeltaTime(midData, midPos, curDelay);
							midPos += tempPos;
							Write8B(&midData[midPos], (0xE0 | curTrack));
							Write8B(&midData[midPos + 1], 0);
							Write8B(&midData[midPos + 2], 0x40);
							Write8B(&midData[midPos + 3], 0);
							firstNote = 1;
							midPos += 3;
							pitchBend = 0;
						}
					}
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_REPEAT1_START)
				{
					repeat1Pos = seqPos + 1;
					repeat1 = -1;
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_REPEAT2_START)
				{
					repeat2Pos = seqPos + 1;
					repeat2 = -1;
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_SONG_LOOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_CONDITIONAL_MARKER)
				{
					condJumpStart = seqPos + 1;
					condJump = 0;
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_CONDITIONAL_JUMP)
				{
					if (condJump == 0)
					{
						condJump++;
						seqPos++;
					}
					else if (condJump == 1)
					{
						condJump++;
						condJumpStop = seqPos + 1;
						seqPos = condJumpStart;
					}
					else
					{
						condJump--;
						seqPos = condJumpStop;
					}
				}

				else if (EventMap[command[0]] == KCEO_EVENT_GOTO_LOOP)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;

						if (pitchBend == 1)
						{
							tempPos = WriteDeltaTime(midData, midPos, curDelay);
							midPos += tempPos;
							Write8B(&midData[midPos], (0xE0 | curTrack));
							Write8B(&midData[midPos + 1], 0);
							Write8B(&midData[midPos + 2], 0x40);
							Write8B(&midData[midPos + 3], 0);
							firstNote = 1;
							midPos += 3;
							pitchBend = 0;
						}
					}
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_TEMPO)
				{
					if (tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						if (spawnFix != 1)
						{
							tempo = command[1] * 1.6;
						}
						else if (spawnFix == 1)
						{
							tempo = command[1] * 0.95;
						}
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 2;

				}

				else if (EventMap[command[0]] == KCEO_EVENT_NOTE_SIZE_ABS)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_DECAY_ABS)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_REST)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;

						if (pitchBend == 1)
						{
							tempPos = WriteDeltaTime(midData, midPos, curDelay);
							midPos += tempPos;
							Write8B(&midData[midPos], (0xE0 | curTrack));
							Write8B(&midData[midPos + 1], 0);
							Write8B(&midData[midPos + 2], 0x40);
							Write8B(&midData[midPos + 3], 0);
							firstNote = 1;
							midPos += 3;
							pitchBend = 0;
						}
					}
					curNoteLen = command[1] * 8;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_ENV_GAIN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_RESET)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_PITCH_BEND)
				{
					pitchBend = 1;
					seqPos += 3;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_FADE)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_TIE)
				{
					curNoteLen = command[1] * 8;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 3;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_GOTO)
				{
					seqPos = ReadLE16(&exRomData[seqPos + 1]);
				}

				else if (EventMap[command[0]] == KCEO_EVENT_DISTORT)
				{
					seqPos += 4;
				}

				else if (EventMap[command[0]] == KCEO_EVENT_REPEAT1_END)
				{
					if (repeat1 == -1)
					{
						repeat1 = command[1];
					}
					else if (repeat1 > 1)
					{
						repeat1--;
						seqPos = repeat1Pos;
					}
					else
					{
						repeat1 = -1;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == KCEO_EVENT_REPEAT2_END)
				{
					if (repeat2 == -1)
					{
						repeat2 = command[1];
					}
					else if (repeat2 > 1)
					{
						repeat2--;
						seqPos = repeat2Pos;
					}
					else
					{
						repeat2 = -1;
						seqPos += 4;
					}
				}

				else
				{
					seqPos++;
				}
			}

			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (tempoVal == 1)
			{
				tempoVal = 2;
			}

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