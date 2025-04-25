/*Ed Magnin*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "EDMAGNIN.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int foundTable;
long firstPtr;
int drvVers;
int ptrOverride;
int curInst;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char MagninMagicBytes[4] = { 0xCB, 0x27, 0x81, 0x4F };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Magninsong2mid(int songNum, long ptr);

void MagninProc(int bank, char parameters[4][50])
{
	foundTable = 0;
	firstPtr = 0;
	ptrOverride = 0;
	drvVers = 1;
	curInst = 0;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	if (parameters[1][0] != 0x00)
	{
		ptrOverride = 1;
		tableOffset = strtol(parameters[1], NULL, 16);
		drvVers = strtol(parameters[0], NULL, 16);
		foundTable = 1;
	}
	else if (parameters[0][0] != 0x00)
	{
		drvVers = strtol(parameters[0], NULL, 16);
	}

	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	if (ptrOverride == 0)
	{
		/*Try to search the bank for song table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], MagninMagicBytes, 4)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i - 2;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		if (ReadLE16(&romData[i]) == 0x0000)
		{
			i += 2;
		}
		firstPtr = ReadLE16(&romData[i]);
		songNum = 1;
		while ((i + bankAmt) != firstPtr && ReadLE16(&romData[i]) <= 0x8000 && ReadLE16(&romData[i]) >= bankAmt)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0 && romData[songPtr - bankAmt] != 0)
			{
				Magninsong2mid(songNum, songPtr);
			}
			i += 2;
			songNum++;
		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}
}

/*Convert the song data to MIDI*/
void Magninsong2mid(int songNum, long ptr)
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
	int curNoteSize = 0;
	long totalTime = 0;
	long nextTime = 0;
	int transpose[4];
	unsigned char command[5];
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
	long tempPos = 0;
	int holdNote = 0;

	midPos = 0;
	ctrlMidPos = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
		ctrlMidData[j] = 0;
	}

	if (multiBanks == 0)
	{
		sprintf(outfile, "song%d.mid", songNum);
	}
	else
	{
		sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
	}
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		if (drvVers == 3)
		{
			tempo = 100;
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

		romPos = ptr - bankAmt;

		if (drvVers == 1)
		{
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}
		}
		else if (drvVers == 2 || drvVers == 3)
		{
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
				transpose[curTrack] = (signed char)romData[romPos + 2];
				romPos += 4;
			}
		}


		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

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

			if (drvVers == 1)
			{
				seqPos = ptr + seqPtrs[curTrack] - bankAmt;
			}
			else if (drvVers == 2 || drvVers == 3)
			{
				seqPos = seqPtrs[curTrack] - bankAmt;
			}

			seqEnd = 0;

			if (drvVers == 1)
			{
				if (ReadLE16(&romData[seqPos]) != 0x0000)
				{
					curDelay = ReadLE16(&romData[seqPos]) * 5;
				}
			}
			else if (drvVers == 2 || drvVers == 3)
			{
				if (romData[seqPos] != 0x00)
				{
					curDelay = romData[seqPos] * 5;
				}
			}



			while (seqEnd == 0)
			{
				if (drvVers == 1)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
					command[4] = romData[seqPos + 4];

					totalTime = ReadLE16(&romData[seqPos]);

					if (command[2] != 0)
					{
						curNote = command[2];
						nextTime = ReadLE16(&romData[seqPos + 5]);

						if (nextTime >= totalTime)
						{

							curNoteSize = command[4] / 0x10;
							if (curNoteSize == 0)
							{
								curNoteSize = 1;
							}
							curNoteSize *= 30;
							curNoteLen = (nextTime - totalTime) * 5;

							if (curNoteLen >= curNoteSize)
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteSize, curDelay, firstNote, curTrack, curInst);
								curDelay = curNoteLen - curNoteSize;
							}
							else
							{
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
							}

							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						seqPos += 5;
					}
					else if (command[2] == 0)
					{
						seqEnd = 1;
					}
				}
				else if (drvVers == 2 || drvVers == 3)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					totalTime = command[0];

					if (drvVers == 3 && curTrack != 3)
					{
						nextTime = command[3];
					}
					else if (drvVers == 2 && curTrack == 2)
					{
						nextTime = command[3];
					}

					else
					{
						nextTime = command[2];
					}

					if (command[1] != 0)
					{
						curNote = command[1] + transpose[curTrack];

						if (drvVers == 2 && curTrack != 3)
						{
							curNote += 12;
						}
						else if (drvVers == 3)
						{
							if (curTrack != 3)
							{
								curNote += 24;
							}
							else if (curTrack == 3)
							{
								curNote += 36;
							}
						}

						curNoteLen = (nextTime - totalTime) * 5;
						if (nextTime >= totalTime)
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						seqPos += 2;

						if (drvVers == 3 && curTrack != 3)
						{
							seqPos++;
						}
						else if (drvVers == 2 && curTrack == 2)
						{
							seqPos++;
						}
					}
					else if (command[1] == 0)
					{
						seqEnd = 1;
					}

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

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}