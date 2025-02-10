/*Realtime Associates (David Warhol)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "REALTIME.H"

#define bankSize 16384

FILE* rom, * mid;
long bank1;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int RTAcurInst[16];
char* argv3;
int multiBanks;
int curBank;

char folderName[100];

int sysMode;

unsigned char* romData;
unsigned char* RTAmidData[16];
unsigned char* ctrlMidData;

long midLength;

const char RTAMagicBytes[8] = { 0x78, 0xE6, 0x3F, 0x87, 0x5F, 0x16, 0x00, 0x21 };
const char RTAMagicBytesGG[6] = { 0x3D, 0x6F, 0x26, 0x00, 0x29, 0x11 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void RTAsong2mid(int songNum, long ptr);

void RTAProc(int bank)
{
	bank1 = bank;
	foundTable = 0;
	firstPtr = 0;
	sysMode = 1;

	if (bank1 != 1)
	{
		if (sysMode == 1)
		{
			bankAmt = bankSize;
		}
		else if (sysMode == 2)
		{
			bankAmt = bankSize * 2;
		}

	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((bank1 - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	if (sysMode == 1)
	{
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], RTAMagicBytes, 8)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}
	else if (sysMode == 2)
	{
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], RTAMagicBytesGG, 6)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 6;
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
		if (sysMode == 2)
		{
			i += 2;
		}
		songPtr = ReadLE16(&romData[i]);
		if (romData[songPtr - bankAmt] <= 3)
		{
			i += 2;
			songPtr = ReadLE16(&romData[i]);

			/*For Mysterium*/
			if (songPtr == 0x7935)
			{
				i = 0x09D7;
				songPtr = ReadLE16(&romData[i]);
			}
			/*For Skate or Die*/
			if (songPtr == 0x4A1F)
			{
				i = 0x09D1;
				songPtr = ReadLE16(&romData[i]);
			}
			/*For Barbie (bank 2)*/
			if (songPtr == 0x56E8 && bank1 == 7)
			{
				i = 0x9F6;
				songPtr = ReadLE16(&romData[i]);
			}
		}
		songNum = 1;
		while (romData[songPtr - bankAmt] > 3 && ReadLE16(&romData[i]) > bankAmt)
		{
			songPtr = ReadLE16(&romData[i]);
			if (romData[songPtr - bankAmt] > 3)
			{
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				if (songPtr != 0)
				{
					RTAsong2mid(songNum, songPtr);
					for (j = 0; j < 16; j++)
					{
						free(RTAmidData[j]);
					}
					free(ctrlMidData);
				}
				i += 2;

				/*Workaround for Barbie empty track*/
				if (songPtr == 0x6D9E)
				{
					i += 2;
				}
				songNum++;
			}

		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		free(romData);
		exit(-1);
	}
}

void RTAsong2mid(int songNum, long ptr)
{
	long seqPos = 0;
	int curDelay[16];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int noteTotal = 0;
	int curNote[16];
	int curNoteLen[16];
	int rowTime = 0;
	int noteBank[300][2];
	int seqEnd = 0;
	int k = 0;
	int curTrack = 0;
	unsigned int command[2];
	int firstNote[16];
	int midPos[16];
	int trackCnt = 16;
	int ticks = 120;
	int tempo = 180;
	long tempPos = 0;
	int trackSize[16];
	int ctrlTrackSize = 0;
	int onOff[16];
	int activeChan[16];

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;
	for (j = 0; j < 16; j++)
	{
		RTAmidData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < 16; k++)
		{
			RTAmidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	for (k = 0; k < 16; k++)
	{
		onOff[k] = 0;
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

		/*Get the note frequency values set*/
		seqPos = ptr - bankAmt;
		noteTotal = romData[seqPos];
		seqPos++;

		/*Get the mapping for each channel*/
		for (k = 0; k < noteTotal; k++)
		{
			noteBank[k][1] = romData[seqPos];
			activeChan[romData[seqPos]] = 1;
			seqPos++;
		}

		/*Get the note values themselves*/
		for (k = 0; k < noteTotal; k++)
		{
			noteBank[k][0] = romData[seqPos];
			seqPos++;
		}

		for (k = 0; k < 16; k++)
		{
			RTAcurInst[k] = 0;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPos[curTrack] = 0;
			firstNote[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&RTAmidData[curTrack][midPos[curTrack]], 0x4D54726B);
			midPos[curTrack] += 8;
			curNoteLen[curTrack] = 0;
			curDelay[curTrack] = 0;
			seqEnd = 0;
			midTrackBase = midPos[curTrack];

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&RTAmidData[curTrack][midTrackBase - 2], trackSize[curTrack]);
		}

		/*Convert the sequence data*/
		while (seqEnd == 0)
		{
			command[0] = romData[seqPos];
			command[1] = romData[seqPos + 1];

			/*Play note*/
			if (command[0] < noteTotal)
			{
				curTrack = noteBank[command[0]][1];
				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEventGen(RTAmidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, RTAcurInst[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				else
				{
					curNote[curTrack] = noteBank[command[0]][0] + 21;
					onOff[curTrack] = 1;
					curNoteLen[curTrack] = 0;
					seqPos++;
				}

			}

			/*Note off/rest*/
			else if (command[0] >= (noteTotal) && command[0] < (noteTotal + 16))
			{
				curTrack = command[0] - noteTotal;
				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEventGen(RTAmidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, RTAcurInst[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				seqPos++;
			}

			/*Change instrument*/
			else if (command[0] >= (noteTotal + 16) && command[0] < (noteTotal + 32))
			{

				curTrack = command[0] - noteTotal - 16;
				if (RTAcurInst[curTrack] != command[1])
				{
					RTAcurInst[curTrack] = command[1];
					firstNote[curTrack] = 1;

				}

				seqPos += 2;
			}

			/*Row time*/
			else if (command[0] >= (noteTotal + 32) && command[0] < 0xFE)
			{
				rowTime = command[0] - (noteTotal + 32);
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					if (activeChan[curTrack] == 1)
					{
						if (onOff[curTrack] == 1)
						{
							curNoteLen[curTrack] += rowTime * 6;
						}
						else
						{
							curDelay[curTrack] += rowTime * 6;
						}
					}
				}
				seqPos++;
			}
			else if (command[0] == 0xFE)
			{
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					if (onOff[curTrack] == 1)
					{
						tempPos = WriteNoteEventGen(RTAmidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, RTAcurInst[curTrack]);
						firstNote[curTrack] = 0;
						midPos[curTrack] = tempPos;
						curDelay[curTrack] = 0;
						onOff[curTrack] = 0;
					}
				}
				seqEnd = 1;
			}
			else if (command[0] == 0xFF)
			{
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					if (onOff[curTrack] == 1)
					{
						tempPos = WriteNoteEventGen(RTAmidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, RTAcurInst[curTrack]);
						firstNote[curTrack] = 0;
						midPos[curTrack] = tempPos;
						curDelay[curTrack] = 0;
						onOff[curTrack] = 0;
					}
				}
				seqEnd = 1;
			}
		}

		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			/*WriteDeltaTime(RTAmidData[curTrack], midPos[curTrack], 0);
			midPos[curTrack]++;*/
			WriteBE32(&RTAmidData[curTrack][midPos[curTrack]], 0xFF2F00);
			midPos[curTrack] += 4;
			firstNote[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&RTAmidData[curTrack][midTrackBase - 2], trackSize[curTrack]);
		}

		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(RTAmidData[curTrack], midPos[curTrack], 1, mid);
		}
		fclose(mid);
	}
}