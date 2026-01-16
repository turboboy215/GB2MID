/*archOnPlayer (by Stello Doussis)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "ARCHON.H"

#define bankSize 16384

FILE* rom, * xm, * data, * cfg;
long bank;
long tablePtrLoc;
long tableOffset;
int i, j;
char archonOutFile[1000000];
long bankAmt;
int songNum;
long songPtr;
int curInst;

unsigned char* romData;
unsigned char* songData;
unsigned char* xmData;
unsigned char* endData;
long xmLength;
int multiBanks;
int curBank;

char folderName[100];

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void archOnsong2xm(int songNum, long songPtr);

void archOnProc(int bank)
{
	if (bank < 2)
	{
		bank = 2;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	tablePtrLoc = ARCHON_SONG_TABLE;
	tableOffset = ReadLE16(&romData[tablePtrLoc]);
	printf("Song table: 0x%04X\n", tableOffset);

	i = tableOffset;
	songNum = 1;
	while (ReadLE16(&romData[i]) >= bankSize && ReadLE16(&romData[i]) < (bankSize * 2))
	{
		songPtr = ReadLE16(&romData[i]);
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		archOnsong2xm(songNum, songPtr);
		i += 2;
		songNum++;
	}


	free(romData);
}

/*Convert the song data to XM*/
void archOnsong2xm(int songNum, long songPtr)
{
	int curPat = 0;
	long pattern[4];
	unsigned char command[5];
	unsigned char trackCommand[5];
	long curPos = 0;
	int index = 0;
	int curSeq = 0;
	long barAddr = 0;
	long trackPtrs[5];
	long trackPos[5];
	int transpose[4] = { 0, 0, 0, 0 };
	int playTracks[5] = { 0, 0, 0, 0 };
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	int playSong = 0;
	int endBar[5] = { 0, 0, 0, 0, 0 };
	long masterPos = 0;
	long xmPos = 0;
	long patPos = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	long packPos = 0;
	long tempPos = 0;
	int rowsLeft[5];
	int curTrack = 0;
	int numPats = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long patSize = 0;
	int curNote;
	int instChan;
	int curInsts[4];
	unsigned char tempByte;
	int rowsUsed[4];
	int endFlag;
	int curFX;
	int fxAmt;

	int l = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	xmLength = 0x40000;
	xmData = ((unsigned char*)malloc(xmLength));

	for (l = 0; l < xmLength; l++)
	{
		xmData[l] = 0;
	}

	if (multiBanks == 0)
	{
		sprintf(archonOutFile, "song%d.xm", songNum);
	}
	else
	{
		sprintf(archonOutFile, "Bank %i/song%d.xm", (curBank + 1), songNum);
	}
	if ((xm = fopen(archonOutFile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.xm!\n", songNum);
		exit(2);
	}
	else
	{
		/*Get basic information*/
		barAddr = ReadLE16(&romData[songPtr + 7]);
		trackPtrs[0] = ReadLE16(&romData[songPtr + 11]);
		trackPtrs[1] = ReadLE16(&romData[songPtr + 13]);
		trackPtrs[2] = ReadLE16(&romData[songPtr + 15]);
		trackPtrs[3] = ReadLE16(&romData[songPtr + 17]);
		trackPtrs[4] = ReadLE16(&romData[songPtr + 9]);

		ARCHON_STATUS_REST = 0x7A;

		ArchonEventMap[0x7B] = ARCHON_EVENT_PITCH_BEND;
		ArchonEventMap[0x7D] = ARCHON_EVENT_ADD_WAVE;
		ArchonEventMap[0x7E] = ARCHON_EVENT_MASTER_VOLUME;
		ArchonEventMap[0x7F] = ARCHON_EVENT_BAR_END;

		ARCHON_STATUS_PROG_CHANGE_MIN = 0x80;
		ARCHON_STATUS_PROG_CHANGE_MAX = 0xFF;

		printf("Song %i bar addresses: 0x%04X\n", songNum, barAddr);
		printf("Song %i master track: 0x%04X\n", songNum, trackPtrs[4]);
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			printf("Song %i channel %i: 0x%04X\n", songNum, (curTrack + 1), trackPtrs[curTrack]);
		}

		/*Get the number of patterns for the XM*/
		j = trackPtrs[0];
		numPats = 0;
		while (romData[j] != 0xFE && romData[j] != 0xFF)
		{
			if (romData[j] <= 0xBF)
			{
				numPats++;
			}
			j++;
		}

		xmPos = 0;
		/*Write the header*/
		sprintf((char*)&xmData[xmPos], "Extended Module: ");
		xmPos += 17;
		sprintf((char*)&xmData[xmPos], "                     ");
		xmPos += 20;
		Write8B(&xmData[xmPos], 0x1A);
		xmPos++;
		sprintf((char*)&xmData[xmPos], "FastTracker v2.00   ");
		xmPos += 20;
		WriteBE16(&xmData[xmPos], 0x0401);
		xmPos += 2;

		/*Header size: 20 + number of patterns (256)*/
		WriteLE32(&xmData[xmPos], 276);
		xmPos += 4;

		/*Song length*/
		WriteLE16(&xmData[xmPos], numPats);
		xmPos += 2;

		/*Restart position*/
		WriteLE16(&xmData[xmPos], 0);
		xmPos += 2;

		/*Number of channels*/
		WriteLE16(&xmData[xmPos], channels + 2);
		xmPos += 2;

		/*Number of patterns*/
		WriteLE16(&xmData[xmPos], numPats);
		xmPos += 2;

		/*Number of instruments*/
		WriteLE16(&xmData[xmPos], 32);
		xmPos += 2;

		/*Flags: Linear frequency*/
		WriteLE16(&xmData[xmPos], 1);
		xmPos += 2;

		/*Default tempo (ticks)*/
		defTicks = 6;
		WriteLE16(&xmData[xmPos], defTicks);
		xmPos += 2;

		/*Default tempo (BPM), always the same for our case*/
		WriteLE16(&xmData[xmPos], bpm);
		xmPos += 2;

		/*Pattern table*/
		for (l = 0; l < numPats; l++)
		{
			Write8B(&xmData[xmPos], l);
			xmPos++;
		}
		xmPos += (256 - l);

		for (curTrack = 0; curTrack < 5; curTrack++)
		{
			trackPos[0] = trackPtrs[0];
			trackPos[1] = trackPtrs[1];
			trackPos[2] = trackPtrs[2];
			trackPos[3] = trackPtrs[3];
			/*Master track*/
			trackPos[4] = trackPtrs[4];
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			playTracks[curTrack] = 0;
		}

		/*Track commands*/
		ARCHON_TRACK_TRANSPOSE_UP_MIN = 0xC0;
		ARCHON_TRACK_TRANSPOSE_UP_MAX = 0xDF;
		ARCHON_TRACK_TRANSPOSE_DOWN_MIN = 0xE0;
		ARCHON_TRACK_TRANSPOSE_DOWN_MAX = 0xFD;
		ARCHON_TRACK_JUMP = 0xFE;
		ARCHON_TRACK_END = 0xFF;
		ARCHON_MASTER_TRACK_END = 0xFF;

		curInsts[0] = 0;
		curInsts[1] = 0;
		curInsts[2] = 0;
		curInsts[3] = 0;

		for (curPat = 0; curPat < numPats; curPat++)
		{
			/*First, pattern header*/
			/*Pattern header length*/
			WriteLE32(&xmData[xmPos], 9);
			xmPos += 4;

			/*Packing type = 0*/
			Write8B(&xmData[xmPos], 0);
			xmPos++;

			/*Number of rows*/
			WriteLE16(&xmData[xmPos], 64);
			xmPos += 2;

			/*Packed pattern data - fill in later*/
			packPos = xmPos;
			WriteLE16(&xmData[xmPos], 0);
			xmPos += 2;

			/*Now the actual pattern data...*/
			rowsLeft[0] = 64;
			rowsLeft[1] = 64;
			rowsLeft[2] = 64;
			rowsLeft[3] = 64;
			rowsLeft[4] = 64;
			patSize = 0;
			rowsUsed[0] = 0;
			rowsUsed[1] = 0;
			rowsUsed[2] = 0;
			rowsUsed[3] = 0;
			playTracks[0] = 0;
			playTracks[1] = 0;
			playTracks[2] = 0;
			playTracks[3] = 0;
			playTracks[4] = 0;

			for (curTrack = 0; curTrack < 5; curTrack++)
			{
				trackCommand[0] = romData[trackPos[curTrack]];
				trackCommand[1] = romData[trackPos[curTrack] + 1];
				trackCommand[2] = romData[trackPos[curTrack] + 2];

				while (playTracks[curTrack] != 1)
				{
					if (trackCommand[0] < ARCHON_TRACK_TRANSPOSE_UP_MIN)
					{
						if (curTrack == 0)
						{
							c1Pos = ReadLE16(&romData[barAddr + (trackCommand[0] * 2)]);
							playTracks[curTrack] = 1;
						}
						else if (curTrack == 1)
						{
							c2Pos = ReadLE16(&romData[barAddr + (trackCommand[0] * 2)]);
							playTracks[curTrack] = 1;
						}
						else if (curTrack == 2)
						{
							c3Pos = ReadLE16(&romData[barAddr + (trackCommand[0] * 2)]);
							playTracks[curTrack] = 1;
						}
						else if (curTrack == 3)
						{
							c4Pos = ReadLE16(&romData[barAddr + (trackCommand[0] * 2)]);
							playTracks[curTrack] = 1;
						}
						else if (curTrack == 4)
						{
							masterPos = ReadLE16(&romData[barAddr + (trackCommand[0] * 2)]);
							playTracks[curTrack] = 1;
						}
						trackPos[curTrack]++;
					}
					else if (trackCommand[0] >= ARCHON_TRACK_TRANSPOSE_UP_MIN && trackCommand[0] <= ARCHON_TRACK_TRANSPOSE_UP_MAX)
					{
						transpose[curTrack] += (trackCommand[0] - ARCHON_TRACK_TRANSPOSE_UP_MIN);
						trackPos[curTrack]++;
					}
					else if (trackCommand[0] >= ARCHON_TRACK_TRANSPOSE_DOWN_MIN && trackCommand[0] <= ARCHON_TRACK_TRANSPOSE_DOWN_MAX)
					{
						transpose[curTrack] -= (trackCommand[0] - ARCHON_TRACK_TRANSPOSE_DOWN_MIN);
						trackPos[curTrack]++;
					}

					else if (trackCommand[0] == ARCHON_TRACK_JUMP)
					{
						trackPos[curTrack] = ReadLE16(&romData[trackPos[curTrack] + 1]);
						trackCommand[0] = romData[trackPos[curTrack]];
						trackCommand[1] = romData[trackPos[curTrack] + 1];
						trackCommand[2] = romData[trackPos[curTrack] + 2];
					}

					else if (trackCommand[0] == ARCHON_MASTER_TRACK_END && curTrack == 4)
					{
						trackPos[curTrack] = ReadLE16(&romData[trackPos[curTrack] + 1]);
						trackCommand[0] = romData[trackPos[curTrack]];
						trackCommand[1] = romData[trackPos[curTrack] + 1];
						trackCommand[2] = romData[trackPos[curTrack] + 2];
					}
				}
			}

			endBar[0] = 0;
			endBar[1] = 0;
			endBar[2] = 0;
			endBar[3] = 0;
			endBar[4] = 0;
			playSong = 1;
			endFlag = 0;
			curFX = 0x23;
			fxAmt = 0x00;

			while (playSong == 1)
			{
				if (endBar[0] == 1 && endBar[1] == 1 && endBar[2] == 1 && endBar[3] == 1)
				{
					playSong = 0;
				}

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					if (curTrack == 0)
					{
						command[0] = romData[c1Pos];
						command[1] = romData[c1Pos + 1];
						command[2] = romData[c1Pos + 2];
						command[3] = romData[c1Pos + 3];
						command[4] = romData[c1Pos + 4];
					}

					else if (curTrack == 1)
					{
						command[0] = romData[c2Pos];
						command[1] = romData[c2Pos + 1];
						command[2] = romData[c2Pos + 2];
						command[3] = romData[c2Pos + 3];
						command[4] = romData[c2Pos + 4];
					}

					else if (curTrack == 2)
					{
						command[0] = romData[c3Pos];
						command[1] = romData[c3Pos + 1];
						command[2] = romData[c3Pos + 2];
						command[3] = romData[c3Pos + 3];
						command[4] = romData[c3Pos + 4];
					}

					else if (curTrack == 3)
					{
						command[0] = romData[c4Pos];
						command[1] = romData[c4Pos + 1];
						command[2] = romData[c4Pos + 2];
						command[3] = romData[c4Pos + 3];
						command[4] = romData[c4Pos + 4];
					}

					if (rowsUsed[curTrack] > 0)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
						rowsUsed[curTrack]--;
						rowsLeft[curTrack]--;
					}
					else if (endBar[curTrack] == 1 && playSong == 1)
					{
						Write8B(&xmData[xmPos], 0x80);
						xmPos++;
						patSize++;
						rowsLeft[curTrack]--;
					}
					else
					{
						if (endBar[curTrack] != 1)
						{
							/*Command*/
							if (command[0] >= 0x7B)
							{
								if (ArchonEventMap[command[0]] == ARCHON_EVENT_PITCH_BEND)
								{
									if (curTrack == 0)
									{
										c1Pos += 4;
									}
									else if (curTrack == 1)
									{
										c2Pos += 4;
									}
									else if (curTrack == 2)
									{
										c3Pos += 4;
									}
									else if (curTrack == 3)
									{
										c4Pos += 4;
									}
									curFX = 0x01;
									fxAmt = command[3];
								}
								else if (ArchonEventMap[command[0]] == ARCHON_EVENT_ADD_WAVE)
								{
									if (curTrack == 0)
									{
										c1Pos += 2;
									}
									else if (curTrack == 1)
									{
										c2Pos += 2;
									}
									else if (curTrack == 2)
									{
										c3Pos += 2;
									}
									else if (curTrack == 3)
									{
										c4Pos += 2;
									}
								}

								else if (ArchonEventMap[command[0]] == ARCHON_EVENT_MASTER_VOLUME)
								{
									if (curTrack == 0)
									{
										c1Pos += 3;
									}
									else if (curTrack == 1)
									{
										c2Pos += 3;
									}
									else if (curTrack == 2)
									{
										c3Pos += 3;
									}
									else if (curTrack == 3)
									{
										c4Pos += 3;
									}
								}

								else if (ArchonEventMap[command[0]] == ARCHON_EVENT_BAR_END)
								{
									endBar[curTrack] = 1;
									Write8B(&xmData[xmPos], 0x80);
									xmPos++;
									patSize++;
									if (rowsLeft[curTrack] > 0)
									{
										rowsLeft[curTrack]--;
									}
								}

								else if (command[0] >= ARCHON_STATUS_PROG_CHANGE_MIN && command[0] <= ARCHON_STATUS_PROG_CHANGE_MAX)
								{
									curInsts[curTrack] = command[0] - ARCHON_STATUS_PROG_CHANGE_MIN + 1;

									/*
									if (curInsts[curTrack] > 30)
									{
										curInsts[curTrack] = 1;
									}
									*/
									if (curTrack == 0)
									{
										c1Pos++;
									}
									else if (curTrack == 1)
									{
										c2Pos++;
									}
									else if (curTrack == 2)
									{
										c3Pos++;
									}
									else if (curTrack == 3)
									{
										c4Pos++;
									}
								}
							}
						}

						if (endBar[curTrack] != 1)
						{
							if (curTrack == 0)
							{
								command[0] = romData[c1Pos];
								command[1] = romData[c1Pos + 1];
								command[2] = romData[c1Pos + 2];
								command[3] = romData[c1Pos + 3];
								command[4] = romData[c1Pos + 4];
							}

							else if (curTrack == 1)
							{
								command[0] = romData[c2Pos];
								command[1] = romData[c2Pos + 1];
								command[2] = romData[c2Pos + 2];
								command[3] = romData[c2Pos + 3];
								command[4] = romData[c2Pos + 4];
							}

							else if (curTrack == 2)
							{
								command[0] = romData[c3Pos];
								command[1] = romData[c3Pos + 1];
								command[2] = romData[c3Pos + 2];
								command[3] = romData[c3Pos + 3];
								command[4] = romData[c3Pos + 4];
							}

							else if (curTrack == 3)
							{
								command[0] = romData[c4Pos];
								command[1] = romData[c4Pos + 1];
								command[2] = romData[c4Pos + 2];
								command[3] = romData[c4Pos + 3];
								command[4] = romData[c4Pos + 4];
							}

							if (command[0] == ARCHON_STATUS_REST)
							{
								if (curFX != 0x00)
								{
									if (fxAmt != 0x00)
									{
										Write8B(&xmData[xmPos], 0x98);
										Write8B(&xmData[xmPos + 1], curFX);
										Write8B(&xmData[xmPos + 2], fxAmt);
										xmPos += 3;
										patSize += 3;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}
									else
									{
										Write8B(&xmData[xmPos], 0x88);
										Write8B(&xmData[xmPos + 1], fxAmt);
										xmPos += 2;
										patSize += 2;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}
								}
								else
								{
									if (fxAmt != 0x00)
									{
										Write8B(&xmData[xmPos], 0x90);
										Write8B(&xmData[xmPos + 1], fxAmt);
										xmPos += 2;
										patSize += 2;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}
									else
									{
										Write8B(&xmData[xmPos], 0x80);
										xmPos++;
										patSize++;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}

								}
								curFX = 0;


								/*Number of rows used*/
								rowsUsed[curTrack] = (command[1] & 0x3F);
								rowsUsed[curTrack]--;
								rowsLeft[curTrack]--;
							}

							/*Play note*/
							else
							{
								curNote = command[0] + 1;
								fxAmt = command[2];

								if (curFX == 0x00)
								{
									curFX = 0x23;
								}

								if (curFX != 0x00)
								{
									if (fxAmt != 0x00)
									{
										Write8B(&xmData[xmPos], 0x9B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInsts[curTrack]);
										Write8B(&xmData[xmPos + 3], curFX);
										Write8B(&xmData[xmPos + 4], fxAmt);
										xmPos += 5;
										patSize += 5;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}
									else
									{
										Write8B(&xmData[xmPos], 0x8B);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInsts[curTrack]);
										Write8B(&xmData[xmPos + 3], curFX);
										xmPos += 4;
										patSize += 4;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}

								}
								else
								{
									if (fxAmt == 0x00)
									{
										Write8B(&xmData[xmPos], 0x93);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInsts[curTrack]);
										Write8B(&xmData[xmPos + 3], fxAmt);
										xmPos += 4;
										patSize += 4;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}
									else
									{
										Write8B(&xmData[xmPos], 0x83);
										Write8B(&xmData[xmPos + 1], curNote);
										Write8B(&xmData[xmPos + 2], curInsts[curTrack]);
										xmPos += 3;
										patSize += 3;
										if (curTrack == 0)
										{
											c1Pos += 3;
										}
										else if (curTrack == 1)
										{
											c2Pos += 3;
										}
										else if (curTrack == 2)
										{
											c3Pos += 3;
										}
										else if (curTrack == 3)
										{
											c4Pos += 3;
										}
									}
								}


								/*Number of rows used*/
								rowsUsed[curTrack] = (command[1] & 0x3F);
								rowsUsed[curTrack]--;
								rowsLeft[curTrack]--;

								curFX = 0x23;
							}
						}

					}

				}

				/*Master track*/
				command[0] = romData[masterPos];
				command[1] = romData[masterPos + 1];
				if (endBar[curTrack] == 1 && playSong == 1)
				{
					Write8B(&xmData[xmPos], 0x80);
					xmPos++;
					patSize++;
					masterPos++;
				}
				else if (endBar[curTrack] != 1)
				{

					if (command[0] == ARCHON_TRACK_END)
					{
						endBar[curTrack] = 1;
						endFlag = 1;
					}
					else
					{
						if (command[0] == 0x00)
						{
							Write8B(&xmData[xmPos], 0x88);
							Write8B(&xmData[xmPos + 1], 0x0F);
							xmPos += 2;
							patSize += 2;
						}
						else
						{
							Write8B(&xmData[xmPos], 0x98);
							Write8B(&xmData[xmPos + 1], 0x0F);
							Write8B(&xmData[xmPos + 2], command[0]);
							xmPos += 3;
							patSize += 3;
						}
						masterPos++;

						if (command[1] == ARCHON_TRACK_END)
						{
							endBar[curTrack] = 1;
							endFlag = 1;
						}
					}
				}

				/*End of song flag control (6th XM channel)*/
				if (endFlag == 1 && rowsLeft[0] > 0)
				{
					Write8B(&xmData[xmPos], 0x88);
					Write8B(&xmData[xmPos + 1], 0x0D);
					xmPos += 2;
					patSize += 2;
					endFlag = 0;
				}
				else
				{
					Write8B(&xmData[xmPos], 0x80);
					xmPos++;
					patSize++;
				}

			}

			WriteLE16(&xmData[packPos], patSize);

		}

		fwrite(xmData, xmPos, 1, xm);

		/*Add data to end of XM file*/
		if ((data = fopen("xmdata.dat", "rb")) == NULL)
		{
			printf("ERROR: Unable to open file xmdata.dat!\n");
			exit(1);
		}
		else
		{
			endData = ((unsigned char*)malloc(11744));
			fread(endData, 1, 11744, data);
			fwrite(endData, 11744, 1, xm);
			xmPos += 11744;
		}


		free(songData);
		free(xmData);
		free(endData);
		fclose(xm);
	}
}