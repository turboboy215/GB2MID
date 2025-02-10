/*Jeroen Tel*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "JEROTEL.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long patTable;
long seqTable;
long speedTable;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
int patNums[4];
long songPtrs[4];
long base;
int numSeqs;
int songTrans;
unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;
int curIns;
long midLength;

char string1[100];
char string2[100];
char JTcheckStrings[11][100] = { "bank=", "numsongs=", "audiotab=", "steptab=",  "base=", "trans=", "chn1=", "chn2=", "chn3=", "chn4=", "speedtab=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void JTsong2mid(int songNum, long ptrList[4], int songTrans);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);

void JTProc(char parameters[4][50])
{
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the bank value*/
		fgets(string1, 6, cfg);

		if (memcmp(string1, JTcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		bank = strtol(string1, NULL, 16);
		printf("Bank: %01X\n", bank);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, JTcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		numSongs = strtod(string1, NULL);
		printf("Total # of songs: %i\n", numSongs);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the "audiotab" (pattern data pointer)*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, JTcheckStrings[2], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);
		patTable = strtol(string1, NULL, 16);
		printf("Pattern table: 0x%04X\n", patTable);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the "steptab" (sequence pointer)*/
		fgets(string1, 9, cfg);

		if (memcmp(string1, JTcheckStrings[3], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);
		seqTable = strtol(string1, NULL, 16);
		printf("Sequence table: 0x%04X\n", seqTable);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the speed pointer*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, JTcheckStrings[10], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);
		speedTable = strtol(string1, NULL, 16);
		printf("Speed table: 0x%04X\n", speedTable);



		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get "base" value*/
		fgets(string1, 6, cfg);

		if (memcmp(string1, JTcheckStrings[4], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);
		base = strtol(string1, NULL, 16);
		printf("Base: 0x%04X\n", base);

		/*Copy the ROM data*/
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);

		/*Get the pointers for each song and convert*/
		for (songNum = 1; songNum <= numSongs; songNum++)
		{
			/*Skip new line*/
			fgets(string1, 2, cfg);
			/*Skip the first line*/
			fgets(string1, 10, cfg);

			/*Get transpose value*/
			fgets(string1, 7, cfg);
			if (memcmp(string1, JTcheckStrings[5], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 3, cfg);

			songTrans = (signed char)strtol(string1, NULL, 16);

			for (i = 0; i < 4; i++)
			{
				/*Skip new line*/
				fgets(string1, 2, cfg);
				/*Skip "chnX="*/
				fgets(string1, 6, cfg);

				/*Get the pattern numbers*/
				fgets(string1, 3, cfg);

				patNums[i] = strtol(string1, NULL, 16);
			}

			j = patTable - base;
			for (i = 0; i < 4; i++)
			{
				songPtrs[i] = ReadLE16(&romData[j + ((patNums[i]) * 2)]);
				printf("Song %i channel %i pattern number: %01X\n", songNum, (i + 1), patNums[i]);
				printf("Song %i channel %i pattern address: 0x%04X\n", songNum, (i + 1), songPtrs[i]);
			}
			JTsong2mid(songNum, songPtrs, songTrans);
		}
		free(romData);
	}
}

void JTsong2mid(int songNum, long ptrList[4], int songTrans)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int patPos = 0;
	int seqPos = 0;
	int seqNum = 0;
	int songEnd = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int command[3] = { 0, 0 };
	int transpose = 0;
	int curSeq = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int repeat = 0;
	int speedVals[2] = { 0, 0 };

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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			transpose = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			curNote = 0;
			curNoteLen = 0;

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

			/*Process the sequence*/
			songEnd = 0;
			patPos = ptrList[curTrack] - base;
			while (songEnd == 0)
			{
				if (romData[patPos] < 0x70)
				{
					seqNum = romData[patPos];
					curSeq = ReadLE16(&romData[(seqTable - base) + (seqNum * 2)]);
					seqEnd = 0;
					seqPos = curSeq - base;
					speedVals[0] = romData[speedTable - base + (patNums[curTrack] * 2)];
					speedVals[1] = romData[speedTable - base + (patNums[curTrack] * 2) + 1];

					while (seqEnd == 0)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];

						/*Play note*/
						if (command[0] < 0x60)
						{
							if (command[0] < 0x50)
							{
								curNote = command[0] + 24 + transpose + songTrans;
								ctrlDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curIns);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}
							else
							{
								curDelay += curNoteLen;
							}

							seqPos++;
						}

						/*Set arpeggio*/
						else if (command[0] >= 0x60 && command[0] < 0x80)
						{
							if (command[1] >= 0x60)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curIns);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}
							seqPos++;
						}

						/*Change note length*/
						else if (command[0] >= 0x80 && command[0] < 0xC0)
						{
							curNoteLen = (command[0] - 0x80) * (speedVals[0] * 5);
							if ((command[1] > 0x80) && command[1] < 0xFD)
							{
								if (curNote >= 0)
								{
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curIns);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
								}

								seqPos++;
							}

							else
							{
								seqPos++;
							}
						}

						/*Change instrument*/
						else if (command[0] >= 0xC0 && command[0] < 0xE0)
						{
							curIns = command[0] - 0xC0;
							firstNote = 1;
							if (command[1] >= 0xC0 && command[0] < 0xE0)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curIns);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}
							seqPos++;
						}

						/*Delay*/
						else if (command[0] >= 0xE0 && command[0] < 0xFD)
						{
							curDelay += ((command[0] - 0xE0) * (speedVals[0] * 5));
							seqPos++;
						}

						/*Portamento*/
						else if (command[0] == 0xFD)
						{
							tempPos = WriteDeltaTime(midData, midPos, curDelay);
							midPos += tempPos;
							Write8B(&midData[midPos], (0xE0 | curTrack));
							Write8B(&midData[midPos + 1], 0);
							Write8B(&midData[midPos + 2], 0x40);
							Write8B(&midData[midPos + 3], 0);
							curDelay = 0;
							firstNote = 1;
							midPos += 3;
							seqPos += 3;
						}

						/*Stop sequence*/
						else if (command[0] == 0xFE)
						{
							seqEnd = 1;
							patPos++;
						}

						/*End of sequence*/
						else if (command[0] == 0xFF)
						{
							seqEnd = 1;
							patPos++;
						}
					}
				}

				/*Set effect*/
				else if (romData[patPos] >= 0x70 && romData[patPos] < 0x80)
				{
					patPos++;
				}
				else if (romData[patPos] >= 0x80 && romData[patPos] < 0xFE)
				{
					transpose = romData[patPos] - 0x80;
					patPos++;
				}

				else if (romData[patPos] >= 0xFE)
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