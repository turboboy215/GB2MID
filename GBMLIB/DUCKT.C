/*Capcom (early - DuckTales 1)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DUCKT.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
int curVol;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char DuckMagicBytes[3] = { 0x79, 0x3D, 0x21 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Ducksong2mid(int songNum, long ptr);

void DuckProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	curVol = 100;
	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], DuckMagicBytes, 3)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 3;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;

		while (songNum <= 11)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			Ducksong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
		free(romData);
	}
}

void Ducksong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	long romPos = 0;
	long seqPos = 0;
	unsigned int midPos = 0;
	long songPtrs[4];
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	int ticks = 120;
	long tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int freq = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curVol = 0;
	int subVal = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int dotted = 0;
	int valSize = 0;
	long tempPos = 0;
	long trackSize = 0;
	int firstNote = 1;
	int override = 0;
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

	sprintf(outfile, "song%i.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
		exit(2);
	}
	else
	{
		romPos = songPtr - bankAmt;

		if (romData[romPos] == 0x01)
		{
			tempoVal = 35;
		}
		else if (romData[romPos] == 0x02)
		{
			tempoVal = 170;
		}
		else if (romData[romPos] == 0x03)
		{
			tempoVal = 200;
		}
		else if (romData[romPos] == 0x04)
		{
			tempoVal = 230;
		}
		else if (romData[romPos] == 0x05)
		{
			tempoVal = 245;
		}
		else if (romData[romPos] == 0x06)
		{
			tempoVal = 260;
		}
		else if (romData[romPos] == 0x07)
		{
			tempoVal = 270;
		}
		else
		{
			tempoVal = 280;
		}
		tempo = tempoVal * (romData[romPos + 1] + 1);
		romPos += 2;
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			songPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			repeat1 = -1;
			override = 0;

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

			if (songPtrs[curTrack] == 0)
			{
				seqEnd = 1;
			}
			else
			{
				seqPos = songPtrs[curTrack] - bankAmt;
				seqEnd = 0;
			}

			while (seqEnd == 0)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				/*Set tempo?*/
				if (command[0] == 0x00)
				{
					seqPos += 2;
				}

				/*Set pitch envelope*/
				else if (command[0] == 0x01)
				{
					seqPos += 2;
				}

				/*Set duty*/
				else if (command[0] == 0x02)
				{
					seqPos += 2;
				}

				/*Set volume*/
				else if (command[0] == 0x03)
				{
					seqPos += 2;
				}

				/*Repeat section*/
				else if (command[0] == 0x04)
				{
					if (command[1] == 0)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pt = ReadLE16(&romData[seqPos + 2]) - bankAmt;
						}
						else if (repeat1 > 0)
						{
							seqPos = repeat1Pt;
							repeat1--;
						}
						else if (repeat1 == 0)
						{
							repeat1 = -1;
							seqPos += 4;
						}
					}
				}

				/*Set base note*/
				else if (command[0] == 0x05)
				{
					freq = command[1] + 12;
					seqPos += 2;
				}

				/*Dotted note*/
				else if (command[0] == 0x06)
				{
					dotted = 1;
					seqPos++;
				}

				/*Set volume envelope*/
				else if (command[0] == 0x07)
				{
					seqPos += 2;
				}

				/*Set modulation*/
				else if (command[0] == 0x08)
				{
					seqPos += 2;
				}

				/*End of channel*/
				else if (command[0] == 0x09)
				{
					seqEnd = 1;
				}

				/*Play note OR rest*/
				else
				{
					if (command[0] >= 0x20 && command[0] <= 0x2F)
					{
						;
					}
					else if (command[0] >= 0x30 && command[0] <= 0x4F)
					{
						if (command[0] == 0x30)
						{
							override = 1;
						}
					}
					if (command[0] >= 0x40 && command[0] <= 0x5F)
					{
						curNoteLen = 30;
						if (override == 1)
						{
							curNoteLen = 20;
							override = 0;
						}
						subVal = 0x40;
					}

					else if (command[0] >= 0x60 && command[0] <= 0x7F)
					{
						curNoteLen = 60;
						if (override == 1)
						{
							curNoteLen = 40;
							override = 0;
						}
						subVal = 0x60;
					}

					else if (command[0] >= 0x80 && command[0] <= 0x9F)
					{
						curNoteLen = 120;
						if (override == 1)
						{
							curNoteLen = 80;
							override = 0;
						}
						subVal = 0x80;
					}

					else if (command[0] >= 0xA0 && command[0] <= 0xBF)
					{
						curNoteLen = 240;
						if (override == 1)
						{
							curNoteLen = 160;
							override = 0;
						}
						subVal = 0xA0;
					}

					else if (command[0] >= 0xC0 && command[0] <= 0xDF)
					{
						curNoteLen = 480;
						if (override == 1)
						{
							curNoteLen = 320;
							override = 0;
						}
						subVal = 0xC0;
					}

					else if (command[0] >= 0xE0)
					{
						curNoteLen = 960;
						if (override == 1)
						{
							curNoteLen = 640;
							override = 0;
						}
						subVal = 0xE0;
					}

					if (dotted == 1)
					{
						curNoteLen = curNoteLen * 1.5;
						dotted = 0;
					}

					if (command[0] == 0x40 || command[0] == 0x60 || command[0] == 0x80 || command[0] == 0xA0 || command[0] == 0xC0 || command[0] == 0xE0)
					{
						curDelay += curNoteLen;
					}
					else
					{
						if (command[0] >= 0x40)
						{
							curNote = freq + (command[0] - subVal);
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							midPos = tempPos;
						}

					}
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
		fclose(mid);
	}
}