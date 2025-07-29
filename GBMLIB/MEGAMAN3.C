/*Kouji Murata (Mega Man 3-5/Bionic Commando)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MEGAMAN3.H"

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
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int bcfix;
int pkFmt;
int songBank;
int isSFX;
int multiBanks;
int curBank;

char folderName[100];

const char MagicBytesA[6] = { 0xCE, 0x00, 0x67, 0x2A, 0x66, 0x6F };
const char MagicBytesB[7] = { 0xCE, 0x00, 0x67, 0xF1, 0x2A, 0x66, 0x6F };

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
void song2mid(int songNum, long ptr);
void pksong2mid(int songNum, long ptr);

extern void MM3Proc(int bank, char parameters[4][100])
{
	foundTable = 0;
	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	if (!strcmp(parameters[0], "n") || !strcmp(parameters[0], "N"))
	{
		pkFmt = 1;
	}
	else
	{
		pkFmt = 0;
	}
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	if (romData[0x0099] == 0x44 && romData[0x009A] == 0x07)
	{
		bcfix = 1;
	}
	else
	{
		bcfix = 0;
	}

	/*Try to search the bank for song table loader - Method 1: Mega Man 3/Bionic Commando*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MagicBytesA, 6)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc - bankAmt] + (romData[tablePtrLoc + 3 - bankAmt] * 0x100);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Method 2: Mega Man 4/5*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MagicBytesB, 7)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc - bankAmt] + (romData[tablePtrLoc + 3 - bankAmt] * 0x100);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		firstPtr = ReadLE16(&romData[i]);
		songNum = 1;
		while ((i + bankAmt) != firstPtr)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0)
			{
				if (pkFmt == 0)
				{
					song2mid(songNum, songPtr);
				}
				else
				{
					pksong2mid(songNum, songPtr);
				}


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
}

/*Convert the song data to MIDI*/
void song2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
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
	unsigned char command[8];
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
		mask = romData[romPos];

		/*Try to get active channels for sound effects*/
		maskArray[3] = mask >> 7 & 1;
		maskArray[2] = mask >> 6 & 1;
		maskArray[1] = mask >> 5 & 1;
		maskArray[0] = mask >> 4 & 1;

		/*Otherwise, it is music*/
		if (maskArray[0] == 0 && maskArray[1] == 0 && maskArray[2] == 0 && maskArray[3] == 0)
		{
			maskArray[3] = mask >> 3 & 1;
			maskArray[2] = mask >> 2 & 1;
			maskArray[1] = mask >> 1 & 1;
			maskArray[0] = mask & 1;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			transpose = 0;
			jumpPos1 = 0;
			jumpPosRet1 = 0;
			jumpPos2 = 0;
			jumpPosRet2 = 0;
			songLoopAmt = 0;
			songLoopPt = 0;


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

			if (maskArray[curTrack] != 0)
			{
				seqEnd = 0;
				romPos += 2;
				seqPos = ReadLE16(&romData[romPos]);
				repeatStart = seqPos;
				startPos = seqPos;
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && seqPos > bankAmt)
			{
				command[0] = romData[seqPos - bankAmt];
				command[1] = romData[seqPos + 1 - bankAmt];
				command[2] = romData[seqPos + 2 - bankAmt];
				command[3] = romData[seqPos + 3 - bankAmt];
				command[4] = romData[seqPos + 4 - bankAmt];
				command[5] = romData[seqPos + 5 - bankAmt];
				command[6] = romData[seqPos + 6 - bankAmt];
				command[7] = romData[seqPos + 7 - bankAmt];

				/*Rest*/
				if (command[0] >= 0xC0 && command[0] < 0xD0)
				{
					if (command[0] == 0xC0)
					{
						rest = 16;
					}
					else
					{
						rest = command[0] - 0xC0;
					}

					curDelay += (rest * chanSpeed * 5);
					seqPos++;
				}

				/*Initialize channel details*/
				else if (command[0] >= 0xD0 && command[0] < 0xE0)
				{
					chanSpeed = command[0] - 0xD0;
					if (curTrack == 0 || curTrack == 1)
					{
						if (command[4] < 0x80)
						{
							seqPos += 5;
						}
						else
						{
							seqPos += 7;
						}
					}
					else if (curTrack == 2)
					{
						seqPos += 4;
					}

					else if (curTrack == 3)
					{
						/*Fix for Bionic Commando drums*/
						if (bcfix == 1)
						{
							seqPos += 5;
						}
						else
						{
							seqPos++;
						}

					}
				}

				/*Set octave*/
				else if (command[0] >= 0xE0 && command[0] < 0xE8)
				{
					if (curTrack != 3)
					{
						octave = command[0] - 0xDF;
					}
					seqPos++;
				}

				/*Change noise?*/
				else if (command[0] == 0xE8)
				{
					seqPos += 2;
				}

				/*Change instrument parameter?*/
				else if (command[0] == 0xE9)
				{
					seqPos += 2;
				}

				/*Modulate sweep frequency?*/
				else if (command[0] == 0xEA)
				{
					seqPos += 2;
				}

				/*Change vibrato?*/
				else if (command[0] == 0xEB)
				{
					seqPos += 2;
				}

				/*Set channel transpose*/
				else if (command[0] == 0xEC)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				/*Command ED*/
				else if (command[0] == 0xED)
				{
					seqPos += 2;
				}

				/*Turn on sweep?*/
				else if (command[0] == 0xEE)
				{
					if (command[1] < 0x80)
					{
						seqPos += 2;
					}
					else
					{
						seqPos += 4;
					}
				}

				/*Set pitch tuning*/
				else if (command[0] == 0xEF)
				{
					seqPos += 2;
				}

				/*Set noise envelope?*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				/*Set noise frequency?*/
				else if (command[0] == 0xF1)
				{
					seqPos += 2;
				}

				/*Change envelope*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				/*Set echo?*/
				else if (command[0] == 0xF3)
				{
					seqPos += 2;
				}

				/*Noise sweep?*/
				else if (command[0] == 0xF4)
				{
					seqPos += 2;
				}

				/*Change channel speed*/
				else if (command[0] == 0xF6)
				{
					chanSpeed = command[1];
					seqPos += 2;
				}

				/*Jump to position (1)*/
				else if (command[0] == 0xF7)
				{
					/*Workaround for "endless loop" issue in MM3 Dr. Wily Stage 1*/
					if ((ReadLE16(&romData[seqPos + 1 - bankAmt]) == jumpPos1) && jumpPos1 == 0x6C29 && songNum == 78)
					{
						seqEnd = 1;
					}

					else if (ReadLE16(&romData[seqPos + 1 - bankAmt]) == startPos)
					{
						seqEnd = 1;
					}
					jumpPos1 = ReadLE16(&romData[seqPos + 1 - bankAmt]);
					jumpPosRet1 = seqPos + 3;
					seqPos = jumpPos1;
					holdNote = 0;
				}

				/*Jump to position (2)*/
				else if (command[0] == 0xF8)
				{
					/*Workaround for "endless loop" issue in MM3 Dr. Wily Stage 1*/
					if ((ReadLE16(&romData[seqPos + 1 - bankAmt]) == jumpPos1) && jumpPos1 == 0x6C29 && songNum == 78)
					{
						seqEnd = 1;
					}

					else if (ReadLE16(&romData[seqPos + 1 - bankAmt]) == startPos)
					{
						seqEnd = 1;
					}

					jumpPos2 = ReadLE16(&romData[seqPos + 1 - bankAmt]);
					jumpPosRet2 = seqPos + 3;
					seqPos = jumpPos2;
					holdNote = 0;
				}

				/*Return from jump (1)*/
				else if (command[0] == 0xF9)
				{
					seqPos = jumpPosRet1;
				}

				/*Return from jump (2)*/
				else if (command[0] == 0xFA)
				{
					seqPos = jumpPosRet2;
				}

				/*Start of repeat/loop*/
				else if (command[0] == 0xFB)
				{
					repeatStart = seqPos + 1;
					seqPos++;
				}

				/*Song loop point*/
				else if (command[0] == 0xFC)
				{
					songLoopPt = seqPos + 1;
					seqPos++;
				}

				/*Loop section amount of times*/
				else if (command[0] == 0xFD)
				{

					if (command[1] == 0x00)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat1 > 0)
						{
							seqPos = repeatStart;
							repeat1--;
						}

						else if (repeat1 == 0)
						{
							seqPos += 2;
							repeat1 = -1;
							repeatStart = seqPos;
						}

						else if (repeat1 == -1)
						{
							repeat1 = command[1];
							if (repeat1 == 255)
							{
								repeat1 = 1;
							}
						}
					}
				}

				/*Loop entire song amount of times*/
				else if (command[0] == 0xFE)
				{

					if (command[1] == 0x00)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat2 > 0)
						{
							seqPos = songLoopPt;
							repeat2--;
						}

						else if (repeat2 == 0)
						{
							seqPos += 2;
							repeat2 = -1;
							songLoopPt = seqPos;
						}

						else if (repeat2 == -1)
						{
							repeat2 = command[1];
							if (repeat1 == 255)
							{
								repeat1 = 1;
							}
						}

					}
				}

				/*End track*/
				else if (command[0] == 0xFF)
				{
					seqEnd = 1;
				}

				/*Play note*/
				else if (command[0] < 0xC0)
				{
					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);
					if (highNibble == 0)
					{
						highNibble = 16;
					}
					curNote = lowNibble + (12 * octave) + transpose;
					if (curTrack == 0 || curTrack == 1)
					{
						curNote += 12;
					}
					curNoteLen = highNibble * chanSpeed * 5;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos++;
				}


				/*Unknown command*/
				else
				{
					seqPos++;
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

/*Convert the song data to MIDI (Pocket King)*/
void pksong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
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
	unsigned char command[8];
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
		mask = romData[romPos];

		/*Try to get active channels for sound effects*/
		maskArray[3] = mask >> 7 & 1;
		maskArray[2] = mask >> 6 & 1;
		maskArray[1] = mask >> 5 & 1;
		maskArray[0] = mask >> 4 & 1;
		isSFX = 1;

		/*Otherwise, it is music*/
		if (maskArray[0] == 0 && maskArray[1] == 0 && maskArray[2] == 0 && maskArray[3] == 0)
		{
			maskArray[3] = mask >> 3 & 1;
			maskArray[2] = mask >> 2 & 1;
			maskArray[1] = mask >> 1 & 1;
			maskArray[0] = mask & 1;
			isSFX = 1;
		}

		/*Get the song's current bank*/
		songBank = romData[romPos + 2];
		fseek(rom, (songBank * bankSize), SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize);
		fread(exRomData, 1, bankSize, rom);
		romPos++;

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			transpose = 0;
			jumpPos1 = 0;
			jumpPosRet1 = 0;
			jumpPos2 = 0;
			jumpPosRet2 = 0;
			songLoopAmt = 0;
			songLoopPt = 0;


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

			if (maskArray[curTrack] != 0)
			{
				seqEnd = 0;
				romPos += 2;
				seqPos = ReadLE16(&romData[romPos]);
				repeatStart = seqPos;
				startPos = seqPos;
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && seqPos > bankAmt)
			{
				command[0] = exRomData[seqPos - bankAmt];
				command[1] = exRomData[seqPos + 1 - bankAmt];
				command[2] = exRomData[seqPos + 2 - bankAmt];
				command[3] = exRomData[seqPos + 3 - bankAmt];
				command[4] = exRomData[seqPos + 4 - bankAmt];
				command[5] = exRomData[seqPos + 5 - bankAmt];
				command[6] = exRomData[seqPos + 6 - bankAmt];
				command[7] = exRomData[seqPos + 7 - bankAmt];

				/*Rest*/
				if (command[0] >= 0xC0 && command[0] < 0xD0)
				{
					if (command[0] == 0xC0)
					{
						rest = command[1];
						seqPos++;
					}
					else
					{
						rest = command[0] - 0xC0;
					}

					curDelay += (rest * chanSpeed * 5);
					seqPos++;
				}

				/*Initialize channel details*/
				else if (command[0] >= 0xD0 && command[0] < 0xE0)
				{
					chanSpeed = command[0] - 0xD0;
					if (curTrack == 0 || curTrack == 1)
					{
						if (command[4] < 0x80)
						{
							seqPos += 5;
						}
						else
						{
							seqPos += 7;
						}
					}
					else if (curTrack == 2)
					{
						seqPos += 4;
					}

					else if (curTrack == 3)
					{
						/*Fix for Bionic Commando drums*/
						if (bcfix == 1)
						{
							seqPos += 5;
						}
						else
						{
							seqPos++;
						}

					}
				}

				/*Set octave*/
				else if (command[0] >= 0xE0 && command[0] < 0xE8)
				{
					if (curTrack != 3)
					{
						octave = command[0] - 0xDF;
					}
					seqPos++;
				}

				/*Change noise?*/
				else if (command[0] == 0xE8)
				{
					seqPos += 2;
				}

				/*Change instrument parameter?*/
				else if (command[0] == 0xE9)
				{
					seqPos += 2;
				}

				/*Modulate sweep frequency?*/
				else if (command[0] == 0xEA)
				{
					seqPos += 2;
				}

				/*Change vibrato?*/
				else if (command[0] == 0xEB)
				{
					seqPos += 2;
				}

				/*Set channel transpose*/
				else if (command[0] == 0xEC)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				/*Command ED*/
				else if (command[0] == 0xED)
				{
					seqPos += 2;
				}

				/*Turn on sweep?*/
				else if (command[0] == 0xEE)
				{
					if (command[1] < 0x80)
					{
						seqPos += 2;
					}
					else
					{
						seqPos += 4;
					}
				}

				/*Set pitch tuning*/
				else if (command[0] == 0xEF)
				{
					seqPos += 2;
				}

				/*Set noise envelope?*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				/*Set noise frequency?*/
				else if (command[0] == 0xF1)
				{
					seqPos += 2;
				}

				/*Change envelope*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				/*Set echo?*/
				else if (command[0] == 0xF3)
				{
					seqPos += 2;
				}

				/*Noise sweep?*/
				else if (command[0] == 0xF4)
				{
					seqPos += 2;
				}

				/*Change channel speed*/
				else if (command[0] == 0xF6)
				{
					chanSpeed = command[1];
					seqPos += 2;
				}

				/*Jump to position (1)*/
				else if (command[0] == 0xF7)
				{
					/*Workaround for "endless loop" issue in MM3 Dr. Wily Stage 1*/
					if ((ReadLE16(&exRomData[seqPos + 1 - bankAmt]) == jumpPos1) && jumpPos1 == 0x6C29 && songNum == 78)
					{
						seqEnd = 1;
					}

					else if (ReadLE16(&exRomData[seqPos + 1 - bankAmt]) == startPos)
					{
						seqEnd = 1;
					}
					jumpPos1 = ReadLE16(&exRomData[seqPos + 1 - bankAmt]);
					jumpPosRet1 = seqPos + 3;
					seqPos = jumpPos1;
					holdNote = 0;
				}

				/*Jump to position (2)*/
				else if (command[0] == 0xF8)
				{
					/*Workaround for "endless loop" issue in MM3 Dr. Wily Stage 1*/
					if ((ReadLE16(&exRomData[seqPos + 1 - bankAmt]) == jumpPos1) && jumpPos1 == 0x6C29 && songNum == 78)
					{
						seqEnd = 1;
					}

					else if (ReadLE16(&exRomData[seqPos + 1 - bankAmt]) == startPos)
					{
						seqEnd = 1;
					}

					jumpPos2 = ReadLE16(&exRomData[seqPos + 1 - bankAmt]);
					jumpPosRet2 = seqPos + 3;
					seqPos = jumpPos2;
					holdNote = 0;
				}

				/*Return from jump (1)*/
				else if (command[0] == 0xF9)
				{
					seqPos = jumpPosRet1;
				}

				/*Return from jump (2)*/
				else if (command[0] == 0xFA)
				{
					seqPos = jumpPosRet2;
				}

				/*Start of repeat/loop*/
				else if (command[0] == 0xFB)
				{
					repeatStart = seqPos + 1;
					seqPos++;
				}

				/*Song loop point*/
				else if (command[0] == 0xFC)
				{
					songLoopPt = seqPos + 1;
					seqPos++;
				}

				/*Loop section amount of times*/
				else if (command[0] == 0xFD)
				{

					if (command[1] == 0x00)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat1 > 0)
						{
							seqPos = repeatStart;
							repeat1--;
						}

						else if (repeat1 == 0)
						{
							seqPos += 2;
							repeat1 = -1;
							repeatStart = seqPos;
						}

						else if (repeat1 == -1)
						{
							repeat1 = command[1];
							if (repeat1 == 255)
							{
								repeat1 = 1;
							}
						}
					}
				}

				/*Loop entire song amount of times*/
				else if (command[0] == 0xFE)
				{

					if (command[1] == 0x00)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat2 > 0)
						{
							seqPos = songLoopPt;
							repeat2--;
						}

						else if (repeat2 == 0)
						{
							seqPos += 2;
							repeat2 = -1;
							songLoopPt = seqPos;
						}

						else if (repeat2 == -1)
						{
							repeat2 = command[1];
							if (repeat1 == 255)
							{
								repeat1 = 1;
							}
						}

					}
				}

				/*End track*/
				else if (command[0] == 0xFF)
				{
					seqEnd = 1;
				}

				/*Play note*/
				else if (command[0] < 0xC0)
				{
					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);
					if (highNibble == 0)
					{
						highNibble = 16;
					}
					curNote = lowNibble + (12 * octave) + transpose;
					if (curTrack == 0 || curTrack == 1)
					{
						curNote += 12;
					}
					if (highNibble != 16)
					{
						curNoteLen = highNibble * chanSpeed * 5;
					}
					else
					{
						curNoteLen = command[1] * 5;
						seqPos++;
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					holdNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos++;
				}


				/*Unknown command*/
				else
				{
					seqPos++;
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
		free(exRomData);
		fclose(mid);
	}
}