/*Equilibrium*/
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "SHARED.H"
#include "EQUIL.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int numSongs;
long bankAmt;
int curInst;
int drvVers;
int songBank;
int curVol;
int songTranspose;

int cfgPtr;
int exitError;
int fileExit;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

char* argv3;
char string1[100];
char string2[100];
char EquilcheckStrings[5][100] = { "version=", "numSongs=", "bank=", "start=", "transpose=" };

unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Equilsong2mid(int songNum, long ptr);

void EquilProc(char parameters[4][100])
{
	curInst = 0;
	curVol = 100;
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;
	songTranspose = 0;
	drvVers = EQUIL_VER_COMMON;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the driver version value*/
		fgets(string1, 9, cfg);
		if (memcmp(string1, EquilcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		drvVers = strtod(string1, NULL);

		fgets(string1, 3, cfg);
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, EquilcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtod(string1, NULL);
		songNum = 1;

		fgets(string1, 3, cfg);

		if (drvVers <= 3)
		{
			printf("Version: %01X\n", drvVers);
			while (songNum <= numSongs)
			{
				/*Skip the first line*/
				fgets(string1, 10, cfg);

				/*Get the song bank*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, EquilcheckStrings[2], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);

				songBank = strtol(string1, NULL, 16);

				fgets(string1, 3, cfg);

				/*Get the start position of the song*/
				fgets(string1, 7, cfg);
				if (memcmp(string1, EquilcheckStrings[3], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);

				songPtr = strtol(string1, NULL, 16);

				fgets(string1, 3, cfg);

				/*Get the song transpose*/
				fgets(string1, 11, cfg);
				if (memcmp(string1, EquilcheckStrings[4], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 3, cfg);

				songTranspose = (signed char)strtol(string1, NULL, 16);

				printf("Song %i: 0x%04X, bank: %01X, transpose: %i\n", songNum, songPtr, songBank, songTranspose);


				if (songBank < 0x02)
				{
					songBank = 0x02;
				}

				fgets(string1, 3, cfg);

				fseek(rom, 0, SEEK_SET);
				romData = (unsigned char*)malloc(bankSize * 2);
				fread(romData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(romData + bankSize, 1, bankSize, rom);

				Equilsong2mid(songNum, songPtr);
				free(romData);

				songNum++;
			}
		}
		else
		{
			printf("ERROR: Invalid version number!\n");
			exit(1);
		}
	}
}

/*Convert the song data to MIDI*/
void Equilsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int numTracks = 0;
	int ticks = 120;
	int tempo = 150;
	int chanSpeed;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int autoLen = 0;
	int transpose = 0;
	int transposeVal = 0;
	int initTranspose = 0;
	unsigned char command[4];
	int firstNote = 1;
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
	long macros[5][2];
	long repeat[5][2];
	int repeatNum = 0;

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

	for (j = 0; j < 4; j++)
	{
		seqPtrs[j] = 0x0000;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		/*Get channel pointer information*/
		romPos = songPtr;
		numTracks = romData[romPos];
		romPos++;


		for (j = 0; j < numTracks; j++)
		{
			curTrack = romData[romPos + 2] - 1;
			if (curTrack > 3 || curTrack < 0)
			{
				curTrack = 0;
			}
			if (drvVers != EQUIL_VER_GBC)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			}
			else
			{
				seqPtrs[curTrack] = ReadBE16(&romData[romPos]);
			}

			romPos += 3;
		}

		for (j = 0; j < 5; j++)
		{
			macros[j][0] = 0;
			macros[j][1] = 0;
			repeat[j][0] = -1;
			repeat[j][1] = 0;
		}

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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			inMacro = 0;
			repeatNum = 0;
			transpose = 0;

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
			transposeVal = 0;
			initTranspose = 0;

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

			EQUIL_STATUS_NOTE_MIN = 0x00;
			EQUIL_STATUS_NOTE_MAX = 0x7F;
			EQUIL_STATUS_REST = 0x80;

			switch (drvVers)
			{
			case EQUIL_VER_EARLY:
				EventMap[0x81] = EQUIL_EVENT_DUTY;
				EventMap[0x82] = EQUIL_EVENT_TRANSPOSE;
				EventMap[0x83] = EQUIL_EVENT_NOTEDUR;
				EventMap[0x84] = EQUIL_EVENT_DECAY;
				EventMap[0x85] = EQUIL_EVENT_VOLUME;
				EventMap[0x86] = EQUIL_EVENT_PAN;
				EventMap[0x87] = EQUIL_EVENT_SWEEP;
				EventMap[0x88] = EQUIL_EVENT_TUNING;
				EventMap[0x8F] = EQUIL_EVENT_RESTART_SONG;
				EventMap[0x90] = EQUIL_EVENT_RESTART_CHANNEL;
				EventMap[0x91] = EQUIL_EVENT_REPEAT1;
				EventMap[0x92] = EQUIL_EVENT_REPEAT2;
				EventMap[0x93] = EQUIL_EVENT_CONDITIONAL_JUMP;
				EventMap[0x94] = EQUIL_EVENT_JUMP;
				EventMap[0x95] = EQUIL_EVENT_CALL;
				EventMap[0x96] = EQUIL_EVENT_RETURN;
				EventMap[0xA2] = EQUIL_EVENT_WAVE;
				EventMap[0xB0] = EQUIL_EVENT_UNKNOWN2;
				EventMap[0xFD] = EQUIL_EVENT_RESTART_CHANNEL;
				EventMap[0xFE] = EQUIL_EVENT_STOP;
				break;
			case EQUIL_VER_COMMON:
				/*Fall-through*/
			case EQUIL_VER_GBC:
			default:
				EventMap[0xEA] = EQUIL_EVENT_DUTY;
				EventMap[0xEB] = EQUIL_EVENT_TRANSPOSE;
				EventMap[0xEC] = EQUIL_EVENT_NOTEDUR;
				EventMap[0xED] = EQUIL_EVENT_DECAY;
				EventMap[0xEE] = EQUIL_EVENT_VOLUME;
				EventMap[0xEF] = EQUIL_EVENT_PAN;
				EventMap[0xF0] = EQUIL_EVENT_SWEEP;
				EventMap[0xF1] = EQUIL_EVENT_TUNING;
				EventMap[0xF2] = EQUIL_EVENT_RESTART_SONG;
				EventMap[0xF3] = EQUIL_EVENT_RESTART_CHANNEL;
				EventMap[0xF4] = EQUIL_EVENT_REPEAT1;
				EventMap[0xF5] = EQUIL_EVENT_REPEAT2;
				EventMap[0xF6] = EQUIL_EVENT_JUMP;
				EventMap[0xF7] = EQUIL_EVENT_CONDITIONAL_JUMP;
				EventMap[0xF8] = EQUIL_EVENT_CONDITIONAL_JUMP;
				EventMap[0xF9] = EQUIL_EVENT_CALL;
				EventMap[0xFA] = EQUIL_EVENT_RETURN;
				EventMap[0xFB] = EQUIL_EVENT_WAVE;
				EventMap[0xFC] = EQUIL_EVENT_UNKNOWN0;
				EventMap[0xFD] = EQUIL_EVENT_UNKNOWN0;
				EventMap[0xFE] = EQUIL_EVENT_STOP;
				EventMap[0xFF] = EQUIL_EVENT_RESTART_CHANNEL;
			}

			while (seqEnd == 0 && midPos < 48000 && masterDelay < 110000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				if (command[0] >= EQUIL_STATUS_NOTE_MIN && command[0] <= EQUIL_STATUS_NOTE_MAX)
				{
					curNote = command[0];

					if (curTrack != 3)
					{
						curNote += transpose;
					}
					/*
					if (curTrack != 3)
					{
						if (drvVers != EQUIL_VER_GBC)
						{
							curNote += 24;
						}
						else
						{
							curNote += 24;
						}
					}
					*/

					/*
					if (curTrack == 2)
					{
						curNote -= 12;
					}
					*/

					if (drvVers != EQUIL_VER_GBC)
					{
						curNoteLen = command[1] * 5;
					}
					else
					{
						curNoteLen = (command[1] + 1) * 5;
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				else if (command[0] == EQUIL_STATUS_REST)
				{
					if (drvVers != EQUIL_VER_GBC)
					{
						if (command[1] == 0x00)
						{
							curNoteLen = 256 * 5;
						}
						else
						{
							curNoteLen = command[1] * 5;
						}
					}
					else
					{
						curNoteLen = (command[1] + 1) * 5;
					}


					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_UNKNOWN0 || EventMap[command[0]] == EQUIL_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_UNKNOWN1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_UNKNOWN2)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_TRANSPOSE)
				{
					if (transposeVal == 0)
					{
						transposeVal = 1;
						initTranspose = (signed char)command[1];
					}
					else
					{
						transpose = (signed char)command[1] - initTranspose;
					}
					/*
					transpose = (signed char)command[1];
					*/
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_NOTEDUR)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_DECAY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_VOLUME)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_RESTART_SONG)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_RESTART_CHANNEL)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_REPEAT1)
				{
					if (repeat[0][0] == -1)
					{
						repeat[0][0] = command[1];

						if (drvVers != EQUIL_VER_GBC)
						{
							repeat[0][1] = ReadLE16(&romData[seqPos + 2]);
						}
						else
						{
							repeat[0][1] = ReadBE16(&romData[seqPos + 2]);
						}


					}

					else if (repeat[0][0] > 1)
					{
						repeat[0][0]--;
						seqPos = repeat[0][1];
					}

					else
					{
						repeat[0][0] = -1;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_REPEAT2)
				{
					if (repeat[1][0] == -1)
					{
						repeat[1][0] = command[1];

						if (drvVers != EQUIL_VER_GBC)
						{
							repeat[1][1] = ReadLE16(&romData[seqPos + 2]);
						}
						else
						{
							repeat[1][1] = ReadBE16(&romData[seqPos + 2]);
						}


					}

					else if (repeat[1][0] > 1)
					{
						repeat[1][0]--;
						seqPos = repeat[1][1];
					}

					else
					{
						repeat[1][0] = -1;
						seqPos += 4;
					}
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_CONDITIONAL_JUMP)
				{
					seqPos += 5;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_JUMP)
				{
					if (drvVers != EQUIL_VER_GBC)
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
					else
					{
						if (ReadBE16(&romData[seqPos + 1]) > seqPos)
						{
							seqPos = ReadBE16(&romData[seqPos + 1]);
						}
						else
						{
							seqEnd = 1;
						}
					}
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_CALL)
				{
					inMacro++;

					if (inMacro > 5)
					{
						seqEnd = 1;
					}
					else
					{
						if (drvVers != EQUIL_VER_GBC)
						{
							macros[inMacro][0] = ReadLE16(&romData[seqPos + 1]);
						}
						else
						{
							macros[inMacro][0] = ReadBE16(&romData[seqPos + 1]);
						}
						macros[inMacro][1] = seqPos + 3;
						seqPos = macros[inMacro][0];
					}
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_RETURN)
				{
					if (inMacro < 1)
					{
						seqEnd = 1;
					}
					else
					{
						seqPos = macros[inMacro][1];
						inMacro--;
					}
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_WAVE)
				{
					seqPos += 17;
				}

				else if (EventMap[command[0]] == EQUIL_EVENT_STOP)
				{
					seqEnd = 1;
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