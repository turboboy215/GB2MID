/*Hirotomo Nakamura - Giraffe Soft (Mega Man 2)*/
/*Also works with other games with audio by Giraffe Soft*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MEGAMAN2.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long sfxPtrLoc;
long sfxOffset;
int i, j;
char outfile[1000000];
int format;
int cmdFmt;
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int seqDiff;
int foundTable;
int songTempo;
int highestSeq;
int curInst;
int compatibility;
int octaveSet;

unsigned static char* romData;
unsigned static char* midData;
unsigned static char* ctrlMidData;

long midLength;

const char MM2TableFind1[5] = { 0x87, 0x5F, 0x16, 0x00, 0x21 };
const char MM2TableFind2[7] = { 0x87, 0xCF, 0x2A, 0x66, 0x6F, 0xAF, 0xE0 };
const char MM2TableFind3[5] = { 0x17, 0x5F, 0x16, 0x00, 0x21 };

const int MM2octave8[12] = { 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71 };
const int MM2octave14[12] = { 72, 73, 74, 75, 76, 77, 66, 67, 68, 69, 70, 71 };
const int MM2octave15[12] = { 60, 61, 62, 63, 64, 65, 54, 55, 56, 57, 58, 59 };

const int MM2lengths[16] = { 480, 240, 120, 60, 30, 15, 7, 3, 720, 360, 180, 90, 45, 12, 6, 3 };
const int MM2lengthsBM[16] = { 720, 360, 180, 90, 40, 0, 11, 5, 960, 480, 240, 120, 60, 30, 15, 7 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void MM2song2mid(int songNum, long ptrs[4]);
void MM2Proc(int bank);

void MM2Proc(int bank)
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

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MM2TableFind1, 5)))
		{
			if (foundTable == 0)
			{
				tablePtrLoc = bankAmt + i + 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
			else if (foundTable == 1)
			{
				sfxPtrLoc = bankAmt + i + 5;
				printf("Found pointer to sound effects table at address 0x%04x!\n", sfxPtrLoc);
				sfxOffset = ReadLE16(&romData[sfxPtrLoc - bankAmt]);
				printf("Sound effects table starts at 0x%04x...\n", sfxOffset);
			}
		}
	}


	/*Alternate method - Ayakashi no Shiro*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MM2TableFind2, 7)) && foundTable == 0)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	compatibility = 0;

	/*Alternate method - Rentaiou/The Shinri Game 1/2*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MM2TableFind3, 5)) && foundTable == 0)
		{
			tablePtrLoc = bankAmt + i + 5;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Fix for Blaster Master note length*/
	if (romData[0x0001] == 0x80)
	{
		compatibility = 1;
	}

	/*Fix for Trump Boy II crashing at invalid songs*/
	if (romData[0x0DBB] == 0xE6 && romData[0x0000] == 0xC3 && romData[0x0140] == 0x32)
	{
		compatibility = 2;
	}

	/*Skip first "empty" track*/
	i = tableOffset - bankAmt + 2;
	songNum = 1;
	while ((ReadLE16(&romData[i]) < bankSize * 2) && i != (sfxOffset - bankAmt))
	{
		if (compatibility == 2)
		{
			if (songNum > 5)
			{
				break;
			}
		}
		songPtr = ReadLE16(&romData[i]);
		printf("Song %i: 0x%04X\n", songNum, songPtr);

		seqPtrs[0] = ReadLE16(&romData[songPtr - bankAmt]);
		printf("Channel 1: 0x%04X\n", seqPtrs[0]);
		seqPtrs[1] = ReadLE16(&romData[songPtr + 2 - bankAmt]);
		printf("Channel 2: 0x%04X\n", seqPtrs[1]);
		seqPtrs[2] = ReadLE16(&romData[songPtr + 4 - bankAmt]);
		printf("Channel 3: 0x%04X\n", seqPtrs[2]);
		seqPtrs[3] = ReadLE16(&romData[songPtr + 6 - bankAmt]);
		printf("Channel 4: 0x%04X\n", seqPtrs[3]);
		MM2song2mid(songNum, seqPtrs);
		i += 2;
		songNum++;
	}
}

void MM2song2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[3];
	int curTrack = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	int octave = 4;
	int seqEnd = 0;
	long romPos = 0;
	long midPos = 0;
	long ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	long tempPos = 0;
	int valSize = 0;
	long trackSize = 0;
	int initTempo = 0;
	int tempo = 120;
	long jumpPos = 0;
	long jumpPosEnd = 0;
	int curVol = 0;
	int holdNote = 0;
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int firstNote = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
		ctrlMidData[j] = 0;
	}
	sprintf(outfile, "song%i.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
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
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;

			if (curTrack == 3)
			{
				octave = 3;
			}


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
			romPos = ptrs[curTrack] - bankAmt;

			if (ptrs[curTrack] >= bankSize * 2 || ptrs[curTrack] < bankAmt)
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && romPos < bankSize * 2)
			{
				command[0] = romData[romPos];
				command[1] = romData[romPos + 1];
				command[2] = romData[romPos + 2];

				/*Set tempo*/
				if (command[0] == 0xC0)
				{
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					initTempo = command[1];

					if (compatibility != 1)
					{
						if (initTempo < 70)
						{
							tempo = 240;
						}
						else if (initTempo >= 70 && initTempo < 90)
						{
							tempo = 160;
						}
						else if (initTempo >= 90 && initTempo < 110)
						{
							tempo = 150;
						}
						else if (initTempo >= 110 && initTempo < 128)
						{
							tempo = 120;
						}
						else if (initTempo >= 128 && initTempo < 140)
						{
							tempo = 110;
						}
						else if (initTempo >= 140 && initTempo < 192)
						{
							if (compatibility != 1)
							{
								tempo = 72;
							}
							else
							{
								tempo = 120;
							}
						}
						else if (initTempo >= 192 && initTempo < 196)
						{
							tempo = 120;
						}
						else if (initTempo >= 196 && initTempo < 200)
						{
							tempo = 140;
						}
						else if (initTempo >= 200 && initTempo < 208)
						{
							tempo = 150;
						}
						else if (initTempo >= 208 && initTempo < 212)
						{
							tempo = 170;
						}
						else if (initTempo >= 212 && initTempo < 215)
						{
							tempo = 175;
						}
						else if (initTempo >= 215 && initTempo < 220)
						{
							tempo = 180;
						}
						else if (initTempo >= 220)
						{
							tempo = 190;
						}
						else
						{
							tempo = 120;
						}
					}

					else if (compatibility == 1)
					{
						if (initTempo <= 120)
						{
							tempo = 70;
						}

						else if (initTempo > 120 && initTempo < 144)
						{
							tempo = 80;
						}

						else if (initTempo >= 144 && initTempo < 162)
						{
							tempo = 90;
						}

						else if (initTempo >= 162 && initTempo < 168)
						{
							tempo = 110;
						}

						else if (initTempo >= 168 && initTempo < 176)
						{
							tempo = 115;
						}

						else if (initTempo >= 176 && initTempo < 188)
						{
							tempo = 120;
						}

						else if (initTempo >= 188 && initTempo < 192)
						{
							tempo = 150;
						}

						else if (initTempo >= 192 && initTempo <= 199)
						{
							tempo = 120;
						}

						else if (initTempo >= 199)
						{
							tempo = 170;
						}
					}

					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;

					romPos += 2;
				}

				/*Set duty*/
				else if (command[0] == 0xC1)
				{
					romPos += 2;
				}

				/*Set volume/note size*/
				else if (command[0] == 0xC2)
				{
					romPos += 2;
				}

				/*Jump to loop position*/
				else if (command[0] == 0xC3)
				{
					seqEnd = 1;
				}

				/*Set panning?*/
				else if (command[0] == 0xC4)
				{
					romPos += 2;
				}

				/*Exit from CD jump*/
				else if (command[0] == 0xC9)
				{
					romPos = jumpPosEnd;
				}

				/*Stop channel?*/
				else if (command[0] == 0xCB)
				{
					seqEnd = 1;
				}

				/*Jump to position*/
				else if (command[0] == 0xCD)
				{
					jumpPos = ReadLE16(&romData[romPos + 1]) - bankAmt;
					jumpPosEnd = romPos + 3;
					romPos = jumpPos;
				}

				/*End sequence*/
				else if (command[0] == 0xCE)
				{
					seqEnd = 1;
				}

				/*Disable channel*/
				else if (command[0] == 0xCF)
				{
					seqEnd = 1;
				}

				/*Hold note*/
				else if (command[0] >= 0xD0 && command[0] < 0xE0)
				{
					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);

					if (compatibility != 1)
					{
						holdNote = MM2lengths[highNibble];
					}

					else if (compatibility == 1)
					{
						holdNote = MM2lengthsBM[highNibble];
						if (highNibble == 4 || highNibble == 5 || highNibble == 6 || highNibble == 7)
						{
							holdNote = 0;
						}
					}

					curDelay += holdNote;
					ctrlDelay += holdNote;
					romPos++;
				}

				/*Rest*/
				else if (command[0] >= 0xE0 && command[0] < 0xF0)
				{
					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);


					if (compatibility != 1)
					{
						curNoteLen = MM2lengths[highNibble];
					}

					else if (compatibility == 1)
					{
						curNoteLen = MM2lengthsBM[highNibble];
					}

					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					romPos++;
				}

				/*Set octave*/
				else if (command[0] >= 0xF0 && command[0] <= 0xFF)
				{
					highNibble = (command[0] & 15);
					if (curTrack != 3)
					{
						if (highNibble < 8)
						{
							octave = highNibble;
						}
						else if (highNibble == 8)
						{
							octaveSet = 1;
						}
						else if (highNibble == 15)
						{
							octaveSet = 0;
						}
						else if (highNibble == 14)
						{
							octaveSet = 2;
						}
					}

					else if (curTrack == 3)
					{
						octave = 3;
					}

					romPos++;
				}

				/*Play note*/
				else if (command[0] < 0xC0)
				{
					holdNote = 0;
					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);
					if (curTrack != 3)
					{
						if (octaveSet == 0)
						{
							curNote = lowNibble + (octave * 12);
						}

						else if (octaveSet == 1)
						{
							curNote = lowNibble + (octave * 12) + 12;
						}

						else if (octaveSet == 2)
						{
							curNote = lowNibble + (octave * 12) - 12;
						}
					}

					else if (curTrack == 3)
					{
						curNote = lowNibble + (octave * 12);
					}

					if (curNote >= 128)
					{
						while (curNote >= 128)
						{
							curNote -= 12;
						}
					}

					if (compatibility != 1)
					{
						curNoteLen = MM2lengths[highNibble];
					}

					else if (compatibility == 1)
					{
						curNoteLen = MM2lengthsBM[highNibble];
					}


					if (command[1] >= 0xD0 && command[1] < 0xE0)
					{
						highNibble = (command[1] & 15);
						if (compatibility != 1)
						{
							holdNote = MM2lengths[highNibble];
						}

						else if (compatibility == 1)
						{
							holdNote = MM2lengthsBM[highNibble];
							if (highNibble == 4 || highNibble == 5 || highNibble == 6 || highNibble == 7)
							{
								holdNote = 0;
								romPos++;
							}
							else if (curNoteLen == 360 && highNibble == 12 && songNum == 1)
							{
								holdNote = 30;
								romPos++;
							}
						}
					}
					curNoteLen += holdNote;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					romPos++;
					if (holdNote > 0)
					{
						romPos++;
					}
				}

				/*Unknown command*/
				else
				{
					romPos++;
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
		fclose(mid);

	}
}