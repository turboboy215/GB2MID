/*Pandora Box*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "PBOX.H"
#include "PBDEC.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long songBank;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
long songPtr;
long seqPtrs[4];
long songEnd;
long songSize;
int cfgPtr = 0;
int exitError = 0;
int fileExit = 0;
int curTrack;
int drvVers;
int curVol;
int curInst;
int ptrFmt;
int compressed;
int cmpMethod;

char string1[100];
char string2[100];
char PBcheckStrings[6][100] = { "numSongs=", "format=", "bank=", "start=", "end=", "comp=" };
unsigned char* romData;
unsigned char* songData;
unsigned char* compData;
unsigned char* cfgData;
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
void PBcopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd);
void PBsong2mid(int songNum);

void PBProc(int bank, char parameters[4][100])
{
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, PBcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		numSongs = strtol(string1, NULL, 16);
		printf("Total # of songs: %i\n", numSongs);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the format number*/
		fgets(string1, 8, cfg);

		if (memcmp(string1, PBcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		drvVers = strtod(string1, NULL);
		/*printf("Format: %i\n", drvVers);*/

		songNum = 1;

		while (fileExit == 0 && exitError == 0)
		{
			if (songNum > numSongs)
			{
				fileExit = 1;
			}
			if (fileExit == 0)
			{
				/*Skip new line*/
				fgets(string1, 2, cfg);
				/*Skip the first line*/
				fgets(string1, 12, cfg);

				/*Now look for the "bank"*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, PBcheckStrings[2], 1))
				{
					exitError = 1;
				}
				fgets(string1, 5, cfg);
				songBank = strtol(string1, NULL, 16);

				if (songBank == 1)
				{
					fseek(rom, 0, SEEK_SET);
					romData = (unsigned char*)malloc(bankSize * 2);
					fread(romData, 1, bankSize, rom);
					fseek(rom, (1 * bankSize), SEEK_SET);
					fread(romData + bankSize, 1, bankSize, rom);
				}
				else
				{
					fseek(rom, 0, SEEK_SET);
					romData = (unsigned char*)malloc(bankSize * 2);
					fread(romData, 1, bankSize, rom);
					fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
					fread(romData + bankSize, 1, bankSize, rom);
				}

				/*Skip new line*/
				fgets(string1, 2, cfg);

				/*Now look for the start of the song*/
				fgets(string1, 7, cfg);
				if (memcmp(string1, PBcheckStrings[3], 1))
				{
					exitError = 1;
				}
				fgets(string1, 5, cfg);
				songPtr = strtol(string1, NULL, 16);

				/*Skip new line*/
				fgets(string1, 2, cfg);

				/*Now look for the end of the song*/
				fgets(string1, 5, cfg);
				if (memcmp(string1, PBcheckStrings[4], 1))
				{
					exitError = 1;
				}
				fgets(string1, 5, cfg);
				songEnd = strtol(string1, NULL, 16);

				/*Skip new line*/
				fgets(string1, 2, cfg);

				/*Now check for the compression method*/
				/*0 = Uncompressed, 1 = Oni 4, 2 = Oni 5*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, PBcheckStrings[5], 1))
				{
					exitError = 1;
				}
				fgets(string1, 5, cfg);
				compressed = strtol(string1, NULL, 16);

				if (compressed == 0)
				{
					PBcopyData(romData, songData, songPtr, songEnd);
					songSize = songEnd - songPtr;
				}
				else
				{
					PBcopyData(romData, compData, songPtr, songEnd);
					songSize = songEnd - songPtr;
					PBDecomp(compData, songData, songSize, compressed);
					free(compData);
				}



				printf("Song %i: 0x%04X (size: 0x%04X, bank %02X), compression: %i\n", songNum, songPtr, songSize, songBank, compressed);
				PBsong2mid(songNum);
				songNum++;

			}
		}
	}
}

void PBcopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd)
{
	int k = dataStart;
	int l = 0;

	if (compressed == 0)
	{
		songSize = dataEnd - dataStart;
		songData = (unsigned char*)malloc(songSize);

		while (k < dataEnd)
		{
			songData[l] = romData[k];
			k++;
			l++;
		}
	}
	else
	{
		songSize = dataEnd - dataStart;
		compData = (unsigned char*)malloc(bankSize);
		songData = (unsigned char*)malloc(bankSize);

		while (k < dataEnd)
		{
			compData[l] = romData[k];
			k++;
			l++;
		}
	}
	free(romData);
}

/*Convert the song data to MIDI*/
void PBsong2mid(int songNum)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
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
	long jumpPos1 = 0;
	long jumpPosRet1 = 0;
	long jumpPos2 = 0;
	long jumpPosRet2 = 0;
	unsigned char command[5];
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
	int repeat1 = 0;
	long repeat1Start = 0x0000;
	int repeat2 = 0;
	long repeat2Start = 0x0000;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int repeats[500][2];
	int repeatNum;
	int jumpAmt = 0;
	int noise = 0;


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

		romPos = 0;
		/*Get the channel pointers*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&songData[romPos]);
			romPos += 2;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			for (k = 0; k < 500; k++)
			{
				repeats[k][0] = -1;
				repeats[k][1] = -1;
			}
			seqPos = seqPtrs[curTrack];
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			songLoopAmt = 0;
			songLoopPt = 0;
			repeatNum = 0;
			k = 0;
			noise = 0;

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

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = songData[seqPos];
				command[1] = songData[seqPos + 1];
				command[2] = songData[seqPos + 2];
				command[3] = songData[seqPos + 3];
				command[4] = songData[seqPos + 4];

				/*Play note*/
				if (command[0] <= 0x53)
				{
					curNote = command[0] + 24;
					if (curTrack == 2)
					{
						curNote -= 12;
					}
					curNoteLen = (command[1] + 1) * 5;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos += 2;
				}

				/*Rest*/
				else if (command[0] == 0x54)
				{
					curDelay += ((command[1] + 1) * 5);
					ctrlDelay += ((command[1] + 1) * 5);
					seqPos += 2;
				}

				/*Drum/percussion note*/
				else if (command[0] == 0x55)
				{
					curNote = 24 + noise;
					curNoteLen = (command[1] + 1) * 5;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos += 2;
				}

				/*Hold note*/
				else if (command[0] >= 0x80 && command[0] <= 0xD4)
				{
					curNote = command[0] - 0x80 + 24;
					if (curTrack == 2)
					{
						curNote -= 12;
					}
					curNoteLen = (command[1] + 1) * 5;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos += 2;
				}

				/*Drum "cymbal" note*/
				else if (command[0] == 0xD5)
				{
					curNote = 48;
					curNoteLen = (command[1] + 1) * 5;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos += 2;
				}

				/*Set duty: 00*/
				else if (command[0] == 0xE8)
				{
					seqPos++;
				}

				/*Set duty: 40*/
				else if (command[0] == 0xE9)
				{
					seqPos++;
				}

				/*Set duty: 80*/
				else if (command[0] == 0xEA)
				{
					seqPos++;
				}

				/*Set duty: C0*/
				else if (command[0] == 0xEB)
				{
					seqPos++;
				}

				/*Manually set volume: 0*/
				else if (command[0] == 0xEC)
				{
					seqPos++;
				}

				/*Manually set volume: 1*/
				else if (command[0] == 0xED)
				{
					seqPos++;
				}

				/*Manually set volume: 2*/
				else if (command[0] == 0xEE)
				{
					seqPos++;
				}

				/*Manually set volume: 3*/
				else if (command[0] == 0xEF)
				{
					seqPos++;
				}

				/*Set waveform*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				/*Set volume*/
				else if (command[0] == 0xF1)
				{
					seqPos += 2;
				}

				/*Set note size*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				/*Set tuning*/
				else if (command[0] == 0xF3)
				{
					seqPos += 2;
				}

				/*Set noise*/
				else if (command[0] == 0xF4)
				{
					if (noise > 127)
					{
						noise -= 128;
					}
					noise = command[1];
					seqPos += 2;
				}

				/*Set tempo*/
				else if (command[0] == 0xF5)
				{
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;

					if (command[1] <= 0x2F)
					{
						tempo = 40;
					}
					else if (command[1] >= 0x30 && command[1] <= 0x3F)
					{
						tempo = 50;
					}
					else if (command[1] >= 0x40 && command[1] <= 0x4F)
					{
						tempo = 55;
					}
					else if (command[1] >= 0x50 && command[1] <= 0x5F)
					{
						tempo = 60;
					}
					else if (command[1] >= 0x60 && command[1] <= 0x6F)
					{
						tempo = 65;
					}
					else if (command[1] >= 0x70 && command[1] <= 0x7F)
					{
						tempo = 70;
					}
					else if (command[1] >= 0x80 && command[1] <= 0x8F)
					{
						tempo = 80;
					}
					else if (command[1] >= 0x90 && command[1] <= 0x9F)
					{
						tempo = 90;
					}
					else if (command[1] >= 0xA0 && command[1] <= 0xA8)
					{
						tempo = 110;
					}
					else if (command[1] >= 0xA9 && command[1] <= 0xAF)
					{
						tempo = 120;
					}
					else if (command[1] >= 0xB0 && command[1] <= 0xB8)
					{
						tempo = 140;
					}
					else if (command[1] >= 0xB9 && command[1] <= 0xBF)
					{
						tempo = 150;
					}
					else if (command[1] >= 0xC0 && command[1] <= 0xC8)
					{
						tempo = 170;
					}
					else if (command[1] >= 0xC9 && command[1] <= 0xCF)
					{
						tempo = 200;
					}
					else if (command[1] >= 0xD0 && command[1] <= 0xD8)
					{
						tempo = 240;
					}
					else if (command[1] >= 0xD9 && command[1] <= 0xDF)
					{
						tempo = 280;
					}
					else if (command[1] >= 0xE0 && command[1] <= 0xE8)
					{
						tempo = 350;
					}
					else
					{
						tempo = 420;
					}

					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;
					seqPos += 2;
				}

				/*Repeat section*/
				else if (command[0] == 0xF6)
				{
					if (songNum == 6 && curTrack == 2 && seqPos == 0x216)
					{
						curTrack = 2;
					}
					jumpAmt = ReadLE16(&songData[seqPos + 2]);
					if (repeats[repeatNum][0] == seqPos)
					{
						if (repeats[repeatNum][1] == -1)
						{
							repeats[repeatNum][1] = command[1];
						}
						else if (repeats[repeatNum][1] > 1)
						{
							repeats[repeatNum][1]--;
							seqPos = (seqPos + 4) - jumpAmt;
						}
						else
						{
							repeats[repeatNum][0] = -1;
							repeats[repeatNum][1] = -1;
							seqPos += 4;

							if (repeatNum > 0)
							{
								repeatNum--;
							}
						}
					}
					else
					{
						repeatNum++;
						repeats[repeatNum][0] = seqPos;
						repeats[repeatNum][1] = -1;
					}
				}

				/*Set portamento?*/
				else if (command[0] == 0xF7)
				{
					if (command[1] == 0x00)
					{
						seqPos += 4;
					}
					else
					{
						seqPos += 5;
					}

				}

				/*Set envelope*/
				else if (command[0] == 0xF9)
				{
					seqPos += 2;
				}

				/*Set waveform volume*/
				else if (command[0] == 0xFA)
				{
					seqPos += 2;
				}

				/*Enable sweep*/
				else if (command[0] == 0xFB)
				{
					seqPos++;
				}

				/*Disable sweep*/
				else if (command[0] == 0xFC)
				{
					seqPos++;
				}

				/*Jump to repeat position on final loop*/
				else if (command[0] == 0xFD)
				{
					if (repeats[1][1] == 1)
					{
						seqPos = repeats[1][0];
					}
					else
					{
						seqPos++;
					}
				}

				/*Reset volume?*/
				else if (command[0] == 0xFE)
				{
					seqPos++;
				}

				/*End of channel/Jump back to position*/
				else if (command[0] == 0xFF)
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
		free(songData);
		fclose(mid);
	}
}