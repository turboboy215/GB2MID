/*Ei-How Yang (Sachen/Yong Yong (Makon Soft))*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <math.h>
#include "SHARED.H"
#include "EHY.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long songDir;
long patDir;
int i, j, x;
char outfile[1000000];
int songNum;
long songPtr;
long seqsPtr;
int numSongs;
long bankAmt;
int curInst;
long firstPtr;
int drvVers;
int cfgPtr;
int exitError;
int fileExit;
int curVol;
int mgMode;
int startBank;

char* argv3;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

char* argv3;
char string1[100];
char string2[100];
char EHYcheckStrings[6][100] = { "bank=", "numSongs=", "version=", "songList=", "patList=", "start=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void EHYsong2mid(int songNum, long ptr, long seqs);

void EHYProc(int bank, char parameters[4][100])
{
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;
	mgMode = 0;
	curVol = 100;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}

	if (parameters[1][0] != 0)
	{
		mgMode = 1;
	}

	if (mgMode == 1)
	{
		/*Get the starting bank of the game data*/
		fgets(string1, 7, cfg);
		if (memcmp(string1, EHYcheckStrings[5], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}

		fgets(string1, 5, cfg);
		startBank = strtol(string1, NULL, 16);
		fgets(string1, 3, cfg);
	}
	/*Get the bank number*/
	fgets(string1, 6, cfg);
	if (memcmp(string1, EHYcheckStrings[0], 1))
	{
		printf("ERROR: Invalid CFG data!\n");
		exit(1);

	}
	fgets(string1, 3, cfg);

	bank = strtol(string1, NULL, 16);
	fgets(string1, 10, cfg);
	printf("Bank: %02X\n", bank);

	if (mgMode != 1)
	{
		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);
	}
	else
	{
		fseek(rom, ((startBank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((bank - 1) * bankSize + (startBank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);
	}

	/*Get the total number of songs*/
	fgets(string1, 10, cfg);
	if (memcmp(string1, EHYcheckStrings[1], 1))
	{
		printf("ERROR: Invalid CFG data!\n");
		exit(1);

	}
	fgets(string1, 3, cfg);

	numSongs = strtol(string1, NULL, 16);

	printf("Number of songs: %i\n", numSongs);
	fgets(string1, 3, cfg);

	/*Get version number*/
	fgets(string1, 9, cfg);
	if (memcmp(string1, EHYcheckStrings[2], 1))
	{
		printf("ERROR: Invalid CFG data!\n");
		exit(1);

	}
	fgets(string1, 3, cfg);
	drvVers = strtol(string1, NULL, 16);

	printf("Version: %i\n", drvVers);
	if (drvVers < 1 || drvVers > 3)
	{
		printf("ERROR: Invalid version number!\n");
		exit(2);
	}
	fgets(string1, 3, cfg);

	/*Get song list*/
	fgets(string1, 10, cfg);
	if (memcmp(string1, EHYcheckStrings[3], 1))
	{
		printf("ERROR: Invalid CFG data!\n");
		exit(1);

	}
	fgets(string1, 5, cfg);
	songDir = strtol(string1, NULL, 16);

	printf("Song list: 0x%04X\n", songDir);
	fgets(string1, 3, cfg);

	/*Get sequence list*/
	fgets(string1, 9, cfg);
	if (memcmp(string1, EHYcheckStrings[4], 1))
	{
		printf("ERROR: Invalid CFG data!\n");
		exit(1);

	}
	fgets(string1, 5, cfg);
	patDir = strtol(string1, NULL, 16);

	printf("Sequence list: 0x%04X\n", patDir);
	fgets(string1, 3, cfg);

	songNum = 1;
	i = songDir;

	while (songNum <= numSongs)
	{
		songPtr = ReadLE16(&romData[i]);
		seqsPtr = ReadLE16(&romData[patDir + ((songNum - 1) * 2)]);
		printf("Song %i: 0x%04X, sequences: 0x%04X\n", songNum, songPtr, seqsPtr);
		EHYsong2mid(songNum, songPtr, seqsPtr);
		i += 2;
		songNum++;
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void EHYsong2mid(int songNum, long ptr, long seqs)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int curSeq;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int songEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned char command[4];
	unsigned char patCommand[4];
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
	int repeat = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int tempoVal = 0;

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

	curVol = 100;

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

		romPos = ptr;

		mask = romData[romPos];
		romPos++;

		/*Get active channels*/
		maskArray[0] = mask & 1;
		maskArray[1] = mask >> 1 & 1;
		maskArray[2] = mask >> 2 & 1;
		maskArray[3] = mask >> 3 & 1;

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			activeChan[curTrack] = 0;
		}

		/*Now get the pointers for each channel*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{

			if (maskArray[curTrack] == 1)
			{
				activeChan[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}
			else
			{
				activeChan[curTrack] = 0x0000;
			}
		}


		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat = -1;
			curVol = 100;

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

			if (activeChan[curTrack] != 0x0000)
			{
				songEnd = 0;
				romPos = activeChan[curTrack];

				if (drvVers == 3 && songNum == 1)
				{
					songEnd = 1;
				}
			}
			else
			{
				songEnd = 1;
			}

			if (drvVers == 1 || drvVers == 3)
			{
				while (songEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
				{

					patCommand[0] = romData[romPos];
					patCommand[1] = romData[romPos + 1];
					patCommand[2] = romData[romPos + 2];
					patCommand[3] = romData[romPos + 3];

					/*Play sequence*/
					if (patCommand[0] < 0x80)
					{
						curSeq = patCommand[0];
						seqPos = ReadLE16(&romData[seqs + (curSeq * 2)]);
						seqEnd = 0;

						while (seqEnd == 0)
						{
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];

							/*End of sequence*/
							if (command[0] == 0x00)
							{
								seqEnd = 1;
							}

							/*Play note*/
							else
							{
								/*Rest*/
								if (command[1] == 0x00)
								{
									curDelay += (command[0] * 20);
									ctrlDelay += (command[0] * 20);
								}
								else
								{
									curNote = command[1] - 1;
									if (curTrack != 2)
									{
										curNote += 12;
									}
									curNoteLen = command[0] * 20;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
								}
								seqPos += 2;
							}
						}
						romPos++;
					}

					/*End of pattern*/
					else if (patCommand[0] == 0xFC)
					{
						songEnd = 1;
					}

					/*Set volume/envelope*/
					else if (patCommand[0] == 0xFD)
					{
						if (drvVers == 1)
						{
							tempByte = patCommand[1] & 0x0F;
						}
						else
						{
							tempByte = (patCommand[1] >> 4) & 0x0F;
						}

						tempByte = log(tempByte / 15.0) * 8.65617024533378;

						if (tempByte > 0.0)
						{
							tempByte = 0;
						}
						tempByte = (unsigned char)(pow(10.0, tempByte / 40.0) * 0x7F + 0.5);
						curVol = tempByte;

						romPos += 2;
					}

					/*Set tempo*/
					else if (patCommand[0] == 0xFE)
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
							if (drvVers == 1)
							{
								tempo = patCommand[1] * 1.5;
							}
							else
							{
								tempo = patCommand[1] * 0.8;
							}

							if (tempo < 2)
							{
								tempo = 150;
							}
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
							ctrlMidPos += 2;
						}
						romPos += 2;
					}

					/*Set channel parameters*/
					else if (patCommand[0] == 0xFF)
					{
						if (drvVers == 1)
						{
							romPos += 4;
						}
						else
						{
							if (patCommand[0] == 0x00 && patCommand[1] == 0x00 && patCommand[2] == 0x00 && patCommand[3] == 0xFC)
							{
								songEnd = 1;
							}
							else
							{
								if (curTrack == 0)
								{
									romPos += 7;
								}
								else
								{
									romPos += 6;
								}
							}
						}

					}

					/*Unknown command*/
					else
					{
						romPos++;
					}
				}
			}
			else if (drvVers == 2)
			{
				while (songEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
				{
					patCommand[0] = romData[romPos];
					patCommand[1] = romData[romPos + 1];
					patCommand[2] = romData[romPos + 2];
					patCommand[3] = romData[romPos + 3];

					/*Play sequence*/
					if (patCommand[0] != 0xFF)
					{
						curSeq = patCommand[0];
						seqPos = ReadLE16(&romData[seqs + (curSeq * 2)]);
						seqEnd = 0;

						while (seqEnd == 0)
						{
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];

							/*End of sequence*/
							if (command[0] == 0x00)
							{
								seqEnd = 1;
							}

							/*Play note*/
							else
							{
								/*Rest*/
								if (command[1] == 0x00)
								{
									curDelay += (command[0] * 20);
									ctrlDelay += (command[0] * 20);
								}
								else
								{
									curNote = command[1] - 1;
									if (curTrack != 2)
									{
										curNote += 12;
									}
									curNoteLen = command[0] * 20;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
								}
								seqPos += 2;
							}
						}
						romPos++;
					}

					/*Set flags*/
					else if (patCommand[0] == 0xFF)
					{
						/*Set tempo*/
						if (patCommand[1] == 0x00)
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
								tempo = patCommand[2] * 1.5;
								if (tempo < 2)
								{
									tempo = 150;
								}
								WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
								ctrlMidPos += 2;
							}

							romPos += 3;
						}

						/*Set duty*/
						else if (patCommand[1] == 0x01)
						{
							romPos += 3;
						}

						/*Set envelope (channel)*/
						else if (patCommand[1] == 0x02)
						{
							romPos += 3;
						}

						/*Set global volume*/
						else if (patCommand[1] == 0x03)
						{
							romPos += 3;
						}

						/*Set repeat amount*/
						else if (patCommand[1] == 0x04)
						{
							repeatStart = activeChan[curTrack] + patCommand[2];

							/*Go to song loop*/
							if (patCommand[3] == 0x00)
							{
								songEnd = 1;
							}
							else
							{
								romPos += 3;
							}

						}

						/*Repeat*/
						else if (patCommand[1] == 0x05)
						{
							if (repeat == -1)
							{
								repeat = patCommand[2];
							}
							else if (repeat > 0)
							{
								romPos = repeatStart;
								repeat--;
							}
							else
							{
								repeat = -1;
								romPos += 3;
							}
						}

						/*Set volume/envelope*/
						else if (patCommand[1] == 0x06)
						{
							tempByte = patCommand[1] & 0x0F;

							tempByte = log(tempByte / 15.0) * 8.65617024533378;

							if (tempByte > 0.0)
							{
								tempByte = 0;
							}
							tempByte = (unsigned char)(pow(10.0, tempByte / 40.0) * 0x7F + 0.5);
							curVol = tempByte;
							romPos += 3;
						}

						/*End of song (no loop)*/
						else if (patCommand[1] == 0x07)
						{
							songEnd = 1;
						}

						else
						{
							romPos += 3;
						}

					}
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