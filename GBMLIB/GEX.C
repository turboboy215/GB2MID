/*FIRQ (Gex: Enter the Gecko)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "GEX.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
long numPtrs;
int curVol;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Gexsong2mid(int songNum, long ptrs[4]);

void GexProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	firstPtr = 0;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	tableOffset = ReadLE16(&romData[0x4001]);
	printf("Song table: 0x%04X\n", tableOffset);

	i = tableOffset;

	if (ReadLE16(&romData[i]) == 0x0086)
	{
		numPtrs = 0;
	}
	else
	{
		numPtrs = ReadLE16(&romData[i]) / 8;
	}

	i += 2;

	firstPtr = ReadLE16(&romData[i]) + i;
	songNum = 1;

	while (songNum <= numPtrs)
	{
		seqPtrs[0] = ReadLE16(&romData[i]) + i;
		seqPtrs[1] = ReadLE16(&romData[i + 2]) + i + 2;
		seqPtrs[2] = ReadLE16(&romData[i + 4]) + i + 4;
		seqPtrs[3] = ReadLE16(&romData[i + 6]) + i + 6;

		printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
		printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
		printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
		printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
		Gexsong2mid(songNum, seqPtrs);
		i += 8;
		songNum++;
	}

	while (i < firstPtr)
	{
		seqPtrs[0] = ReadLE16(&romData[i]) + i;
		seqPtrs[1] = 0x0000;
		seqPtrs[2] = 0x0000;
		seqPtrs[3] = 0x0000;
		printf("Song %i: 0x%04X\n", songNum, seqPtrs[0]);
		Gexsong2mid(songNum, seqPtrs);
		i += 2;
		songNum++;
	}
}

/*Convert the song data to MIDI*/
void Gexsong2mid(int songNum, long ptrs[4])
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
			seqEnd = 0;
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

			if (ptrs[curTrack] != 0x0000)
			{
				seqEnd = 0;
				seqPos = ptrs[curTrack] + 1;

				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note*/
					if (command[0] <= 0xBF)
					{
						if (command[0] == 0x00)
						{
							curDelay += (command[1] * 5);
						}
						else if (command[0] >= 0x01 && command[0] <= 0x48)
						{
							curNote = command[0] + 23;
							curNoteLen = command[1] * 5;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}

						/*Hold note?*/
						else if (command[0] == 0x49)
						{
							curDelay += (command[1] * 5);
						}

						else if (command[0] >= 0x4A)
						{
							curNote = command[0] - 0x49 + 23;
							curNoteLen = command[1] * 5;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}

						seqPos += 2;
					}

					/*Unknown commands C0-DF*/
					else if (command[0] >= 0xC0 && command[0] <= 0xDF)
					{
						seqPos += 2;
					}

					/*Set direct channel registers*/
					else if (command[0] >= 0xE0 && command[0] <= 0xFC)
					{
						if (command[0] == 0xE2 || command[0] == 0xE7)
						{
							curVol = command[1] * 0.5;
							if (curVol == 0)
							{
								curVol = 1;
							}
						}
						else if (command[0] == 0xEC)
						{
							switch (command[1])
							{
							case 0x00:
								curVol = 1;
								break;
							case 0x20:
								curVol = 100;
								break;
							case 0x40:
								curVol = 50;
								break;
							case 0x60:
								curVol = 25;
								break;
							default:
								curVol = 100;
								break;
							}
						}
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0xFD)
					{
						seqPos += 17;
					}

					/*Go to song loop*/
					else if (command[0] == 0xFE)
					{
						seqEnd = 1;
					}

					/*End of song (no loop)?*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}
				}
			}
			else
			{
				seqEnd = 1;
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
	}

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