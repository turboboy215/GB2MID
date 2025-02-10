/*Nintendo (Ryohji Yoshitomi) (Metroid 2/Wario Land 1)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "METROID2.H"

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
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int songTranspose;
int curStepTab[16];
int curVol;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;
long switchPoint1[400][2];
long switchPoint2[400][2];
int switchNum1;
int switchNum2;

const char MagicBytesM2[6] = { 0xE0, 0x25, 0xFA, 0xDC, 0xCE, 0x21 };
const char MagicBytesWL[6] = { 0xE0, 0x25, 0xFA, 0x1C, 0xA6, 0x21 };

/*Re-maps for MIDI note values*/
const int m2noteVals[154] =
{
	0, 0,  																				/*0x00-0x01 (too low)*/
	12, 0, 13, 0, 14, 0, 15, 0, 16, 0, 17, 0, 18, 0, 19, 0, 20, 0, 21, 0, 22, 0, 23, 0, /*0x02-0x19 - Octave 0*/
	24, 0, 25, 0, 26, 0, 27, 0, 28, 0, 29, 0, 30, 0, 31, 0, 32, 0, 33, 0, 34, 0, 35, 0,	/*0x1A-0x31 - Octave 1*/
	36, 0, 37, 0, 38, 0, 39, 0, 40, 0, 41, 0, 42, 0, 43, 0, 44, 0, 45, 0, 46, 0, 47, 0, /*0x32-0x49 - Octave 2*/
	48, 0, 49, 0, 50, 0, 51, 0, 52, 0, 53, 0, 54, 0, 55, 0, 56, 0, 57, 0, 58, 0, 59, 0, /*0x4A-0x61 - Octave 3*/
	60, 0, 61, 0, 62, 0, 63, 0, 64, 0, 65, 0, 66, 0, 67, 0, 68, 0, 69, 0, 70, 0, 71, 0, /*0x62-0x79 - Octave 4*/
	72, 0, 73, 0, 74, 0, 75, 0, 76, 0, 77, 0, 78, 0, 79, 0, 80, 0, 81, 0, 82, 0, 83, 0, /*0x7A-0x91 - Octave 5*/
	0, 0, 0, 0, 0, 0, 0, 0																/*0x92-0x99 (too high)*/
};

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void m2song2mid(int songNum, long ptr);

void M2Proc(int bank)
{
	foundTable = 0;
	firstPtr = 0;
	songTranspose = 0;
	curVol = 100;
	switchNum1 = 0;
	switchNum2 = 0;

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

	/*Try to search the bank for song table loader - Metroid 2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MagicBytesM2, 6)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 6;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Wario Land*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MagicBytesWL, 6)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 6;
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
		while (ReadLE16(&romData[i]) < (bankSize * 2))
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0)
			{
				m2song2mid(songNum, songPtr);
			}
			i += 2;
			songNum++;
		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
	free(romData);
	return 0;
}

/*Convert the song data to MIDI*/
void m2song2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	long speedPtr = 0;
	int curTrack = 0;
	long chanPtrs[4];
	long curSeq = 0;
	int seqEnd = 0;
	int chanEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int repeat = 0;
	int repeatPt = 0;
	int transpose = 0;
	int tempo = 150;
	unsigned char command[4];
	long wavePos = 0;
	long loopPt = 0;
	int seqCtr = 0;
	unsigned long seqList[500];
	int k = 0;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int trackCnt = 4;
	int ticks = 120;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long tempPos = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int masterDelay = 0;
	int firstNote = 0;

	int curInst = 0;

	int valSize = 0;

	long trackSize = 0;
	int newSpeedPtr = 0;

	midPos = 0;
	ctrlMidPos = 0;

	switchNum1 = 0;
	switchNum2 = 0;
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

		romPos = ptr - bankAmt;
		songTranspose = (signed char)romData[romPos] / 2;
		romPos++;
		speedPtr = ReadLE16(&romData[romPos]);
		/*Get the step table*/
		for (j = 0; j < 16; j++)
		{
			curStepTab[j] = romData[speedPtr - bankAmt + j] * 5;
		}
		romPos += 2;
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			chanPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			transpose = 0;
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;

			curNote = 0;
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

			if (chanPtrs[curTrack] != 0 && chanPtrs[curTrack] < (bankSize * 2))
			{
				romPos = chanPtrs[curTrack] - bankAmt;
				while (ReadLE16(&romData[romPos]) != 0x00F0 && ReadLE16(&romData[romPos]) != 0x0000)
				{
					curSeq = ReadLE16(&romData[romPos]);

					seqEnd = 0;
					seqPos = curSeq - bankAmt;
					firstNote = 1;
					transpose = 0;

					while (seqEnd == 0)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];
						command[3] = romData[seqPos + 3];

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
									for (k = 0; k < 16; k++)
									{
										curStepTab[k] = (romData[switchPoint2[switchNum2][1] + k - bankAmt]) * 5;
									}
								}
							}
						}

						/*End of sequence*/
						if (command[0] == 0x00)
						{
							seqEnd = 1;
						}

						/*Rest*/
						else if (command[0] == 0x01)
						{
							curDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos++;
						}

						/*Echo (v1)*/
						else if (command[0] == 0x03)
						{
							curVol = 75;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							curVol = 100;
							seqPos++;
						}

						/*Echo (v2)*/
						else if (command[0] == 0x05)
						{
							curVol = 50;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							curVol = 100;
							seqPos++;
						}

						/*Play note*/
						else if (command[0] < 0xA0)
						{
							curNote = m2noteVals[(command[0])] + 12;
							if (curTrack == 3)
							{
								curNote += 12;
							}
							else
							{
								curNote += transpose + songTranspose;
							}
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}

						/*Change note length*/
						else if (command[0] >= 0xA0 && command[0] < 0xB0)
						{
							if (romData[seqPos - 1] >= 0xA0 && romData[seqPos - 1] < 0xB0)
							{
								curNote = 24;
								if (curTrack == 3)
								{
									curNote += 12;
								}
								ctrlDelay += curNoteLen;
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos++;
							}
							else
							{
								curNoteLen = curStepTab[command[0] - 0xA0];
							}

							seqPos++;
						}

						/*Set parameters*/
						else if (command[0] == 0xF1)
						{
							seqPos += 4;
						}

						/*Change speed table*/
						else if (command[0] == 0xF2)
						{
							if (curTrack == 0)
							{
								newSpeedPtr = ReadLE16(&romData[seqPos + 1]);
								for (k = 0; k < 16; k++)
								{
									curStepTab[k] = (romData[newSpeedPtr + k - bankAmt]) * 5;
								}
								switchPoint2[switchNum2][0] = masterDelay;
								switchPoint2[switchNum2][1] = newSpeedPtr;
								switchNum2++;
							}

							seqPos += 3;
						}

						/*Set transpose*/
						else if (command[0] == 0xF3)
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

						/*Repeat point start*/
						else if (command[0] == 0xF4)
						{
							repeat = command[1];
							repeatPt = seqPos + 2;
							seqPos += 2;
						}

						/*End of repeat point*/
						else if (command[0] == 0xF5)
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
					}
					romPos += 2;
					seqCtr++;
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