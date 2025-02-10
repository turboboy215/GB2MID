/*Hiroshi Wada*/
/*(Mega Man 1)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <direct.h>
#include "SHARED.H"
#include "MEGAMAN1.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
int i, j;
char outfile[1000000];
int songNum;
long curPtr;
long bankAmt;
int gameNum = 0;
long ptrList[100];
int curVol;
int curReptPt = 0;

int multiBanks;
int curBank;

char folderName[100];

/*Pointers for SolarStriker*/
const long SolarPtrs[14] = { 0x5541, 0x5603, 0x5D41, 0x5D8A, 0x5DDB, 0x5E5B, 0x5EB0, 0x5F23, 0x600E, 0x6210, 0x651D, 0x6B60, 0x6FCF, 0x70F8 };
/*Pointers for QIX*/
const long QixPtrs[28] = { 0x1725, 0x17F2, 0x1847, 0x188F, 0x18D9, 0x18F9, 0x191F, 0x1945, 0x196B, 0x199F, 0x19BC, 0x19D9, 0x19FF, 0x1A99, 0x1ABB, 0x1B0C, 0x1B32, 0x1B54, 0x1BFF, 0x1D04, 0x1D57, 0x1FD5, 0x22FD, 0x244D, 0x254D, 0x2611, 0x26E6, 0x287F };

/*Re-maps for MIDI note values*/
const int MM1noteVals[128] =
{
0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*Octave 0 - 00-0F (Percussion only)*/
24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,	/*Octave 1 - 10-1F*/
36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, /*Octave 2 - 20-2F*/
48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, /*Octave 3 - 30-3F*/
60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, /*Octave 4 - 40-4F*/
72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, /*Octave 5 - 50-5F*/
84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, /*Octave 6 - 60-6F*/
96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,/*Octave 7 - 70-7F*/
};

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void MM1song2mid(int songNum, long ptr);

unsigned static char* romData;
unsigned static char* midData;
unsigned static char* ctrlMidData;

long midLength;

long repeatPts[500][2];

void MM1Proc(int bank)
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

	/*Look for the start of the pointer table*/
	if (romData[0x05] == 0x43 && romData[0x06] == 0xC3)
	{
		printf("Detected game: Mega Man 1\n");
		gameNum = 1;
	}
	else if (romData[0x703] == 0x01 && romData[0x704] == 0x23)
	{
		printf("Detected game: SolarStriker\n");
		gameNum = 2;
	}
	else if (romData[0x600] == 0x01 && romData[0x601] == 0x23)
	{
		printf("Detected game: QIX\n");
		gameNum = 3;
	}

	if (gameNum == 0)
	{
		printf("ERROR: Unsupported game data!\n");
		exit(1);
	}
	else if (gameNum == 1)
	{
		/*Get pointers from list*/
		i = 0x0015;
		songNum = 1;
		while (ReadLE16(&romData[i]) >= bankSize)
		{
			curPtr = ReadLE16(&romData[i]);
			ptrList[songNum - 1] = curPtr;
			printf("Song %i address: 0x%04X\n", songNum, curPtr);
			MM1song2mid(songNum, curPtr);
			i += 2;
			songNum++;
		}
	}
	else if (gameNum == 2)
	{
		/*Get pointers from list*/
		songNum = 1;
		for (i = 0; i < 14; i++)
		{
			curPtr = SolarPtrs[i];
			printf("Song %i address: 0x%04X\n", songNum, curPtr);
			MM1song2mid(songNum, curPtr);
			songNum++;
		}
	}
	else if (gameNum == 3)
	{
		/*Get pointers from list*/
		songNum = 1;
		for (i = 0; i < 28; i++)
		{
			curPtr = QixPtrs[i];
			printf("Song %i address: 0x%04X\n", songNum, curPtr);
			MM1song2mid(songNum, curPtr);
			songNum++;
		}
	}
}

void MM1song2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int trackEnd = 0;
	int ticks = 120;
	int speed = 0;
	int k = 0;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int repeat1 = 0;
	int jumpAmt = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 120;

	int curInst = 0;

	unsigned long seqPos = 0;

	unsigned char command[3];

	unsigned long startPtrs[4];

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	midPos = 0;
	ctrlMidPos = 0;

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

		/*Get the tempo*/
		speed = romData[ptr - bankAmt];
		switch (speed)
		{
		case 0:
			tempo = 580;
			break;
		case 1:
			tempo = 430;
			break;
		case 2:
			tempo = 300;
			break;
		case 3:
			tempo = 220;
			break;
		case 4:
			tempo = 180;
			break;
		case 5:
			tempo = 150;
			break;
		case 6:
			tempo = 120;
			break;
		case 7:
			tempo = 110;
			break;
		case 8:
			tempo = 100;
			break;
		case 9:
			tempo = 90;
			break;
		case 10:
			tempo = 85;
			break;
		case 11:
			tempo = 80;
			break;
		case 12:
			tempo = 70;
			break;
		case 13:
			tempo = 68;
			break;
		case 14:
			tempo = 65;
			break;
		case 15:
			tempo = 60;
			break;
		default:
			tempo = 120;
			break;
		}

		/*Get starting pointers for each channel*/
		startPtrs[0] = ReadLE16(&romData[ptr + 2 - bankAmt]);
		startPtrs[1] = ReadLE16(&romData[ptr + 5 - bankAmt]);
		startPtrs[2] = ReadLE16(&romData[ptr + 8 - bankAmt]);
		startPtrs[3] = ReadLE16(&romData[ptr + 11 - bankAmt]);

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
			transpose = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			curVol = 100;


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
			seqPos = startPtrs[curTrack] - bankAmt;
			if (startPtrs[curTrack] == 0)
			{
				trackEnd = 1;
			}

			for (k = 0; k < 500; k++)
			{
				repeatPts[k][0] = 0;
				repeatPts[k][1] = 0;
			}
			curReptPt = 0;

			while (trackEnd == 0)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				/*Rest*/
				if (command[0] == 0x00)
				{
					curDelay += (command[1] * 30);
					seqPos += 2;
				}

				/*"Modulation" command - used in QIX for background drone*/
				else if (command[0] == 0x87)
				{
					seqPos += 2;
				}

				/*"Modulation" command - used in QIX for background drone*/
				else if (command[0] == 0x88)
				{
					seqPos += 2;
				}

				/*Jump command 1*/
				else if (command[0] >= 0x90 && command[0] <= 0x9F)
				{
					jumpAmt = (signed char)command[1];
					if ((command[0] - 0x90) == 0)
					{
						if (jumpAmt <= 0)
						{
							trackEnd = 1;
						}
						else
						{
							seqPos += jumpAmt * 2;
						}
					}
					else
					{
						for (k = 0; k < 500; k++)
						{
							if (repeatPts[k][0] == seqPos)
							{
								if (repeatPts[k][1] > 0)
								{
									repeatPts[k][1]--;
									seqPos = seqPos += jumpAmt * 2;
								}
								else if (repeatPts[k][1] == 0)
								{
									repeatPts[k][1] = command[0] - 0x90;
									seqPos += 2;
								}
								break;
							}
						}

						if (k == 500)
						{
							repeatPts[curReptPt][0] = seqPos;
							repeatPts[curReptPt][1] = command[0] - 0x90;
							curReptPt++;
						}
					}
				}

				/*Jump command 2*/
				else if (command[0] >= 0xA0 && command[0] <= 0xAF)
				{
					jumpAmt = (signed char)command[1];
					if ((command[0] - 0xA0) == 0)
					{
						if (jumpAmt <= 0)
						{
							trackEnd = 1;
						}
						else
						{
							seqPos += jumpAmt * 2;
						}
					}
					else
					{
						for (k = 0; k < 500; k++)
						{
							if (repeatPts[k][0] == seqPos)
							{
								if (repeatPts[k][1] > 0)
								{
									repeatPts[k][1]--;
									seqPos = seqPos += jumpAmt * 2;
								}
								else if (repeatPts[k][1] == 0)
								{
									repeatPts[k][1] = command[0] - 0xA0;
									seqPos += 2;
								}
								break;
							}
						}

						if (k == 500)
						{
							repeatPts[curReptPt][0] = seqPos;
							repeatPts[curReptPt][1] = command[0] - 0xA0;
							curReptPt++;
						}
					}
				}

				/*Jump command 3*/
				else if (command[0] >= 0xB0 && command[0] <= 0xBF)
				{
					jumpAmt = (signed char)command[1];
					if ((command[0] - 0xB0) == 0)
					{
						if (jumpAmt <= 0)
						{
							trackEnd = 1;
						}
						else
						{
							/*Workaround for "invalid" loop in Dr. Wily stage 1*/
							if (jumpAmt == 89 && command[2] == 0xFF)
							{
								trackEnd = 1;
							}
							else
							{
								seqPos += jumpAmt * 2;
							}

						}
					}
					else
					{
						for (k = 0; k < 500; k++)
						{
							if (repeatPts[k][0] == seqPos)
							{
								if (repeatPts[k][1] > 0)
								{
									repeatPts[k][1]--;
									seqPos = seqPos += jumpAmt * 2;
								}
								else if (repeatPts[k][1] == 0)
								{
									repeatPts[k][1] = command[0] - 0xB0;
									seqPos += 2;
								}
								break;
							}
						}

						if (k == 500)
						{
							repeatPts[curReptPt][0] = seqPos;
							repeatPts[curReptPt][1] = command[0] - 0xB0;
							curReptPt++;
						}
					}
				}


				else if (command[0] == 0xE6)
				{
					seqPos += 2;
				}

				/*Set transpose + 1 octave*/
				else if (command[0] == 0xE7)
				{
					if (command[1] == 1)
					{
						transpose = 12;
					}
					else if (command[1] == 0)
					{
						transpose = 0;
					}
					seqPos += 2;
				}

				/*Set transpose*/
				else if (command[0] == 0xEB)
				{
					seqPos += 2;
				}

				/*Set transpose*/
				else if (command[0] == 0xEC)
				{
					seqPos += 2;
				}

				/*Set transpose*/
				else if (command[0] == 0xED)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (command[0] == 0xEE)
				{
					seqPos += 2;
				}

				else if (command[0] == 0xEF)
				{
					seqPos += 2;
				}

				/*Set panning*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				else if (command[0] == 0xF1)
				{
					seqPos += 2;
				}

				/*Set waveform (channel 3 only)*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				else if (command[0] == 0xF3)
				{
					seqPos += 2;
				}

				/*Set instrument duty*/
				else if (command[0] == 0xF4)
				{
					seqPos += 2;
				}

				/*Set instrument sweep/pitch change*/
				else if (command[0] == 0xF5)
				{
					seqPos += 2;
				}

				else if (command[0] == 0xF6)
				{
					seqPos += 2;
				}

				else if (command[0] == 0xF7)
				{
					seqPos += 2;
				}

				/*Music/SFX identifier?*/
				else if (command[0] == 0xF8)
				{
					seqPos += 2;
				}

				else if (command[0] == 0xF9)
				{
					seqPos += 2;
				}

				else if (command[0] == 0xFA)
				{
					seqPos += 2;
				}

				/*Set note length*/
				else if (command[0] == 0xFB)
				{
					seqPos += 2;
				}

				/*Set envelope length?*/
				else if (command[0] == 0xFC)
				{
					seqPos += 2;
				}

				/*Set "echo" effect?*/
				else if (command[0] == 0xFD)
				{
					seqPos += 2;
				}

				/*Set velocity/volume?*/
				else if (command[0] == 0xFE)
				{
					curVol = command[1] * 10;

					if (curVol > 120)
					{
						curVol = 120;
					}
					seqPos += 2;
				}

				/*End channel*/
				else if (command[0] == 0xFF)
				{
					trackEnd = 1;
				}

				else if (command[0] < 0x80)
				{
					curNoteLen = command[1] * 30;
					curNote = MM1noteVals[command[0] + transpose];

					if (curTrack == 3)
					{
						/*Fix percussion mapping*/
						if (command[0] == 0x02)
						{
							curNote = 42;
						}
						else if (command[0] == 0x03)
						{
							curNote = 36;
						}
						else if (command[0] == 0x20)
						{
							curNote = 38;
						}
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos += 2;
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