/*C-lab*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CLAB.H"

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

long bankAmt;

int curVol;
int drvVers;

int ptrOverride;

const char ClabMagicBytesA[6] = { 0x85, 0x6F, 0x30, 0x01, 0x24, 0x2A };
const char ClabMagicBytesB[6] = { 0x85, 0x6F, 0x30, 0x01, 0x24, 0x5D };

unsigned char* romData;
unsigned char* exRomData;
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

int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);

void Clabsong2mid(int songNum, long songPtr);

void ClabProc(int bank, char parameters[4][100])
{
	drvVers = CLAB_VER_STD;
	curVol = 120;
	ptrOverride = 0;
	curInst = 0;
	firstPtr = 0;
	foundTable = 0;

	if (parameters[1][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers < CLAB_VER_RAMPART || drvVers > CLAB_VER_STD)
		{
			drvVers = CLAB_VER_STD;
		}
		tableOffset = strtol(parameters[1], NULL, 16);
		foundTable = 1;
	}
	else if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers < CLAB_VER_RAMPART || drvVers > CLAB_VER_STD)
		{
			drvVers = CLAB_VER_STD;
		}
	}

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

	if (ptrOverride != 1)
	{
		/*Try to search the bank for song table loader - Method 1*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], ClabMagicBytesA, 6)) && foundTable != 1)
			{
				tablePtrLoc = i - 2;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
			}
		}

		/*Try to search the bank for song table loader - Method 2 - GBC*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], ClabMagicBytesB, 6)) && foundTable != 1)
			{
				tablePtrLoc = i - 2;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
			}
		}
	}


	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;
		firstPtr = ReadLE16(&romData[i]);
		while (i != firstPtr && ReadLE16(&romData[i]) < (bankSize * 2))
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Clabsong2mid(songNum, songPtr);
			i += 2;
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
void Clabsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int chanSpeed = 0;
	int octave = 0;
	int transpose = 0;
	int macros[8][2];
	int macroNum;
	int repeats[8][2];
	int repeatNum;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
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
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;

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

	for (j = 0; j < 8; j++)
	{
		repeats[j][0] = 0;
		repeats[j][1] = 0;
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
		case CLAB_VER_RAMPART:
			CLAB_STATUS_NOTE_MIN = 0x00;
			CLAB_STATUS_NOTE_MAX = 0x11;
			CLAB_STATUS_REST = 0x12;
			CLAB_STATUS_NOTE_LEN_MIN = 0x28;
			CLAB_STATUS_NOTE_LEN_MAX = 0xFF;
			EventMap[0x13] = CLAB_EVENT_OCTAVE;
			EventMap[0x14] = CLAB_EVENT_OCTAVE_UP;
			EventMap[0x15] = CLAB_EVENT_OCTAVE_DOWN;
			EventMap[0x16] = CLAB_EVENT_CALL;
			EventMap[0x17] = CLAB_EVENT_RETURN;
			EventMap[0x18] = CLAB_EVENT_REPEAT_START;
			EventMap[0x19] = CLAB_EVENT_REPEAT_END;
			EventMap[0x1A] = CLAB_EVENT_SFX_END;
			EventMap[0x1B] = CLAB_EVENT_CH234_END;
			EventMap[0x1C] = CLAB_EVENT_CH1_END;
			EventMap[0x1D] = CLAB_EVENT_DUTY;
			EventMap[0x1E] = CLAB_EVENT_SWEEP;
			EventMap[0x1F] = CLAB_EVENT_ENV;
			EventMap[0x20] = CLAB_EVENT_TEMPO;
			EventMap[0x21] = CLAB_EVENT_NOP1;
			EventMap[0x22] = CLAB_EVENT_NOP1;
			EventMap[0x23] = CLAB_EVENT_DUR;
			EventMap[0x24] = CLAB_EVENT_TRANSPOSE;
			EventMap[0x25] = CLAB_EVENT_JUMP;
			EventMap[0x26] = CLAB_EVENT_PAN;
			EventMap[0x27] = CLAB_EVENT_NOP1;
			break;
		case CLAB_VER_STD:
			/*Fall-through*/
		default:
			CLAB_STATUS_NOTE_MIN = 0x00;
			CLAB_STATUS_NOTE_MAX = 0x11;
			CLAB_STATUS_REST = 0x12;
			CLAB_STATUS_NOTE_LEN_MIN = 0x28;
			CLAB_STATUS_NOTE_LEN_MAX = 0xFF;
			EventMap[0x13] = CLAB_EVENT_OCTAVE;
			EventMap[0x14] = CLAB_EVENT_OCTAVE_UP;
			EventMap[0x15] = CLAB_EVENT_OCTAVE_DOWN;
			EventMap[0x16] = CLAB_EVENT_CALL;
			EventMap[0x17] = CLAB_EVENT_RETURN;
			EventMap[0x18] = CLAB_EVENT_REPEAT_START;
			EventMap[0x19] = CLAB_EVENT_REPEAT_END;
			EventMap[0x1A] = CLAB_EVENT_SFX_END;
			EventMap[0x1B] = CLAB_EVENT_CH234_END;
			EventMap[0x1C] = CLAB_EVENT_CH1_END;
			EventMap[0x1D] = CLAB_EVENT_DUTY;
			EventMap[0x1E] = CLAB_EVENT_SWEEP;
			EventMap[0x1F] = CLAB_EVENT_ENV;
			EventMap[0x20] = CLAB_EVENT_TEMPO;
			EventMap[0x21] = CLAB_EVENT_NOP1;
			EventMap[0x22] = CLAB_EVENT_NOP;
			EventMap[0x23] = CLAB_EVENT_DUR;
			EventMap[0x24] = CLAB_EVENT_TRANSPOSE;
			EventMap[0x25] = CLAB_EVENT_JUMP;
			EventMap[0x26] = CLAB_EVENT_PAN;
			EventMap[0x27] = CLAB_EVENT_NOP1;
			break;
		}

		/*Get channel pointers*/
		romPos = songPtr;
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos + (curTrack * 2)]);
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPos = seqPtrs[curTrack];
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 3;
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
			macroNum = 0;
			transpose = 0;
			repeatNum = 0;
			curVol = 120;

			int curFreq;

			if (seqPtrs[curTrack] == 0x0000)
			{
				seqEnd = 1;
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

			while (seqEnd == 0 && midPos < 48000 && ctrlMidPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				if (command[0] >= CLAB_STATUS_NOTE_MIN && command[0] <= CLAB_STATUS_NOTE_MAX)
				{
					curNote = command[0] + (octave * 12) + transpose;

					if (curTrack == 0 || curTrack == 1)
					{
						curNote += 36;
					}
					else if (curTrack == 2)
					{
						curNote += 24;
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;

					seqPos++;
				}

				else if (command[0] == CLAB_STATUS_REST)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (command[0] >= CLAB_STATUS_NOTE_LEN_MIN && command[0] <= CLAB_STATUS_NOTE_LEN_MAX)
				{
					curNoteLen = (command[0] - CLAB_STATUS_NOTE_LEN_MIN) * 5;

					seqPos++;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_UNKNOWN0 || EventMap[command[0]] == CLAB_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_UNKNOWN1 || EventMap[command[0]] == CLAB_EVENT_NOP1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_OCTAVE)
				{
					octave = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_OCTAVE_UP)
				{
					octave++;
					seqPos++;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_OCTAVE_DOWN)
				{
					octave--;
					seqPos++;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_CALL)
				{
					macroNum++;
					if (macroNum >= 8)
					{
						seqEnd = 1;
					}
					macros[macroNum][0] = ReadLE16(&romData[seqPos + 1]);
					macros[macroNum][1] = seqPos + 3;
					seqPos = macros[macroNum][0];
				}

				else if (EventMap[command[0]] == CLAB_EVENT_RETURN)
				{
					if (macroNum > 0)
					{
						seqPos = macros[macroNum][1];
						macroNum--;
					}
					else
					{
						seqPos++;
					}
				}

				else if (EventMap[command[0]] == CLAB_EVENT_REPEAT_START)
				{
					repeatNum++;

					if (repeatNum >= 8)
					{
						seqEnd = 1;
					}

					repeats[repeatNum][0] = command[1];
					repeats[repeatNum][1] = seqPos + 2;

					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_REPEAT_END)
				{
					if (repeatNum > 0)
					{
						if (repeats[repeatNum][0] > 1)
						{
							repeats[repeatNum][0]--;
							seqPos = repeats[repeatNum][1];
						}
						else
						{
							seqPos++;
							repeatNum--;
						}
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == CLAB_EVENT_SFX_END || EventMap[command[0]] == CLAB_EVENT_CH234_END || EventMap[command[0]] == CLAB_EVENT_CH1_END)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_ENV)
				{
					if (curTrack != 2)
					{
						if (curVol > 120)
						{
							curVol = (command[1] & 0xF0) / 2;
						}

						if (curVol == 0)
						{
							curVol = 1;
						}
					}
					else
					{
						switch (command[1])
						{
						case 0x20:
							curVol = 120;
							break;
						case 0x40:
							curVol = 60;
							break;
						case 0x60:
							curVol = 30;
							break;
						case 0x00:
							curVol = 1;
							break;
						default:
							curVol = 120;
							break;
						}
					}

					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_TEMPO)
				{
					tempo = command[1] * 2.3;

					if (tempo < 2)
					{
						tempo = 2;
					}

					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;

					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_DUR)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLAB_EVENT_JUMP)
				{
					if (ReadLE16(&romData[seqPos + 1]) > seqPos)
					{
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqEnd = 1;
					}
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