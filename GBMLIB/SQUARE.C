/*Square*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "SQUARE.H"

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
int drvVers;
int curInst;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char SqrMagicBytesA[13] = { 0x19, 0x11, 0x02, 0xCE, 0xCD, 0x53, 0x40, 0x11, 0x22, 0xCE, 0xCD, 0x53, 0x40 };
const char SqrMagicBytesB[7] = { 0x5F, 0x16, 0x00, 0x19, 0x2A, 0xEA, 0x04 };
const char SqrMagicBytesC[7] = { 0x5F, 0x16, 0x00, 0x19, 0x2A, 0xEA, 0x09 };

const unsigned char SqrNoteLen1[15] = { 0x60, 0x48, 0x30, 0x24, 0x20, 0x18, 0x12, 0x10, 0x0C, 0x09, 0x08, 0x06, 0x04, 0x03, 0x02 };
const unsigned char SqrNoteLen2[13] = { 0x60, 0x48, 0x30, 0x20, 0x24, 0x18, 0x10, 0x12, 0x0C, 0x08, 0x06, 0x04, 0x03 };
const unsigned char SqrNoteLen3[16] = { 0x90, 0x60, 0x48, 0x30, 0x24, 0x20, 0x18, 0x12, 0x10, 0x0C, 0x09, 0x08, 0x06, 0x04, 0x03, 0x02 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Sqrsong2mid(int songNum, long ptrs[4]);

void SqrProc(int bank)
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

	/*Try to search the bank for song table loader - Method 1: Final Fantasy Legend 1*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], SqrMagicBytesA, 13)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			drvVers = 1;
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 2: Final Fantasy Legend 2/3/Adventure*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], SqrMagicBytesB, 7)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			drvVers = 2;
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 3: Final Fantasy Legend 3*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], SqrMagicBytesC, 7)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			drvVers = 3;
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;

		if (drvVers == 1)
		{
			while ((ReadLE16(&romData[i]) > bankAmt) && (ReadLE16(&romData[i]) < (bankSize * 2)))
			{
				for (j = 0; j < 3; j++)
				{
					seqPtrs[j] = ReadLE16(&romData[i + (j * 2)]);
					printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
				}
				Sqrsong2mid(songNum, seqPtrs);
				i += 6;
				songNum++;
			}
		}

		else if (drvVers == 2)
		{
			while ((ReadLE16(&romData[i]) > bankAmt) && (ReadLE16(&romData[i]) < (bankSize * 2)))
			{
				for (j = 0; j < 3; j++)
				{
					if (j == 0)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 2]);
					}
					else if (j == 1)
					{
						seqPtrs[j] = ReadLE16(&romData[i]);
					}
					else if (j == 2)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 4]);
					}

					printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
				}
				Sqrsong2mid(songNum, seqPtrs);
				i += 6;
				songNum++;
			}
		}

		else if (drvVers == 3)
		{
			while ((ReadLE16(&romData[i]) > bankAmt) && (ReadLE16(&romData[i]) < (bankSize * 2)))
			{
				for (j = 0; j < 4; j++)
				{
					if (j == 0)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 2]);
					}
					else if (j == 1)
					{
						seqPtrs[j] = ReadLE16(&romData[i]);
					}
					else if (j == 2)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 4]);
					}
					else if (j == 3)
					{
						seqPtrs[j] = ReadLE16(&romData[i + 6]);
					}
					printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
				}
				Sqrsong2mid(songNum, seqPtrs);
				i += 8;
				songNum++;
			}
		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
}

/*Convert the song data to MIDI*/
void Sqrsong2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	int curTrack = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int songTempo = 0;
	int octave = 0;
	long jumpPos = 0;
	long jumpPosRet = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
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
	long tempPos = 0;

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

		if (drvVers != 3)
		{
			trackCnt = 3;
		}
		else if (drvVers == 3)
		{
			trackCnt = 4;
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
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			jumpPos = 0;
			jumpPosRet = 0;

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

			if (ptrs[curTrack] >= bankAmt && ptrs[curTrack] <= (bankSize * 2))
			{
				seqEnd = 0;
				seqPos = ptrs[curTrack] - bankAmt;

				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
					command[4] = romData[seqPos + 4];
					command[5] = romData[seqPos + 5];
					command[6] = romData[seqPos + 6];
					command[7] = romData[seqPos + 7];

					if (drvVers == 1)
					{
						if (command[0] < 0xE0)
						{
							lowNibble = (command[0] >> 4);
							highNibble = (command[0] & 15);

							curNoteLen = SqrNoteLen1[highNibble] * 5;

							/*Rest*/
							if (lowNibble == 0x0C)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

							/*Hold note/wait?*/
							else if (lowNibble == 0x0D)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

							/*Play note*/
							else
							{
								curNote = lowNibble + (12 * octave) + 24;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
							}

							seqPos++;
						}

						/*Set tempo*/
						else if (command[0] == 0xE0)
						{
							songTempo = command[1] * 2.5;

							if (tempo != songTempo)
							{
								ctrlMidPos++;
								valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
								ctrlDelay = 0;
								ctrlMidPos += valSize;
								WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
								ctrlMidPos += 3;
								tempo = (command[1] * 2.5);
								WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
								ctrlMidPos += 2;
							}
							seqPos += 2;
						}

						/*Set volume/duty*/
						else if (command[0] >= 0xE1 && command[0] <= 0xEF)
						{
							seqPos++;
						}

						/*Set octave*/
						else if (command[0] >= 0xF0 && command[0] <= 0xF4)
						{
							highNibble = (command[0] & 15);
							octave = highNibble;
							seqPos++;
						}

						/*Unknown command F5*/
						else if (command[0] == 0xF5)
						{
							seqPos++;
						}

						/*Command F6 (Invalid)*/
						else if (command[0] == 0xF6)
						{
							seqPos++;
						}

						/*Set note envelope*/
						else if (command[0] == 0xF7)
						{
							seqPos += 2;
						}

						/*Set waveform*/
						else if (command[0] == 0xF8)
						{
							seqPos += 2;
						}

						/*Repeat section*/
						else if (command[0] >= 0xF9 && command[0] <= 0xFB)
						{
							repeat1 = command[0] - 0xF8;
							seqPos++;
						}

						/*End of repeat section*/
						else if (command[0] == 0xFC)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							if (repeat1 > 0)
							{
								seqPos = jumpPos;
								repeat1--;
							}
							else
							{
								seqPos += 3;
							}
						}

						/*Jump to position if still repeating*/
						else if (command[0] == 0xFD)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							if (repeat1 <= 0)
							{
								seqPos = jumpPos;
							}
							else
							{
								seqPos += 3;
							}
						}

						/*Jump to position*/
						else if (command[0] == 0xFE)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;

							if (jumpPos > seqPos)
							{
								seqPos = jumpPos;
							}
							else
							{
								seqEnd = 1;
							}
						}

						/*End of channel*/
						else if (command[0] == 0xFF)
						{
							seqEnd = 1;
						}
					}

					else if (drvVers == 2)
					{
						if (command[0] < 0xD0)
						{
							lowNibble = (command[0] >> 4);
							highNibble = (command[0] & 15);

							curNoteLen = SqrNoteLen2[lowNibble] * 5;

							/*Rest*/
							if (highNibble == 0x0E)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

							/*Hold note/wait?*/
							else if (highNibble == 0x0F)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

							/*Play note*/
							else
							{
								curNote = highNibble + (12 * octave) + 12;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
							}

							seqPos++;
						}

						/*Set octave*/
						else if (command[0] >= 0xD0 && command[0] <= 0xD7)
						{
							highNibble = (command[0] & 15);
							octave = highNibble;
							seqPos++;
						}

						/*Octave up*/
						else if (command[0] >= 0xD8 && command[0] <= 0xDB)
						{
							octave += (command[0] - 0xD7);
							seqPos++;
						}

						/*Octave down*/
						else if (command[0] >= 0xDC && command[0] <= 0xDF)
						{
							octave -= (command[0] - 0xDB);
							seqPos++;
						}

						/*Set volume*/
						else if (command[0] == 0xE0)
						{
							if (curTrack != 2)
							{
								seqPos += 3;
							}
							else
							{
								seqPos += 2;
							}
						}

						/*Jump to position*/
						else if (command[0] == 0xE1)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;

							if (jumpPos > seqPos)
							{
								seqPos = jumpPos;
							}
							else
							{
								seqEnd = 1;
							}
						}

						/*Jump to position if still repeating (v1)*/
						else if (command[0] == 0xE2)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							if (repeat1 > 1)
							{
								seqPos = jumpPos;
								repeat1--;
							}
							else if (repeat1 == -1)
							{
								seqEnd = 1;
							}
							else
							{
								seqPos += 3;
							}
						}

						/*Repeat section (v1)*/
						else if (command[0] == 0xE3)
						{
							repeat1 = command[1];
							seqPos += 2;
						}

						/*Set vibrato*/
						else if (command[0] == 0xE4)
						{
							seqPos += 3;
						}

						/*Set duty*/
						else if (command[0] == 0xE5)
						{
							seqPos += 2;
						}

						/*Set panning*/
						else if (command[0] == 0xE6)
						{
							seqPos += 2;
						}

						/*Set tempo*/
						else if (command[0] == 0xE7)
						{
							songTempo = command[1] * 1.25;

							if (tempo != songTempo)
							{
								ctrlMidPos++;
								valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
								ctrlDelay = 0;
								ctrlMidPos += valSize;
								WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
								ctrlMidPos += 3;
								tempo = (command[1] * 1.25);

								if (tempo == 0)
								{
									tempo = 150;
								}
								WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
								ctrlMidPos += 2;
							}
							seqPos += 2;
						}

						/*Set waveform*/
						else if (command[0] == 0xE8)
						{
							seqPos += 3;
						}

						/*Jump to position if still repeating (v2)*/
						else if (command[0] == 0xE9)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							if (repeat2 > 1)
							{
								seqPos = jumpPos;
								repeat2--;
							}
							else
							{
								seqPos += 3;
							}
						}

						/*Repeat section (v2)*/
						else if (command[0] == 0xEA)
						{
							repeat2 = command[1];
							seqPos += 2;
						}

						/*If repeat (v1) is X, then jump to position Y*/
						else if (command[0] == 0xEB)
						{
							jumpPos = ReadLE16(&romData[seqPos + 2]) - bankAmt;
							if (repeat1 == command[1])
							{
								seqPos = jumpPos;
							}
							else
							{
								seqPos += 4;
							}
						}

						/*Set "double time" mode (broken)*/
						else if (command[0] == 0xEC)
						{
							seqPos++;
						}

						/*End of channel*/
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

					else if (drvVers == 3)
					{
						if (command[0] < 0xE0)
						{
							lowNibble = (command[0] >> 4);
							highNibble = (command[0] & 15);

							curNoteLen = SqrNoteLen3[highNibble] * 5;

							/*Rest*/
							if (lowNibble == 0x0C)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

							/*Hold note/wait?*/
							else if (lowNibble == 0x0D)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}

							/*Play note*/
							else
							{
								curNote = lowNibble + (12 * octave) + 24;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
							}

							seqPos++;
						}

						else if (command[0] >= 0xE0 && command[0] <= 0xE7)
						{
							highNibble = (command[0] & 15);
							octave = highNibble;
							seqPos++;
						}

						/*Octave up*/
						else if (command[0] >= 0xE8 && command[0] <= 0xEB)
						{
							octave += (command[0] - 0xE7);
							seqPos++;
						}

						/*Octave down*/
						else if (command[0] >= 0xEC && command[0] <= 0xEF)
						{
							octave -= (command[0] - 0xEB);
							seqPos++;
						}

						/*Set instrument pointer*/
						else if (command[0] == 0xF0)
						{
							seqPos += 3;
						}

						/*Jump to position*/
						else if (command[0] == 0xF1)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;

							if (jumpPos > seqPos)
							{
								seqPos = jumpPos;
							}
							else
							{
								seqEnd = 1;
							}
						}

						/*Jump to position if still repeating (v1)*/
						else if (command[0] == 0xF2)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							if (repeat1 > 1)
							{
								seqPos = jumpPos;
								repeat1--;
							}
							else if (repeat1 == -1)
							{
								seqEnd = 1;
							}
							else
							{
								seqPos += 3;
							}
						}

						/*Repeat section (v1)*/
						else if (command[0] == 0xF3)
						{
							repeat1 = command[1];
							seqPos += 2;
						}

						/*Set vibrato*/
						else if (command[0] == 0xF4)
						{
							seqPos += 3;
						}

						/*Set duty*/
						else if (command[0] == 0xF5)
						{
							seqPos += 2;
						}

						/*Set volume?*/
						else if (command[0] == 0xF6)
						{
							seqPos += 2;
						}

						/*Set tempo*/
						else if (command[0] == 0xF7)
						{
							songTempo = command[1] * 1.25;

							if (tempo != songTempo)
							{
								ctrlMidPos++;
								valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
								ctrlDelay = 0;
								ctrlMidPos += valSize;
								WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
								ctrlMidPos += 3;
								tempo = (command[1] * 1.25);

								if (tempo == 0)
								{
									tempo = 150;
								}
								WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
								ctrlMidPos += 2;
							}
							seqPos += 2;
						}

						/*End of channel (F8)*/
						else if (command[0] == 0xF8)
						{
							seqEnd = 1;
						}

						/*Jump to position if still repeating (v2)*/
						else if (command[0] == 0xF9)
						{
							jumpPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							if (repeat2 > 1)
							{
								seqPos = jumpPos;
								repeat2--;
							}
							else
							{
								seqPos += 3;
							}
						}

						/*Repeat section (v2)*/
						else if (command[0] == 0xFA)
						{
							repeat2 = command[1];
							seqPos += 2;
						}

						/*If repeat (v1) is X, then jump to position Y*/
						else if (command[0] == 0xFB)
						{
							jumpPos = ReadLE16(&romData[seqPos + 2]) - bankAmt;
							if (repeat1 == command[1])
							{
								seqPos = jumpPos;
							}
							else
							{
								seqPos += 4;
							}
						}

						/*Set "double time" mode (broken)*/
						else if (command[0] == 0xFC)
						{
							seqPos++;
						}

						/*End of channel (FD)*/
						else if (command[0] == 0xFD)
						{
							seqEnd = 1;
						}

						/*Set parameters*/
						else if (command[0] == 0xFE)
						{
							if (curTrack != 1)
							{
								seqPos += 7;
							}
							else
							{
								songTempo = command[1] * 1.25;

								if (tempo != songTempo)
								{
									ctrlMidPos++;
									valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
									ctrlDelay = 0;
									ctrlMidPos += valSize;
									WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
									ctrlMidPos += 3;
									tempo = (command[1] * 1.25);

									if (tempo == 0)
									{
										tempo = 150;
									}
									WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
									ctrlMidPos += 2;
								}
								seqPos += 8;
							}
						}

						/*End of channel*/
						else if (command[0] == 0xFF)
						{
							seqEnd = 1;
						}

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