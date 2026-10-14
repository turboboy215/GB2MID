/*Sataru Kurita (Star Wars Episode I: Racer)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <direct.h>
#include "SHARED.H"
#include "SWRACER.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
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
int songBase;
int progBank;

const char SWRacerMagicBytes[10] = { 0xFC, 0xFA, 0x0A, 0xCB, 0xCB, 0x27, 0x06, 0x00, 0x4F, 0x21 };

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
void SWRacersong2mid(int songNum, long songPtr);

void SWRacerProc(int bank)
{
	drvVers = SWRACER_VER_STD;
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

	/*Try to search the bank for song pattern table loader*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], SWRacerMagicBytes, 10) && foundTable != 1)
		{
			tablePtrLoc = i + 10;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	if (foundTable == 1)
	{
		/*Skip empty (0 pointer) song*/
		i = tableOffset + 2;
		songNum = 1;

		while (songNum <= 10)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			SWRacersong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
		free(romData);
	}

}

/*Convert the song data to MIDI*/
void SWRacersong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;
	int repeats[5][2];
	int repeatNum;
	int jumpTimes;
	long lenTab;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curVol = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 0;

	int curInst = 0;

	unsigned long seqPos = 0;

	unsigned char command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;
	int octave = 0;
	int noteLenAdd = 0;
	int volVal = 0;

	midPos = 0;
	ctrlMidPos = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;

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

		tempo = 150;

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
		case SWRACER_VER_STD:
			/*Fall-through*/
		default:
			SWRACER_STATUS_NOTE_MIN = 0x00;
			SWRACER_STATUS_NOTE_MAX = 0x0F;
			SWRACER_STATUS_NOTE_LEN_MIN = 0x10;
			SWRACER_STATUS_NOTE_LEN_MAX = 0x1F;
			SWRACER_STATUS_REST_MIN = 0x20;
			SWRACER_STATUS_REST_MAX = 0x2F;
			SWRACER_STATUS_REST_CUR_LEN = 0x30;
			EventMap[0x40] = SWRACER_EVENT_OCTAVE_UP;
			for (j = 0x41; j <= 0x4F; j++)
			{
				EventMap[j] = SWRACER_EVENT_OCTAVE_DOWN;
			}
			for (j = 0x50; j <= 0x5F; j++)
			{
				EventMap[j] = SWRACER_EVENT_OCTAVE;
			}
			EventMap[0x60] = SWRACER_EVENT_VOL_UP;
			for (j = 0x61; j <= 0x6F; j++)
			{
				EventMap[j] = SWRACER_EVENT_VOL_DOWN;
			}
			for (j = 0x70; j <= 0x7F; j++)
			{
				EventMap[j] = SWRACER_EVENT_VOL;
			}
			EventMap[0x80] = SWRACER_EVENT_NR11_DUTY0;
			EventMap[0x81] = SWRACER_EVENT_NR11_DUTY1;
			EventMap[0x82] = SWRACER_EVENT_NR11_DUTY2;
			EventMap[0x83] = SWRACER_EVENT_NR11_DUTY3;
			EventMap[0x84] = SWRACER_EVENT_NR21_DUTY0;
			EventMap[0x85] = SWRACER_EVENT_NR21_DUTY1;
			EventMap[0x86] = SWRACER_EVENT_NR21_DUTY2;
			EventMap[0x87] = SWRACER_EVENT_NR21_DUTY3;
			EventMap[0x88] = SWRACER_EVENT_PAN_L;
			EventMap[0x89] = SWRACER_EVENT_PAN_R;
			EventMap[0x8A] = SWRACER_EVENT_PAN_LR;
			for (j = 0xB0; j <= 0xBF; j++)
			{
				EventMap[j] = SWRACER_EVENT_DRUM;
			}

			for (j = 0xC0; j <= 0xCF; j++)
			{
				EventMap[j] = SWRACER_EVENT_NOTE_DUR;
			}

			for (j = 0xD0; j <= 0xDF; j++)
			{
				EventMap[j] = SWRACER_EVENT_VIBRATO;
			}
			EventMap[0xF0] = SWRACER_EVENT_ENV;
			EventMap[0xF1] = SWRACER_EVENT_NOP;
			EventMap[0xF2] = SWRACER_EVENT_REPEAT_START;
			EventMap[0xF3] = SWRACER_EVENT_REPEAT_END;
			EventMap[0xF4] = SWRACER_EVENT_JUMP_TIMES;
			EventMap[0xF5] = SWRACER_EVENT_JUMP;
			EventMap[0xF6] = SWRACER_EVENT_STOP;
			EventMap[0xF7] = SWRACER_EVENT_FADE_STEP;
			EventMap[0xF8] = SWRACER_EVENT_MASTER_RESET;
			for (j = 0xF9; j <= 0xFD; j++)
			{
				EventMap[j] = SWRACER_EVENT_NOP;
			}
			EventMap[0xFF] = SWRACER_EVENT_NOP;
			break;

		}

		/*Get the channel pointers*/
		lenTab = ReadLE16(&romData[songPtr]);

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[songPtr + ((curTrack + 1) * 2)]);
		}


		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			holdNote = 0;
			curVol = 120;
			noteLenAdd = 0;
			repeatNum = 0;
			jumpTimes = 0;
			volVal = 0xF0;
			for (j = 0; j < 5; j++)
			{
				repeats[j][0] = 0;
				repeats[j][1] = 0;
			}

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

			seqPos = seqPtrs[curTrack];
			seqEnd = 0;
			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				lowNibble = (command[0] >> 4);
				highNibble = (command[0] & 15);

				if (command[0] >= SWRACER_STATUS_NOTE_MIN && command[0] <= SWRACER_STATUS_NOTE_MAX)
				{
					curNote = highNibble + (octave * 12) + 24;
					if (curTrack < 2)
					{
						curNote += 12;
					}
					curVol = volVal / 2;

					if (curVol > 120)
					{
						curVol = 120;
					}

					if (curVol <= 0)
					{
						curVol = 1;
					}
					noteLenAdd = 0;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (command[0] >= SWRACER_STATUS_NOTE_LEN_MIN && command[0] <= SWRACER_STATUS_NOTE_LEN_MAX)
				{
					if (noteLenAdd == 0)
					{
						curNoteLen = romData[lenTab + highNibble] * 5;
						noteLenAdd = 1;
					}
					else
					{
						curNoteLen += (romData[lenTab + highNibble] * 5);
					}
					seqPos++;
				}

				else if (command[0] >= SWRACER_STATUS_REST_MIN && command[0] <= SWRACER_STATUS_REST_MAX)
				{
					curNoteLen = romData[lenTab + highNibble] * 5;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					noteLenAdd = 0;
					seqPos++;
				}

				else if (command[0] == SWRACER_STATUS_REST_CUR_LEN)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					noteLenAdd = 0;
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_OCTAVE_UP)
				{
					octave++;
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_OCTAVE_DOWN)
				{
					octave--;
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_OCTAVE)
				{
					octave = highNibble;
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_VOL_UP)
				{
					if (volVal < 0xF0)
					{
						volVal += 0x10;
					}

					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_VOL_DOWN)
				{
					if (volVal > 0x00)
					{
						volVal += 0x10;
					}
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_VOL)
				{
					volVal = highNibble;
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NR11_DUTY0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NR11_DUTY1)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NR11_DUTY2)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NR11_DUTY3)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NR21_DUTY1)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NR21_DUTY2)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NR21_DUTY3)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_PAN_L)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_PAN_R)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_PAN_LR)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_DRUM)
				{
					curNote = highNibble + 36;
					noteLenAdd = 0;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_NOTE_DUR)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_VIBRATO)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_ENV)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_REPEAT_START)
				{
					repeatNum++;

					/*No more repeat levels available*/
					if (repeatNum >= 5)
					{
						seqEnd = 1;
					}
					else
					{
						repeats[repeatNum][0] = seqPos + 1;
						repeats[repeatNum][1] = 0;
						seqPos++;
					}

				}

				else if (EventMap[command[0]] == SWRACER_EVENT_REPEAT_END)
				{
					repeats[repeatNum][1]++;
					if (repeats[repeatNum][1] < command[1])
					{
						seqPos = repeats[repeatNum][0];
					}
					else
					{
						repeats[repeatNum][1] = 0;
						if (repeatNum > 0)
						{
							repeatNum--;
						}
						seqPos += 2;
					}
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_JUMP_TIMES)
				{
					if (jumpTimes < command[1])
					{
						seqPos = ReadLE16(&romData[seqPos + 2]);
						jumpTimes++;
					}
					else
					{
						jumpTimes = 0;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_JUMP)
				{
					if ((ReadLE16(&romData[seqPos + 1])) > seqPos)
					{
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_STOP)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_FADE_STEP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == SWRACER_EVENT_MASTER_RESET)
				{
					seqPos++;
				}

				/*Unknown command*/
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