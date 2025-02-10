/*Ocean (Jonathan Dunn)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "OCEAN.H"

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
int format;
int cmdFmt;
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int seqDiff;
int addSeq;
int foundTable;
int songTempo;
int highestSeq;
int rcFix = 0;
int multiBanks;
int curBank;

char folderName[100];

unsigned long seqList[500];
unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;
int totalSeqs;

long switchPoint1[400][2];
long switchPoint2[400][2];
int switchNum1;
int switchNum2;

const char Table1FindA[7] = { 0x26, 0x00, 0x29, 0x29, 0x29, 0x7D, 0xC6 };
const char Table1FindB[10] = { 0x26, 0x00, 0x5D, 0x54, 0x29, 0x29, 0x29, 0x19, 0x7D, 0xC6 };
const char Table1FindC[7] = { 0x26, 0x00, 0x29, 0x29, 0x29, 0x7D, 0x11 };
const char Table1FindD[4] = { 0x29, 0x19, 0x7D, 0xC6 };
const char Table2FindA[6] = { 0x2B, 0x72, 0x2B, 0x73, 0x87, 0xC6 };
const char Table2FindB[8] = { 0x2B, 0x72, 0x2B, 0x73, 0x5F, 0x16, 0x00, 0x21 };
const char Table2FindC[5] = { 0x2B, 0x72, 0x2B, 0x73, 0x11 };
const char Table2FindD[9] = { 0x2B, 0x72, 0x2B, 0x73, 0x26, 0x00, 0x6F, 0x29, 0x11 };
const char Table2FindE[7] = { 0x2B, 0x72, 0x2B, 0x73, 0x87, 0x5F, 0xFA };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void OcngetSeqList(unsigned long list[], long offset);
void Ocnsong2mid(int songNum, long ptrs[4]);
void OcnNutzSong2mid(int songNum, long ptr, long tempo);
void OcnProc(int bank);

void OcnProc(int bank)
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


	/*Fix for RoboCop*/
	if (romData[0x2602] == 0x66 && romData[0x2604] == 0x10 && romData[0x2605] == 0x66)
	{
		rcFix = 1;
	}
	else
	{
		rcFix = 0;
	}

	/*Try to search the bank for song pattern table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], Table1FindC, 7)) && foundTable == 0)
		{
			tablePtrLoc = bankAmt + i + 7;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			format = 1;
			cmdFmt = 1;
			foundTable = 1;
			break;
		}
	}

	/*Try to search the bank for song pattern table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], Table1FindB, 10)) && foundTable == 0)
		{
			tablePtrLoc = bankAmt + i + 10;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc - bankAmt] + (romData[tablePtrLoc + 4 - bankAmt] * 0x100);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			format = 2;
			cmdFmt = 1;
			foundTable = 1;
			break;
		}
	}

	/*Try to search the bank for song pattern table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], Table1FindD, 4)) && foundTable == 0)
		{
			tablePtrLoc = bankAmt + i + 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc - bankAmt] + (romData[tablePtrLoc + 4 - bankAmt] * 0x100);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			format = 4;
			cmdFmt = 4;
			foundTable = 1;
			break;
		}
	}

	/*Try to search the bank for song pattern table loader*/
	if (format != 2)
	{
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], Table1FindA, 7)) && foundTable == 0)
			{
				tablePtrLoc = bankAmt + i + 7;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = romData[tablePtrLoc - bankAmt] + (romData[tablePtrLoc + 4 - bankAmt] * 0x100);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				format = 1;
				foundTable = 1;
				break;
			}
		}
	}


	/*Now try to search the bank for sequence table loader*/

	/*Method 1 - RoboCop*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2FindA, 6))
		{
			seqPtrLoc = bankAmt + i + 6;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = romData[seqPtrLoc - bankAmt] + (romData[seqPtrLoc + 3 - bankAmt] * 0x100);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			if (cmdFmt == 0)
			{
				cmdFmt = 1;
			}
			break;

		}
	}

	/*Method 2 - RoboCop 2*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2FindB, 8))
		{
			seqPtrLoc = bankAmt + i + 8;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc - bankAmt]);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			cmdFmt = 2;
			seqDiff = 0;
			break;

		}
	}

	/*Method 3 - Beauty and the Beast*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2FindC, 5))
		{
			seqPtrLoc = bankAmt + i + 5;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc - bankAmt]);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			cmdFmt = 2;
			seqDiff = 1;
			break;

		}
	}

	/*Method 4 - Parasol Stars*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2FindD, 9))
		{
			seqPtrLoc = bankAmt + i + 9;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc - bankAmt]);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			cmdFmt = 3;
			break;

		}
	}

	/*Method 5 - Super James Pond*/
	for (i = 0; i < bankSize; i++)
	{
		if (!memcmp(&romData[i], Table2FindE, 7))
		{
			seqPtrLoc = bankAmt + i + 11;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqPtrLoc);
			seqOffset = romData[seqPtrLoc - bankAmt] + (romData[seqPtrLoc + 3 - bankAmt] * 0x100);
			printf("Sequence table starts at 0x%04x...\n", seqOffset);
			cmdFmt = 1;
			addSeq = 1;
			break;

		}
	}


	if (tableOffset != NULL)
	{
		songNum = 1;
		i = tableOffset;
		OcngetSeqList(seqList, seqOffset);
		if (format == 1)
		{
			while (i != seqOffset && ReadLE16(&romData[i - bankAmt]) > bankSize)
			{
				seqPtrs[0] = ReadLE16(&romData[i - bankAmt]);
				printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 2 - bankAmt]);
				printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[i + 4 - bankAmt]);
				printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[i + 6 - bankAmt]);
				printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
				Ocnsong2mid(songNum, seqPtrs);
				songNum++;
				i += 8;
			}
		}
		else if (format == 2)
		{
			while (i != seqOffset && ReadLE16(&romData[i - bankAmt]) > bankSize)
			{
				seqPtrs[0] = ReadLE16(&romData[i - bankAmt]);
				printf("Song %i channel 1: 0x%04x\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 2 - bankAmt]);
				printf("Song %i channel 2: 0x%04x\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[i + 4 - bankAmt]);
				printf("Song %i channel 3: 0x%04x\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[i + 6 - bankAmt]);
				printf("Song %i channel 4: 0x%04x\n", songNum, seqPtrs[3]);
				songTempo = romData[i + 8 - bankAmt];
				printf("Song %i tempo: %i\n", songNum, songTempo);
				Ocnsong2mid(songNum, seqPtrs);
				songNum++;
				i += 9;
			}
		}

		else if (format == 4)
		{
			i = tableOffset;
			songNum = 1;
			while (ReadLE16(&romData[i + 1 - bankAmt]) != 0)
			{
				songTempo = romData[i - bankAmt];
				printf("Song %i tempo: %i\n", songNum, songTempo);
				songPtr = ReadLE16(&romData[i + 1 - bankAmt]);
				printf("Song %i location: 0x%04X\n", songNum, songPtr);
				OcnNutzSong2mid(songNum, songPtr, songTempo);
				songNum++;
				i += 3;
			}
		}

	}
	free(romData);
}

void OcngetSeqList(unsigned long list[], long offset)
{
	int cnt = 0;
	unsigned long curValue;
	unsigned long curValue2;
	long newOffset = offset;
	long offset2 = offset - bankAmt;

	for (cnt = 0; cnt < 500; cnt++)
	{
		curValue = 0;
		curValue2 = 0;

		if (newOffset > 0)
		{
			curValue = (ReadLE16(&romData[newOffset - bankAmt])) - bankAmt;
			curValue2 = (ReadLE16(&romData[newOffset - bankAmt]));
		}
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

void Ocnsong2mid(int songNum, long ptrs[4])
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
	int globalTranspose = 0;
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

	long tempo = 150;

	int curInst = 0;

	unsigned long patPos = 0;
	unsigned long seqPos = 0;

	unsigned char command[4];
	unsigned char pattern[4];

	signed int transpose = 0;
	int repeat = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;
	int automatic = 0;

	long masterDelay = 0;

	int totalLen = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midPos = 0;
	ctrlMidPos = 0;

	switchNum1 = 0;
	switchNum2 = 0;

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

		if (format == 2)
		{
			if (songTempo == 1)
			{
				tempo = 70;
			}
			else if (songTempo == 2)
			{
				tempo = 100;
			}
			else if (songTempo == 3)
			{
				tempo = 110;
			}
			else if (songTempo == 4)
			{
				tempo = 120;
			}
			else if (songTempo == 5)
			{
				tempo = 140;
			}
			else
			{
				tempo = 150;
			}
		}
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

		switchNum1 = 0;
		switchNum2 = 0;

		if (cmdFmt == 1)
		{
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				firstNote = 1;
				automatic = 0;

				if (format == 2)
				{
					automatic = 1;
				}
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;

				curDelay = 0;
				masterDelay = 0;
				trackEnd = 0;
				seqEnd = 1;

				curNote = 0;
				lastNote = 0;
				curNoteLen = 0;
				transpose = 0;
				globalTranspose = 0;


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


				while (trackEnd == 0)
				{
					pattern[0] = romData[patPos];
					pattern[1] = romData[patPos + 1];
					pattern[2] = romData[patPos + 2];
					pattern[3] = romData[patPos + 3];

					/*Repeat sequence*/
					if (pattern[0] == 0x80)
					{
						repeat = pattern[1];
						patPos += 2;
					}

					/*Transpose sequence*/
					else if (pattern[0] == 0x82)
					{
						transpose = (signed char)pattern[1];
						patPos += 2;
					}

					/*Jump to position*/
					else if (pattern[0] == 0x84)
					{
						trackEnd = 1;
					}

					/*End of track*/
					else if (pattern[0] == 0x86)
					{
						trackEnd = 1;
					}

					/*Transpose song (all channels)*/
					else if (pattern[0] == 0xB0)
					{
						globalTranspose = (signed char)pattern[1];
						if (curTrack == 0)
						{
							switchPoint1[switchNum1][0] = masterDelay;
							switchPoint1[switchNum1][1] = globalTranspose;
							switchNum1++;
						}
						patPos += 2;
					}

					/*Play sequence*/
					else if (pattern[0] < 0x80)
					{
						if (addSeq == 1)
						{
							pattern[0] += 11;
						}
						curSeq = seqList[pattern[0]];

						if (addSeq == 1 && (songNum == 8 || songNum == 7))
						{
							curSeq = seqList[pattern[0] + 118];
						}
						seqEnd = 0;
						seqPos = curSeq;
					}

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
									globalTranspose = switchPoint1[switchNum1][1];
								}
							}
						}

						/*End of sequence*/
						if (command[0] == 0x86)
						{
							if (repeat > 1)
							{
								seqPos = curSeq;
								repeat--;
							}
							else
							{
								seqEnd = 1;
								transpose = 0;
								patPos++;
							}

						}

						/*Set automatic note length*/
						else if (command[0] == 0x88)
						{
							automatic = 1;
							curNoteLen = command[1] * 5;
							seqPos += 2;
						}

						/*Set manual note length*/
						else if (command[0] == 0x8A)
						{
							automatic = 0;
							seqPos++;
						}

						/*"Hold" note*/
						else if (command[0] == 0x8C)
						{

							/*Fix for RoboCop*/
							if (rcFix == 1)
							{
								curDelay += 1290;
							}
							else
							{
								curDelay += 480;
							}

							seqPos++;
						}

						/*Rest*/
						else if (command[0] == 0x8E)
						{
							if (automatic == 0)
							{
								curDelay += command[1] * 5;
								masterDelay += command[1] * 5;
								seqPos += 2;
							}
							else if (automatic == 1)
							{
								curDelay += curNoteLen;
								masterDelay += curNoteLen;
								seqPos++;
							}

						}

						/*Turn on portamento*/
						else if (command[0] == 0x90)
						{
							seqPos += 3;
						}

						/*Turn off portamento*/
						else if (command[0] == 0x92)
						{
							seqPos++;
						}

						/*Turn on sweep*/
						else if (command[0] == 0x94)
						{
							seqPos += 3;
						}

						/*Turn off sweep*/
						else if (command[0] == 0x96)
						{
							seqPos++;
						}

						/*Turn on vibrato*/
						else if (command[0] == 0x98)
						{
							seqPos += 4;
						}

						/*Turn off vibrato*/
						else if (command[0] == 0x9A)
						{
							seqPos++;
						}

						/*Turn on arpeggio*/
						else if (command[0] == 0x9C)
						{
							seqPos += 2;
						}

						/*Turn off arpeggio*/
						else if (command[0] == 0x9E)
						{
							seqPos++;
						}

						/*Set envelope*/
						else if (command[0] == 0xA0)
						{
							seqPos += 2;
						}

						/*Play drum*/
						else if (command[0] == 0xA2)
						{
							curNote = command[1] + 30;
							if (automatic == 0)
							{
								curNoteLen = command[2] * 5;
								masterDelay += command[2] * 5;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 3;
							}
							else if (automatic == 1)
							{
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 2;
							}

						}

						/*Poke location*/
						else if (command[0] == 0xA4)
						{
							seqPos += 3;
						}

						/*Switch waveform*/
						else if (command[0] == 0xA6)
						{
							seqPos += 17;
						}

						/*Switch waveform*/
						else if (command[0] == 0xA8)
						{
							seqPos += 17;
						}

						/*Switch waveform from stored address*/
						else if (command[0] == 0xAA)
						{
							seqPos++;
						}

						/*Set duty*/
						else if (command[0] == 0xAC)
						{
							seqPos += 2;
						}

						/*Set manual sweep*/
						else if (command[0] == 0xAE)
						{
							seqPos += 4;
						}

						/*Set registers directly*/
						else if (command[0] == 0xB0)
						{
							seqPos += 4;
						}

						/*Set note size*/
						else if (command[0] >= 0xB0 && command[0] < 0xC0)
						{
							seqPos += 2;
						}

						else if (command[0] >= 0xC0 && command[0] < 0xE0)
						{
							curNoteLen = (command[0] - 0xBF) * 5;
							seqPos++;
						}

						/*Play note*/
						else if (command[0] < 0x80)
						{
							if (automatic == 0)
							{
								curNote = command[0] + 24 + transpose + globalTranspose;
								curNoteLen = command[1] * 5;
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 2;
							}
							else if (automatic == 1)
							{
								curNote = command[0] + 24 + transpose + globalTranspose;
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos++;
							}
						}

						/*Unknown command*/
						else
						{
							seqPos++;
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
		}

		else if (cmdFmt == 2)
		{
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				firstNote = 1;
				automatic = 0;

				if (format == 2)
				{
					automatic = 1;
				}
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;

				curDelay = 0;
				masterDelay = 0;
				trackEnd = 0;
				seqEnd = 1;

				curNote = 0;
				lastNote = 0;
				curNoteLen = 0;
				transpose = 0;
				globalTranspose = 0;


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


				while (trackEnd == 0)
				{
					pattern[0] = romData[patPos];
					pattern[1] = romData[patPos + 1];
					pattern[2] = romData[patPos + 2];
					pattern[3] = romData[patPos + 3];

					/*For Beauty and the Beast/NBA 3 on 3/The Little Mermaid 2*/
					if (seqDiff == 1)
					{
						/*Exit*/
						if (pattern[0] == 0xC8)
						{
							trackEnd = 1;
						}
						else if (pattern[0] >= 0xC8)
						{
							pattern[0] -= 2;
						}
					}

					/*Repeat sequence*/
					if (pattern[0] == 0xC0)
					{
						repeat = pattern[1];
						if (repeat == 255)
						{
							repeat = 1;
						}
						patPos += 2;
					}

					/*Transpose sequence*/
					else if (pattern[0] == 0xC2)
					{
						transpose = (signed char)pattern[1];
						patPos += 2;
					}

					/*Jump to position*/
					else if (pattern[0] == 0xC4)
					{
						trackEnd = 1;
					}

					/*End of track*/
					else if (pattern[0] == 0xC6)
					{
						trackEnd = 1;
					}

					/*Play sequence*/
					else if (pattern[0] < 0xC0)
					{
						curSeq = seqList[pattern[0]];
						seqEnd = 0;
						seqPos = curSeq;
					}

					while (seqEnd == 0)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];
						command[3] = romData[seqPos + 3];

						/*For Beauty and the Beast/NBA 3 on 3/The Little Mermaid 2*/
						if (seqDiff == 1)
						{
							/*Exit*/
							if (command[0] == 0xC8)
							{
								trackEnd = 1;
							}
							else if (command[0] >= 0xC8)
							{
								command[0] -= 2;
							}
						}


						/*Transpose check code*/
						if (curTrack != 0 && curTrack != 3)
						{
							for (switchNum1 = 0; switchNum1 < 400; switchNum1++)
							{
								if (switchPoint1[switchNum1][0] == masterDelay)
								{
									globalTranspose = switchPoint1[switchNum1][1];
								}
							}
						}

						/*End of sequence*/
						if (command[0] == 0xC6)
						{
							if (repeat > 1)
							{
								seqPos = curSeq;
								repeat--;
							}
							else
							{
								seqEnd = 1;
								transpose = 0;
								patPos++;
							}

						}

						/*Set automatic note length*/
						else if (command[0] == 0xC8)
						{
							automatic = 1;
							curNoteLen = command[1] * 5;
							seqPos += 2;
						}

						/*Set manual note length*/
						else if (command[0] == 0xCA)
						{
							automatic = 0;
							seqPos++;
						}

						/*"Hold" note*/
						else if (command[0] == 0xCC)
						{
							curDelay += 480;
							seqPos++;
						}

						/*Rest*/
						else if (command[0] == 0xCE)
						{
							if (automatic == 0)
							{
								curDelay += command[1] * 5;
								masterDelay += command[1] * 5;
								seqPos += 2;
							}
							else if (automatic == 1)
							{
								curDelay += curNoteLen;
								masterDelay += curNoteLen;
								seqPos++;
							}

						}

						/*Turn on portamento*/
						else if (command[0] == 0xD0)
						{
							seqPos += 3;
						}

						/*Turn off portamento*/
						else if (command[0] == 0xD2)
						{
							seqPos++;
						}

						/*Turn on sweep*/
						else if (command[0] == 0xD4)
						{
							seqPos += 3;
						}

						/*Turn off sweep*/
						else if (command[0] == 0xD6)
						{
							seqPos++;
						}

						/*Turn on vibrato*/
						else if (command[0] == 0xD8)
						{
							seqPos += 4;
						}

						/*Turn off vibrato*/
						else if (command[0] == 0xDA)
						{
							seqPos++;
						}

						/*Turn on arpeggio*/
						else if (command[0] == 0xDC)
						{
							seqPos += 2;
						}

						/*Turn off arpeggio*/
						else if (command[0] == 0xDE)
						{
							seqPos++;
						}

						/*Set envelope*/
						else if (command[0] == 0xE0)
						{
							seqPos += 2;
						}

						/*Play drum*/
						else if (command[0] == 0xE2)
						{
							curNote = command[1] + 30;
							if (automatic == 0)
							{
								curNoteLen = command[2] * 5;
								masterDelay += command[2] * 5;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 3;
							}
							else if (automatic == 1)
							{
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 2;
							}

						}

						/*Poke location*/
						else if (command[0] == 0xE4)
						{
							seqPos += 3;
						}

						/*Switch waveform*/
						else if (command[0] == 0xE6)
						{
							seqPos += 17;
						}

						/*Switch waveform*/
						else if (command[0] == 0xE8)
						{
							seqPos += 17;
						}

						/*Switch waveform from stored address*/
						else if (command[0] == 0xEA)
						{
							seqPos++;
						}

						/*Set duty*/
						else if (command[0] == 0xEC)
						{
							seqPos += 2;
						}

						/*Set manual sweep*/
						else if (command[0] == 0xEE)
						{
							seqPos += 4;
						}

						/*Set registers directly*/
						else if (command[0] == 0xF0)
						{
							seqPos += 4;
						}


						/*Play note*/
						else if (command[0] < 0x80)
						{
							if (automatic == 0)
							{
								curNote = command[0] + 24 + transpose + globalTranspose;
								curNoteLen = command[1] * 5;
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 2;
							}
							else if (automatic == 1)
							{
								curNote = command[0] + 24 + transpose + globalTranspose;
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos++;
							}
						}

						/*Unknown command*/
						else
						{
							seqPos++;
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
		}

		else if (cmdFmt == 3)
		{
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				firstNote = 1;
				automatic = 0;

				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;

				curDelay = 0;
				masterDelay = 0;
				trackEnd = 0;
				seqEnd = 1;

				curNote = 0;
				lastNote = 0;
				curNoteLen = 0;
				transpose = 0;
				globalTranspose = 0;


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


				while (trackEnd == 0)
				{
					pattern[0] = romData[patPos];
					pattern[1] = romData[patPos + 1];
					pattern[2] = romData[patPos + 2];
					pattern[3] = romData[patPos + 3];

					/*Repeat sequence*/
					if (pattern[0] == 0xE8)
					{
						repeat = pattern[1];
						patPos += 2;
					}

					/*Transpose sequence*/
					else if (pattern[0] == 0xE9)
					{
						transpose = (signed char)pattern[1];
						patPos += 2;
					}

					/*Jump to position*/
					else if (pattern[0] == 0xEA)
					{
						trackEnd = 1;
					}

					/*End of track*/
					else if (pattern[0] == 0xEB)
					{
						trackEnd = 1;
					}

					else if (pattern[0] == 0xEF)
					{
						patPos += 2;
					}

					else if (pattern[0] == 0xCB)
					{
						trackEnd = 1;
					}

					/*Play sequence*/
					else if (pattern[0] < 0x90)
					{
						curSeq = seqList[pattern[0]];
						seqEnd = 0;
						seqPos = curSeq;
					}

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
									globalTranspose = switchPoint1[switchNum1][1];
								}
							}
						}

						/*End of sequence*/
						if (command[0] == 0xEB)
						{
							if (repeat > 1)
							{
								seqPos = curSeq;
								repeat--;
							}
							else
							{
								seqEnd = 1;
								transpose = 0;
								patPos++;
							}

						}

						/*Set automatic note length*/
						else if (command[0] == 0xEC)
						{
							automatic = 1;
							curNoteLen = command[1] * 5;
							seqPos += 2;
						}

						/*Set manual note length*/
						else if (command[0] == 0xED)
						{
							automatic = 0;
							seqPos++;
						}

						/*"Hold" note*/
						else if (command[0] == 0xEE)
						{
							curDelay += 480;
							seqPos++;
						}

						/*Rest*/
						else if (command[0] == 0xEF)
						{
							if (automatic == 0)
							{
								curDelay += command[1] * 5;
								masterDelay += command[1] * 5;
								seqPos += 2;
							}
							else if (automatic == 1)
							{
								curDelay += curNoteLen;
								masterDelay += curNoteLen;
								seqPos++;
							}

						}

						/*Turn on portamento*/
						else if (command[0] == 0xF0)
						{
							seqPos += 3;
						}

						/*Turn off portamento*/
						else if (command[0] == 0xF1)
						{
							seqPos++;
						}

						/*Turn on sweep*/
						else if (command[0] == 0xF2)
						{
							seqPos += 3;
						}

						/*Turn off sweep*/
						else if (command[0] == 0xF3)
						{
							seqPos++;
						}

						/*Turn on vibrato*/
						else if (command[0] == 0xF4)
						{
							seqPos += 4;
						}

						/*Turn off vibrato*/
						else if (command[0] == 0xF5)
						{
							seqPos++;
						}

						/*Turn on arpeggio*/
						else if (command[0] == 0xF6)
						{
							seqPos += 2;
						}

						/*Turn off arpeggio*/
						else if (command[0] == 0xF7)
						{
							seqPos++;
						}

						/*Set envelope*/
						else if (command[0] == 0xF8)
						{
							seqPos += 2;
						}

						/*Play drum*/
						else if (command[0] == 0xF9)
						{
							curNote = command[1] + 30;
							if (automatic == 0)
							{
								curNoteLen = command[2] * 5;
								masterDelay += command[2] * 5;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 3;
							}
							else if (automatic == 1)
							{
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 2;
							}

						}

						/*Poke location*/
						else if (command[0] == 0xFA)
						{
							seqPos += 3;
						}

						/*Switch waveform*/
						else if (command[0] == 0xFB)
						{
							seqPos += 17;
						}

						/*Switch waveform*/
						else if (command[0] == 0xFC)
						{
							seqPos += 17;
						}

						/*Switch waveform from stored address*/
						else if (command[0] == 0xFD)
						{
							seqPos++;
						}

						/*Set duty*/
						else if (command[0] == 0xFE)
						{
							seqPos += 2;
						}

						/*Set manual sweep*/
						else if (command[0] == 0xFF)
						{
							seqPos += 4;
						}

						/*Set registers directly
						else if (command[0] == 0xFF)
						{
							seqPos += 4;
						}*/


						/*Play note*/
						else if (command[0] < 0xE8)
						{
							if (automatic == 0)
							{
								curNote = command[0] + 24 + transpose + globalTranspose;
								curNoteLen = command[1] * 5;
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos += 2;
							}
							else if (automatic == 1)
							{
								curNote = command[0] + 24 + transpose + globalTranspose;
								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos++;
							}
						}

						/*Unknown command*/
						else
						{
							seqPos++;
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

/*Special method for Mr. Nutz*/
void OcnNutzSong2mid(int songNum, long ptr, long tempo)
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
	int globalTranspose = 0;
	int ticks = 120;
	int k = 0;
	int curPat = 0;
	int tempoValue = 0;

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

	int curInst = 0;

	unsigned long patPos = 0;
	unsigned long seqPos = 0;

	unsigned char command[4];
	unsigned char pattern[4];

	signed int transpose = 0;
	int repeat = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;
	int automatic = 0;

	long masterDelay = 0;
	long ctrlDelay = 0;

	int totalLen = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	midPos = 0;
	ctrlMidPos = 0;

	switchNum1 = 0;
	switchNum2 = 0;

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

		/*Put tempo code here*/
		if (tempo == 1)
		{
			tempoValue = 200;
		}

		else if (tempo == 2)
		{
			tempoValue = 150;
		}

		else if (tempo == 3)
		{
			tempoValue = 110;
		}

		else if (tempo == 4)
		{
			tempoValue = 90;
		}

		else if (tempo == 5)
		{
			tempoValue = 80;
		}

		else if (tempo == 6)
		{
			tempoValue = 70;
		}

		else if (tempo == 7)
		{
			tempoValue = 60;
		}

		else
		{
			tempoValue = 150;
		}
		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempoValue);
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

		switchNum1 = 0;
		switchNum2 = 0;


		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			patPos = ptr - bankAmt;

			firstNote = 1;
			automatic = 1;

			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			masterDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			transpose = 0;
			globalTranspose = 0;


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
			while (ReadLE16(&romData[patPos]) != 0xFFFF && ReadLE16(&romData[patPos]) != 0x0000)
			{
				curPat = ReadLE16(&romData[patPos]) - bankAmt;
				curSeq = ReadLE16(&romData[curPat] + (curTrack * 2)) - bankAmt;
				seqPos = curSeq;
				seqEnd = 0;

				curNoteLen = 100;

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
								globalTranspose = switchPoint1[switchNum1][1];
							}
						}
					}

					/*Transpose sequence*/
					if (command[0] == 0x81)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*End of sequence*/
					if (command[0] == 0x82)
					{
						seqEnd = 1;
						transpose = 0;
						patPos += 2;
					}

					/*Set automatic note length*/
					else if (command[0] == 0x83)
					{
						automatic = 1;
						curNoteLen = (command[1] - 0xC0) * 15;
						seqPos += 2;
					}

					/*Set manual note length*/
					else if (command[0] == 0x84)
					{
						automatic = 0;
						seqPos++;
					}

					/*"Hold" note*/
					else if (command[0] == 0x85)
					{
						curDelay += 480;
						seqPos += 2;
					}

					/*Rest*/
					else if (command[0] == 0x86)
					{
						if (automatic == 0)
						{
							curDelay += (command[1] - 0xC0) * 15;
							ctrlDelay += (command[1] - 0xC0) * 15;
							masterDelay += (command[1] - 0xC0) * 15;
							seqPos += 2;
						}
						else if (automatic == 1)
						{
							if (command[1] >= 0xC0 && command[1] <= 0xE0)
							{
								curNoteLen = (command[1] - 0xBF) * 15;
								seqPos++;
							}
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos++;
						}

					}

					/*Turn on portamento*/
					else if (command[0] == 0x87)
					{
						seqPos += 3;
					}

					/*Turn off portamento*/
					else if (command[0] == 0x88)
					{
						seqPos++;
					}

					/*Turn on sweep*/
					else if (command[0] == 0x89)
					{
						seqPos += 3;
					}

					/*Turn off sweep*/
					else if (command[0] == 0x8A)
					{
						seqPos++;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x8B)
					{
						seqPos += 4;
					}

					/*Turn off vibrato*/
					else if (command[0] == 0x8C)
					{
						seqPos++;
					}

					/*Turn on arpeggio*/
					else if (command[0] == 0x8D)
					{
						seqPos += 2;
					}

					/*Turn off arpeggio*/
					else if (command[0] == 0x8E)
					{
						seqPos++;
					}

					/*Set envelope*/
					else if (command[0] == 0x8F)
					{
						seqPos += 2;
					}

					/*Play drum*/
					else if (command[0] == 0x90)
					{
						curNote = command[1] + 30;
						if (automatic == 0)
						{
							curNoteLen = command[2] * 50;
							ctrlDelay += command[2] * 50;
							masterDelay += command[2] * 50;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos += 3;
						}
						else if (automatic == 1)
						{
							ctrlDelay += command[2] * 50;
							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos += 2;
						}

					}

					/*Poke location*/
					else if (command[0] == 0x91)
					{
						seqPos += 3;
					}

					/*Switch waveform*/
					else if (command[0] == 0x92)
					{
						seqPos += 17;
					}

					/*Switch waveform*/
					else if (command[0] == 0x93)
					{
						seqPos += 17;
					}

					/*Switch waveform from stored address*/
					else if (command[0] == 0x94)
					{
						seqPos++;
					}

					/*Set duty*/
					else if (command[0] == 0x95)
					{
						seqPos += 2;
					}

					/*Set manual sweep*/
					else if (command[0] == 0x96)
					{
						seqPos += 4;
					}

					/*Transpose song (all channels)*/
					else if (command[0] == 0x97)
					{
						globalTranspose = (signed char)command[1];
						if (curTrack == 0)
						{
							switchPoint1[switchNum1][0] = masterDelay;
							switchPoint1[switchNum1][1] = globalTranspose;
							switchNum1++;
						}
						seqPos += 2;
					}

					/*Set envelope (v2)*/
					else if (command[0] == 0x98)
					{
						seqPos += 2;
					}

					/*Set volume effect?*/
					else if (command[0] == 0x99)
					{
						seqPos += 2;
					}

					/*Set duty (v2)*/
					else if (command[0] >= 0x9A && command[0] <= 0x9D)
					{
						seqPos += 4;
					}

					/*Change speed?*/
					else if (command[0] == 0x9E)
					{
						seqPos += 2;
					}

					/*Change tempo*/
					else if (command[0] == 0x9F)
					{
						if (command[1] == 1)
						{
							tempoValue = 200;
						}

						else if (command[1] == 2)
						{
							tempoValue = 150;
						}

						else if (command[1] == 3)
						{
							tempoValue = 110;
						}

						else if (command[1] == 4)
						{
							tempoValue = 90;
						}

						else if (command[1] == 5)
						{
							tempoValue = 80;
						}

						else if (command[1] == 6)
						{
							tempoValue = 70;
						}

						else if (command[1] == 7)
						{
							tempoValue = 60;
						}

						else
						{
							tempoValue = 150;
						}

						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempoValue);
						ctrlMidPos += 2;

						seqPos += 2;
					}

					/*Set note length*/
					else if (command[0] >= 0xC0 && command[0] < 0xE0)
					{
						curNoteLen = (command[0] - 0xBF) * 15;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] < 0x80)
					{
						if (automatic == 0)
						{
							curNote = command[0] + 24 + transpose + globalTranspose;
							curNoteLen = command[1] * 50;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos += 2;
						}
						else if (automatic == 1)
						{
							curNote = command[0] + 24 + transpose + globalTranspose;
							if (command[1] >= 0xC0 && command[1] <= 0xE0)
							{
								curNoteLen = (command[1] - 0xBF) * 15;
								seqPos++;
							}
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}
					}

					/*Unknown command*/
					else
					{
						seqPos++;
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}

}