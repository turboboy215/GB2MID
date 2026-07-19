/*Hirotomo Nakamura (Giraffe Soft)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MEGAMAN2.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long sfxPtrLoc;
long sfxOffset;
int i, j;
char outfile[1000000];
int songNum;
long curPtr;
long seqPtrs[4];
long songPtr;
long bankAmt;
int curVol;
int drvVers;
int seqDiff = 0;
int foundTable;
int songTempo = 0;
int highestSeq = 0;
int curInst;
int compatibility = 0;
int octaveSet = 0;

unsigned char* romData;
unsigned char* multiMidData[8];
unsigned char* midData;

unsigned char* ctrlMidData;

long midLength;

const char MM2TableFind1[5] = { 0x87, 0x5F, 0x16, 0x00, 0x21 };
const char MM2TableFind2[7] = { 0x87, 0xCF, 0x2A, 0x66, 0x6F, 0xAF, 0xE0 };
const char MM2TableFind3[5] = { 0x17, 0x5F, 0x16, 0x00, 0x21 };

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
void MM2song2mid(int songNum, long ptrs[4]);

void MM2Proc(int bank, char parameters[4][100])
{
	drvVers = MM2_VER_STD;
	curVol = 120;
	compatibility = 0;
	foundTable = 0;
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


	if (parameters[0][0] != 0x00)
	{
		drvVers = strtol(parameters[0], NULL, 16);

		if (drvVers != MM2_VER_EARLY && drvVers != MM2_VER_STD)
		{
			printf("ERROR: Invalid version number!\n");
		}

		compatibility = 1;
	}

	/*Try to search the bank for song table loader*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MM2TableFind1, 5)))
		{
			if (foundTable == 0)
			{
				tablePtrLoc = i + 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
			else if (foundTable == 1)
			{
				sfxPtrLoc = i + 5;
				printf("Found pointer to sound effects table at address 0x%04x!\n", sfxPtrLoc);
				sfxOffset = ReadLE16(&romData[sfxPtrLoc]);
				printf("Sound effects table starts at 0x%04x...\n", sfxOffset);
			}
		}
	}


	/*Alternate method - Ayakashi no Shiro*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MM2TableFind2, 7)) && foundTable == 0)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			if (compatibility != 1)
			{
				drvVers = MM2_VER_EARLY;
			}
			break;
		}
	}

	/*Alternate method - Rentaiou/The Shinri Game 1/2*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MM2TableFind3, 5)) && foundTable == 0)
		{
			tablePtrLoc = i + 5;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			if (compatibility != 1)
			{
				drvVers = MM2_VER_EARLY;
			}
			break;
		}
	}

	if (foundTable == 1)
	{
		/*Skip first "empty" track*/
		i = tableOffset + 2;
		songNum = 1;
		while ((ReadLE16(&romData[i]) < bankSize * 2) && i != (sfxOffset))
		{
			if (compatibility == 2)
			{
				if (songNum > 5)
				{
					break;
				}
			}
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);

			seqPtrs[0] = ReadLE16(&romData[songPtr]);
			printf("Channel 1: 0x%04X\n", seqPtrs[0]);
			seqPtrs[1] = ReadLE16(&romData[songPtr + 2]);
			printf("Channel 2: 0x%04X\n", seqPtrs[1]);
			seqPtrs[2] = ReadLE16(&romData[songPtr + 4]);
			printf("Channel 3: 0x%04X\n", seqPtrs[2]);
			seqPtrs[3] = ReadLE16(&romData[songPtr + 6]);
			printf("Channel 4: 0x%04X\n", seqPtrs[3]);
			MM2song2mid(songNum, seqPtrs);
			i += 2;
			songNum++;
		}
		free(romData);
		fclose(rom);
		printf("The operation was successfully completed!\n");
		exit(0);

	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(0);
	}

}

/*Convert the song data to MIDI*/
void MM2song2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	int curTrack = 0;
	int curNote = 0;
	unsigned int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
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
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int inMacro;
	int macroPos;
	int macroRet;
	int inMacroM[4];
	int macros[4][3];
	int firstNote = 0;
	int firstNotes[4];
	long seqTime = 0;
	int curInsts[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

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
		case MM2_VER_EARLY:
		case MM2_VER_STD:
			/*Fall-through*/
		default:
			MM2_STATUS_NOTE_MIN = 0x00;
			MM2_STATUS_NOTE_MAX = 0xBF;
			MM2_STATUS_TIE_MIN = 0xD0;
			MM2_STATUS_TIE_MAX = 0xDF;
			MM2_STATUS_REST_MIN = 0xE0;
			MM2_STATUS_REST_MAX = 0xEF;
			MM2_STATUS_OCTAVE_MIN = 0xF0;
			MM2_STATUS_OCTAVE_MAX = 0xFF;
			EventMap[0xC0] = MM2_EVENT_TEMPO;
			EventMap[0xC1] = MM2_EVENT_DUTY;
			EventMap[0xC2] = MM2_EVENT_VOLUME;
			EventMap[0xC3] = MM2_EVENT_JUMP;
			EventMap[0xC4] = MM2_EVENT_PAN;
			EventMap[0xC9] = MM2_EVENT_RETURN;
			EventMap[0xCA] = MM2_EVENT_STOP;
			EventMap[0xCB] = MM2_EVENT_STOP;
			EventMap[0xCC] = MM2_EVENT_STOP;
			EventMap[0xCD] = MM2_EVENT_CALL;
			EventMap[0xCE] = MM2_EVENT_STOP;
			EventMap[0xCF] = MM2_EVENT_STOP;
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
			inMacroM[curTrack] = 0;
			macros[curTrack][0] = 0;
			macros[curTrack][1] = 0;

			if (curTrack != 3)
			{
				octaves[curTrack] = 0;
			}
			else
			{
				octaves[curTrack] = 3;
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

			seqPosM[curTrack] = ptrs[curTrack];

			if (ptrs[curTrack] >= bankSize * 2)
			{
				tracksEnd[curTrack] = 1;
			}
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
				while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] == 0)
				{
					if (seqPosM[curTrack] >= bankSize * 2 || midPosM[curTrack] > 48000)
					{
						tracksEnd[curTrack] = 1;
					}

					command[0] = romData[seqPosM[curTrack]];
					command[1] = romData[seqPosM[curTrack] + 1];
					command[2] = romData[seqPosM[curTrack] + 2];

					if (command[0] >= MM2_STATUS_NOTE_MIN && command[0] <= MM2_STATUS_NOTE_MAX)
					{
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = 0;
						}
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);

						curNotes[curTrack] = (octaves[curTrack] * 12) + lowNibble;

						if (curTrack != 3)
						{
							curNotes[curTrack] += 12;
							if (curTrack != 2)
							{
								curNotes[curTrack] += 12;
							}
						}

						k = highNibble & 0x07;

						if (drvVers == MM2_VER_STD)
						{
							curNoteLens[curTrack] = 128;
						}
						else
						{
							curNoteLens[curTrack] = tempo;
						}


						for (j = 0; j < k; j++)
						{
							curNoteLens[curTrack] /= 2;
						}

						k = highNibble & 0x08;

						if (k != 0)
						{
							curNoteLens[curTrack] *= 1.5;
						}

						curNoteLens[curTrack] *= 5;

						curVol = curVols[curTrack];
						tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
						firstNotes[curTrack] = 0;
						holdNotes[curTrack] = 1;
						midPosM[curTrack] = tempPos;
						curDelays[curTrack] = curNoteLens[curTrack];
						masterDelays[curTrack] += curNoteLens[curTrack];

						seqPosM[curTrack]++;

					}

					else if (command[0] >= MM2_STATUS_TIE_MIN && command[0] <= MM2_STATUS_TIE_MAX)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);

						k = highNibble & 0x07;
						if (drvVers == MM2_VER_STD)
						{
							curNoteLens[curTrack] = 128;
						}
						else
						{
							curNoteLens[curTrack] = tempo;
						}

						for (j = 0; j < k; j++)
						{
							curNoteLens[curTrack] /= 2;
						}

						k = highNibble & 0x08;

						if (k != 0)
						{
							curNoteLens[curTrack] *= 1.5;
						}

						curNoteLens[curTrack] *= 5;

						curDelays[curTrack] += curNoteLens[curTrack];
						ctrlDelay += curNoteLens[curTrack];
						masterDelays[curTrack] += curNoteLens[curTrack];
						seqPosM[curTrack]++;
					}

					else if (command[0] >= MM2_STATUS_REST_MIN && command[0] <= MM2_STATUS_REST_MAX)
					{
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = 0;
						}

						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);

						k = highNibble & 0x07;
						if (drvVers == MM2_VER_STD)
						{
							curNoteLens[curTrack] = 128;
						}
						else
						{
							curNoteLens[curTrack] = tempo;
						}

						for (j = 0; j < k; j++)
						{
							curNoteLens[curTrack] /= 2;
						}

						k = highNibble & 0x08;

						if (k != 0)
						{
							curNoteLens[curTrack] *= 1.5;
						}

						curNoteLens[curTrack] *= 5;

						curDelays[curTrack] += curNoteLens[curTrack];
						ctrlDelay += curNoteLens[curTrack];
						masterDelays[curTrack] += curNoteLens[curTrack];
						seqPosM[curTrack]++;
					}

					else if (command[0] >= MM2_STATUS_OCTAVE_MIN && command[0] <= MM2_STATUS_OCTAVE_MAX)
					{
						j = command[0] & 0x07;
						k = command[0] & 0x08;

						if (k != 0)
						{
							if (j < 4)
							{
								octaves[curTrack] += j + 1;
							}
							else
							{
								octaves[curTrack] = (octaves[curTrack] + command[0]) & 0xFF;
							}
						}
						else
						{
							octaves[curTrack] = j;
						}

						if (octaves[curTrack] < 0)
						{
							octaves[curTrack] = 0;
						}

						if (octaves[curTrack] > 7)
						{
							octaves[curTrack] = 7;
						}

						seqPosM[curTrack]++;

					}

					else if (EventMap[command[0]] == MM2_EVENT_TEMPO)
					{
						if (drvVers == MM2_VER_STD)
						{
							double ticksPerSec;
							ticksPerSec = 4096.0 / (0x100 - command[1]);	// timer interrupt rate

							tempo = ticksPerSec * 2.5;

							ctrlMidPos++;
							valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
							ctrlDelay = 0;
							ctrlMidPos += valSize;
							WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
							ctrlMidPos += 3;
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);

							if (tempo < 2)
							{
								tempo = 2;
							}
							ctrlMidPos += 2;
						}
						else
						{
							tempo = command[1];
						}
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MM2_EVENT_DUTY)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MM2_EVENT_VOLUME)
					{
						if (curTrack != 2)
						{
							/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
							if ((command[1] & 0x08) == 0x00)
							{
								curVols[curTrack] = (command[1] & 0xF0) / 2;
							}
							else
							{
								if (((command[1] & 0xF0) / 2) == curVol)
								{
									curVols[curTrack] = (command[1] & 0xF0) / 2;
								}
								else
								{
									curVols[curTrack] = 120;
								}

							}

							if (curVol == 0)
							{
								curVols[curTrack] = 1;
							}
						}
						else
						{
							switch (command[1])
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

					else if (EventMap[command[0]] == MM2_EVENT_JUMP)
					{
						jumpPos = ReadLE16(&romData[seqPosM[curTrack] + 1]);
						if (jumpPos > seqPosM[curTrack])
						{
							seqPosM[curTrack] = jumpPos;
						}
						else
						{
							if (holdNotes[curTrack] == 1)
							{
								tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								holdNotes[curTrack] = 0;
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
							}
							tracksEnd[curTrack] = 1;
						}
					}

					else if (EventMap[command[0]] == MM2_EVENT_PAN && curTrack == 0)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MM2_EVENT_RETURN)
					{
						seqPosM[curTrack] = macros[curTrack][1];
						inMacroM[curTrack] = 0;
					}

					else if (EventMap[command[0]] == MM2_EVENT_CALL)
					{
						macros[curTrack][0] = ReadLE16(&romData[seqPosM[curTrack] + 1]);
						macros[curTrack][1] = seqPosM[curTrack] + 3;
						seqPosM[curTrack] = macros[curTrack][0];
						inMacroM[curTrack] = 1;
					}

					else if (EventMap[command[0]] == MM2_EVENT_STOP)
					{
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = 0;
						}
						tracksEnd[curTrack] = 1;
					}

					/*Unknown command*/
					else
					{
						seqPosM[curTrack]++;
					}

				}

				seqTime += 5;
				ctrlDelay += 5;

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
	}

	free(multiMidData[0]);
	free(ctrlMidData);
	fclose(mid);


}