/*Probe Software*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "PROBE.H"

#define bankSize 16384
#define ramSize 65536

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtr;
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int curInst;
int curVol;
int stopCvt;
int square;

int sysMode;

int drvVers;

int fmtOverride;

char* tempPnt;
char OutFileBase[0x100];

unsigned char* romData;
unsigned char* spcData;
unsigned char* midData;
unsigned char* ctrlMidData;
long midLength;

const char ProbGBMagicBytes[6] = { 0xF1, 0x87, 0x26, 0x00, 0x6F, 0x11 };
const char ProbGGMagicBytes[8] = { 0xC3, 0x5E, 0x88, 0x6F, 0x26, 0x00, 0x29, 0x11 };
const char ProbSNESMagicBytes1[3] = { 0x1C, 0x5D, 0xF5 };
const char ProbSNESMagicBytes2[4] = { 0x12, 0xC4, 0x12, 0xF6 };
const unsigned char ProbSNESnoteLens[16] = { 12, 24, 36, 48, 72, 96, 144, 168, 192, 24, 36, 48, 72, 96, 144, 168 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Probsong2mid(int songNum, long ptr);
void ProbSNESsong2mid(int songNum, long ptr);
void ProbSNESsong2mid2(int songNum, long ptr);
void ProbSMDsong2mid(int songNum, long ptr);

void ProbProc(int bank)
{
	foundTable = 0;
	firstPtr = 0;
	curInst = 0;
	curVol = 0;
	stopCvt = 0;

	sysMode = 1;

	drvVers = 2;
	fmtOverride = 0;
	sysMode = 1;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	if (sysMode < 3)
	{
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}
	else if (sysMode == 3)
	{
		/*Copy filename from argument - based on code by ValleyBell*/
		/*strcpy(OutFileBase, argv[1]);*/
		tempPnt = strrchr(OutFileBase, '.');
		if (tempPnt == NULL)
		{
			tempPnt = OutFileBase + strlen(OutFileBase);
		}
		*tempPnt = 0;

		fseek(rom, 0x100, SEEK_SET);
		spcData = (unsigned char*)malloc(ramSize);
		fread(spcData, 1, ramSize, rom);
	}

	else if (sysMode == 4)
	{
		/*Use the "bank" variable as a global ROM pointer*/
		fseek(rom, bank, SEEK_SET);
		romData = (unsigned char*)malloc(0x20000);
		fread(romData, 1, 0x20000, rom);
	}


	if (sysMode == 1)
	{
		/*Try to search the bank for song table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], ProbGBMagicBytes, 6)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 6;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}
	else if (sysMode == 2)
	{
		/*Try to search the bank for song table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], ProbGGMagicBytes, 8)) && foundTable != 1)
			{
				bankAmt = 0x8000;
				tablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}

	else if (sysMode == 3)
	{
		/*Try to search the bank for song table loader (late version)*/
		if (fmtOverride == 0 || drvVers == 3)
		{
			for (i = 0; i < ramSize; i++)
			{
				if ((!memcmp(&spcData[i], ProbSNESMagicBytes2, 4)) && foundTable != 1 && ReadLE16(&spcData[i + 4]) < 0x1500)
				{
					tablePtrLoc = i + 4;
					printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
					tableOffset = ReadLE16(&spcData[tablePtrLoc]);
					printf("Song table starts at 0x%04x...\n", tableOffset);
					foundTable = 1;
					drvVers = 3;
				}
			}
		}


		/*Try to search the bank for song table loader*/
		for (i = 0; i < ramSize; i++)
		{
			if ((!memcmp(&spcData[i], ProbSNESMagicBytes1, 3)) && foundTable != 1)
			{
				tablePtrLoc = i + 3;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&spcData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}

	else if (sysMode == 4)
	{
		foundTable = 1;
	}


	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;
		if (sysMode == 1)
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < (bankAmt * 2) && stopCvt == 0)
			{
				songPtr = ReadLE16(&romData[i]);
				if (songPtr >= tableOffset)
				{
					stopCvt = 1;
				}
				else
				{
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Probsong2mid(songNum, songPtr);
					i += 2;
					songNum++;
				}

			}

			free(romData);
		}
		else if (sysMode == 2)
		{
			firstPtr = ReadLE16(&romData[i]) + tableOffset;
			while ((i + bankAmt) < firstPtr)
			{
				songPtr = ReadLE16(&romData[i]) + tableOffset;
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Probsong2mid(songNum, songPtr);
				i += 2;
				songNum++;
			}
			free(romData);
		}

		else if (sysMode == 3)
		{
			songNum = 1;

			if (drvVers != 3)
			{
				/*$5250 = "PR" ("PRIOR") (in later games)*/
				while (ReadLE16(&spcData[i]) != 0x4040 && ReadLE16(&spcData[i]) != 0x5250)
				{
					songPtr = ReadLE16(&spcData[i]);
					if (ReadLE16(&spcData[songPtr]) < 16)
					{
						printf("Song %i: 0x%04X\n", songNum, songPtr);
						ProbSNESsong2mid(songNum, songPtr);
					}

					i += 2;
					songNum++;
				}
			}

			else if (drvVers == 3)
			{
				/*$5250 = "PR" ("PRIOR") (in later games)*/
				while ((i + bankAmt) < (tableOffset + 0x40))
				{
					songPtr = ReadLE16(&spcData[i]);
					if (songPtr != 0x0000 && spcData[songPtr] < 16 && spcData[songPtr + 2] == 0)
					{
						printf("Song %i: 0x%04X\n", songNum, songPtr);
						ProbSNESsong2mid2(songNum, songPtr);
					}

					i += 2;
					songNum++;
				}
			}


			free(spcData);
		}

		else if (sysMode == 4)
		{
			i = 0;
			songNum = 1;
			bankAmt = 0;
			firstPtr = ReadBE16(&romData[i + 2]);

			while (i < firstPtr)
			{
				songPtr = ReadBE16(&romData[i + 2]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				ProbSMDsong2mid(songNum, songPtr);
				i += 4;
				songNum++;
			}
		}

	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		free(romData);
		exit(1);
	}
}

void Probsong2mid(int songNum, long ptr)
{
	int numChan = 0;
	int chanPtr = 0;
	long romPos = 0;
	long seqPos = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int curTrack = 0;
	int noteSize = 0;
	int restTime = 0;
	int tempo = 0;
	int ticks = 120;
	int noteOn = 0;
	int repeat1Times = 0;
	int repeat1Pos = 0;
	int repeat2Times = 0;
	int repeat2Pos = 0;
	int repeat3Times = 0;
	int repeat3Pos = 0;
	unsigned char command[8];
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int firstNote = 1;
	int timeVal = 0;
	int holdNote = 0;
	int lastNote = 0;

	int valSize = 0;

	int tempByte = 0;
	long tempPos = 0;
	long trackSize = 0;
	long delayPos = 0;


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
		romPos = ptr - bankAmt;
		numChan = romData[romPos];
		romPos++;

		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], numChan + 1);
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

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / 150);
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

		for (curTrack == 0; curTrack < numChan; curTrack++)
		{
			firstNote = 1;

			curDelay = 0;
			ctrlDelay = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;

			masterDelay = 0;

			repeat1Times = -1;
			repeat1Pos = -1;
			repeat2Times = -1;
			repeat2Pos = -1;
			repeat3Times = -1;
			repeat3Pos = -1;
			delayPos = 0;

			/*Add track header*/
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			chanPtr = ReadLE16(&romData[romPos]);

			seqPos = ptr + chanPtr - bankAmt;
			seqEnd = 0;
			noteOn = 0;
			curInst = 0;
			curVol = 100;
			while (seqEnd == 0 && seqPos < bankAmt)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];
				command[4] = romData[seqPos + 4];
				command[5] = romData[seqPos + 5];
				command[6] = romData[seqPos + 6];
				command[7] = romData[seqPos + 7];

				/*Delay OR play note*/
				if (command[0] < 0x80)
				{
					if (noteOn == 0)
					{
						if (seqPos != delayPos)
						{
							curDelay += command[0] * 2;
						}
						seqPos++;
						noteOn = 1;
					}
					else if (noteOn == 1)
					{
						curNote = command[0];
						curVol = command[1] * 0.5;
						delayPos = seqPos + 2;

						if (command[2] < 0x80)
						{
							curNoteLen = command[2] * 2;
						}
						else
						{
							curNoteLen = (ReadBE16(&romData[seqPos + 2]) - 0x8000) * 2;
						}

						if (curNoteLen >= noteSize)
						{
							tempPos = WriteNoteEventGen(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
							curDelay = curNoteLen - noteSize;
							firstNote = 0;
							midPos = tempPos;
						}

						else
						{
							tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = 0;
							midPos = tempPos;
						}
						seqPos += 2;
						noteOn = 0;
					}
				}

				/*Delay (long) OR set vibrato*/
				else if (command[0] >= 0x80 && command[0] < 0x9F)
				{
					if (noteOn == 0)
					{
						if (seqPos != delayPos)
						{
							curDelay += (ReadBE16(&romData[seqPos]) - 0x8000) * 2;
						}
						seqPos += 2;
						noteOn = 1;
					}
					else
					{
						noteSize = 60;
						seqPos++;
					}

				}

				/*Set note size*/
				else if (command[0] == 0x9F)
				{
					if (command[1] < 0x80)
					{
						noteSize = command[1] * 2;
						seqPos += 2;
					}
					else
					{
						noteSize = (ReadBE16(&romData[seqPos]) - 0x8000) * 2;
						seqPos += 3;
					}
				}

				/*Rest*/
				else if (command[0] > 0x9F && command[0] <= 0xEF)
				{
					curDelay = command[0];
					seqPos++;
				}

				/*Unknown command F0*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set envelope effect*/
				else if (command[0] == 0xF1)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set SFX priority?*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 1*/
				else if (command[0] == 0xF5)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 0*/
				else if (command[0] == 0xF6)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Unknown command F7*/
				else if (command[0] == 0xF7)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set SFX priority*/
				else if (command[0] == 0xF8)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 1 (v2)*/
				else if (command[0] == 0xF9)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Repeat section # of times*/
				else if (command[0] == 0xFA)
				{
					if (repeat1Times == -1)
					{
						repeat1Times = command[1];
						repeat1Pos = seqPos + 2;
						seqPos += 2;
					}
					else if (repeat2Times == -1)
					{
						repeat2Times = command[1];
						repeat2Pos = seqPos + 2;
						seqPos += 2;
					}
					else
					{
						repeat3Times = command[1];
						repeat3Pos = seqPos + 2;
						seqPos += 2;
					}
					noteOn = 0;
				}

				/*End of repeat*/
				else if (command[0] == 0xFB)
				{
					if (repeat3Times > -1)
					{
						if (repeat3Times > 1)
						{
							seqPos = repeat3Pos;
							repeat3Times--;
							noteOn = 0;
						}
						else if (repeat3Times <= 1)
						{
							repeat3Times = -1;
							seqPos += 2;
							noteOn = 0;
						}
					}
					else if (repeat2Times == -1)
					{
						if (repeat1Times > 1)
						{
							seqPos = repeat1Pos;
							repeat1Times--;
							noteOn = 0;
						}
						else if (repeat1Times <= 1)
						{
							repeat1Times = -1;
							seqPos += 2;
							noteOn = 0;
						}
					}
					else if (repeat2Times > -1)
					{
						if (repeat2Times > 1)
						{
							seqPos = repeat2Pos;
							repeat2Times--;
							noteOn = 0;
						}
						else if (repeat2Times <= 1)
						{
							repeat2Times = -1;
							seqPos += 2;
							noteOn = 0;
						}
					}

					else
					{
						seqEnd = 1;
					}

				}

				/*Set tempo*/
				else if (command[0] == 0xFC)
				{

					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					if (command[1] >= 0x80)
					{
						tempo = ((ReadBE16(&romData[seqPos + 1])) - 0x8000) * 1.66;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 3;
						noteOn = 0;
					}
					else
					{
						tempo = command[1] * 1.66;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
						noteOn = 0;
					}
				}

				/*Skip*/
				else if (command[0] == 0xFD)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set instrument*/
				else if (command[0] == 0xFE)
				{
					curInst = command[1];
					firstNote = 1;
					seqPos += 2;
					noteOn = 0;
				}

				/*End of sequence*/
				else if (command[0] == 0xFF)
				{
					seqEnd = 1;
				}

				else
				{
					seqEnd = 1;
					stopCvt = 1;
				}
			}
			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			romPos += 2;
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

/*Convert the song data to MIDI - SNES version (before late)*/
void ProbSNESsong2mid(int songNum, long ptr)
{
	int numChan = 0;
	int chanPtr = 0;
	long spcPos = 0;
	long seqPos = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int curTrack = 0;
	int noteSize = 0;
	int restTime = 0;
	int tempo = 0;
	int ticks = 120;
	int noteOn = 0;
	int repeat1Times = 0;
	int repeat1Pos = 0;
	int repeat2Times = 0;
	int repeat2Pos = 0;
	int repeat3Times = 0;
	int repeat3Pos = 0;
	unsigned char command[8];
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int firstNote = 1;
	int timeVal = 0;
	int holdNote = 0;
	int lastNote = 0;

	int valSize = 0;

	int tempByte = 0;
	long tempPos = 0;
	long trackSize = 0;
	long delayPos = 0;
	int noteVar = 1;
	int noteVarLen = 0;


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

	sprintf(outfile, "%s_%01X.mid", OutFileBase, songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file %s_%01X.mid!\n", OutFileBase, songNum);
		exit(2);
	}
	else
	{
		spcPos = ptr;
		numChan = ReadLE16(&spcData[spcPos]);
		spcPos += 2;

		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], numChan + 1);
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

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / 150);
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

		for (curTrack = 0; curTrack < numChan; curTrack++)
		{
			chanPtr = ReadLE16(&spcData[spcPos]);

			seqPos = ptr + chanPtr;
			seqEnd = 0;
			curDelay = 0;
			ctrlDelay = 0;
			curNoteLen = 0;
			curInst = 0;
			curVol = 100;
			firstNote = 1;
			noteVar = 1;

			/*Add track header*/
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			while (seqEnd == 0 && midPos < 48000)
			{
				if (curTrack == 4)
				{
					curTrack = 4;
				}
				command[0] = spcData[seqPos];
				command[1] = spcData[seqPos + 1];
				command[2] = spcData[seqPos + 2];
				command[3] = spcData[seqPos + 3];
				command[4] = spcData[seqPos + 4];
				command[5] = spcData[seqPos + 5];
				command[6] = spcData[seqPos + 6];
				command[7] = spcData[seqPos + 7];

				/*Rest*/
				if (command[0] == 0x00)
				{
					if (drvVers == 2)
					{
						if (command[1] < 0x80)
						{
							curDelay += (command[1] * 2);
							seqPos += 2;
						}
						else
						{
							if (command[2] % 2)
							{
								square = 256 * command[2];
								curDelay += (square + ((command[1] - 0x80) * 2));

							}
							else
							{
								square = command[2] / 2;
								curDelay += ((square * 256) * 2) + ((command[1] - 0x80) * 2);
							}


							seqPos += 3;
						}
					}

					else
					{
						if (command[1] < 0x80)
						{
							curDelay += (command[1] * 2);
							seqPos += 2;
						}
						else
						{
							if (command[2] % 2)
							{
								square = 256 * command[2];
								curDelay += (square + ((command[1] - 0x80) * 2));

							}
							else
							{
								square = command[2] / 2;
								curDelay += ((square * 256) * 2) + ((command[1] - 0x80) * 2);
							}


							seqPos += 3;
						}
					}

				}

				/*Play note*/
				else if (command[0] > 0x00 && command[0] < 0x80)
				{
					if (drvVers == 2)
					{
						curNote = command[0];
						curVol = command[1] * 0.5;

						if (curVol == 0)
						{
							curVol = 1;
						}

						if (noteVar == 0)
						{
							;
						}
						else if (noteVar == 1)
						{
							if (command[2] < 0x80)
							{
								curNoteLen = command[2] * 2;
								seqPos++;
							}
							else
							{
								if (command[3] % 2)
								{
									square = 256 * command[3];
									curNoteLen = (square + ((command[2] - 0x80) * 2));

								}
								else
								{
									square = command[3] / 2;
									curNoteLen = ((square * 256) * 2) + ((command[2] - 0x80) * 2);
								}
								seqPos += 2;
							}

						}

						tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos += 2;
					}
					else
					{
						if (noteVar == 0)
						{
							curNote = command[0];
							tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}
						else if (noteVar == 1)
						{
							curNote = command[0];

							if (command[1] < 0x80)
							{
								curNoteLen = command[1] * 2;
								tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 2;
							}
							else
							{
								if (command[2] % 2)
								{
									square = 256 * command[2];
									curNoteLen = (square + ((command[1] - 0x80) * 2));

								}
								else
								{
									square = command[2] / 2;
									curNoteLen = ((square * 256) * 2) + ((command[1] - 0x80) * 2);
								}

								tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 3;
							}

						}

					}

				}

				/*Delay*/
				else if (command[0] >= 0x80 && command[0] <= 0x8F)
				{
					curDelay += ((ProbSNESnoteLens[command[0] - 0x80]) * 2);
					seqPos++;
				}

				/*Set note length*/
				else if (command[0] >= 0x90 && command[0] <= 0x9F)
				{
					noteVar = 0;
					curNoteLen = (ProbSNESnoteLens[command[0] - 0x90]) * 2;
					seqPos++;
				}

				/*Repeat note*/
				else if (command[0] == 0xA0)
				{
					tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos++;
				}

				/*Set length of notes to be variable*/
				else if (command[0] == 0xA1)
				{
					noteVar = 1;
					seqPos++;
				}

				/*Repeat previous note*/
				else if (command[0] == 0xE0)
				{
					tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos++;
				}

				/*Set length of notes to be variable*/
				else if (command[0] == 0xE1)
				{
					noteVar = 1;
					seqPos++;
				}

				/*Unknown command E4*/
				else if (command[0] == 0xE4)
				{
					seqPos += 2;
				}

				/*Unknown command E9*/
				else if (command[0] == 0xE9)
				{
					seqPos += 2;
				}

				/*Set tremolo delay*/
				else if (command[0] == 0xEB)
				{
					seqPos += 2;
				}

				/*Set tremolo length*/
				else if (command[0] == 0xEC)
				{
					seqPos += 2;
				}

				/*Set tremolo*/
				else if (command[0] == 0xED)
				{
					seqPos += 2;
				}

				/*Set vibrato delay*/
				else if (command[0] == 0xEE)
				{
					seqPos += 2;
				}

				/*Set vibrato length*/
				else if (command[0] == 0xEF)
				{
					seqPos += 2;
				}

				/*Set vibrato*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				/*End of channel (loop with faster tempo)*/
				else if (command[0] == 0xF1)
				{
					seqEnd = 1;
				}

				/*End of channel (play next song)*/
				else if (command[0] == 0xF2)
				{
					seqEnd = 1;
				}

				/*Velocity control off*/
				else if (command[0] == 0xF3)
				{
					seqPos++;
				}

				/*Velocity control on*/
				else if (command[0] == 0xF4)
				{
					seqPos++;
				}

				/*Wipe echo workspace*/
				else if (command[0] == 0xF5)
				{
					seqPos++;
				}

				/*Turn pitch modulation off*/
				else if (command[0] == 0xF6)
				{
					seqPos++;
				}

				/*Turn pitch modulation on*/
				else if (command[0] == 0xF7)
				{
					seqPos += 2;
				}

				/*End of channel (loop)*/
				else if (command[0] == 0xF8)
				{
					seqEnd = 1;
				}

				/*Trigger a track*/
				else if (command[0] == 0xF9)
				{
					seqPos += 2;
				}

				/*Turn echo off*/
				else if (command[0] == 0xFA)
				{
					seqPos++;
				}

				/*Turn echo on*/
				else if (command[0] == 0xFB)
				{
					seqPos++;
				}

				/*Effects*/
				else if (command[0] == 0xFC)
				{
					seqPos += 2;
				}

				/*Set tempo*/
				else if (command[0] == 0xFD)
				{
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					tempo = 65535 / command[1] / 8;

					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;
					seqPos += 2;
					noteOn = 0;
				}

				/*Set instrument*/
				else if (command[0] == 0xFE)
				{
					if (command[1] != curInst)
					{
						curInst = command[1];
						firstNote = 1;
					}
					seqPos += 2;
				}

				/*End of channel (no loop)*/
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

			spcPos += 2;
		}
		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		trackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], trackSize);

		sprintf(outfile, "%s_%01X.mid", OutFileBase, songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);

		fclose(mid);
		free(midData);
		free(ctrlMidData);
	}
}

/*Later version for SNES*/
void ProbSNESsong2mid2(int songNum, long ptr)
{
	int numChan = 0;
	int chanPtr = 0;
	long romPos = 0;
	long seqPos = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int curTrack = 0;
	int noteSize = 0;
	int restTime = 0;
	int tempo = 0;
	int ticks = 120;
	int noteOn = 0;
	int repeat1Times = 0;
	int repeat1Pos = 0;
	int repeat2Times = 0;
	int repeat2Pos = 0;
	int repeat3Times = 0;
	int repeat3Pos = 0;
	unsigned char command[8];
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int firstNote = 1;
	int timeVal = 0;
	int holdNote = 0;
	int lastNote = 0;

	int valSize = 0;

	int tempByte = 0;
	long tempPos = 0;
	long trackSize = 0;
	long delayPos = 0;


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

	sprintf(outfile, "%s_%01X.mid", OutFileBase, songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file %s_%01X.mid!\n", OutFileBase, songNum);
		exit(2);
	}
	else
	{

		romPos = ptr - bankAmt;
		numChan = spcData[romPos];
		romPos++;

		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], numChan + 1);
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

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / 150);
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

		for (curTrack == 0; curTrack < numChan; curTrack++)
		{
			firstNote = 1;

			curDelay = 0;
			ctrlDelay = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;

			masterDelay = 0;

			repeat1Times = -1;
			repeat1Pos = -1;
			repeat2Times = -1;
			repeat2Pos = -1;
			repeat3Times = -1;
			repeat3Pos = -1;
			delayPos = 0;

			/*Add track header*/
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			chanPtr = ReadLE16(&spcData[romPos]);

			seqPos = ptr + chanPtr - bankAmt;
			seqEnd = 0;
			noteOn = 0;
			curInst = 0;
			curVol = 100;
			while (seqEnd == 0 && midPos < 48000)
			{
				if (curTrack == 2)
				{
					curTrack = 2;
				}
				command[0] = spcData[seqPos];
				command[1] = spcData[seqPos + 1];
				command[2] = spcData[seqPos + 2];
				command[3] = spcData[seqPos + 3];
				command[4] = spcData[seqPos + 4];
				command[5] = spcData[seqPos + 5];
				command[6] = spcData[seqPos + 6];
				command[7] = spcData[seqPos + 7];

				/*Delay OR play note*/
				if (command[0] < 0x80)
				{
					if (noteOn == 0)
					{
						if (seqPos != delayPos)
						{
							curDelay += command[0] * 4;
						}
						seqPos++;
						noteOn = 1;
					}
					else if (noteOn == 1)
					{
						curNote = command[0];
						curVol = command[1] * 0.5;
						delayPos = seqPos + 2;

						if (command[2] < 0x80)
						{
							curNoteLen = command[2] * 4;
						}
						else
						{
							curNoteLen = (ReadBE16(&spcData[seqPos + 2]) - 0x8000) * 4;
						}

						if (curNoteLen >= noteSize)
						{
							if (curNote > 0x80)
							{
								curNote = 1;
							}
							tempPos = WriteNoteEventGen(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
							curDelay = curNoteLen - noteSize;
							firstNote = 0;
							midPos = tempPos;
						}

						else
						{
							if (curNote > 0x80)
							{
								curNote = 1;
							}
							tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = 0;
							midPos = tempPos;
						}
						seqPos += 2;
						noteOn = 0;
					}
				}

				/*Delay (long) OR set vibrato*/
				else if (command[0] >= 0x80 && command[0] < 0x9F)
				{
					if (noteOn == 0)
					{
						if (seqPos != delayPos)
						{
							curDelay += (ReadBE16(&spcData[seqPos]) - 0x8000) * 4;
						}
						seqPos += 2;
						noteOn = 1;
					}
					else
					{
						noteSize = 60;
						seqPos++;
					}

				}

				/*Set note size*/
				else if (command[0] == 0x9F)
				{
					if (command[1] < 0x80)
					{
						noteSize = command[1] * 2;
						seqPos += 2;
					}
					else
					{
						noteSize = (ReadBE16(&spcData[seqPos]) - 0x8000) * 2;
						seqPos += 3;
					}
				}

				/*Rest*/
				else if (command[0] > 0x9F && command[0] < 0xF0)
				{
					curDelay = command[0] * 2;
					seqPos++;
				}

				else if (command[0] == 0xF0)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set panning*/
				else if (command[0] == 0xF1)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set echo?*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 1*/
				else if (command[0] == 0xF5)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 0*/
				else if (command[0] == 0xF6)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Unknown command F7*/
				else if (command[0] == 0xF7)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set SFX priority*/
				else if (command[0] == 0xF8)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 1 (v2)*/
				else if (command[0] == 0xF9)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Repeat section # of times*/
				else if (command[0] == 0xFA)
				{
					if (repeat1Times == -1)
					{
						repeat1Times = command[1];
						repeat1Pos = seqPos + 2;
						seqPos += 2;
					}
					else if (repeat2Times == -1)
					{
						repeat2Times = command[1];
						repeat2Pos = seqPos + 2;
						seqPos += 2;
					}
					else
					{
						repeat3Times = command[1];
						repeat3Pos = seqPos + 2;
						seqPos += 2;
					}
					noteOn = 0;
				}

				/*End of repeat*/
				else if (command[0] == 0xFB)
				{
					if (repeat3Times > -1)
					{
						if (repeat3Times > 1)
						{
							seqPos = repeat3Pos;
							repeat3Times--;
							noteOn = 0;
						}
						else if (repeat3Times <= 1)
						{
							repeat3Times = -1;
							seqPos++;
							noteOn = 0;
						}
					}
					else if (repeat2Times == -1)
					{
						if (repeat1Times > 1)
						{
							seqPos = repeat1Pos;
							repeat1Times--;
							noteOn = 0;
						}
						else if (repeat1Times <= 1)
						{
							repeat1Times = -1;
							seqPos++;
							noteOn = 0;
						}
					}
					else if (repeat2Times > -1)
					{
						if (repeat2Times > 1)
						{
							seqPos = repeat2Pos;
							repeat2Times--;
							noteOn = 0;
						}
						else if (repeat2Times <= 1)
						{
							repeat2Times = -1;
							seqPos++;
							noteOn = 0;
						}
					}

					else
					{
						seqEnd = 1;
					}

				}

				/*Set tempo*/
				else if (command[0] == 0xFC)
				{

					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					if (command[1] >= 0x80)
					{
						tempo = command[1] * 0.8;

						if (tempo < 2)
						{
							tempo = 150;
						}
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 3;
						noteOn = 0;
					}
					else
					{
						tempo = command[1] * 0.8;

						if (tempo < 2)
						{
							tempo = 150;
						}
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
						noteOn = 0;
					}
				}

				/*Skip*/
				else if (command[0] == 0xFD)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set instrument*/
				else if (command[0] == 0xFE)
				{
					curInst = command[1];
					firstNote = 1;
					seqPos += 2;
					noteOn = 0;
				}

				/*End of sequence*/
				else if (command[0] == 0xFF)
				{
					seqEnd = 1;
				}

				else
				{
					seqEnd = 1;
					stopCvt = 1;
				}
			}
			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			romPos += 2;
		}
		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		trackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], trackSize);

		sprintf(outfile, "%s_%01X.mid", OutFileBase, songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		fclose(mid);
	}
}

void ProbSMDsong2mid(int songNum, long ptr)
{
	int numChan = 0;
	int chanPtr = 0;
	long romPos = 0;
	long seqPos = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int curTrack = 0;
	int noteSize = 0;
	int restTime = 0;
	int tempo = 0;
	int ticks = 120;
	int noteOn = 0;
	int repeat1Times = 0;
	int repeat1Pos = 0;
	int repeat2Times = 0;
	int repeat2Pos = 0;
	int repeat3Times = 0;
	int repeat3Pos = 0;
	unsigned char command[8];
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int firstNote = 1;
	int timeVal = 0;
	int holdNote = 0;
	int lastNote = 0;

	int valSize = 0;

	int tempByte = 0;
	long tempPos = 0;
	long trackSize = 0;
	long delayPos = 0;


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
		romPos = ptr - bankAmt;
		numChan = romData[romPos];
		romPos++;

		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], numChan + 1);
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

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / 150);
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

		for (curTrack == 0; curTrack < numChan; curTrack++)
		{
			firstNote = 1;

			curDelay = 0;
			ctrlDelay = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;

			masterDelay = 0;

			repeat1Times = -1;
			repeat1Pos = -1;
			repeat2Times = -1;
			repeat2Pos = -1;
			repeat3Times = -1;
			repeat3Pos = -1;
			delayPos = 0;

			/*Add track header*/
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			chanPtr = ReadLE16(&romData[romPos]);

			seqPos = ptr + chanPtr;
			seqEnd = 0;
			noteOn = 0;
			curInst = 0;
			curVol = 100;
			while (seqEnd == 0 && midPos < 48000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];
				command[4] = romData[seqPos + 4];
				command[5] = romData[seqPos + 5];
				command[6] = romData[seqPos + 6];
				command[7] = romData[seqPos + 7];

				/*Delay OR play note*/
				if (command[0] < 0x80)
				{
					if (noteOn == 0)
					{
						if (seqPos != delayPos)
						{
							curDelay += command[0] * 2;
						}
						seqPos++;
						noteOn = 1;
					}
					else if (noteOn == 1)
					{
						curNote = command[0];
						curVol = command[1] * 0.5;

						if (curVol == 0)
						{
							curVol = 1;
						}
						delayPos = seqPos + 2;

						if (command[2] < 0x80)
						{
							curNoteLen = command[2] * 2;
						}
						else
						{
							curNoteLen = (ReadBE16(&romData[seqPos + 2]) - 0x8000) * 2;
						}

						if (curNoteLen >= noteSize)
						{
							tempPos = WriteNoteEventGen(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
							curDelay = curNoteLen - noteSize;
							firstNote = 0;
							midPos = tempPos;
						}

						else
						{
							tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = 0;
							midPos = tempPos;
						}
						seqPos += 2;
						noteOn = 0;
					}
				}

				/*Delay (long) OR set vibrato*/
				else if (command[0] >= 0x80 && command[0] < 0x9F)
				{
					if (noteOn == 0)
					{
						if (seqPos != delayPos)
						{
							curDelay += (ReadBE16(&romData[seqPos]) - 0x8000) * 2;
						}
						seqPos += 2;
						noteOn = 1;
					}
					else
					{
						noteSize = 60;
						seqPos++;
					}

				}

				/*Set note size*/
				else if (command[0] == 0x9F)
				{
					if (command[1] < 0x80)
					{
						noteSize = command[1] * 2;
						seqPos += 2;
					}
					else
					{
						noteSize = (ReadBE16(&romData[seqPos]) - 0x8000) * 2;
						seqPos += 3;
					}
				}

				/*Rest*/
				else if (command[0] > 0x9F && command[0] < 0xF1)
				{
					curDelay = command[0];
					seqPos++;
				}

				/*Unknown command F0*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set panning*/
				else if (command[0] == 0xF1)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set echo*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 1*/
				else if (command[0] == 0xF5)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 0*/
				else if (command[0] == 0xF6)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Unknown command F7*/
				else if (command[0] == 0xF7)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set SFX priority*/
				else if (command[0] == 0xF8)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set bit 1 of envelope to 1 (v2)*/
				else if (command[0] == 0xF9)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Repeat section # of times*/
				else if (command[0] == 0xFA)
				{
					if (repeat1Times == -1)
					{
						repeat1Times = command[1];
						repeat1Pos = seqPos + 2;
						seqPos += 2;
					}
					else if (repeat2Times == -1)
					{
						repeat2Times = command[1];
						repeat2Pos = seqPos + 2;
						seqPos += 2;
					}
					else
					{
						repeat3Times = command[1];
						repeat3Pos = seqPos + 2;
						seqPos += 2;
					}
					noteOn = 0;
				}

				/*End of repeat*/
				else if (command[0] == 0xFB)
				{
					if (repeat3Times > -1)
					{
						if (repeat3Times > 1)
						{
							seqPos = repeat3Pos;
							repeat3Times--;
							noteOn = 0;
						}
						else if (repeat3Times <= 1)
						{
							repeat3Times = -1;
							seqPos += 2;
							noteOn = 0;
						}
					}
					else if (repeat2Times == -1)
					{
						if (repeat1Times > 1)
						{
							seqPos = repeat1Pos;
							repeat1Times--;
							noteOn = 0;
						}
						else if (repeat1Times <= 1)
						{
							repeat1Times = -1;
							seqPos += 2;
							noteOn = 0;
						}
					}
					else if (repeat2Times > -1)
					{
						if (repeat2Times > 1)
						{
							seqPos = repeat2Pos;
							repeat2Times--;
							noteOn = 0;
						}
						else if (repeat2Times <= 1)
						{
							repeat2Times = -1;
							seqPos += 2;
							noteOn = 0;
						}
					}

					else
					{
						seqEnd = 1;
					}

				}

				/*Set tempo*/
				else if (command[0] == 0xFC)
				{

					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					if (command[1] >= 0x80)
					{
						tempo = ((ReadBE16(&romData[seqPos + 1])) - 0x8000) * 1.66;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 3;
						noteOn = 0;
					}
					else
					{
						tempo = command[1] * 1.66;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
						noteOn = 0;
					}
				}

				/*Skip*/
				else if (command[0] == 0xFD)
				{
					seqPos += 2;
					noteOn = 0;
				}

				/*Set instrument*/
				else if (command[0] == 0xFE)
				{
					curInst = command[1];
					firstNote = 1;
					seqPos += 2;
					noteOn = 0;
				}

				/*End of sequence*/
				else if (command[0] == 0xFF)
				{
					seqEnd = 1;
				}

				else
				{
					seqEnd = 1;
					stopCvt = 1;
				}
			}
			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			romPos += 2;
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
		free(midData);
		free(ctrlMidData);
	}
}