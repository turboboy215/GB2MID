/*AudioArts*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "AUDIOART.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long seqPtrLoc;
long seqOffset;
int i, j;
char outfile[1000000];
int format = 0;
int songNum;
long seqPtrs[4];
long curSpeed;
long nextPtr;
long endPtr;
long bankAmt;
int multiBanks;
int formatFix = 0;

char folderName[100];

const char Table1Find[15] = { 0x16, 0x00, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19 };
const char Table2Find[12] = { 0x1A, 0x22, 0x13, 0x1A, 0x13, 0x73, 0x2C, 0x72, 0x5F, 0x16, 0x00, 0x21 };

/*Alternate method for another variation of the driver (e.g. Extreme Ghostbusters, Barca, Wendy)*/
const char Table2FindAlt[4] = { 0x72, 0x59, 0x57, 0x21 };

/*Alternate code to look for in Carmageddon (early driver?)*/
const char Table1FindCarma[9] = { 0x16, 0x00, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x2A };
const char Table2FindCarma[13] = { 0x1A, 0x77, 0x13, 0x1A, 0x13, 0x23, 0x73, 0x23, 0x72, 0x5F, 0x16, 0x00, 0x21 };

/*Look for these bytes since there isn't a standard table in the US version of Casper*/
const char Table1FindCU[10] = { 0xE0, 0x66, 0xCA, 0x4A, 0x36, 0x4B, 0xA2, 0x4B, 0x0E, 0x4C };

unsigned long seqList[500];
int totalSeqs;

unsigned char* romData;
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
void AAsong2mid(int songNum, long ptrs[4], long nextPtr, long endPtr, int speed);
void AAgetSeqList(unsigned long list[], long offset);

void AAProc(int bank)
{
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

	/*Try to search the bank for song pattern table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table1Find, 15))
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			if (romData[i + 15] == 0x2A)
			{
				format = 1;
				printf("Detected old table format.\n");
			}
			else if (romData[i + 15] == 0x19)
			{
				format = 2;
				printf("Detected new table format.\n");
			}
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	/*Alternate method for Carmageddon*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table1FindCarma, 9) && format != 1 && format != 2)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			format = 0;
			printf("Detected old (Carmageddon) table format.\n");
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	/*Now try to search the bank for sequence table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2Find, 12))
		{
			seqPtrLoc = bankAmt + i + 12;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc - bankAmt]);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			AAgetSeqList(seqList, seqOffset);
			break;
		}
	}

	/*Alternate method for Carmageddon*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2FindCarma, 13) && format != 1 && format != 2)
		{
			seqPtrLoc = bankAmt + i + 13;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc - bankAmt]);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			AAgetSeqList(seqList, seqOffset);
			break;
		}
	}

	/*Alternate method for Total Soccer 2000/Barca/Wendy*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2FindAlt, 4))
		{
			seqPtrLoc = bankAmt + i + 4;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc - bankAmt]);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			AAgetSeqList(seqList, seqOffset);
			formatFix = 1;
			break;
		}
	}

	/*Workaround for Casper (US)*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table1FindCU, 10) && format != 1 && format != 2)
		{
			tablePtrLoc = 0x4952;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = 0x4952;
			format = 1;
			printf("Detected old table format.\n");
			printf("Song table starts at 0x%04x...\n", tableOffset);
			break;
		}
	}

	if (tableOffset != NULL && seqOffset != NULL)
	{
		songNum = 1;
		i = tableOffset;

		if (multiBanks != 0)
		{
			snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
			_mkdir(folderName);
		}

		if (format == 1)
		{
			i -= 4;

			while ((i + 4) != seqOffset)
			{
				curSpeed = romData[i + 12 - bankAmt];
				printf("Song %i tempo: %i\n", songNum, curSpeed);
				seqPtrs[0] = ReadLE16(&romData[i + 4 - bankAmt]);
				printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 6 - bankAmt]);
				printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[i + 8 - bankAmt]);
				printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[i + 10 - bankAmt]);
				printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
				nextPtr = ReadLE16(&romData[i + 17 - bankAmt]);
				if (nextPtr < bankAmt)
				{
					AAsong2mid(songNum, seqPtrs, nextPtr, endPtr, curSpeed);
					break;
				}
				endPtr = seqOffset;
				AAsong2mid(songNum, seqPtrs, nextPtr, endPtr, curSpeed);
				i += 13;
				songNum++;
			}
		}
		else if (format == 2)
		{
			/*Skip first "empty" song*/
			i += 14;

			while (i != seqOffset)
			{

				curSpeed = romData[i - bankAmt];
				printf("Song %i tempo: %i\n", songNum, curSpeed);
				seqPtrs[0] = ReadLE16(&romData[i + 2 - bankAmt]);
				printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 4 - bankAmt]);
				printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[i + 6 - bankAmt]);
				printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[i + 8 - bankAmt]);
				printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
				nextPtr = ReadLE16(&romData[i + 16 - bankAmt]);
				endPtr = seqOffset;
				AAsong2mid(songNum, seqPtrs, nextPtr, endPtr, curSpeed);
				i += 14;
				songNum++;
			}
		}

		/*Carmageddon format - doesn't use channel 3 for music*/
		else if (format == 0)
		{
			while (romData[i - bankAmt] != 0)
			{
				curSpeed = 7;
				seqPtrs[0] = ReadLE16(&romData[i - bankAmt]);
				printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 2 - bankAmt]);
				printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
				seqPtrs[2] = 0;
				seqPtrs[3] = ReadLE16(&romData[i + 4 - bankAmt]);
				printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
				nextPtr = ReadLE16(&romData[i + 6 - bankAmt]);
				AAsong2mid(songNum, seqPtrs, nextPtr, endPtr, curSpeed);
				i += 6;
				songNum++;
			}
			endPtr = i;
		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
}

void AAsong2mid(int songNum, long ptrs[4], long nextPtr, long endPtr, int speed)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curVol = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 0;

	int curInst = 0;

	unsigned long patPos = 0;
	unsigned long seqPos = 0;

	unsigned char command[3];

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;

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

		/*Get the tempo*/
		switch (speed)
		{
		case 1:
			tempo = 960;
			break;
		case 2:
			tempo = 500;
			break;
		case 3:
			tempo = 300;
			break;
		case 4:
			tempo = 200;
			break;
		case 5:
			tempo = 170;
			break;
		case 6:
			tempo = 150;
			break;
		case 7:
			tempo = 130;
			break;
		case 8:
			tempo = 110;
			break;
		case 9:
			tempo = 100;
			break;
		case 10:
			tempo = 90;
			break;
		case 11:
			tempo = 80;
			break;
		default:
			tempo = 130;
			break;
		}

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
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
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
			patPos = ptrs[curTrack] - bankAmt;
			if (ptrs[curTrack] == 0)
			{
				trackEnd = 1;
			}

			while (trackEnd == 0)
			{
				seqEnd = 0;
				/*Get current sequence and transpose*/
				if (formatFix != 1)
				{
					transpose = (signed char)romData[patPos];
					curSeq = romData[patPos + 1];
				}
				else
				{
					transpose = (signed char)romData[patPos];
					curSeq = romData[patPos + 2];
					if (curSeq == 0xFF)
					{
						seqEnd = 1;
					}
				}


				/*Go to the current sequence*/
				seqPos = seqList[curSeq];
				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						seqEnd = 1;
					}
					/*Repeat note*/
					else if (command[1] == 0xFD && command[0] != 0xFF)
					{
						curNoteLen = command[0] * 30;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						seqPos += 2;
					}
					/*Rest*/
					else if (command[1] == 0xFE)
					{
						curDelay += (command[0] * 30);
						seqPos += 2;
					}
					/*Hold note*/
					else if (command[1] == 0xFF)
					{
						curDelay += (command[0] * 30);
						seqPos += 2;
					}
					/*Loop back to point*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
						trackEnd = 1;
					}
					/*Standard note value*/
					else
					{
						curNoteLen = command[0] * 30;
						curNote = command[1] + transpose + 24;

						if (curNote > 127)
						{
							curNote = 127;
						}
						if (curInst != command[2])
						{
							curInst = command[2];
							firstNote = 1;
						}

						if (curInst >= 128)
						{
							curInst = 127;
						}

						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos += 3;
					}
				}
				patPos += 2;
				if (formatFix == 1)
				{
					patPos += 2;
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
	}
}

void AAgetSeqList(unsigned long list[], long offset)
{
	int cnt = 0;
	unsigned long curValue;
	unsigned long curValue2;
	long newOffset = offset;
	long offset2 = offset - bankAmt;

	for (cnt = 0; cnt < 500; cnt++)
	{
		curValue = (ReadLE16(&romData[newOffset - bankAmt])) - bankAmt;
		curValue2 = (ReadLE16(&romData[newOffset - bankAmt]));
		if (curValue2 >= bankAmt && curValue2 < (bankAmt * 2))
		{
			list[cnt] = curValue;
			newOffset += 2;
		}
		else
		{
			totalSeqs = cnt;
			break;
		}
	}
}
