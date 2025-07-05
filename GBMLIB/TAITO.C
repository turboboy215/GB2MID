/*Taito*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "TAITO.H"

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
int curInst;
long firstPtr;
int ptrOverride;
int drvVers;

const unsigned char TaitoNoteVals[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
								   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
								   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,0, 0, 0, 0,
								   12,13,14,15,16,17,18,19,20,21,22,23,0, 0, 0, 0,
								   24,25,26,27,28,29,30,31,32,33,34,35,0, 0, 0, 0,
								   36,37,38,39,40,41,42,43,44,45,46,47,0, 0, 0, 0,
								   48,49,50,51,52,53,54,55,56,57,58,59,0, 0, 0, 0,
								   60,61,62,63,64,65,66,67,68,69,70,71,0, 0, 0, 0,
								   72,73,74,75,76,77,78,79,80,81,82,83,0, 0, 0, 0,
								   84,85,86,87,88,89,90,91,92,93,94,95,0, 0, 0, 0 };

const unsigned char TaitoMagicBytes[5] = { 0x26, 0x00, 0x6F, 0x29, 0x11 };

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

char* argv3;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Taitosong2mid(int songNum, long ptr);
void Taitosong2midMB(int songNum, long ptr);

void TaitoProc(int bank, char parameters[4][50])
{
	foundTable = 0;
	curInst = 0;
	drvVers = 1;

	if (parameters[1][0] != 0x00)
	{
		tableOffset = strtol(parameters[1], NULL, 16);
		drvVers = strtol(parameters[0], NULL, 16);
		ptrOverride = 1;
		foundTable = 1;
	}
	else if (parameters[0][0] != 0x00)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		ptrOverride = 1;
		foundTable = 1;
	}

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

	/*Try to search the bank for song table loader*/
	if (ptrOverride != 1)
	{
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], TaitoMagicBytes, 5)) && foundTable != 1)
			{
				tablePtrLoc = i + 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}


	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		if (tableOffset == 0x0B4B)
		{
			while (songNum < 55)
			{
				songPtr = ReadLE16(&romData[i]);
				if (romData[songPtr] < 5)
				{
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Taitosong2midMB(songNum, songPtr);
				}
				else
				{
					printf("Song %i: 0x%04X (invalid)\n", songNum, songPtr);
				}

				i += 2;
				songNum++;
			}
		}
		else
		{
			while (ReadLE16(&romData[i]) > bankAmt && ReadLE16(&romData[i]) < 0x8000)
			{
				songPtr = ReadLE16(&romData[i]);
				if (romData[songPtr] < 5)
				{
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Taitosong2mid(songNum, songPtr);
				}
				else
				{
					printf("Song %i: 0x%04X (invalid)\n", songNum, songPtr);
				}

				i += 2;
				songNum++;
			}
		}

		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void Taitosong2mid(int songNum, long ptr)
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
	int octave = 0;
	int transpose = 0;
	long jumpPos1 = 0;
	long jumpPosRet1 = 0;
	long jumpPos2 = 0;
	long jumpPosRet2 = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	unsigned char command[4];
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
	int repeat = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int noteFlag = 0;
	unsigned int chanFlags[4];

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (curTrack = 0; curTrack < 4; curTrack++)
	{
		seqPtrs[curTrack] = 0x0000;
	}

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

		/*Get the channel pointers*/
		romPos = ptr;
		trackCnt = romData[romPos];
		if (trackCnt > 4)
		{
			trackCnt = 4;
		}
		romPos++;
		for (j = 0; j < trackCnt; j++)
		{
			tempByte = romData[romPos] >> 4;

			switch (tempByte)
			{
			case 0x00:
				curTrack = 0;
				break;
			case 0x01:
				curTrack = 1;
				break;
			case 0x02:
				curTrack = 2;
				break;
			case 0x03:
				curTrack = 3;
				break;
			case 0x04:
				curTrack = 0;
				break;
			case 0x05:
				curTrack = 1;
				break;
			case 0x06:
				curTrack = 2;
				break;
			case 0x07:
				curTrack = 3;
				break;
			default:
				curTrack = 0;
				break;
			}
			seqPtrs[curTrack] = ReadLE16(&romData[romPos + 1]);
			chanFlags[curTrack] = tempByte;
			romPos += 3;

		}

		trackCnt = 4;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			noteFlag = 0;

			repeat1 = -1;

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

			if (seqPtrs[curTrack] != 0x0000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];

				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*End of channel*/
					if (command[0] == 0x00)
					{
						seqEnd = 1;
					}

					/*Set note length*/
					else if (command[0] >= 0x01 && command[0] <= 0x8B)
					{
						if (noteFlag == 0)
						{
							chanSpeed = command[0];
							noteFlag = 1;
						}
						else
						{
							curDelay += (chanSpeed * 5);
							chanSpeed = command[0];
						}
						seqPos++;

					}

					/*Set volume*/
					else if (command[0] == 0x8C)
					{
						/*Repeat the following section*/
						if (drvVers > 1)
						{
							repeat1 = command[1];
							repeatStart = seqPos + 2;
						}
						seqPos += 2;
					}

					/*Set sweep*/
					else if (command[0] == 0x8D)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0x8E)
					{
						seqPos += 2;
					}

					/*Set tuning and play note*/
					else if (command[0] == 0x8F)
					{
						if (drvVers < 3)
						{
							curNote = TaitoNoteVals[command[1] & 0x7F] + 24;
							curNoteLen = chanSpeed * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							noteFlag = 0;
							seqPos += 3;
						}
						else
						{
							seqPos += 2;
						}

					}

					/*Set noise*/
					else if (command[0] == 0x9C)
					{
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0x9D)
					{
						seqPos += 2;
					}

					/*Set panning*/
					else if (command[0] == 0x9E)
					{
						seqPos += 2;
					}

					/*Set global volume*/
					else if (command[0] == 0x9F)
					{
						seqPos += 2;
					}

					/*Go to song loop*/
					else if (command[0] == 0xAC)
					{
						seqEnd = 1;
					}

					/*Set length/envelope*/
					else if (command[0] == 0xAD)
					{
						seqPos += 2;
					}

					/*Repeat note*/
					else if (command[0] == 0xAE)
					{
						if (drvVers == 1)
						{
							curNoteLen = chanSpeed * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							noteFlag = 0;
							seqPos++;
						}
						else
						{
							seqPos += 2;
						}
					}

					/*Unknown command AF*/
					else if (command[0] == 0xAF)
					{
						seqPos++;
					}

					/*Unknown command BC*/
					else if (command[0] == 0xBC)
					{
						seqPos++;
					}

					/*Unknown command BD*/
					else if (command[0] == 0xBD)
					{
						seqPos++;
					}

					/*Unknown command BE*/
					else if (command[0] == 0xBE && drvVers > 1)
					{
						seqPos++;
					}

					/*Unknown command EC*/
					else if (command[0] == 0xEC && drvVers > 2)
					{
						seqPos++;
					}

					/*Unknown command ED*/
					else if (command[0] == 0xED && drvVers > 2)
					{
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0xEE)
					{
						curDelay += (chanSpeed * 5);
						noteFlag = 0;
						seqPos++;
					}

					/*End of repeat section*/
					else if (command[0] == 0xEF && drvVers > 1)
					{
						if (repeat1 > 1)
						{
							seqPos = repeatStart;
							repeat1--;
						}
						else
						{
							seqPos++;
							repeat1 = -1;
						}
					}

					/*Play note*/
					else
					{
						curNote = TaitoNoteVals[command[0] & 0x7F] + 24;
						curNoteLen = chanSpeed * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						noteFlag = 0;
						seqPos++;
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}

}

/*Convert the song data to MIDI*/
void Taitosong2midMB(int songNum, long ptr)
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
	int octave = 0;
	int transpose = 0;
	long jumpPos1 = 0;
	long jumpPosRet1 = 0;
	long jumpPos2 = 0;
	long jumpPosRet2 = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	unsigned char command[4];
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
	int repeat = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int noteFlag = 0;
	unsigned int chanFlags[4];

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (curTrack = 0; curTrack < 4; curTrack++)
	{
		seqPtrs[curTrack] = 0x0000;
	}

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
		/*Get the channel pointers*/
		romPos = ptr;
		trackCnt = romData[romPos];
		if (trackCnt > 4)
		{
			trackCnt = 4;
		}
		romPos++;
		for (j = 0; j < trackCnt; j++)
		{
			tempByte = romData[romPos] >> 4;

			switch (tempByte)
			{
			case 0x00:
				curTrack = 0;
				break;
			case 0x01:
				curTrack = 1;
				break;
			case 0x02:
				curTrack = 2;
				break;
			case 0x03:
				curTrack = 3;
				break;
			case 0x04:
				curTrack = 0;
				break;
			case 0x05:
				curTrack = 1;
				break;
			case 0x06:
				curTrack = 2;
				break;
			case 0x07:
				curTrack = 3;
				break;
			default:
				curTrack = 0;
				break;
			}
			seqPtrs[curTrack] = ReadLE16(&romData[romPos + 1]);
			chanFlags[curTrack] = tempByte;
			romPos += 3;

		}

		trackCnt = 4;

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
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			noteFlag = 0;

			repeat1 = -1;

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

			if (chanFlags[curTrack] == 0)
			{
				if (songNum >= 47)
				{
					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, (7 * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
				}
				else
				{
					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, (5 * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
				}

			}
			else if (chanFlags[curTrack] == 1)
			{
				if (songNum >= 47)
				{
					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, (6 * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
				}
				else
				{
					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, (4 * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
				}

			}
			else if (chanFlags[curTrack] == 2)
			{
				if (songNum >= 47)
				{
					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, (7 * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
				}
				else
				{
					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, (5 * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
				}

			}
			else if (chanFlags[curTrack] == 3)
			{
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, (6 * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
			}
			else
			{
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, (4 * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
			}


			if (seqPtrs[curTrack] != 0x0000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];

				while (seqEnd == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];

					/*End of channel*/
					if (command[0] == 0x00)
					{
						seqEnd = 1;
					}

					/*Set note length*/
					else if (command[0] >= 0x01 && command[0] <= 0x8B)
					{
						if (noteFlag == 0)
						{
							chanSpeed = command[0];
							noteFlag = 1;
						}
						else
						{
							curDelay += (chanSpeed * 5);
							chanSpeed = command[0];
						}
						seqPos++;

					}

					/*Set volume*/
					else if (command[0] == 0x8C)
					{
						/*Repeat the following section*/
						if (drvVers > 1)
						{
							repeat1 = command[1];
							repeatStart = seqPos + 2;
						}
						seqPos += 2;
					}

					/*Set sweep*/
					else if (command[0] == 0x8D)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0x8E)
					{
						seqPos += 2;
					}

					/*Set tuning and play note*/
					else if (command[0] == 0x8F)
					{
						if (drvVers < 3)
						{
							curNote = TaitoNoteVals[command[1] & 0x7F] + 24;
							curNoteLen = chanSpeed * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							noteFlag = 0;
							seqPos += 3;
						}
						else
						{
							seqPos += 2;
						}

					}

					/*Set noise*/
					else if (command[0] == 0x9C)
					{
						curNote = 60;
						curNoteLen = chanSpeed * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						noteFlag = 0;
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0x9D)
					{
						seqPos += 2;
					}

					/*Set panning*/
					else if (command[0] == 0x9E)
					{
						seqPos += 2;
					}

					/*Set global volume*/
					else if (command[0] == 0x9F)
					{
						seqPos += 2;
					}

					/*Go to song loop*/
					else if (command[0] == 0xAC)
					{
						seqEnd = 1;
					}

					/*Set length/envelope*/
					else if (command[0] == 0xAD)
					{
						seqPos += 2;
					}

					/*Repeat note*/
					else if (command[0] == 0xAE)
					{
						if (drvVers == 1)
						{
							curNoteLen = chanSpeed * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							noteFlag = 0;
							seqPos++;
						}
						else
						{
							seqPos += 2;
						}
					}

					/*Unknown command AF*/
					else if (command[0] == 0xAF)
					{
						seqPos++;
					}

					/*Unknown command BC*/
					else if (command[0] == 0xBC)
					{
						seqPos++;
					}

					/*Unknown command BD*/
					else if (command[0] == 0xBD)
					{
						seqPos++;
					}

					/*Unknown command BE*/
					else if (command[0] == 0xBE && drvVers > 1)
					{
						seqPos++;
					}

					/*Unknown command EC*/
					else if (command[0] == 0xEC && drvVers > 2)
					{
						seqPos++;
					}

					/*Unknown command ED*/
					else if (command[0] == 0xED && drvVers > 2)
					{
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0xEE)
					{
						curDelay += (chanSpeed * 5);
						noteFlag = 0;
						seqPos++;
					}

					/*End of repeat section*/
					else if (command[0] == 0xEF && drvVers > 1)
					{
						if (repeat1 > 1)
						{
							seqPos = repeatStart;
							repeat1--;
						}
						else
						{
							seqPos++;
							repeat1 = -1;
						}
					}

					/*Play note*/
					else
					{
						curNote = TaitoNoteVals[command[0] & 0x7F] + 24;
						curNoteLen = chanSpeed * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 0;
						midPos = tempPos;
						curDelay = 0;
						noteFlag = 0;
						seqPos++;
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
			free(exRomData);
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