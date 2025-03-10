/*Digital Eclipse (Jeremy Sikas)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DE1.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long songBank;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
long songPtr;
int cfgPtr;
int exitError;
int fileExit;
int curTrack;
int version;
long songStart;
unsigned int DE1curInst[4];

char string1[100];
char string2[100];
char DE1checkStrings[4][100] = { "numSongs=", "version=", "bank=", "start=" };
unsigned char* romData;
unsigned char* DE1midData[4];
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
void DE1song2mid(int songNum, long songPtr);

void DE1Proc(int bank, char parameters[4][50])
{
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;
	version = 1;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, DE1checkStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		numSongs = strtod(string1, NULL);
		printf("Total # of songs: %i\n", numSongs);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the version number*/
		fgets(string1, 9, cfg);

		if (memcmp(string1, DE1checkStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 4, cfg);
		version = strtod(string1, NULL);
		printf("Version: %i\n", version);
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
				fgets(string1, 10, cfg);

				/*Now look for the "bank"*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, DE1checkStrings[2], 1))
				{
					exitError = 1;
				}
				fgets(string1, 5, cfg);
				songBank = strtol(string1, NULL, 16);

				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				romData = (unsigned char*)malloc(bankSize);
				fread(romData, 1, bankSize, rom);

				/*Skip new line*/
				fgets(string1, 2, cfg);

				/*Now look for the start of the song*/
				fgets(string1, 7, cfg);
				if (memcmp(string1, DE1checkStrings[3], 1))
				{
					exitError = 1;
				}

				fgets(string1, 5, cfg);
				songStart = strtol(string1, NULL, 16);

				/*Display the information*/
				printf("Song %i bank: %01X, start: 0x%04X\n", songNum, songBank, songStart);
				DE1song2mid(songNum, songStart);
				songNum++;
			}
		}
	}
}

void DE1song2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	int curDelay[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int curNote[4];
	int curNoteLen[4];
	int rowTime = 0;
	int seqEnd = 0;
	int k = 0;
	int curTrack = 0;
	unsigned int command[3];
	int firstNote[16];
	int midPos[4];
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	long tempPos = 0;
	int trackSize[4];
	int ctrlTrackSize = 0;
	int onOff[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int valSize = 0;


	midLength = 0x10000;
	for (j = 0; j < 4; j++)
	{
		DE1midData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < 4; k++)
		{
			DE1midData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	for (k = 0; k < 4; k++)
	{
		onOff[k] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		seqPos = songPtr - bankSize;
		seqEnd = 0;

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

		for (k = 0; k < 4; k++)
		{
			DE1curInst[k] = 0;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPos[curTrack] = 0;
			firstNote[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&DE1midData[curTrack][midPos[curTrack]], 0x4D54726B);
			midPos[curTrack] += 8;
			curNoteLen[curTrack] = 0;
			curDelay[curTrack] = 0;
			seqEnd = 0;
			midTrackBase = midPos[curTrack];

			/*Add track header*/
			valSize = WriteDeltaTime(DE1midData[curTrack], midPos[curTrack], 0);
			midPos[curTrack] += valSize;
			WriteBE16(&DE1midData[curTrack][midPos[curTrack]], 0xFF03);
			midPos[curTrack] += 2;
			Write8B(&DE1midData[curTrack][midPos[curTrack]], strlen(TRK_NAMES[curTrack]));
			midPos[curTrack]++;
			sprintf((char*)&DE1midData[curTrack][midPos[curTrack]], TRK_NAMES[curTrack]);
			midPos[curTrack] += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&DE1midData[curTrack][midTrackBase - 2], trackSize[curTrack]);
		}

		/*Convert the sequence data*/
		while (seqEnd == 0)
		{
			command[0] = romData[seqPos];
			command[1] = romData[seqPos + 1];
			command[2] = romData[seqPos + 2];

			/*Row time*/
			if (command[0] < 0x80)
			{
				rowTime = (command[0] + 1) * 5;

				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					if (onOff[curTrack] == 1)
					{
						curNoteLen[curTrack] += rowTime;
					}
					else
					{
						curDelay[curTrack] += rowTime;
					}
				}
				seqPos++;
			}

			/*Note off*/
			else if (command[0] >= 0x80 && command[0] <= 0x8F)
			{
				highNibble = (command[0] & 15);
				curTrack = highNibble;
				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEvent(DE1midData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, DE1curInst[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				seqPos++;
			}

			/*Note on/Play note*/
			else if (command[0] >= 0x90 && command[0] <= 0x9F)
			{
				highNibble = (command[0] & 15);
				curTrack = highNibble;

				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEvent(DE1midData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, DE1curInst[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				else
				{
					curNote[curTrack] = command[1];
					onOff[curTrack] = 1;
					curNoteLen[curTrack] = 0;

					if (version == 1)
					{
						seqPos += 2;
					}
					else
					{
						seqPos += 3;
					}

				}
			}

			/*Play sample*/
			else if (command[0] >= 0xA0 && command[0] <= 0xAF)
			{
				curTrack = 2;

				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEvent(DE1midData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, DE1curInst[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				else
				{
					if (command[1] < 0x80)
					{
						curNote[curTrack] = command[1];
					}
					else
					{
						curNote[curTrack] = command[1] - 0x80;
					}

					onOff[curTrack] = 1;
					curNoteLen[curTrack] = 0;

					seqPos += 2;

				}
			}

			/*Unknown command Bx*/
			else if (command[0] >= 0xB0 && command[0] <= 0xBF)
			{
				seqPos++;
			}

			/*Set instrument*/
			else if (command[0] >= 0xC0 && command[0] <= 0xCF)
			{
				highNibble = (command[0] & 15);
				curTrack = highNibble;
				if (DE1curInst[curTrack] != command[1])
				{
					DE1curInst[curTrack] = command[1];
					firstNote[curTrack] = 1;
				}

				seqPos += 2;
			}

			/*Unknown command Dx*/
			else if (command[0] >= 0xD0 && command[0] <= 0xDF)
			{
				seqPos++;
			}

			/*End of track (go to loop)*/
			else if (command[0] >= 0xE0 && command[0] <= 0xEF)
			{
				curTrack = 2;
				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEvent(DE1midData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, DE1curInst[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				seqEnd = 1;
			}

			/*End of track (no loop)*/
			else if (command[0] >= 0xF0 && command[0] <= 0xFF)
			{
				curTrack = 2;
				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEvent(DE1midData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, DE1curInst[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				seqEnd = 1;
			}
		}

		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			/*WriteDeltaTime(DE1midData[curTrack], midPos[curTrack], 0);
			midPos[curTrack]++;*/
			WriteBE32(&DE1midData[curTrack][midPos[curTrack]], 0xFF2F00);
			midPos[curTrack] += 4;
			firstNote[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&DE1midData[curTrack][midTrackBase - 2], trackSize[curTrack]);
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
			fwrite(DE1midData[curTrack], midPos[curTrack], 1, mid);
		}

		free(DE1midData[0]);
		free(romData);
		fclose(mid);
	}
}