/*UGB Player (by Thalamus/Jon Wells)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "UGB.H"

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
int curVol;
int masterBank;
int songBank;
int songIndex;
int speed1;
int speed2;
int totalSpeed;
unsigned int mask;
int copyMethod;
long copyAddr;
long testPos;
long seqTable;

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
char UGBcheckStrings[10][100] = { "version=", "numSongs=", "bank=", "seqTable", "index=", "speed1=", "speed2=", "mask=", "copy=", "masterBank=" };

unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned static char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned static char* buffer, unsigned int pos, unsigned int value);
void UGBsong2mid(int songNum);

void UGBProc(char parameters[4][100])
{
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the driver version value*/
		fgets(string1, 9, cfg);
		if (memcmp(string1, UGBcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		drvVers = strtod(string1, NULL);

		if (drvVers != UGB_VER_COMMON && drvVers != UGB_VER_ALT)
		{
			printf("ERROR: Invalid version number!\n");
			exit(2);
		}

		fgets(string1, 3, cfg);
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, UGBcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtod(string1, NULL);
		songNum = 1;

		fgets(string1, 3, cfg);

		/*Get the master bank number*/
		fgets(string1, 12, cfg);
		if (memcmp(string1, UGBcheckStrings[9], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);

		masterBank = strtol(string1, NULL, 16);

		fgets(string1, 3, cfg);

		/*Get the song bank number*/
		fgets(string1, 6, cfg);
		if (memcmp(string1, UGBcheckStrings[2], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);

		songBank = strtol(string1, NULL, 16);

		fgets(string1, 3, cfg);

		/*Get the sequence table offset*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, UGBcheckStrings[3], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);
		seqTable = strtol(string1, NULL, 16);

		fgets(string1, 3, cfg);

		printf("Version: %01X\n", drvVers);
		printf("Number of songs: %i\n", numSongs);
		printf("Bank: %02X\n", songBank);
		printf("Sequence table: 0x%04X\n", seqTable);

		while (songNum <= numSongs)
		{
			/*Skip the first line*/
			fgets(string1, 10, cfg);

			/*Get the song index*/
			fgets(string1, 7, cfg);
			if (memcmp(string1, UGBcheckStrings[4], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}

			fgets(string1, 3, cfg);
			songIndex = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get speed value 1*/
			fgets(string1, 8, cfg);
			if (memcmp(string1, UGBcheckStrings[5], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}

			fgets(string1, 3, cfg);
			speed1 = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get speed value 2*/
			fgets(string1, 8, cfg);
			if (memcmp(string1, UGBcheckStrings[6], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}

			fgets(string1, 3, cfg);
			speed2 = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get channel mask value*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, UGBcheckStrings[7], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}

			fgets(string1, 3, cfg);
			mask = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get copy method*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, UGBcheckStrings[8], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}

			fgets(string1, 3, cfg);
			copyMethod = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			printf("Song %i: index: %i, speed 1: %i, speed 2: %i, mask: %i, copy method: %i\n", songNum, songIndex, speed1, speed2, mask, copyMethod);

			if (songBank < 0x02)
			{
				songBank = 0x02;
			}

			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 4);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);

			/*For International Karate, copy the song data into RAM as necessary*/
			if (copyMethod != 0)
			{
				if (copyMethod == 1)
				{
					copyAddr = 0x4000;
				}
				else if (copyMethod == 2)
				{
					copyAddr = 0x4CC0;
				}
				else if (copyMethod == 3)
				{
					copyAddr = 0x5FC0;
				}

				/*
				testPos = ((songBank - 1) * bankSize + (copyAddr - bankSize));
				*/
				fseek(rom, ((songBank - 1) * bankSize + (copyAddr - bankSize)), SEEK_SET);
				fread(romData + 0xD000, 1, 0x2000, rom);
			}
			UGBsong2mid(songNum);
			free(romData);

			songNum++;
		}
	}
}

/*Convert the song data to MIDI*/
void UGBsong2mid(int songNum)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int maskArray[4];
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int numTracks = 0;
	int ticks = 120;
	int tempo = 150;
	int chanSpeed;
	int k = 0;
	int songEnd = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int transpose = 0;
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
	int seqNum = 0;
	long seqPtr = 0;
	int seqTimes = 0;
	int holdNote = 0;
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

	for (j = 0; j < 4; j++)
	{
		seqPtrs[j] = 0x0000;
	}

	/*Get the song channel pointers*/
	if (copyMethod == 0)
	{
		romPos = 0x6000 + (songIndex * 8);
	}
	else
	{
		romPos = 0xD000 + (songIndex * 8);
	}

	if (drvVers == 2)
	{
		romPos = 0x567E + (songIndex * 8);
	}

	if (drvVers != 2)
	{
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			printf("Song %i channel %i: 0x%04X\n", songNum, (curTrack + 1), seqPtrs[curTrack]);
			romPos += 2;
		}
	}
	else
	{
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			if (curTrack == 2)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos + 2]);
				printf("Song %i channel %i: 0x%04X\n", songNum, (curTrack + 1), seqPtrs[curTrack]);
				romPos += 2;
			}
			else if (curTrack == 3)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos - 2]);
				printf("Song %i channel %i: 0x%04X\n", songNum, (curTrack + 1), seqPtrs[curTrack]);
				romPos += 2;
			}
			else
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
				printf("Song %i channel %i: 0x%04X\n", songNum, (curTrack + 1), seqPtrs[curTrack]);
				romPos += 2;
			}
		}
	}


	/*Get channel mask*/

	maskArray[3] = mask >> 3 & 1;
	maskArray[2] = mask >> 2 & 1;
	maskArray[1] = mask >> 1 & 1;
	maskArray[0] = mask & 1;


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

		switch (speed1)
		{
		case 0x00:
			tempo = 800;
			break;
		case 0x01:
			tempo = 500;
			break;
		case 0x02:
			tempo = 300;
			break;
		case 0x03:
			tempo = 250;
			break;
		case 0x04:
			tempo = 100;
			break;
		case 0x05:
			tempo = 70;
			break;
		case 0x06:
			tempo = 60;
			break;
		default:
			tempo = 60;
		}

		tempo += (speed2) * 10;

		if (tempo == 0)
		{
			tempo = 150;
		}

		if (speed2 == 0)
		{
			tempo *= 2;
		}
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
			firstNote = 0;
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

			songEnd = 0;
			seqEnd = 0;
			romPos = seqPtrs[curTrack];

			UGB_STATUS_NOTE_MIN = 0x00;
			UGB_STATUS_NOTE_MAX = 0xF9;
			UGB_TRACK_EVENT_STOP = 0xFE;
			UGB_TRACK_EVENT_RESTART = 0xFF;

			switch (drvVers)
			{
			case UGB_VER_COMMON:
				/*Fall-through*/
			case UGB_VER_ALT:
			default:
				EventMap[0xFA] = UGB_EVENT_PITCH_BEND;
				EventMap[0xFB] = UGB_EVENT_PROG_CHANGE;
				EventMap[0xFC] = UGB_EVENT_TIE;
				EventMap[0xFD] = UGB_EVENT_REST;
				EventMap[0xFE] = UGB_EVENT_STOP;
			}

			if (maskArray[curTrack] == 1)
			{
				songEnd = 0;
			}
			else
			{
				songEnd = 1;
			}

			while (songEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				if (romData[romPos] < UGB_TRACK_EVENT_STOP)
				{
					seqNum = romData[romPos];
					seqTimes = (romData[romPos + 1]) & 0x0F;
					transpose = (romData[romPos + 1] >> 4);
					seqPtr = ReadLE16(&romData[seqTable + (seqNum * 2)]);
					seqPos = seqPtr;
					seqEnd = 0;

					while (seqEnd == 0)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];

						if (curNote > 127)
						{
							curNote = 0;
						}

						if (command[0] >= UGB_STATUS_NOTE_MIN && command[0] <= UGB_STATUS_NOTE_MAX)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}

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
							curNote = (command[0] / 2) + 24 + transpose;
							if (curTrack < 2)
							{
								curNote += 12;
							}
							curNoteLen = ((command[1] + 1) * 20);
							tempPos = WriteNoteEventOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 1;
							midPos = tempPos;
							curDelay = curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos += 2;
						}

						else if (EventMap[command[0]] == UGB_EVENT_PITCH_BEND)
						{
							pitchBend = 1;
							seqPos += 3;
						}

						else if (EventMap[command[0]] == UGB_EVENT_PROG_CHANGE)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							curInst = command[1];
							firstNote = 1;
							seqPos += 2;
						}

						else if (EventMap[command[0]] == UGB_EVENT_TIE)
						{
							curDelay += ((command[1] + 1) * 20);
							ctrlDelay += ((command[1] + 1) * 20);
							masterDelay += ((command[1] + 1) * 20);
							seqPos += 2;
						}

						else if (EventMap[command[0]] == UGB_EVENT_REST)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							curDelay += ((command[1] + 1) * 20);
							ctrlDelay += ((command[1] + 1) * 20);
							masterDelay += ((command[1] + 1) * 20);
							seqPos += 2;
						}

						else if (EventMap[command[0]] == UGB_EVENT_STOP)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							if (seqTimes > 0)
							{
								seqTimes--;
								seqPos = seqPtr;
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
					romPos += 2;
				}

				else if (romData[romPos] == UGB_TRACK_EVENT_STOP)
				{
					songEnd = 1;
				}

				else if (romData[romPos] == UGB_TRACK_EVENT_RESTART)
				{
					songEnd = 1;
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