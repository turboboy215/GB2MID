/*Sunsoft*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "SUNSOFT.H"

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
int curInst;
int version;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char SSMagicBytesA[5] = { 0x87, 0x5F, 0x16, 0x00, 0x21 };
const char SSMagicBytesB[6] = { 0x6D, 0x07, 0x6E, 0x07, 0x6F, 0x07 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void SSsong2mid(int songNum, long ptr);

void SSProc(int bank)
{
	foundTable = 0;
	firstPtr = 0;
	curInst = 0;
	version = 0;
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

	/*Try to search the bank for song table loader - Method 1*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], SSMagicBytesA, 5)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 5;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			version = 1;
		}
	}

	/*Method 2: frequency table*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], SSMagicBytesB, 6)) && foundTable != 1)
		{
			tableOffset = i + bankAmt + 6;
			printf("Song table starts at 0x%04x...\n", tableOffset);
			version = 0;
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;
		while ((ReadLE16(&romData[i])) > bankAmt && (ReadLE16(&romData[i])) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			SSsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
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

void SSsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curDelay = 0;
	int transpose = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	int macro1Times = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	int macro2Times = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	int macro3Times = 0;
	int repeat1 = 0;
	long repeat1Pos = 0;
	int repeat2 = 0;
	long repeat2Pos = 0;
	int autoLen = 0;
	int autoLenNum = 0;
	int k = 0;
	int inMacro = 0;
	unsigned char command[4];
	int ctrlDelay = 0;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	long tempPos = 0;
	int valSize = 0;
	long trackSize = 0;
	int tempCurTrack = 0;
	int actTrack = 0;

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

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

		romPos = ptr - bankAmt;

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			activeChan[curTrack] = 0;
		}

		while (romData[romPos] != 0xFF)
		{
			tempCurTrack = romData[romPos];
			switch (tempCurTrack)
			{
			case 0x00:
				curTrack = 0;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[0] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			case 0x01:
				curTrack = 1;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[1] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			case 0x02:
				curTrack = 2;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[2] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			case 0x03:
				curTrack = 3;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[3] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			case 0x04:
				curTrack = 0;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[0] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			case 0x05:
				curTrack = 1;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[1] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			case 0x06:
				curTrack = 2;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[2] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			case 0x07:
				curTrack = 3;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[3] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			default:
				curTrack = 0;
				actTrack = (romData[romPos + 1]) + 1;
				activeChan[0] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
				break;
			}
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
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
			repeat1 = -1;
			repeat2 = -1;
			transpose = 0;
			inMacro = 0;

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

			if (activeChan[curTrack] != 0 && activeChan[curTrack] >= bankAmt && activeChan[curTrack] < (bankSize * 2))
			{
				seqPos = activeChan[curTrack] - bankAmt;
				seqEnd = 0;
				autoLen = 0;
				curDelay = 0;
				ctrlDelay = 0;
				inMacro = 0;
				transpose = 0;


				while (seqEnd == 0 && seqPos < bankSize && midPos < 48000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note*/
					if (command[0] < 0xC8)
					{
						if (autoLen != 1)
						{
							curNote = command[0];
							curNoteLen = command[1] * 5;
							seqPos += 2;
						}
						else if (autoLen == 1)
						{
							curNote = command[0];
							seqPos++;
						}

						if (curNote < 0x4E && curTrack != 3)
						{
							curNote += 24;
						}
						else if (curNote > 0x4E)
						{
							curNote -= 0x4C;
						}

						if (curTrack == 3 && curNote < 36)
						{
							curNote += 36;
						}

						if (curNoteLen > 1000 && curTrack == 3 && songNum == 5)
						{
							curNoteLen = 1000;
						}

						if (curTrack != 3)
						{
							curNote += transpose;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
					}

					/*Set panning?*/
					else if (command[0] >= 0xC8 && command[0] <= 0xCA)
					{
						seqPos += 2;
					}


					/*Play drum note*/
					else if (command[0] >= 0xCB && command[0] <= 0xCE)
					{
						if (autoLen != 1)
						{
							curNote = 43;
							curNoteLen = command[1] * 5;
							seqPos += 2;
						}

						else if (autoLen == 1)
						{
							curNote = 43;
							seqPos++;
						}

						if (command[0] == 0xCE)
						{
							curNote = 44;
						}
						else if (command[0] == 0xCD)
						{
							curNote = 42;
						}
						else if (command[0] == 0xCE)
						{
							curNote = 41;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
					}

					/*Set noise parameters*/
					else if (command[0] >= 0xCF && command[0] <= 0xD2)
					{
						seqPos += 4;
					}

					/*Turn off automatic note length*/
					else if (command[0] == 0xD3)
					{
						autoLen = 0;
						seqPos++;
					}

					/*Tom tom note*/
					else if (command[0] >= 0xD4 && command[0] <= 0xD9)
					{
						seqPos++;
					}

					/*Set waveform, volume*/
					else if (command[0] >= 0xDA && command[0] <= 0xDF)
					{
						seqPos += 5;
						if (version == 1)
						{
							seqPos--;
						}
					}

					/*Set sweep?*/
					else if (command[0] >= 0xE0 && command[0] <= 0xE4)
					{
						seqPos++;
					}

					/*Set volume values?*/
					else if (command[0] >= 0xE5 && command[0] <= 0xE7)
					{
						seqPos++;
					}

					/*Sweep effect*/
					else if (command[0] == 0xE8)
					{
						seqPos += 2;
					}

					/*Fade note effect (v1)*/
					else if (command[0] == 0xE9)
					{
						seqPos++;
					}

					/*Fade note effect (v2)*/
					else if (command[0] == 0xEA)
					{
						seqPos++;
					}

					/*Hold note*/
					else if (command[0] == 0xEB)
					{
						if (autoLen != 1)
						{
							curDelay += command[1] * 5;
							seqPos += 2;
						}
						else if (autoLen == 1)
						{
							curDelay += curNoteLen;
							seqPos++;
						}

					}

					/*Set duty*/
					else if (command[0] == 0xEC)
					{
						seqPos += 2;
					}

					/*Exit macro*/
					else if (command[0] == 0xED)
					{
						if (inMacro == 1)
						{
							if (macro1Times > 0)
							{
								seqPos = macro1Pos;
								macro1Times--;
							}
							else
							{
								inMacro = 0;
								seqPos = macro1Ret;
							}

						}
						else if (inMacro == 2)
						{
							if (macro2Times > 0)
							{
								seqPos = macro2Pos;
								macro2Times--;
							}
							else
							{
								inMacro = 1;
								seqPos = macro2Ret;
							}
						}
						else if (inMacro == 3)
						{
							if (macro3Times > 0)
							{
								seqPos = macro3Pos;
								macro3Times--;
							}
							else
							{
								inMacro = 2;
								seqPos = macro2Ret;
							}
						}
						else if (inMacro == 0)
						{
							seqPos++;
						}
					}

					/*Go to macro*/
					else if (command[0] == 0xEE)
					{
						if (inMacro == 0)
						{
							inMacro = 1;
							macro1Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro1Ret = seqPos + 3;
							macro1Times = 0;
							seqPos = macro1Pos;
						}

						else if (inMacro == 1)
						{
							inMacro = 2;
							macro2Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro2Ret = seqPos + 3;
							macro2Times = 0;
							seqPos = macro2Pos;
						}

						else if (inMacro == 2)
						{
							inMacro = 3;
							macro3Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro3Ret = seqPos + 3;
							macro3Times = 0;
							seqPos = macro3Pos;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Repeat section (1)*/
					else if (command[0] == 0xEF)
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pos = ReadLE16(&romData[seqPos + 2]) - bankAmt;
						}
						else if (repeat1 > 0)
						{
							seqPos = repeat1Pos;
							repeat1--;
						}
						else
						{
							repeat1 = -1;
							seqPos += 4;
						}
					}

					/*Repeat section (2)*/
					else if (command[0] == 0xF0)
					{
						if (repeat2 == -1)
						{
							repeat2 = command[1];
							repeat2Pos = ReadLE16(&romData[seqPos + 2]) - bankAmt;
						}
						else if (repeat2 > 0)
						{
							seqPos = repeat2Pos;
							repeat2--;
						}
						else
						{
							repeat2 = -1;
							seqPos += 4;
						}
					}

					/*Go to loop point*/
					else if (command[0] == 0xF1)
					{
						seqEnd = 1;
					}

					/*Automatically set note length*/
					else if (command[0] == 0xF2)
					{
						autoLen = 1;

						curNoteLen = command[1] * 5;

						if (curNoteLen == 0)
						{
							autoLen = 0;
						}
						seqPos += 2;
					}

					/*Set tuning*/
					else if (command[0] == 0xF3)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0xF4)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Set echo?*/
					else if (command[0] == 0xF5)
					{
						seqPos += 2;
					}

					/*Rest*/
					else if (command[0] == 0xF6)
					{
						if (autoLen != 1)
						{
							curDelay += command[1] * 5;

							seqPos += 2;
						}
						else if (autoLen == 1)
						{
							curDelay += curNoteLen;
							seqPos++;
						}

					}

					/*Set vibrato delay time*/
					else if (command[0] == 0xF7)
					{
						seqPos += 2;
					}

					/*Pointer to vibrato parameters*/
					else if (command[0] == 0xF8)
					{
						seqPos += 3;
					}

					/*Set envelope*/
					else if (command[0] == 0xF9)
					{
						seqPos += 2;
					}

					/*Pointer to envelope parameters*/
					else if (command[0] == 0xFA)
					{
						seqPos += 3;
					}

					/*Pointer to waveform?*/
					else if (command[0] == 0xFB)
					{
						seqPos += 3;
					}

					/*Pointer to envelope fade parameters*/
					else if (command[0] == 0xFC)
					{
						seqPos += 3;
					}

					/*Set noise sweep strength?*/
					else if (command[0] == 0xFD || command[0] == 0xFE)
					{
						seqPos += 2;
					}

					/*End of track*/
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
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}