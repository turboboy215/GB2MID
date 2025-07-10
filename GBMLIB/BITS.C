/*BITS/M4 (Shahid Ahmad)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "BITS.H"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j, x;
char outfile[1000000];
int songNum;
long songPtrs[4];
int numSongs;
int songBank;
long bankAmt;
int curInst;
long firstPtr;
int drvVers;
int cfgPtr;
int exitError;
int fileExit;
int curVols[4];
long tempoMap;
long bankMap;
int songTempo;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* multiMidData[4];
unsigned char* ctrlMidData;

long midLength;

char* argv3;
char string1[100];
char string2[100];
char BITScheckStrings[6][100] = { "masterBank=", "numSongs=", "format=", "songList=", "tempoMap=", "bankMap=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void BITSsong2mid(int songNum, long ptrs[4]);

void BITSProc(int bank, char parameters[4][50])
{
	curInst = 0;
	firstPtr = 0;
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the master bank*/
		fgets(string1, 12, cfg);
		if (memcmp(string1, BITScheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		masterBank = strtol(string1, NULL, 16);
		fgets(string1, 10, cfg);
		printf("Master bank: %02X\n", masterBank);


		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);

		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, BITScheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtol(string1, NULL, 16);

		printf("Number of songs: %i\n", numSongs);
		fgets(string1, 3, cfg);

		/*Get version number*/
		fgets(string1, 8, cfg);
		if (memcmp(string1, BITScheckStrings[2], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		drvVers = strtol(string1, NULL, 16);

		printf("Version: %i\n", drvVers);
		if (drvVers != 1 && drvVers != 2)
		{
			printf("ERROR: Invalid version number!\n");
			exit(2);
		}
		fgets(string1, 3, cfg);

		/*Get song list*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, BITScheckStrings[3], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);
		tableOffset = strtol(string1, NULL, 16);

		printf("Song list: 0x%04X\n", tableOffset);
		fgets(string1, 3, cfg);

		/*Get tempo mapping*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, BITScheckStrings[4], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);
		tempoMap = strtol(string1, NULL, 16);

		printf("Tempo mapping: 0x%04X\n", tempoMap);

		if (drvVers != 1)
		{
			fgets(string1, 3, cfg);

			/*Get bank mapping*/
			fgets(string1, 9, cfg);
			if (memcmp(string1, BITScheckStrings[5], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 5, cfg);
			bankMap = strtol(string1, NULL, 16);
			printf("Bank mapping: 0x%04X\n", bankMap);
		}

		songNum = 1;
		i = tableOffset;

		if (drvVers == 1)
		{
			while (songNum <= numSongs)
			{
				songTempo = romData[tempoMap + (songNum - 1)];
				songBank = masterBank;
				songPtrs[0] = ReadLE16(&romData[i]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				songPtrs[2] = ReadLE16(&romData[i + 4]);
				songPtrs[3] = ReadLE16(&romData[i + 6]);

				printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
				printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
				printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
				printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				BITSsong2mid(songNum, songPtrs);

				i += 8;
				songNum++;
			}
		}
		else
		{
			while (songNum <= numSongs)
			{
				songTempo = romData[tempoMap + (songNum - 1)];
				songBank = romData[bankMap + (songNum - 1)] + 1;
				songPtrs[0] = ReadLE16(&romData[i]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				songPtrs[2] = ReadLE16(&romData[i + 4]);
				songPtrs[3] = ReadLE16(&romData[i + 6]);

				printf("Song %i channel 1: 0x%04X (bank %02X)\n", songNum, songPtrs[0], songBank);
				printf("Song %i channel 2: 0x%04X (bank %02X)\n", songNum, songPtrs[1], songBank);
				printf("Song %i channel 3: 0x%04X (bank %02X)\n", songNum, songPtrs[2], songBank);
				printf("Song %i channel 4: 0x%04X (bank %02X)\n", songNum, songPtrs[3], songBank);

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				BITSsong2mid(songNum, songPtrs);

				i += 8;
				songNum++;


			}
		}
	}


}

/*Convert the song data to MIDI*/
void BITSsong2mid(int songNum, long ptrs[4])
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
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int octave = 0;
	int transpose = 0;
	unsigned char command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	long trackSize = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	int patMode = 0;
	long curPat = 0;
	int patEnd = 0;
	long patPos = 0;

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

		tempo = songTempo * 30;

		if (tempo < 2)
		{
			tempo = 150;
		}

		/*Set initial tempo*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
		ctrlMidPos += 3;

		/*Set key signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5902);
		ctrlMidPos += 4;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;

			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;
			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			transpose = 0;
			patMode = 0;

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

			if (songPtrs[curTrack] == 0x0000)
			{
				seqEnd = 1;
			}

			seqPos = songPtrs[curTrack];

			while (seqEnd == 0)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];

				/*Play note*/
				if (command[0] <= 0x7F)
				{
					curNote = command[0] + 24 + transpose;

					seqPos++;

					if (exRomData[seqPos] < 0x80)
					{
						curNoteLen = exRomData[seqPos];
						seqPos++;
					}
					else
					{
						curNoteLen = exRomData[seqPos + 1] + ((exRomData[seqPos] - 0x80) * 0x100);
						seqPos += 2;
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;

					if (exRomData[seqPos] < 0x80)
					{
						curDelay = exRomData[seqPos];
						seqPos++;
					}
					else
					{
						curDelay = ((exRomData[seqPos + 1] + ((exRomData[seqPos] - 0x80) * 0x100)));
						seqPos += 2;
					}
				}

				/*Command 80 (no effect)*/
				else if (command[0] == 0x80)
				{
					seqPos++;
				}

				/*Go to macro*/
				else if (command[0] == 0x81)
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&exRomData[seqPos + 1]);
						seqPos = macro1Pos;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Set instrument*/
				else if (command[0] == 0x82)
				{
					curInst = command[1];

					if (curInst > 127)
					{
						curInst = command[1] - 127;
					}
					firstNote = 1;
					seqPos += 2;
				}

				/*End of channel track*/
				else if (command[0] == 0x83)
				{
					if (patMode == 1)
					{
						patPos += 2;
						curPat = ReadLE16(&exRomData[patPos]);
						if (curPat == 0x0000)
						{
							seqEnd = 1;
						}
						seqPos = curPat;
					}
					else
					{
						seqEnd = 1;
					}

				}

				/*End of channel track (loop)*/
				else if (command[0] == 0x84)
				{
					if (patMode == 1)
					{
						patPos += 2;
						curPat = ReadLE16(&exRomData[patPos]);
						if (curPat == 0x0000)
						{
							seqEnd = 1;
						}
						seqPos = curPat;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*End of channel track (no loop)*/
				else if (command[0] == 0x85)
				{
					if (patMode == 1)
					{
						patPos += 2;
						curPat = ReadLE16(&exRomData[patPos]);
						if (curPat == 0x0000)
						{
							seqEnd = 1;
						}
						seqPos = curPat;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Set delay*/
				else if (command[0] == 0x86)
				{
					if (command[1] < 0x80)
					{
						curNoteLen = command[1];
						seqPos += 2;
					}
					else
					{
						curNoteLen = command[2] + ((command[1] - 0x80) * 0x100);
						seqPos += 3;
					}
					curDelay += curNoteLen;
				}

				/*Set waveform*/
				else if (command[0] == 0x87)
				{
					seqPos += 3;
				}

				/*Restart song*/
				else if (command[0] == 0x88)
				{
					seqEnd = 1;
				}

				/*Set transpose*/
				else if (command[0] == 0x89)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				/*Set envelope*/
				else if (command[0] == 0x8A)
				{
					seqPos += 2;
				}

				/*Set panning?*/
				else if (command[0] == 0x8B)
				{
					seqPos += 2;
				}

				/*Reset panning?*/
				else if (command[0] == 0x8C)
				{
					seqPos++;
				}

				/*Start of song loop?*/
				else if (command[0] == 0x8D)
				{
					seqPos++;
				}

				/*Return from macro*/
				else if (command[0] == 0x8E)
				{
					if (inMacro == 1)
					{
						seqPos = macro1Ret;
						inMacro = 0;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Enter "pattern mode"*/
				else if (command[0] == 0x8F)
				{
					patMode = 1;
					patPos = seqPos + 1;
					curPat = ReadLE16(&exRomData[patPos]);
					if (curPat == 0x0000)
					{
						seqEnd = 1;
					}
					seqPos = curPat;
				}

				/*Exit pattern*/
				else if (command[0] == 0x90)
				{
					if (patMode == 1)
					{
						patPos += 2;
						curPat = ReadLE16(&exRomData[patPos]);
						if (curPat == 0x0000)
						{
							seqEnd = 1;
						}
						seqPos = curPat;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Unknown command 91*/
				else if (command[0] == 0x91)
				{
					patMode = 0;
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
		free(exRomData);
		fclose(mid);
	}
}