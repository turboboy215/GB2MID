/*Nintendo (Hirokazu Tanaka)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "NINTENDO.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
long songLoc;
int i, j, k, z;
int stepSize;
char outfile[1000000];
int songNum;
long songPtrs[4];
long speedPtr;
long speedPtr2;
long bankAmt;
long nextPtr;
int highestSeq;
int curVol;
int endList = 0;
int curStepTab[16];
unsigned static char* romData;
unsigned static char* midData;
unsigned static char* ctrlMidData;
unsigned long seqList[30];
unsigned long patList[30];
unsigned long songList[90];

long midLength;
long switchPoint1[400][2];
long switchPoint2[400][2];
int switchNum1 = 0;
int switchNum2 = 0;

int totalLen = 0;

int songTrans = 0;
int startTrans = 0;
int setFix;

int multiBanks;
int curBank;
char folderName[30];

/*Most common variant*/
const unsigned char NintMagicBytes[3] = { 0xE6, 0x1F, 0xCD };

/*Lunar Chase/Super Mario Land 2*/
const unsigned char NintMagicBytes2[3] = { 0xE6, 0x3F, 0xCD };

/*Game Boy Wars*/
const unsigned char NintMagicBytes3[5] = { 0xC3, 0x78, 0x77, 0x47, 0x21 };

/*Game Boy Camera*/
const unsigned char NintMagicBytes4[3] = { 0xE6, 0x7F, 0xCD };

/*Re-maps for MIDI note values*/
const int NintnoteVals[154] =
{
	0, 0,  																				/*0x00-0x01 (too low)*/
	12, 0, 13, 0, 14, 0, 15, 0, 16, 0, 17, 0, 18, 0, 19, 0, 20, 0, 21, 0, 22, 0, 23, 0, /*0x02-0x19 - Octave 0*/
	24, 0, 25, 0, 26, 0, 27, 0, 28, 0, 29, 0, 30, 0, 31, 0, 32, 0, 33, 0, 34, 0, 35, 0,	/*0x1A-0x31 - Octave 1*/
	36, 0, 37, 0, 38, 0, 39, 0, 40, 0, 41, 0, 42, 0, 43, 0, 44, 0, 45, 0, 46, 0, 47, 0, /*0x32-0x49 - Octave 2*/
	48, 0, 49, 0, 50, 0, 51, 0, 52, 0, 53, 0, 54, 0, 55, 0, 56, 0, 57, 0, 58, 0, 59, 0, /*0x4A-0x61 - Octave 3*/
	60, 0, 61, 0, 62, 0, 63, 0, 64, 0, 65, 0, 66, 0, 67, 0, 68, 0, 69, 0, 70, 0, 71, 0, /*0x62-0x79 - Octave 4*/
	72, 0, 73, 0, 74, 0, 75, 0, 76, 0, 77, 0, 78, 0, 79, 0, 80, 0, 81, 0, 82, 0, 83, 0, /*0x7A-0x91 - Octave 5*/
};

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Nintsong2mid(int songNum, long ptrList[4], long speedPtr, int songTrans);
void NintgetLength(long ptrList[4]);
void NintProc(int bank, char parameters[4][50]);

void NintProc(int bank, char parameters[4][50])
{
	if (bank != 1)
	{
		bankAmt = bankSize;
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}

	else
	{
		bankAmt = 0;
		fseek(rom, ((bank - 1) * bankSize * 2), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize * 2, rom);
	}

	if (parameters[0][0] == '1')
	{
		setFix = 1;
	}
	else
	{
		setFix = 0;
	}

	/*Try to search the bank for base table*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes, 3) && ReadLE16(&romData[i + 3]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = bankAmt + i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = bankAmt + i - 2;
			}

			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	/*If not found, try again for another version*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes2, 3) && ReadLE16(&romData[i + 3]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = bankAmt + i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = bankAmt + i - 2;
			}

			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	/*Another version*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes3, 5) && ReadLE16(&romData[i + 5]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = bankAmt + i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = bankAmt + i + 5;
			}

			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	/*Yet another version*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], NintMagicBytes4, 3) && ReadLE16(&romData[i + 3]) < 0x8000))
		{
			/*Super Mario Land*/
			if (romData[i - 1] == 0x78)
			{
				tablePtrLoc = bankAmt + i - 3;
			}
			/*Tetris/Dr. Mario/etc.*/
			else
			{
				tablePtrLoc = bankAmt + i - 2;
			}

			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}
	if (tableOffset != 0)
	{
		for (i = 0; i < 30; i++)
		{
			seqList[i] = 0;
		}
		for (i = 0; i < 30; i++)
		{
			patList[i] = 0;
		}
		for (i = 0; i < 90; i++)
		{
			songList[i] = 0;
		}
		k = 0;
		songNum = 1;
		i = tableOffset;
		highestSeq = 0;
		z = 0;
		endList = 0;

		while (ReadLE16(&romData[i - bankAmt]) < (bankSize * 2) && endList == 0)
		{
			songLoc = ReadLE16(&romData[i - bankAmt]);

			if (ReadLE16(&romData[songLoc + 1 - bankAmt]) < (bankSize * 2) && ReadLE16(&romData[songLoc + 1 - bankAmt]) > bankAmt && songLoc > bankAmt)
			{

				if (songNum == 1)
				{
					stepSize = ReadLE16(&romData[songLoc + 1 - bankAmt]);
					startTrans = romData[songLoc - bankAmt];
				}

				/*"Fix" for Super Mario Land 2*/
				if (stepSize == 0x5C61)
				{
					printf("Song %i: %04X\n", songNum, songLoc);
					songTrans = (signed char)romData[songLoc - bankAmt] / 2;
					printf("Song %i transpose: %i\n", songNum, songTrans);
					speedPtr = ReadLE16(&romData[songLoc + 1 - bankAmt]);
					printf("Song %i step table 1: %04X\n", songNum, speedPtr);
					speedPtr2 = ReadLE16(&romData[songLoc + 3 - bankAmt]);
					printf("Song %i step table 2: %04X\n", songNum, speedPtr2);
					songPtrs[0] = ReadLE16(&romData[songLoc + 5 - bankAmt]);
					printf("Song %i channel 1: %04X\n", songNum, songPtrs[0]);
					songPtrs[1] = ReadLE16(&romData[songLoc + 7 - bankAmt]);
					printf("Song %i channel 2: %04X\n", songNum, songPtrs[1]);
					songPtrs[2] = ReadLE16(&romData[songLoc + 9 - bankAmt]);
					printf("Song %i channel 3: %04X\n", songNum, songPtrs[2]);
					songPtrs[3] = ReadLE16(&romData[songLoc + 11 - bankAmt]);
					printf("Song %i channel 4: %04X\n", songNum, songPtrs[3]);

					patList[k] = songPtrs[0];
					patList[k + 1] = songPtrs[1];
					patList[k + 2] = songPtrs[2];
					patList[k + 3] = songPtrs[3];

					songList[z] = songLoc;

					i += 2;
					k += 4;
					z++;
				}

				else
				{
					printf("Song %i: %04X\n", songNum, songLoc);
					songTrans = (signed char)romData[songLoc - bankAmt] / 2;
					printf("Song %i transpose: %i\n", songNum, songTrans);
					speedPtr = ReadLE16(&romData[songLoc + 1 - bankAmt]);
					printf("Song %i step table: %04X\n", songNum, speedPtr);
					songPtrs[0] = ReadLE16(&romData[songLoc + 3 - bankAmt]);
					printf("Song %i channel 1: %04X\n", songNum, songPtrs[0]);
					songPtrs[1] = ReadLE16(&romData[songLoc + 5 - bankAmt]);
					printf("Song %i channel 2: %04X\n", songNum, songPtrs[1]);
					songPtrs[2] = ReadLE16(&romData[songLoc + 7 - bankAmt]);
					printf("Song %i channel 3: %04X\n", songNum, songPtrs[2]);
					songPtrs[3] = ReadLE16(&romData[songLoc + 9 - bankAmt]);
					printf("Song %i channel 4: %04X\n", songNum, songPtrs[3]);

					patList[k] = songPtrs[0];
					patList[k + 1] = songPtrs[1];
					patList[k + 2] = songPtrs[2];
					patList[k + 3] = songPtrs[3];

					songList[z] = songLoc;

					i += 2;
					k += 4;
					z++;
				}

				Nintsong2mid(songNum, songPtrs, speedPtr, songTrans);
				songNum++;
			}
			else
			{
				endList = 1;
			}

		}

		free(romData);
	}
}

void Nintsong2mid(int songNum, long ptrList[4], long speedPtr, int songTrans)
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
	long command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int curTrack = 0;
	int endSeq = 0;
	int endChan = 0;
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

	long tempo = 150;

	int curInst = 0;

	int valSize = 0;

	long trackSize = 0;
	int repeat = 0;
	long repeatPt = 0;
	int lowestChan = 0;
	int lowestPat = 0;
	int length = 0;
	int songLen = 0;
	int curTrack2 = 0;
	int restLen = 0;

	int c1Pos = 0;
	int c2Pos = 0;
	int c3Pos = 0;
	int c4Pos = 0;
	int c1Len = 0;
	int c2Len = 0;
	int c3Len = 0;
	int c4Len = 0;
	int endCnt = 0;
	int endCnt1 = 0;
	int endCnt2 = 0;
	int endCnt3 = 0;
	int endCnt4 = 0;

	int startSeq = 0;

	long stepPtr = 0;

	midPos = 0;
	ctrlMidPos = 0;

	switchNum1 = 0;
	switchNum2 = 0;
	totalLen = 0;

	highestSeq = 0;

	for (switchNum1 = 0; switchNum1 < 400; switchNum1++)
	{
		switchPoint1[switchNum1][0] = -1;
		switchPoint1[switchNum1][1] = 0;
	}

	for (switchNum2 = 0; switchNum2 < 400; switchNum2++)
	{
		switchPoint2[switchNum2][0] = -1;
		switchPoint2[switchNum2][1] = 0;
	}

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

		c1Pos = songPtrs[0];
		c2Pos = songPtrs[1];
		c3Pos = songPtrs[2];
		c4Pos = songPtrs[3];

		endCnt2 = 0;

		while (endCnt == 0)
		{
			if (ReadLE16(&romData[c1Pos - bankAmt]) == 0x0000 ||
				ReadLE16(&romData[c2Pos - bankAmt]) == 0x0000 ||
				ReadLE16(&romData[c3Pos - bankAmt]) == 0x0000 ||
				ReadLE16(&romData[c4Pos - bankAmt]) == 0x0000)
			{
				songLen = length;
				NintgetLength(songPtrs);
				endCnt = 1;

			}

			if (ReadLE16(&romData[c1Pos - bankAmt]) == 0xFFFF ||
				ReadLE16(&romData[c2Pos - bankAmt]) == 0xFFFF ||
				ReadLE16(&romData[c3Pos - bankAmt]) == 0xFFFF ||
				ReadLE16(&romData[c4Pos - bankAmt]) == 0xFFFF)
			{
				endCnt = 1;
			}

			else
			{
				c1Pos += 2;
				c2Pos += 2;
				c3Pos += 2;
				c4Pos += 2;
				length++;
			}
		}

		for (curSeq = 0; curSeq < 30; curSeq++)
		{
			seqList[curSeq] = -1;
		}

		for (curSeq = 0; curSeq < 30; curSeq++)
		{
			patList[curSeq] = -1;
		}

		for (switchNum1 = 0; switchNum1 < 400; switchNum1++)
		{
			switchPoint1[switchNum1][0] = -1;
			switchPoint1[switchNum1][1] = 0;
		}

		for (switchNum2 = 0; switchNum2 < 400; switchNum2++)
		{
			switchPoint2[switchNum2][0] = -1;
			switchPoint2[switchNum2][1] = 0;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
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

			switchNum1 = 0;
			switchNum2 = 0;

			repeat = 0;

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

			/*Get the step table*/
			for (j = 0; j < 16; j++)
			{
				curStepTab[j] = romData[speedPtr - bankAmt + j] * 5;
			}



			endChan = 0;
			endSeq = 0;
			length = 0;
			patPos = songPtrs[curTrack] - bankAmt;

			if (songPtrs[curTrack] == 0)
			{
				endChan = 1;
			}

			while (endChan == 0 && seqPos < 0x8000)
			{
				curSeq = ReadLE16(&romData[patPos]);
				if (curSeq != 0x0000 && curSeq != 0xFFFF)
				{
					endSeq = 0;
					startSeq = 1;
					if (length >= songLen)
					{
						if (songLen != 0)
						{
							endChan = 1;
						}
					}
					seqPos = curSeq - bankAmt;
					for (j = 0; j < 30; j++)
					{
						if (seqList[j] == curSeq)
						{
							break;
						}
					}
					if (j == 30)
					{
						seqList[highestSeq] = curSeq;
						highestSeq++;
					}
					while (endSeq == 0 && seqPos < bankAmt)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];

						/*Transpose check code*/
						if (curTrack != 0 && curTrack != 3)
						{
							for (switchNum1 = 0; switchNum1 < 400; switchNum1++)
							{
								if (switchPoint1[switchNum1][0] == masterDelay)
								{
									transpose = switchPoint1[switchNum1][1];
								}
							}
						}

						/*Step table check code*/
						if (curTrack != 0 && curTrack != 3)
						{
							for (switchNum2 = 0; switchNum2 < 400; switchNum2++)
							{
								if (switchPoint2[switchNum2][0] == masterDelay)
								{
									curStepTab[k] = (romData[stepPtr + k - bankAmt]) * 5;
								}
							}
						}


						/*Workarounds for "invalid" ends of some songs*/
						for (k = 0; k < 30; k++)
						{
							if (seqPos == patList[k])
							{
								endSeq = 1;
							}
						}

						for (z = 0; z < 90; z++)
						{
							if (seqPos == songList[z])
							{
								endSeq = 1;
							}
						}

						for (k = 0; k < 30; k++)
						{
							if ((seqPos == seqList[k] - bankAmt) && startSeq == 0)
							{
								endSeq = 1;
							}
						}

						if (seqPos < 0x0200)
						{
							endSeq = 1;
						}

						if (masterDelay >= totalLen)
						{
							if (totalLen != 0)
							{
								endSeq = 1;
								endChan = 1;
							}
						}

						/*End of sequence*/
						if (command[0] == 0x00)
						{
							endSeq = 1;
						}

						/*Rest (channels 1-3) or noise type (channel 4), also note value for even numbers*/
						else if (command[0] > 0x00 && command[0] < 0x10)
						{
							if (command[0] % 2 && curTrack != 3)
							{
								restLen = curNoteLen * command[0];
								curDelay += curNoteLen * command[0];
								ctrlDelay += curNoteLen * command[0];
								masterDelay += curNoteLen * command[0];
								seqPos++;
							}
							else
							{
								if (curTrack != 3)
								{
									curNote = NintnoteVals[command[0]] + 20;
									curNote += songTrans;
								}
								else
								{
									curNote = command[0] + 30;
								}

								if (curNote == 31 && curTrack == 3)
								{
									restLen = curNoteLen * command[0];
									curDelay += curNoteLen * command[0];
									ctrlDelay += curNoteLen * command[0];
									masterDelay += curNoteLen * command[0];
									seqPos++;
								}
								else
								{
									ctrlDelay += curNoteLen;
									masterDelay += curNoteLen;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									seqPos++;
								}

							}
						}

						/*Repeat point start*/
						else if (command[0] == 0x9B)
						{
							repeat = command[1];
							repeatPt = seqPos + 2;
							seqPos += 2;
						}

						/*End of repeat point*/
						else if (command[0] == 0x9C)
						{
							if (repeat > 1)
							{
								seqPos = repeatPt;
								repeat--;
							}
							else
							{
								seqPos++;
							}
						}

						/*Set parameters*/
						else if (command[0] == 0x9D)
						{
							seqPos += 4;
						}

						/*Change step table*/
						else if (command[0] == 0x9E)
						{
							stepPtr = ReadLE16(&romData[seqPos + 1]);
							for (k = 0; k < 16; k++)
							{
								curStepTab[k] = (romData[stepPtr + k - bankAmt]) * 5;
							}
							switchPoint2[switchNum2][0] = masterDelay;
							switchPoint2[switchNum2][1] = stepPtr;
							switchNum2++;
							seqPos += 3;
						}

						/*Transpose (all channels)*/
						else if (command[0] == 0x9F)
						{
							transpose = (signed char)command[1] / 2;
							if (curTrack == 0)
							{
								switchPoint1[switchNum1][0] = masterDelay;
								switchPoint1[switchNum1][1] = transpose;
								switchNum1++;
							}
							seqPos += 2;
						}

						/*Set note length*/
						else if (command[0] >= 0xA0)
						{
							curNoteLen = curStepTab[command[0] - 0xA0];
							seqPos++;
						}

						/*Play note*/
						else if (command[0] >= 0x10 && command[0] <= 0x90)
						{

							if (curTrack != 3)
							{
								curNote = NintnoteVals[(command[0] - 0x10)] + 20;
								curNote += transpose;
								curNote += songTrans;
							}
							else
							{
								curNote = command[0] + 10;
							}
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}
						else if (command[0] > 0x90 && command[0] <= 0x9A)
						{
							if (setFix == 1)
							{
								seqPos++;
							}
							else
							{
								seqPos += 2;
							}
						}


					}
					patPos += 2;
					length++;
				}
				else if (curSeq == 0x0000)
				{
					songLen = length;
					endChan = 1;
				}
				else if (curSeq == 0xFFFF)
				{
					endChan = 1;
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
		fclose(mid);
		free(midData);
		free(ctrlMidData);
	}
}

void NintgetLength(long ptrList[4])
{
	int curChan = 0;
	long lowestPat = 0;
	int repeat = 0;
	int delay = 0;
	int patPos = 0;
	int seqPos = 0;
	int curSeq = 0;
	long command[4];
	int endSeq = 0;
	int endChan = 0;
	int startSeq = 0;
	int curNoteLen = 0;
	int repeatPt = 0;
	totalLen = 0;

	/*Find the lowest pointer*/
	for (curChan == 0; curChan < 4; curChan++)
	{
		if (songPtrs[curChan] < songPtrs[curChan - 1])
		{
			if (songPtrs[curChan] != 0 && curChan != 0)
			{
				lowestPat = songPtrs[curChan];
				break;
			}
		}

	}

	if (curChan == 4)
	{
		lowestPat = songPtrs[0];
	}

	/*Get the step table*/
	for (j = 0; j < 16; j++)
	{
		curStepTab[j] = romData[speedPtr - bankAmt + j] * 5;
	}

	patPos = lowestPat - bankAmt;

	endChan = 0;
	endSeq = 0;
	if (lowestPat == 0)
	{
		endChan = 1;
	}

	while (endChan == 0)
	{
		curSeq = ReadLE16(&romData[patPos]);
		if (curSeq != 0x0000 && curSeq != 0xFFFF)
		{
			endSeq = 0;
			startSeq = 1;
			seqPos = curSeq - bankAmt;
			for (j = 0; j < 30; j++)
			{
				if (seqList[j] == curSeq)
				{
					break;
				}
			}
			if (j == 30)
			{
				seqList[highestSeq] = curSeq;
				highestSeq++;
			}
			while (endSeq == 0)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				/*Step table check code*/
				if (curChan != 0 && curChan != 3)
				{
					for (switchNum2 = 0; switchNum2 < 400; switchNum2++)
					{
						if (switchPoint2[switchNum2][0] == delay)
						{
							curStepTab[k] = (romData[speedPtr + k - bankAmt]) * 5;
						}
					}
				}

				/*End of sequence*/
				if (command[0] == 0x00)
				{
					endSeq = 1;
				}

				/*Rest (channels 1-3) or noise type (channel 4)*/
				else if (command[0] > 0x00 && command[0] < 0x10)
				{
					if (curChan != 3)
					{
						delay += curNoteLen * command[0];
						seqPos++;
					}
					else
					{
						delay += curNoteLen;
						seqPos++;
					}

				}

				/*Repeat point start*/
				else if (command[0] == 0x9B)
				{
					repeat = command[1];
					repeatPt = seqPos + 2;
					seqPos += 2;
				}

				/*End of repeat point*/
				else if (command[0] == 0x9C)
				{
					if (repeat > 1)
					{
						seqPos = repeatPt;
						repeat--;
					}
					else
					{
						seqPos++;
					}
				}

				/*Set parameters*/
				else if (command[0] == 0x9D)
				{
					seqPos += 4;
				}

				/*Change step table*/
				else if (command[0] == 0x9E)
				{
					speedPtr = ReadLE16(&romData[seqPos + 1]);
					for (k = 0; k < 16; k++)
					{
						curStepTab[k] = (romData[speedPtr + k - bankAmt]) * 5;
					}
					switchPoint2[switchNum2][0] = delay;
					switchPoint2[switchNum2][1] = speedPtr;
					switchNum2++;
					seqPos += 3;
				}

				/*Transpose (all channels)*/
				else if (command[0] == 0x9F)
				{
					seqPos += 3;
				}

				/*Set note length*/
				else if (command[0] >= 0xA0)
				{
					curNoteLen = curStepTab[command[0] - 0xA0];
					seqPos++;
				}

				/*Play note*/
				else if (command[0] >= 0x10 && command[0] <= 0x9A)
				{
					delay += curNoteLen;
					seqPos++;
				}
			}
			patPos += 2;
		}

		else if (curSeq == 0x0000)
		{
			totalLen = delay;
			endChan = 1;
		}
		else if (curSeq == 0xFFFF)
		{
			endChan = 1;
		}
	}

}