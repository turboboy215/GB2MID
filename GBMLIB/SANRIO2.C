/*Unknown (Sanrio Carnival 2)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "SANRIO2.H"

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
long nextPtr;
long endPtr;
long bankAmt;
int curVol;
int curInst;
int drvVers;
int foundTable;

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
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Sanrio2song2mid(int songNum, long ptrs[4]);

void Sanrio2Proc(int bank)
{
	drvVers = SANRIO2_VER_STD;
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

	tableOffset = 0x6DB2;
	i = tableOffset + 8;
	songNum = 1;

	while (songNum <= 13)
	{
		seqPtrs[0] = ReadLE16(&romData[i]);
		printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
		seqPtrs[1] = ReadLE16(&romData[i + 2]);
		printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
		seqPtrs[2] = ReadLE16(&romData[i + 4]);
		printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
		seqPtrs[3] = ReadLE16(&romData[i + 6]);
		printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
		Sanrio2song2mid(songNum, seqPtrs);
		i += 8;
		songNum++;
	}
	free(romData);
}

/*Convert the song data to MIDI*/
void Sanrio2song2mid(int songNum, long ptrs[4])
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
	int curVol = 0;
	int curVols[4];
	int curSpeeds[4];
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
	int macros[4][4][2];
	int inMacroM[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long framePtr;
	int numChan;
	int envVals[4];
	int repeatNums[4];
	int firstRepeats[4];
	int noteTab;

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
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
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
			case SANRIO2_VER_STD:
				/*Fall-through*/
			default:
				SANRIO2_STATUS_REST = 0x00;
				SANRIO2_STATUS_NOTE_MIN = 0x01;
				SANRIO2_STATUS_NOTE_MAX = 0x7F;
				EventMap[0x80] = SANRIO2_EVENT_WAVEFORM;
				EventMap[0x81] = SANRIO2_EVENT_GLOBAL_VOL;
				EventMap[0x82] = SANRIO2_EVENT_PAN;
				EventMap[0x83] = SANRIO2_EVENT_NR10;
				EventMap[0x84] = SANRIO2_EVENT_NR11;
				EventMap[0x85] = SANRIO2_EVENT_NR21;
				EventMap[0x86] = SANRIO2_EVENT_NR31;
				EventMap[0x87] = SANRIO2_EVENT_NR41;
				EventMap[0x88] = SANRIO2_EVENT_ENV_SEQ_CH1;
				EventMap[0x89] = SANRIO2_EVENT_ENV_SEQ_CH2;
				EventMap[0x8A] = SANRIO2_EVENT_ENV_SEQ_CH3;
				EventMap[0x8B] = SANRIO2_EVENT_ENV_SEQ_CH4;
				EventMap[0x8C] = SANRIO2_EVENT_VIB_SEQ_CH1;
				EventMap[0x8D] = SANRIO2_EVENT_VIB_SEQ_CH2;
				EventMap[0x8E] = SANRIO2_EVENT_VIB_SEQ_CH3;
				EventMap[0x8F] = SANRIO2_EVENT_NOTE_LEN_TABLE;
				EventMap[0x90] = SANRIO2_EVENT_NOP;
				EventMap[0x91] = SANRIO2_EVENT_NOP;
				EventMap[0x92] = SANRIO2_EVENT_NOP;
				EventMap[0x93] = SANRIO2_EVENT_JUMP;
				EventMap[0x94] = SANRIO2_EVENT_CALL_CH1;
				EventMap[0x95] = SANRIO2_EVENT_CALL_CH2;
				EventMap[0x96] = SANRIO2_EVENT_CALL_CH3;
				EventMap[0x97] = SANRIO2_EVENT_CALL_CH4;
				EventMap[0x98] = SANRIO2_EVENT_RETURN_CH1;
				EventMap[0x99] = SANRIO2_EVENT_RETURN_CH2;
				EventMap[0x9A] = SANRIO2_EVENT_RETURN_CH3;
				EventMap[0x9B] = SANRIO2_EVENT_RETURN_CH4;
				EventMap[0x9C] = SANRIO2_EVENT_STOP_CH1;
				EventMap[0x9D] = SANRIO2_EVENT_STOP_CH2;
				EventMap[0x9E] = SANRIO2_EVENT_STOP_CH3;
				EventMap[0x9F] = SANRIO2_EVENT_STOP_CH4;
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
				noteTab = 0;
				firstRepeats[curTrack] = 0;

				repeatNums[curTrack] = 0;
				for (j = 0; j < 4; j++)
				{
					macros[curTrack][j][0] = 0;
					macros[curTrack][j][1] = 0;
				}
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

				if (seqPtrs[curTrack] != 0x0000)
				{
					seqPosM[curTrack] = seqPtrs[curTrack];
					tracksEnd[curTrack] = 0;
				}
				else
				{
					tracksEnd[curTrack] = 1;
				}

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

			while (songEnd == 0)
			{
				if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1 && tracksEnd[3] == 1)
				{
					songEnd = 1;
				}

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (tracksEnd[curTrack] != 1)
					{
						while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] != 1)
						{
							command[0] = romData[seqPosM[curTrack]];
							command[1] = romData[seqPosM[curTrack] + 1];
							command[2] = romData[seqPosM[curTrack] + 2];
							command[3] = romData[seqPosM[curTrack] + 3];

							if (command[0] == SANRIO2_STATUS_REST || (command[0] >= SANRIO2_STATUS_NOTE_MIN && command[0] <= SANRIO2_STATUS_NOTE_MAX))
							{
								lowNibble = (command[1] >> 4);
								highNibble = (command[1] & 15);

								if (repeatNums[curTrack] <= 1)
								{
									repeatNums[curTrack] = highNibble + 1;

									curNoteLens[curTrack] = (romData[curNoteLenTab + lowNibble]) * 5;
									seqPosM[curTrack]++;
								}
								else
								{
									repeatNums[curTrack]--;
								}
								if (command[0] == SANRIO2_STATUS_REST)
								{
									curDelays[curTrack] += curNoteLens[curTrack];
								}
								else
								{
									curNotes[curTrack] = command[0] - SANRIO2_STATUS_NOTE_MIN + 24;

									if (curTrack < 2)
									{
										curNotes[curTrack] += 12;
									}

									tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									firstNotes[curTrack] = 0;
									holdNotes[curTrack] = 1;
									midPosM[curTrack] = tempPos;
									curDelays[curTrack] = 0;

								}
								masterDelays[curTrack] += curNoteLens[curTrack];
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_NOP)
							{
								seqPosM[curTrack]++;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_WAVEFORM)
							{
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_GLOBAL_VOL)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_PAN)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_NR10)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_NR11 || EventMap[command[0]] == SANRIO2_EVENT_NR21 || EventMap[command[0]] == SANRIO2_EVENT_NR31 || EventMap[command[0]] == SANRIO2_EVENT_NR41)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_ENV_SEQ_CH1 || EventMap[command[0]] == SANRIO2_EVENT_ENV_SEQ_CH2 || EventMap[command[0]] == SANRIO2_EVENT_ENV_SEQ_CH3 || EventMap[command[0]] == SANRIO2_EVENT_ENV_SEQ_CH4)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_VIB_SEQ_CH1 || EventMap[command[0]] == SANRIO2_EVENT_VIB_SEQ_CH2 || EventMap[command[0]] == SANRIO2_EVENT_VIB_SEQ_CH3)
							{
								seqPosM[curTrack] += 2;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_NOTE_LEN_TABLE)
							{
								curNoteLenTab = ReadLE16(&romData[seqPosM[curTrack] + 1]);
								seqPosM[curTrack] += 3;
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_JUMP)
							{
								if ((ReadLE16(&romData[seqPosM[curTrack] + 1])) > seqPosM[curTrack])
								{
									seqPosM[curTrack] = ReadLE16(&romData[seqPosM[curTrack] + 1]);
								}
								else
								{
									tracksEnd[curTrack] = 1;
								}
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_CALL_CH1 || EventMap[command[0]] == SANRIO2_EVENT_CALL_CH2 || EventMap[command[0]] == SANRIO2_EVENT_CALL_CH3 || EventMap[command[0]] == SANRIO2_EVENT_CALL_CH4)
							{
								/*Macro level is too high*/
								if (inMacroM[curTrack] >= 5)
								{
									tracksEnd[curTrack] = 1;
								}
								else
								{
									inMacroM[curTrack]++;
									macros[curTrack][inMacroM[curTrack]][0] = ReadLE16(&romData[seqPosM[curTrack] + 1]);
									macros[curTrack][inMacroM[curTrack]][1] = seqPosM[curTrack] + 3;
									seqPosM[curTrack] = macros[curTrack][inMacroM[curTrack]][0];
								}
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_RETURN_CH1 || EventMap[command[0]] == SANRIO2_EVENT_RETURN_CH2 || EventMap[command[0]] == SANRIO2_EVENT_RETURN_CH3 || EventMap[command[0]] == SANRIO2_EVENT_RETURN_CH4)
							{
								seqPosM[curTrack] = macros[curTrack][inMacroM[curTrack]][1];

								if (inMacroM[curTrack] > 0)
								{
									inMacroM[curTrack]--;
								}
							}

							else if (EventMap[command[0]] == SANRIO2_EVENT_STOP_CH1 || EventMap[command[0]] == SANRIO2_EVENT_STOP_CH2 || EventMap[command[0]] == SANRIO2_EVENT_STOP_CH3 || EventMap[command[0]] == SANRIO2_EVENT_STOP_CH4)
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
				}

				seqTime += 5;
				ctrlDelay += 5;
			}
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{

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
}