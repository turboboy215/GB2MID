/*David Whittaker*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DW.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long firstPtrs[4];
long curSpeed;
long bankAmt;
long nextPtr;
int highestSeq;
int curVol;
unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;
unsigned long seqList[500];

long midLength;
long switchPoint[400][2];
int switchNum;

int multiBanks;
int curBank;

char folderName[100];

const unsigned char DWMagicBytes[5] = { 0x22, 0x05, 0x20, 0xFC, 0x21 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void DWsong2mid(int songNum, long ptrList[4], long nextPtr, int curSpeed);
void DWProc(int bank);

void DWProc(int bank)
{
	switchNum = 0;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	if (bank != 1)
	{
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}

	else
	{
		fseek(rom, ((bank - 1) * bankSize * 2), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize * 2, rom);
	}

	/*Try to search the bank for base table*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], DWMagicBytes, 5) && ReadLE16(&romData[i + 5]) < 0x8000))
		{
			tablePtrLoc = bankAmt + i + 5;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	if (tableOffset != 0)
	{
		for (i = 0; i < 500; i++)
		{
			seqList[i] = 0;
		}
		songNum = 1;
		i = tableOffset;
		highestSeq = 0;
		firstPtrs[0] = ReadLE16(&romData[i + 1 - bankAmt]);
		firstPtrs[1] = ReadLE16(&romData[i + 3 - bankAmt]);
		firstPtrs[2] = ReadLE16(&romData[i + 5 - bankAmt]);
		while ((i < firstPtrs[0]) && (i < firstPtrs[1]) && (i < firstPtrs[2]) && ReadLE16(&romData[i + 2 - bankAmt]) != 0 && (ReadLE16(&romData[i + 1 - bankAmt]) < (bankSize * 2)))
		{
			curSpeed = romData[i - bankAmt];
			printf("Song %i tempo: 0x%01x\n", songNum, curSpeed);
			songPtrs[0] = ReadLE16(&romData[i + 1 - bankAmt]);
			printf("Song %i channel 1: 0x%04x\n", songNum, songPtrs[0]);
			songPtrs[1] = ReadLE16(&romData[i + 3 - bankAmt]);
			printf("Song %i channel 2: 0x%04x\n", songNum, songPtrs[1]);
			songPtrs[2] = ReadLE16(&romData[i + 5 - bankAmt]);
			printf("Song %i channel 3: 0x%04x\n", songNum, songPtrs[2]);
			songPtrs[3] = ReadLE16(&romData[i + 7 - bankAmt]);
			printf("Song %i channel 4: 0x%04x\n", songNum, songPtrs[3]);
			nextPtr = ReadLE16(&romData[i + 10 - bankAmt]);
			DWsong2mid(songNum, songPtrs, nextPtr, curSpeed);
			i += 9;
			songNum++;
		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
}

void DWsong2mid(int songNum, long ptrList[4], long nextPtr, int curSpeed)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int patPos = 0;
	int seqPos = 0;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int trackCnt = 4;
	int ticks = 120;
	long curSeq = 0;
	long command[3];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int curTrack = 0;
	int endSeq = 0;
	int endChan = 0;
	int curTempo = curSpeed;
	int transpose = 0;
	int globalTranspose = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int masterDelay = 0;
	long jumpPos = 0;
	int firstNote = 1;
	int timeVal = 0;
	int holdNote = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = curSpeed * 3.5;

	int curInst = 0;

	int valSize = 0;

	long trackSize = 0;

	midPos = 0;
	ctrlMidPos = 0;

	switchNum = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	for (switchNum = 0; switchNum < 400; switchNum++)
	{
		switchPoint[switchNum][0] = -1;
		switchPoint[switchNum][1] = 0;
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
			transpose = 0;
			globalTranspose = 0;
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			endChan = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;

			masterDelay = 0;

			switchNum = 0;

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

			/*Get pointers to each sequence from pattern*/
			endChan = 0;
			patPos = songPtrs[curTrack] - bankAmt;
			while (endChan == 0)
			{
				curSeq = ReadLE16(&romData[patPos]);
				if (curSeq != 0)
				{
					seqPos = curSeq;
					endSeq = 0;
				}
				else
				{
					endChan = 1;
				}

				while (endSeq == 0)
				{
					command[0] = romData[seqPos - bankAmt];
					command[1] = romData[seqPos + 1 - bankAmt];
					command[2] = romData[seqPos + 2 - bankAmt];

					if (curTrack != 0 && curTrack != 3)
					{
						for (switchNum = 0; switchNum < 400; switchNum++)
						{
							if (switchPoint[switchNum][0] == masterDelay)
							{
								globalTranspose = switchPoint[switchNum][1];
							}
						}
					}

					/*Workaround for The Flintstones boss global transpose - done on channel 3*/
					if (ptrList[0] == 0x5122 && ptrList[1] == 0x512E && ptrList[2] == 0x513A && ptrList[3] == 0x5150)
					{
						if (masterDelay == 1920)
						{
							globalTranspose = 1;
						}
					}

					/*Change tempo*/
					if (command[0] == 0xF4)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 3.5;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set channel loop point/end sequence*/
					else if (command[0] == 0xF5)
					{
						endSeq = 1;
					}

					/*Set envelope fade in/out speed*/
					else if (command[0] == 0xF6)
					{
						if (curTrack != 2)
						{
							curVol = 100;
							seqPos += 2;
						}
						else
						{
							lowNibble = command[1] >> 4;
							if (lowNibble == 8)
							{
								curVol = 0;
							}
							else if (lowNibble == 7)
							{
								curVol = 40;
							}
							else if (lowNibble == 6)
							{
								curVol = 50;
							}
							else if (lowNibble == 5)
							{
								curVol = 60;
							}
							else if (lowNibble == 4)
							{
								curVol = 70;
							}
							else if (lowNibble == 3)
							{
								curVol = 90;
							}
							else if (lowNibble == 2)
							{
								curVol = 100;
							}
							else
							{
								curVol = 100;
							}
							seqPos += 3;
						}
					}

					/*Set vibrato*/
					else if (command[0] == 0xF7)
					{
						seqPos += 2;
					}

					/*Rest*/
					else if (command[0] == 0xF8)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;
					}

					/*Hold note*/
					else if (command[0] == 0xF9)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;
					}

					/*Set instrument duty*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Transpose all channels*/
					else if (command[0] == 0xFB)
					{
						globalTranspose = (signed char)command[1];
						if (curTrack == 0)
						{
							switchPoint[switchNum][0] = masterDelay;
							switchPoint[switchNum][1] = globalTranspose;
							switchNum++;
						}
						seqPos += 2;
					}

					/*Transpose current channel*/
					else if (command[0] == 0xFC)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
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
						seqPos += 2;
					}

					/*End of channel (no loop)*/
					else if (command[0] == 0xFE)
					{
						endSeq = 1;
						endChan = 1;
					}

					/*End of sequence*/
					else if (command[0] == 0xFF)
					{
						endSeq = 1;
					}

					/*Set channel speed/note length*/
					else if (command[0] >= 0x60 && command[0] < 0x80)
					{
						curNoteLen = 30 + (30 * (command[0] - 0x60));
						seqPos++;
					}

					/*Play note*/
					else if (command[0] < 0x60)
					{
						curNote = command[0] + 24 + transpose + globalTranspose;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}
				}
				patPos += 2;
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
		fclose(mid);
	}
}