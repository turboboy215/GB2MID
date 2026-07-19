/*Hiroshi Wada*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MEGAMAN1.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
int i, j;
char outfile[1000000];
int songNum;
long curPtr;
long bankAmt;
int gameNum = 0;
long ptrList[100];
int curVol;
int drvVers;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;

unsigned char* ctrlMidData;

long midLength;

/*Pointers for SolarStriker*/
const long MM1SolarPtrs[14] = { 0x5541, 0x5603, 0x5D41, 0x5D8A, 0x5DDB, 0x5E5B, 0x5EB0, 0x5F23, 0x600E, 0x6210, 0x651D, 0x6B60, 0x6FCF, 0x70F8 };
/*Pointers for QIX*/
const long MM1QixPtrs[28] = { 0x1725, 0x17F2, 0x1847, 0x188F, 0x18D9, 0x18F9, 0x191F, 0x1945, 0x196B, 0x199F, 0x19BC, 0x19D9, 0x19FF, 0x1A99, 0x1ABB, 0x1B0C, 0x1B32, 0x1B54, 0x1BFF, 0x1D04, 0x1D57, 0x1FD5, 0x22FD, 0x244D, 0x254D, 0x2611, 0x26E6, 0x287F };

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
void MM1song2mid(int songNum, long ptr);

void MM1Proc(int bank)
{
	drvVers = MM1_VER_MM1;
	curVol = 120;
	gameNum = GAME_UNKNOWN;

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

	/*Look for the start of the pointer table*/
	if (romData[0x4005] == 0x43 && romData[0x4006] == 0xC3)
	{
		printf("Detected game: Mega Man 1\n");
		gameNum = GAME_MM1;
		drvVers = MM1_VER_MM1;
	}
	else if (romData[0x4703] == 0x01 && romData[0x4704] == 0x23)
	{
		printf("Detected game: SolarStriker\n");
		gameNum = GAME_SS;
		drvVers = MM1_VER_SS;
	}
	else if (romData[0x0600] == 0x01 && romData[0x0601] == 0x23)
	{
		printf("Detected game: QIX\n");
		gameNum = GAME_QIX;
		drvVers = MM1_VER_MM1;
	}

	if (gameNum == GAME_UNKNOWN)
	{
		printf("ERROR: Unsupported game data!\n");
		exit(1);
	}
	else if (gameNum == GAME_MM1)
	{
		/*Get pointers from list*/
		i = 0x4015;
		songNum = 1;
		while (ReadLE16(&romData[i]) >= bankSize)
		{
			curPtr = ReadLE16(&romData[i]);
			ptrList[songNum - 1] = curPtr;
			printf("Song %i address: 0x%04X\n", songNum, curPtr);
			MM1song2mid(songNum, curPtr);
			i += 2;
			songNum++;
		}
	}
	else if (gameNum == GAME_SS)
	{
		/*Get pointers from list*/
		songNum = 1;
		for (i = 0; i < 14; i++)
		{
			curPtr = MM1SolarPtrs[i];
			printf("Song %i address: 0x%04X\n", songNum, curPtr);
			MM1song2mid(songNum, curPtr);
			songNum++;
		}
	}
	else if (gameNum == GAME_QIX)
	{
		/*Get pointers from list*/
		songNum = 1;
		for (i = 0; i < 28; i++)
		{
			curPtr = MM1QixPtrs[i];
			printf("Song %i address: 0x%04X\n", songNum, curPtr);
			MM1song2mid(songNum, curPtr);
			songNum++;
		}
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void MM1song2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	int midChan = 0;
	int trackEnd = 0;
	int ticks = 120;
	int speed = 0;
	int k = 0;

	int repeat1 = 0;
	int repeats2[8][2];
	int repeats3[8][2];
	int repeatNum2;
	int repeatNum3;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int jumpAmt = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 120;

	int curInst = 0;

	unsigned long seqPos = 0;

	unsigned char command[3];

	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	unsigned long startPtrs[4];

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int inMacro = 0;
	int macroPos = 0;
	int macroRet = 0;

	int slur = 0;
	int autoLen = 0;
	int autoLenVal = 0;

	int jumpPos = 0;

	int duration = 0;
	int curNoteLenVal = 0;

	int curVol1 = 120;
	int curVol2 = 0;

	midPos = 0;
	ctrlMidPos = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
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

		/*Get the tempo*/
		speed = romData[ptr];
		tempo = 150;

		switch (drvVers)
		{
		case MM1_VER_SS:
		case MM1_VER_MM1:
			/*Fall-through*/
		default:
			MM1_STATUS_NOTE_MIN = 0x00;
			MM1_STATUS_NOTE_MAX = 0x8F;
			MM1_STATUS_JUMP1_MIN = 0xB0;
			MM1_STATUS_JUMP1_MAX = 0xBF;
			MM1_STATUS_JUMP2_MIN = 0xA0;
			MM1_STATUS_JUMP2_MAX = 0xAF;
			MM1_STATUS_JUMP3_MIN = 0x90;
			MM1_STATUS_JUMP3_MAX = 0x9F;
			EventMap[0xFF] = MM1_EVENT_STOP;
			EventMap[0xFE] = MM1_EVENT_VOL;
			EventMap[0xFD] = MM1_EVENT_VOL2;
			EventMap[0xFC] = MM1_EVENT_ENV_SEQ;
			EventMap[0xFB] = MM1_EVENT_SWEEP_PACE;
			EventMap[0xFA] = MM1_EVENT_ENV_SPEED;
			EventMap[0xF9] = MM1_EVENT_CHAN_LEN;
			EventMap[0xF8] = MM1_EVENT_NOTE_DUR;
			EventMap[0xF7] = MM1_EVENT_VIBRATO;
			EventMap[0xF6] = MM1_EVENT_VIB_SPEED;
			EventMap[0xF5] = MM1_EVENT_SWEEP;
			EventMap[0xF4] = MM1_EVENT_DUTY;
			EventMap[0xF3] = MM1_EVENT_UNKNOWN1;
			EventMap[0xF2] = MM1_EVENT_WAVEFORM;
			EventMap[0xF1] = MM1_EVENT_MASTER_VOLUME;
			EventMap[0xF0] = MM1_EVENT_PAN;
			EventMap[0xEF] = MM1_EVENT_VIB_DELAY;
			EventMap[0xEE] = MM1_EVENT_SLUR;
			EventMap[0xED] = MM1_EVENT_TRANSPOSE;
			EventMap[0xEC] = MM1_EVENT_AUTO_LEN_ON;
			EventMap[0xEB] = MM1_EVENT_AUTO_LEN_OFF;
			EventMap[0xEA] = MM1_EVENT_DEC_POS;
			EventMap[0xE9] = MM1_EVENT_CALL;
			EventMap[0xE8] = MM1_EVENT_RETURN;
			EventMap[0xE7] = MM1_EVENT_RNG_VAL;
			EventMap[0xE6] = MM1_EVENT_RNG_START;
			break;
		}

		/*Get starting pointers for each channel*/
		startPtrs[0] = ReadLE16(&romData[ptr + 2]);
		startPtrs[1] = ReadLE16(&romData[ptr + 5]);
		startPtrs[2] = ReadLE16(&romData[ptr + 8]);
		startPtrs[3] = ReadLE16(&romData[ptr + 11]);

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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			transpose = 0;
			inMacro = 0;
			slur = 0;
			autoLen = 0;
			autoLenVal = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			curVol = 120;
			curVol1 = 120;
			curVol2 = 0;

			repeat1 = 0;
			repeatNum2 = 0;
			repeatNum3 = 0;
			duration = 5;

			for (j = 0; j < 8; j++)
			{
				repeats2[j][0] = 0;
				repeats2[j][1] = 0;
				repeats3[j][0] = 0;
				repeats3[j][1] = 0;
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
			seqPos = startPtrs[curTrack];
			if (startPtrs[curTrack] == 0)
			{
				trackEnd = 1;
			}

			while (trackEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				if (command[0] >= MM1_STATUS_NOTE_MIN && command[0] <= MM1_STATUS_NOTE_MAX)
				{
					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);

					if (autoLen != 1)
					{
						curNoteLen = command[1] * 5 * (speed + 1);
						seqPos++;
					}

					if (curTrack != 3)
					{
						if (lowNibble == 0)
						{
							/*Rest*/
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						/*Play note*/
						else
						{
							curNote = ((lowNibble - 1) * 12) + highNibble + transpose + 24;

							if (curTrack < 2)
							{
								curNote += 12;
							}

							if (curNote > 127)
							{
								curNote -= 128;
							}

							curVol = curVol1;

							if (duration < curNoteLen && duration != 0 && curVol2 != 0)
							{
								curNoteLenVal = curNoteLen - duration;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLenVal, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = duration;
							}
							else
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}

						}
					}
					else
					{
						if (command[0] == 0x00)
						{
							/*Rest*/
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						/*Play note*/
						else
						{
							curNote = command[0] + 24;

							/*Fix percussion mapping*/
							if (command[0] == 0x02)
							{
								curNote = 42;
							}
							else if (command[0] == 0x03)
							{
								curNote = 36;
							}
							else if (command[0] == 0x20)
							{
								curNote = 38;
							}

							curVol = curVol1;

							if (duration < curNoteLen && duration != 0 && curVol2 != 0)
							{
								curNoteLenVal = curNoteLen - duration;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLenVal, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = duration;
							}
							else
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}
						}
					}

					seqPos++;
				}

				else if (command[0] >= MM1_STATUS_JUMP1_MIN && command[0] <= MM1_STATUS_JUMP1_MAX)
				{
					tempByte = command[0] - MM1_STATUS_JUMP1_MIN;
					jumpPos = (signed char)command[1] * 2;

					/*Infinite jump*/
					if (tempByte == 0)
					{
						if (jumpPos < 0)
						{
							trackEnd = 1;
						}
						else
						{
							seqPos += jumpPos;
						}
					}
					else
					{
						if (repeat1 == 0)
						{
							repeat1 = tempByte + 1;
						}
						else
						{
							repeat1--;
							if (repeat1 > 0)
							{
								seqPos += jumpPos;
							}
							else
							{
								repeat1 = 0;
								seqPos += 2;
							}
						}
					}
				}

				else if (command[0] >= MM1_STATUS_JUMP2_MIN && command[0] <= MM1_STATUS_JUMP2_MAX)
				{
					tempByte = command[0] - MM1_STATUS_JUMP2_MIN;
					jumpPos = (signed char)command[1] * 2;
					/*Infinite jump*/
					if (tempByte == 0)
					{
						if (jumpPos < 0)
						{
							trackEnd = 1;
						}
						else
						{
							seqPos += jumpPos;
						}
					}
					else
					{
						for (j = 0; j < 8; j++)
						{
							if (repeats2[j][1] == seqPos)
							{
								break;
							}
							else if (repeats2[j][1] == 0)
							{
								repeats2[j][0] = tempByte;
								repeats2[j][1] = seqPos;
								break;
							}
						}

						/*No room for more than 8 repeats in one group*/
						if (j >= 8)
						{
							trackEnd = 1;
						}
						else
						{
							if (repeats2[j][0] <= 0)
							{
								repeats2[j][0] = 0;
								repeats2[j][1] = 0;
								seqPos += 2;
							}
							else
							{
								repeats2[j][0]--;
								seqPos += jumpPos;
							}
						}
					}
				}

				else if (command[0] >= MM1_STATUS_JUMP3_MIN && command[0] <= MM1_STATUS_JUMP2_MAX)
				{
					tempByte = command[0] - MM1_STATUS_JUMP3_MIN;
					jumpPos = (signed char)command[1] * 2;
					/*Infinite jump*/
					if (tempByte == 0)
					{
						if (jumpPos < 0)
						{
							trackEnd = 1;
						}
						else
						{
							seqPos += jumpPos;
						}
					}
					else
					{
						for (j = 0; j < 8; j++)
						{
							if (repeats3[j][1] == seqPos)
							{
								break;
							}
							else if (repeats3[j][1] <= 0)
							{
								repeats3[j][0] = tempByte;
								repeats3[j][1] = seqPos;
								break;
							}
						}

						/*No room for more than 8 repeats in one group*/
						if (j >= 8)
						{
							trackEnd = 1;
						}
						else
						{
							if (repeats3[j][0] <= 0)
							{
								repeats3[j][0] = 0;
								repeats3[j][1] = 0;
								seqPos += 2;
							}
							else
							{
								repeats3[j][0]--;
								if (drvVers == MM1_VER_SS)
								{
									seqPos += jumpPos;
								}
								else
								{
									seqPos += 2;
								}
							}
						}
					}
				}

				else if (EventMap[command[0]] == MM1_EVENT_UNKNOWN0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == MM1_EVENT_UNKNOWN1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_STOP)
				{
					trackEnd = 1;
				}

				else if (EventMap[command[0]] == MM1_EVENT_VOL)
				{
					curVol1 = (command[1] & 0x0F);
					curVol1 = curVol1 * 0x10;

					if (curVol1 > 120)
					{
						curVol1 = 120;
					}

					if (curVol1 < 1)
					{
						curVol1 = 1;
					}
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_VOL2)
				{
					curVol2 = (command[1] & 0x0F);
					curVol2 = curVol2 * 0x10;

					if (curVol2 > 120)
					{
						curVol2 = 120;
					}

					if (curVol2 < 1)
					{
						curVol2 = 0;
					}
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_ENV_SEQ)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_SWEEP_PACE)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_ENV_SPEED)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_CHAN_LEN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_NOTE_DUR)
				{
					duration = command[1] * 5 * (speed + 1);
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_VIBRATO)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_WAVEFORM)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_MASTER_VOLUME)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_VIB_DELAY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_SLUR)
				{
					slur = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_AUTO_LEN_ON)
				{
					autoLen = 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == MM1_EVENT_AUTO_LEN_OFF)
				{
					autoLen = 0;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_DEC_POS)
				{
					seqPos--;
				}

				else if (EventMap[command[0]] == MM1_EVENT_CALL)
				{
					inMacro = 1;
					macroPos = ReadBE16(&romData[seqPos + 1]);
					macroRet = seqPos += 3;
				}

				else if (EventMap[command[0]] == MM1_EVENT_RETURN)
				{
					inMacro = 0;
					seqPos = macroRet;
				}

				else if (EventMap[command[0]] == MM1_EVENT_RNG_VAL)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM1_EVENT_RNG_START)
				{
					seqPos += 2;
				}

				/*Unknown command*/
				else
				{
					seqPos += 2;
				}

			}

			if (songNum == 21)
			{
				songNum = 21;
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

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);

	}
}