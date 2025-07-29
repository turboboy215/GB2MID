/*Alberto José González*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "AJG.H"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[3];
long songPtr;
int chanMask;
long bankAmt;
int foundTable;
int format;
int astNum;
int tempPtr;
int speedStart;
int repStart;
int bfFix;
int ltFix;
int rwFix;
int t3Fix;
int usFlag;
unsigned long seqList[500];

long switchPoint[400][2];
int switchNum;

const int PopUpPtrs[2] = { 0x59A6, 0x5E43 };
const int CoolBallPtrs[2] = { 0x5B14, 0x5FAB };
const int AstPtrs[13] = { 0x5712, 0x585F, 0x5D42, 0x5E3C, 0x60B8, 0x61FC, 0x654B, 0x66AF, 0x676B, 0x6A95, 0x6E62, 0x76CB, 0x7C65 };
const int PinoPtrs[4] = { 0x4C77, 0x5859, 0x5BF4, 0x6117 };

unsigned char* romData;
unsigned char* midData;
unsigned char* drumMidData;
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
void AJGsong2mid(int songNum, long ptr);

void AJGProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	format = 0;
	astNum = 0;
	tempPtr = 0;
	speedStart = 0;
	repStart = 0;
	bfFix = 0;
	ltFix = 0;
	rwFix = 0;
	t3Fix = 0;
	switchNum = 0;
	usFlag = 0;
	offset = strtoul(parameters[0], NULL, 16);
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

	/*Get the song locations from the offset*/
	i = offset - bankAmt;
	songNum = 1;
	if (offset == 0x4018)
	{

		/*The Smurfs*/
		if (ReadLE16(&romData[0x19]) == 0x5D7E)
		{
			format = 0;
			speedStart = 0xB9;
			repStart = 0xE9;
		}

		/*Obelix (GB)*/
		else if (ReadLE16(&romData[0x19]) == 0x4B2B)
		{
			format = 0;
			speedStart = 0xBD;
			repStart = 0xED;
		}

	}

	/*Turok*/
	else if (offset == 0x4065)
	{
		if (ReadLE16(&romData[0x66]) == 0x6F94)
		{
			format = 4;
			speedStart = 0xC8;
			repStart = 0xF7;
		}
		else if (ReadLE16(&romData[0x66]) == 0x6E68)
		{
			format = 1;
			speedStart = 0xC3;
			repStart = 0xF2;
		}
		else
		{
			format = 1;
			speedStart = 0xC5;
			repStart = 0xF4;
		}

	}

	/*Obelix (GBC)*/
	else if (offset == 0x405C)
	{
		format = 0;
		speedStart = 0xC2;
		repStart = 0xF2;
	}

	/*Tintin 1 (same parameters as Obelix (GB))*/
	else if (offset == 0x4060)
	{
		format = 0;
		speedStart = 0xBD;
		repStart = 0xED;
	}

	/*V-Rally (same as Turok 1)*/
	else if (offset == 0x408C)
	{
		format = 1;
		speedStart = 0xC5;
		repStart = 0xF4;
	}

	/*Ronaldo V-Football*/
	else if (offset == 0x406A)
	{
		format = 1;
		speedStart = 0xC9;
		repStart = 0xF8;
	}

	else
	{
		format = 1;
		speedStart = 0xC1;
		repStart = 0xF0;


		/*Fix for Spirou*/
		if (ReadLE16(&romData[0x63]) == 0x734A)
		{
			speedStart = 0xBF;
			repStart = 0xEE;
		}

		/*Fix for Die Maus (same as Obelix (GB))*/
		if (ReadLE16(&romData[0x63]) == 0x6696 || ReadLE16(&romData[0x63]) == 0x4C70)
		{
			speedStart = 0xBD;
			repStart = 0xED;
		}

		/*Fix for Hugo 2*/
		if (ReadLE16(&romData[0x63]) == 0x5F97 || ReadLE16(&romData[0x63]) == 0x5F9F)
		{
			speedStart = 0xC3;
			repStart = 0xF2;
		}

		/*Fix for Sylvester and Tweety*/
		if (ReadLE16(&romData[0x63]) == 0x6B85)
		{
			speedStart = 0xBE;
			repStart = 0xED;
		}

	}
	if (romData[i] == 0xC3)
	{
		if (bank == 0x02 && offset == 0x60F2)
		{
			format = 2;
		}
		while (romData[i] == 0xC3)
		{
			songPtr = ReadLE16(&romData[i + 1]);

			if (songPtr == 0x4925 && offset == 0x4018)
			{
				break;
			}
			if ((songPtr == 0x491D || songPtr == 0x6F8F || songPtr == 0x6888) && offset == 0x4062)
			{
				break;
			}
			if (songPtr == 0x5B30 && offset == 0x406A)
			{
				break;
			}
			if (songPtr == 0x491D && offset == 0x4060)
			{
				break;
			}
			if ((songPtr == 0x4919 || songPtr == 0x4937) && offset == 0x4065)
			{
				break;
			}
			if (songPtr == 0x5BCD && offset == 0x408C)
			{
				break;
			}
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			AJGsong2mid(songNum, songPtr);
			i += 3;
			songNum++;

		}
	}
	/*Alternate method for Pop Up*/
	else if (bank == 0x02 && offset == 0x4000 && romData[0x00] == 0x5D)
	{
		format = 6;
		speedStart = 0xE0;
		songPtr = PopUpPtrs[0];
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
		i += 2;
		songNum++;
		songPtr = PopUpPtrs[1];
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
	}

	/*Alternate method for Cool Ball (Pop Up US)*/
	else if (bank == 0x02 && offset == 0x4000 && romData[0x00] == 0x00)
	{
		format = 6;
		speedStart = 0xE0;
		songPtr = CoolBallPtrs[0];
		usFlag = 1;
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
		i += 2;
		songNum++;
		songPtr = CoolBallPtrs[1];
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
	}

	/*Alternate method for Asterix*/
	else if (bank == 0x04 && offset == 0x4000)
	{
		format = 3;
		speedStart = 0xCD;
		repStart = 0xEB;
		for (astNum = 0; astNum < 13; astNum++)
		{
			songPtr = AstPtrs[astNum];
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			AJGsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
	}

	/*Alternate method for Pinocchio*/
	else if (bank == 0x03 && offset == 0x4000)
	{
		format = 3;
		speedStart = 0xCD;
		repStart = 0xEB;
		for (astNum = 0; astNum < 4; astNum++)
		{
			songPtr = PinoPtrs[astNum];
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			AJGsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
	}

	/*Baby Felix*/
	else if (bank == 0x14 && offset == 0x4086)
	{
		format = 5;
		tempPtr = ReadLE16(&romData[offset - bankAmt]) - bankAmt;
		for (astNum = 0; astNum < 11; astNum++)
		{
			speedStart = 0xC0;
			repStart = 0xEF;
			bfFix = 1;
			songPtr = ReadLE16(&romData[tempPtr]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			AJGsong2mid(songNum, songPtr);
			tempPtr += 2;
			songNum++;
		}
	}

	/*Turok RW/3*/
	else if (bank == 0x40 && offset == 0x4087)
	{
		format = 5;
		tempPtr = romData[offset + 1 - bankAmt] + (romData[offset + 4 - bankAmt] * 0x100);
		if (tempPtr == 0x67BA)
		{
			rwFix = 1;
		}
		else if (tempPtr == 0x6D84)
		{
			t3Fix = 1;
		}
		for (astNum = 0; astNum < 10; astNum++)
		{
			speedStart = 0xC9;
			repStart = 0xF8;
			songPtr = ReadLE16(&romData[tempPtr - bankAmt]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			AJGsong2mid(songNum, songPtr);
			tempPtr += 2;
			songNum++;
		}
	}

	/*Radikal Bikers*/
	else if (bank == 0x40 && offset == 0x4093)
	{
		format = 5;
		tempPtr = romData[offset + 1 - bankAmt] + (romData[offset + 4 - bankAmt] * 0x100);
		for (astNum = 0; astNum < 9; astNum++)
		{
			speedStart = 0xC9;
			repStart = 0xF8;
			songPtr = ReadLE16(&romData[tempPtr - bankAmt]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			AJGsong2mid(songNum, songPtr);
			tempPtr += 2;
			songNum++;
		}
	}

	/*Looney Tunes Collector games*/
	else if ((bank == 0x31 || bank == 0x2C || bank == 0x32 || bank == 0x30) && offset == 0x4087)
	{
		format = 5;
		ltFix = 1;
		tempPtr = romData[offset + 1 - bankAmt] + (romData[offset + 4 - bankAmt] * 0x100);
		for (astNum = 0; astNum < 12; astNum++)
		{
			speedStart = 0xC9;
			repStart = 0xF8;
			songPtr = ReadLE16(&romData[tempPtr - bankAmt]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			AJGsong2mid(songNum, songPtr);
			tempPtr += 2;
			songNum++;
		}
	}

	/*Add "missing" jingles to the Smurfs 1*/
	if (offset == 0x4018 && bank == 7 && romData[0x01] == 0x90)
	{
		songPtr = 0x7DF3;
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
		songNum++;
		songPtr = 0x7ECD;
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
	}

	/*Add additional songs to Bomb Jack*/
	if (offset == 0x60F2 && bank == 2)
	{
		songPtr = 0x60FE;
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
		songNum++;
		songPtr = 0x743E;
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
	}

	/*Add missing "full version" of song 1 from Bugs Bunny*/
	if (offset == 0x4062 && bank == 3 && ReadLE16(&romData[0x1140]) == 0x4CBD)
	{
		songPtr = 0x4CBD;
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		AJGsong2mid(songNum, songPtr);
	}

	free(romData);
}

void AJGsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int curTrack = 0;
	int seqEnd = 0;
	int curPat = 0;
	int totalPats = 0;
	int trackCnt = 4;
	long curSeq = 0;
	long patLoop = 0;
	int curSeqNum = 0;
	int isInSeqPos = 0;
	int k = 0;
	int curNote = 0;
	int curChanSpeed = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	long seqPos = 0;
	int trackEnd = 0;
	unsigned char command[4];
	int transpose = 0;
	int globalTranspose = 0;
	int c4Speed = 0;
	long c4Offset = 0;
	int repeat = -1;
	long loopPt = 0;
	int ticks = 120;
	long startPos = 0;
	int midPos = 0;
	int firstNote = 1;
	int multiplier = 5;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	unsigned int drumMidPos = 0;
	long drumMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 150;

	int curInst = 0;

	long ctrlDelay = 0;
	long masterDelay = 0;
	long drumDelay = 0;
	int seqNums[3];
	int extLen = 0;
	int extPos = 0;
	int jump2Pos = 0;
	long jumpPos = 0;
	int jumpRet = 0;
	int repeatStart = 0;
	int seqCheck = 0;
	int seqListPos = 0;
	int curDrumSpeed = 0;
	long drumOfs = 0;
	long drumPos = 0;
	int curDrum = 0;
	int firstDrumNote = 1;
	int drumPosDelay = 0;
	int drumPt = 0;
	int hasPatLoop[3];
	int doubleLength = 0;
	int tempLength = 0;
	int arpLen = 0;
	seqNums[0] = 0;
	seqNums[1] = 0;
	seqNums[2] = 0;
	hasPatLoop[0] = 0;
	hasPatLoop[1] = 0;
	hasPatLoop[2] = 0;
	midPos = 0;
	ctrlMidPos = 0;
	switchNum = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	drumMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
		ctrlMidData[j] = 0;
		drumMidData[j] = 0;
	}

	for (switchNum = 0; switchNum < 400; switchNum++)
	{
		switchPoint[switchNum][0] = -1;
		switchPoint[switchNum][1] = 0;
	}

	sprintf(outfile, "song%i.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
		exit(2);
	}
	else
	{
		/*Look for the "real" start of the channels in the code*/
		j = ptr - bankAmt;
		while (romData[j] != 0x11)
		{
			j++;
		}
		seqPtrs[0] = ReadLE16(&romData[j + 1]);
		if (ReadLE16(&romData[j + 11]) < ReadLE16(&romData[j + 16]))
		{
			seqPtrs[1] = ReadLE16(&romData[j + 7]);
			seqPtrs[2] = ReadLE16(&romData[j + 13]);
		}
		else
		{
			seqPtrs[1] = ReadLE16(&romData[j + 13]);
			seqPtrs[2] = ReadLE16(&romData[j + 7]);
		}

		/*Clear the sequence array*/
		for (j = 0; j < 500; j++)
		{
			seqList[j] = 0;
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

		/*Add drum (percussion) track header*/
		firstNote = 1;
		/*Write MIDI chunk header with "MTrk"*/
		WriteBE32(&drumMidData[drumMidPos], 0x4D54726B);
		drumMidPos += 8;
		drumMidTrackBase = drumMidPos;
		valSize = WriteDeltaTime(drumMidData, drumMidPos, 0);
		drumMidPos += valSize;
		WriteBE16(&drumMidData[drumMidPos], 0xFF03);
		drumMidPos += 2;
		Write8B(&drumMidData[drumMidPos], strlen(TRK_NAMES[3]));
		drumMidPos++;
		sprintf((char*)&drumMidData[drumMidPos], TRK_NAMES[3]);
		drumMidPos += strlen(TRK_NAMES[3]);

		/*Calculate MIDI channel size*/
		trackSize = drumMidPos - drumMidTrackBase;
		WriteBE16(&drumMidData[drumMidTrackBase - 2], trackSize);

		/*Clear the sequence array*/
		for (j = 0; j < 500; j++)
		{
			seqList[j] = 0;
		}

		for (curTrack = 0; curTrack < 3; curTrack++)
		{
			seqListPos = 0;
			transpose = 0;
			globalTranspose = 0;
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			trackEnd = 0;


			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;

			masterDelay = 0;

			switchNum = 0;



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


			/*Get the pointers to each sequence*/
			j = seqPtrs[curTrack];
			if (curTrack == 0)
			{
				while (j != seqPtrs[1] && j != seqPtrs[2] && ReadLE16(&romData[j - bankAmt]) < (bankSize * 2) && ReadLE16(&romData[j - bankAmt]) > bankAmt && ReadLE16(&romData[j - bankAmt]) != 0x0000 && isInSeqPos == 0)
				{
					for (seqCheck = 0; seqCheck < 500; seqCheck++)
					{
						if (j == seqList[seqCheck])
						{
							isInSeqPos = 1;
						}
					}
					if (isInSeqPos == 0)
					{
						curSeq = ReadLE16(&romData[j - bankAmt]);
						seqList[curSeqNum] = curSeq;
						curSeqNum++;
						seqNums[curTrack]++;
						j += 2;
					}

				}
			}
			else if (curTrack == 1)
			{
				while (j != seqPtrs[0] && j != seqPtrs[2] && ReadLE16(&romData[j - bankAmt]) < (bankSize * 2) && ReadLE16(&romData[j - bankAmt]) > bankAmt && ReadLE16(&romData[j - bankAmt]) != 0x0000 && isInSeqPos == 0)
				{
					for (seqCheck = 0; seqCheck < 500; seqCheck++)
					{
						if (j == seqList[seqCheck])
						{
							isInSeqPos = 1;
						}
					}
					if (isInSeqPos == 0)
					{
						curSeq = ReadLE16(&romData[j - bankAmt]);
						seqList[curSeqNum] = curSeq;
						curSeqNum++;
						seqNums[curTrack]++;
						j += 2;
					}
				}
			}
			else if (curTrack == 2)
			{
				while (j != seqPtrs[0] && j != seqPtrs[1] && ReadLE16(&romData[j - bankAmt]) < (bankSize * 2) && ReadLE16(&romData[j - bankAmt]) > bankAmt && ReadLE16(&romData[j - bankAmt]) != 0x0000 && isInSeqPos == 0)
				{
					for (seqCheck = 0; seqCheck < 500; seqCheck++)
					{
						if (j == seqList[seqCheck])
						{
							isInSeqPos = 1;
						}
					}
					if (isInSeqPos == 0)
					{
						curSeq = ReadLE16(&romData[j - bankAmt]);
						seqList[curSeqNum] = curSeq;
						curSeqNum++;
						seqNums[curTrack]++;
						j += 2;
					}
				}
			}

			if (ReadLE16(&romData[j - bankAmt]) == 0x0000)
			{
				hasPatLoop[curTrack] = 1;
			}

			j = seqPtrs[curTrack];

			if (format == 3 && seqNums[curTrack] == 0)
			{
				seqNums[curTrack] = 1;
			}
			for (curSeqNum = 0; curSeqNum < seqNums[curTrack]; curSeqNum++)
			{
				curSeq = ReadLE16(&romData[j - bankAmt]);
				seqPos = curSeq;
				seqEnd = 0;

				while (seqEnd == 0)
				{
					/*Version 0/1/4/5*/
					if (format == 0 || format == 1 || format == 4 || format == 5)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos - bankAmt + 1];
						command[2] = romData[seqPos - bankAmt + 2];
						command[3] = romData[seqPos - bankAmt + 3];

						if (command[0] == 0x06 && songNum == 6 && ptr == 0x5C6E && offset == 0x4062 && bank == 0x10)
						{
							seqPos++;
							command[0] = romData[seqPos - bankAmt];
							command[1] = romData[seqPos - bankAmt + 1];
							command[2] = romData[seqPos - bankAmt + 2];
							command[3] = romData[seqPos - bankAmt + 3];
						}

						/*Transpose check*/
						if (curTrack != 0)
						{
							for (switchNum = 0; switchNum < 90; switchNum++)
							{
								if (switchPoint[switchNum][0] == masterDelay)
								{
									globalTranspose = switchPoint[switchNum][1];
								}
							}
						}

						if (rwFix == 1 && curSeq == 0x64AC)
						{
							seqEnd = 1;
							j += 2;
						}

						/*Fixes for Smurfs 3*/
						if (offset == 0x4062 && (ptr == 0x55E1) && curTrack == 2)
						{
							if (transpose == 10)
							{
								transpose = 12;
							}
						}

						if (offset == 0x4062 && (ptr == 0x6931) && curTrack < 2)
						{
							globalTranspose = 2;
						}

						/*Fix for Bugs Bunny*/
						if (offset == 0x4062 && (ptr == 0x4B23 || ptr == 0x4CBD) && curTrack == 2)
						{
							if (transpose == -2)
							{
								transpose = 0;
							}
						}

						/*Fixes for Baby Felix*/
						if (bfFix == 1 && songNum == 6)
						{
							if (curTrack < 2)
							{
								globalTranspose = 3;
							}
						}


						if (bfFix == 1 && songNum == 9)
						{
							if (curTrack < 2)
							{
								globalTranspose = 1;
							}
						}

						if (bfFix == 1 && seqPos == 0x68FF && curSeq == 0x68B9)
						{
							jump2Pos = 0;
						}


						if (ltFix == 1 && seqPos == 0x6100)
						{
							seqEnd = 0;
						}
						if (command[0] < 0x60)
						{
							/*if (command[0] == 0)
							{
								seqPos++;
							}*/

							curNote = command[0] + 12 + transpose + globalTranspose;

							if (curNote >= 0x80)
							{
								curNote = 0x7F;
							}
							if (command[1] != 0x65)
							{
								curNoteLen = curChanSpeed;
							}
							else if (command[1] == 0x65)
							{
								curNoteLen = curChanSpeed;
								extLen = 1;
								while (romData[seqPos + extLen - bankAmt] == 0x65)
								{
									curNoteLen += curChanSpeed;
									extLen++;
								}
								seqPos += extLen - 1;
							}

							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}

						/*Enable "tom tom" mode, transpose*/
						else if (command[0] == 0x60)
						{
							if (ltFix == 0)
							{
								seqPos += 2;
							}
							else if (ltFix == 1)
							{
								seqPos++;
							}

						}

						/*Disable "tom tom" mode?*/
						else if (command[0] == 0x61)
						{
							seqPos++;
						}

						/*Rest for a bar*/
						else if (command[0] == 0x62)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*Set channel parameters*/
						else if (command[0] == 0x63)
						{
							seqPos += 4;
						}

						/*Half rest?*/
						else if (command[0] == 0x64)
						{
							seqPos++;
						}

						/*Hold note for a bar*/
						else if (command[0] == 0x65)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*No note/rest for a bar*/
						else if (command[0] == 0x66)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*End of sequence (no loop)?*/
						else if (command[0] == 0x67)
						{
							if (format == 5)
							{
								seqEnd = 1;
							}
							else
							{
								seqPos++;
							}

						}

						/*End of final note*/
						else if (command[0] == 0x68)
						{
							seqEnd = 1;
							trackEnd = 1;
							j += 2;
						}

						/*Transpose (global)*/
						else if (command[0] == 0x69)
						{
							globalTranspose = (signed char)command[1];
							if (curTrack == 0)
							{
								switchPoint[switchNum][0] = masterDelay;
								switchPoint[switchNum][1] = globalTranspose;
								switchNum++;
							}
							seqPos += 2;
						}

						/*End of sequence*/
						else if (command[0] == 0x6A)
						{
							if (rwFix == 1 && songNum >= 8)
							{
								seqEnd = 1;
								j += 2;
							}
							else if (t3Fix == 1 && curTrack == 2 && songNum == 3)
							{
								seqEnd = 1;
								j += 2;
							}
							else
							{
								if (jump2Pos == 0)
								{
									seqEnd = 1;
									j += 2;
								}
								else if (jump2Pos == 1)
								{
									seqPos = jumpRet;
									jump2Pos = 0;
								}
							}


						}

						/*Manually set length of next note OR rest*/
						else if (command[0] == 0x6B)
						{
							curChanSpeed = command[1] * 5;
							seqPos += 2;
						}

						/*Set up channel 4 speed and offset*/
						else if (command[0] == 0x6C)
						{
							curDrumSpeed = command[1] * 5;
							drumOfs = ReadLE16(&romData[seqPos + 2 - bankAmt]);

							drumPos = drumOfs - bankAmt;
							drumDelay = masterDelay - drumPosDelay;
							drumPosDelay += drumDelay;

							while (romData[drumPos] <= 0xD0)
							{
								if (romData[drumPos] < 0x30)
								{
									curDrum = romData[drumPos] + 28;

									if (curDrum != 24)
									{
										tempPos = WriteNoteEvent(drumMidData, drumMidPos, curDrum, curDrumSpeed, drumDelay, firstDrumNote, 3, curInst);
										firstDrumNote = 0;
										drumMidPos = tempPos;
										drumPosDelay += curDrumSpeed;
										drumDelay = 0;
										drumPos++;
									}
									else if (curDrum == 24)
									{
										drumPosDelay += curDrumSpeed;
										drumDelay += curDrumSpeed;
										drumPos++;
									}
								}
								else if (romData[drumPos] >= 0x30)
								{
									drumPos++;
								}

							}
							drumDelay = 0;
							seqPos += 4;
						}

						/*End channel without loop after 0x6D command*/
						else if (command[0] == 0x6D)
						{
							seqPos++;
						}

						/*Effect/decay options*/
						else if (command[0] >= 0x6E && command[0] < 0x74)
						{
							seqPos++;
						}

						/*Set channel note length/others*/
						else if (command[0] == 0x74)
						{
							if (command[2] >= 0x01)
							{
								seqPos += 4;
							}
							else if (command[2] == 0x00)
							{
								seqPos += 3;
							}
						}

						/*Set channel note size?*/
						else if (command[0] == 0x75)
						{
							if (format < 5)
							{
								seqPos += 2;
							}
							else
							{
								if (command[1] == 0x8C)
								{
									seqPos += 2;
								}
								else if (command[1] == 0xF1)
								{
									if (command[2] == 0x00)
									{
										seqPos += 3;
									}
									else
									{
										seqPos += 4;
									}

								}

								else if (command[2] == 0x00)
								{
									seqPos += 3;
									if (romData[seqPos - bankAmt] == 0x00)
									{
										seqPos++;
									}
								}
								else
								{
									seqPos += 4;
								}

							}

						}

						/*Unknown channel 3 setting 76*/
						else if (command[0] == 0x76)
						{
							seqPos += 2;
						}

						/*Fade in effect (all channels)*/
						else if (command[0] == 0x77)
						{
							if (format != 5)
							{
								seqPos += 4;
							}
							else if (format == 5)
							{
								if (rwFix == 0)
								{
									if (command[1] > 0x00)
									{
										seqPos += 2;
									}
									else if (command[1] == 0x00)
									{
										seqPos += 3;
									}
								}
								else if (rwFix == 1)
								{
									if (command[2] == 0x6A && curTrack != 0 && command[1] != 0x08)
									{
										seqPos--;
									}
									if (command[1] == 0x02)
									{
										seqPos--;
									}
									if (command[1] == 0x08)
									{
										seqPos--;
									}
									seqPos += 3;
									if (command[1] == 0x00 && command[3] == 0x00)
									{
										seqPos++;
									}
								}

							}
						}

						/*Unknown command 78*/
						else if (command[0] == 0x78)
						{
							seqPos++;
						}

						/*Switch waveform*/
						else if (command[0] == 0x79)
						{
							seqPos += 3;
						}

						/*Transpose (current channel)*/
						else if (command[0] == 0x7A)
						{
							if (format != 5)
							{
								transpose = (signed char)command[1];
								if (offset == 0x4018 && transpose == 10)
								{
									transpose = 12;
								}
								seqPos += 2;
							}

							/*Switch waveform*/
							else if (format == 5)
							{
								seqPos += 3;
							}

						}

						/*Set channel envelope with/without echo*/
						else if (command[0] == 0x7B)
						{
							if (command[1] >= 0x01)
							{
								seqPos += 3;
								if (format == 5)
								{
									seqPos--;
								}
							}
							else if (command[1] == 0x00)
							{
								seqPos += 2;
							}
						}

						/*Turn off effect*/
						else if (command[0] == 0x7C)
						{
							if (format != 5)
							{
								seqPos++;
							}

							else if (format == 5)
							{
								if (command[1] > 0)
								{
									seqPos += 3;
								}
								else if (command[1] == 0)
								{
									seqPos += 2;
								}

							}
						}

						/*Go to loop point*/
						else if (command[0] == 0x7D)
						{
							if (hasPatLoop[curTrack] == 1 && format != 5)
							{
								seqPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
							}
							else
							{
								if (format == 5)
								{
									seqPos++;
								}
								else
								{
									seqEnd = 1;
									j += 2;
								}

							}

						}

						/*Jump to position*/
						else if (command[0] == 0x7E)
						{
							jump2Pos = 1;
							jumpPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
							jumpRet = seqPos + 3;

							if (jumpPos == 0x4CC9 && format == 5)
							{
								seqEnd = 1;
							}
							seqPos = jumpPos;
						}

						/*Load routine*/
						else if (command[0] == 0x7F)
						{
							if (format != 5)
							{
								seqPos += 3;
							}
							else if (format == 5)
							{
								jump2Pos = 1;
								jumpPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
								jumpRet = seqPos + 3;
								seqPos = jumpPos;
							}

						}

						/*End of repeat point*/
						else if (command[0] == 0x80)
						{
							if (format != 5)
							{
								if (repeat > 1)
								{
									seqPos = repeatStart;
									repeat--;
								}
								else if (repeat <= 1)
								{
									seqPos++;
								}
							}
							else
							{
								seqPos++;
							}

						}

						/*Unknown commands 81 and 82*/
						else if (command[0] == 0x81 || command[0] == 0x82)
						{
							if (format == 5 && command[0] == 0x81)
							{
								if (repeat > 1)
								{
									seqPos = repeatStart;
									repeat--;
								}
								else if (repeat <= 1)
								{
									seqPos++;
								}
							}
							else
							{
								seqPos++;
							}

						}

						/*Unknown command 83*/
						else if (command[0] == 0x83)
						{
							seqPos += 2;
						}

						/*Switch instrument*/
						else if (command[0] >= 0x84 && command[0] < 0x90)
						{
							if (format == 1 && command[0] == 0x84 && speedStart == 0xBF)
							{
								seqPos += 5;
							}
							else
							{
								if (bfFix == 1 && command[0] == 0x89)
								{
									arpLen = 0;
									while (romData[seqPos + arpLen - bankAmt] != 0x6A && romData[seqPos + arpLen - bankAmt] != 0xFF)
									{
										arpLen++;
									}
									seqPos += arpLen;
								}
								/*curInst = command[0] - 0x80;
								firstNote = 1;*/
								seqPos++;
							}

						}

						/*Effect*/
						else if (command[0] >= 0x90 && command[0] < 0xA9)
						{
							if (command[0] == 0xA1 && format == 4 && command[1] < 0x60)
							{
								seqPos += 3;
							}
							seqPos++;
						}

						/*Unknown command A9*/
						else if (command[0] == 0xA9)
						{
							if (format == 0)
							{
								seqPos += 4;
							}
							else if (format == 1 || format == 5)
							{
								seqPos++;
							}

						}

						/*More effects*/
						else if (command[0] >= 0xAA && command[0] < 0xAC)
						{
							seqPos++;
						}

						/*Set up waveform/volume envelope*/
						else if (command[0] >= 0xAC && command[0] < speedStart)
						{
							if (format != 4)
							{
								if (format == 5)
								{
									if (command[0] == 0xB0 && command[1] == 0x75 && command[3] > 0)
									{
										seqPos++;
									}
									if (command[0] == 0xBF && bfFix == 1)
									{
										seqPos += -2;
									}
									if (command[0] < 0xC6)
									{
										if (command[0] == 0xAD)
										{
											seqPos -= 2;
											if (bfFix == 0)
											{
												seqPos++;
											}
										}
										if (bfFix == 0 || bfFix == 1)
										{
											seqPos--;
										}
										seqPos++;
									}

								}
								seqPos += 4;
								if (ltFix == 1 && (command[0] == 0xAE || command[0] == 0xAF))
								{
									seqPos += -3;
								}
								else if (rwFix == 1 && (command[0] == 0xAE || command[0] == 0xAF))
								{
									seqPos += -3;
								}
								else if (t3Fix == 1 && (command[0] == 0xAE || command[0] == 0xAF))
								{
									seqPos += -3;
								}
								else if (rwFix == 1 && command[0] == 0xB5)
								{
									seqPos += -3;
								}
							}
							else if (format == 4)
							{
								if (command[0] < 0xBC)
								{
									seqPos++;
								}
								else
								{
									seqPos += 4;
								}

							}

						}

						/*Change channel speed*/
						else if (command[0] >= speedStart && command[0] < repStart)
						{
							if (format == 0)
							{
								curChanSpeed = (command[0] - (speedStart - 1)) * 5;
								seqPos++;
							}
							else if (format == 1 || format == 4 || format == 5)
							{
								curChanSpeed = (command[0] - (speedStart - 2)) * 5;
								seqPos++;

							}

						}

						/*Repeat*/
						else if (command[0] >= repStart)
						{
							repeat = command[0] - (repStart - 2);
							repeatStart = seqPos + 1;
							seqPos++;

						}

						/*Unknown command*/
						else
						{
							seqPos++;
						}
					}

					/*Format 2 (Bomb Jack)*/
					else if (format == 2)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos - bankAmt + 1];
						command[2] = romData[seqPos - bankAmt + 2];
						command[3] = romData[seqPos - bankAmt + 3];

						/*Transpose check*/
						if (curTrack != 0)
						{
							for (switchNum = 0; switchNum < 90; switchNum++)
							{
								if (switchPoint[switchNum][0] == masterDelay)
								{
									globalTranspose = switchPoint[switchNum][1];
								}
							}
						}

						if (command[0] < 0x60)
						{
							curNote = command[0] + 12 + transpose + globalTranspose;

							if (curNote >= 0x80)
							{
								curNote = 0x7F;
							}
							if (command[1] != 0x63)
							{
								curNoteLen = curChanSpeed;
							}
							else if (command[1] == 0x63)
							{
								curNoteLen = curChanSpeed;
								extLen = 1;
								while (romData[seqPos + extLen - bankAmt] == 0x63)
								{
									curNoteLen += curChanSpeed;
									extLen++;
								}
								seqPos += extLen - 1;
							}

							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}

						/*Enable "tom tom" mode, transpose*/
						else if (command[0] == 0x60)
						{
							seqPos += 2;
						}

						/*Double length of the following note*/
						else if (command[0] == 0x61)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*Unknown command 62*/
						else if (command[0] == 0x62)
						{
							seqPos += 4;
						}

						/*End of sequence (no loop)*/
						else if (command[0] == 0x66)
						{
							seqEnd = 1;
							j += 2;
						}

						/*Hold note for a bar*/
						else if (command[0] == 0x63)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*Rest for a bar*/
						else if (command[0] == 0x64)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*Transpose (global)*/
						else if (command[0] == 0x67)
						{
							globalTranspose = (signed char)command[1];
							if (curTrack == 0)
							{
								switchPoint[switchNum][0] = masterDelay;
								switchPoint[switchNum][1] = globalTranspose;
								switchNum++;
							}
							seqPos += 2;
						}

						/*End of sequence*/
						else if (command[0] == 0x68)
						{
							seqEnd = 1;
							j += 2;
						}

						/*Manually set length of next note OR rest*/
						else if (command[0] == 0x6A)
						{
							curChanSpeed = command[1] * 5;
							seqPos += 2;
						}

						/*Set up channel 4 speed and offset*/
						else if (command[0] == 0x6B)
						{
							if (command[1] > 0)
							{
								curDrumSpeed = command[1] * 5;
								drumOfs = ReadLE16(&romData[seqPos + 2 - bankAmt]);

								drumPos = drumOfs - bankAmt;
								drumDelay = masterDelay - drumPosDelay;
								drumPosDelay += drumDelay;

								while (romData[drumPos] <= 0xD0)
								{
									if (romData[drumPos] < 0x30)
									{
										curDrum = romData[drumPos] + 28;

										if (curDrum != 24)
										{
											tempPos = WriteNoteEvent(drumMidData, drumMidPos, curDrum, curDrumSpeed, drumDelay, firstDrumNote, 3, curInst);
											firstDrumNote = 0;
											drumMidPos = tempPos;
											drumPosDelay += curDrumSpeed;
											drumDelay = 0;
											drumPos++;
										}
										else if (curDrum == 24)
										{
											drumPosDelay += curDrumSpeed;
											drumDelay += curDrumSpeed;
											drumPos++;
										}
									}
									else if (romData[drumPos] >= 0x30)
									{
										drumPos++;
									}

								}
								drumDelay = 0;
								seqPos += 4;
							}
							else
							{
								seqPos += 2;
							}

						}

						/*Rest (with parameters)*/
						else if (command[0] == 0x72)
						{
							if (command[2] > 0x00)
							{
								seqPos += 4;
							}
							else
							{
								if (command[3] < 0x60)
								{
									curDelay = curChanSpeed;
									seqPos += 4;
								}
								else
								{
									seqPos += 3;
								}

							}

						}

						/*Unknown command 73*/
						else if (command[0] == 0x73)
						{
							seqPos += 2;
						}

						/*Unknown command 74*/
						else if (command[0] == 0x74)
						{
							seqPos += 2;
						}

						/*Switch waveform*/
						else if (command[0] == 0x75)
						{
							seqPos += 3;
						}

						/*Unknown command 76*/
						else if (command[0] == 0x76)
						{
							seqPos += 2;
						}

						/*Set on/off input for channel?*/
						else if (command[0] == 0x77)
						{
							seqPos += 2;
						}

						/*Jump to position*/
						else if (command[0] == 0x79)
						{
							jump2Pos = 1;
							jumpPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
							jumpRet = seqPos + 3;
							seqPos = jumpPos;
						}

						/*Play 'tom tom' note*/
						else if (command[0] == 0x7F)
						{
							seqPos++;
						}

						/*Switch instrument*/
						else if (command[0] >= 0x86 && command[0] < 0x8F)
						{
							seqPos++;
						}

						/*Effect*/
						else if (command[0] >= 0x90 && command[0] < 0xA0)
						{
							seqPos++;
						}


						/*Sweep parameters?*/
						else if (command[0] >= 0xC1 && command[0] < 0xC7)
						{
							seqPos += 4;
						}

						/*Change channel speed*/
						else if (command[0] >= 0xC7)
						{
							if (command[0] == 0xD1)
							{
								command[0] = 0xD4;
							}
							curChanSpeed = (command[0] - 0xC6) * 5;
							seqPos++;
						}


						/*Unknown command*/
						else
						{
							seqPos++;
						}
					}

					/*Format 3 (Asterix)*/
					else if (format == 3)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos - bankAmt + 1];
						command[2] = romData[seqPos - bankAmt + 2];
						command[3] = romData[seqPos - bankAmt + 3];

						/*Transpose check*/
						if (curTrack != 0)
						{
							for (switchNum = 0; switchNum < 90; switchNum++)
							{
								if (switchPoint[switchNum][0] == masterDelay)
								{
									globalTranspose = switchPoint[switchNum][1];
								}
							}
						}

						if (command[0] < 0x60)
						{
							curNote = command[0] + 12 + transpose + globalTranspose;

							if (curNote >= 0x80)
							{
								curNote = 0x7F;
							}
							if (command[1] != 0x65)
							{
								curNoteLen = curChanSpeed;
							}
							else if (command[1] == 0x65)
							{
								curNoteLen = curChanSpeed;
								extLen = 1;
								while (romData[seqPos + extLen - bankAmt] == 0x65)
								{
									curNoteLen += curChanSpeed;
									extLen++;
								}
								seqPos += extLen - 1;
							}

							masterDelay += curNoteLen;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							seqPos++;
						}

						/*Set channel parameters*/
						else if (command[0] == 0x63)
						{
							seqPos += 4;
						}

						/*Half rest?*/
						else if (command[0] == 0x64)
						{
							seqPos++;
						}

						/*Hold note for a bar*/
						else if (command[0] == 0x65)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*No note/rest for a bar*/
						else if (command[0] == 0x66)
						{
							curDelay += curChanSpeed;
							masterDelay += curChanSpeed;
							seqPos++;
						}

						/*End of final note*/
						else if (command[0] == 0x68)
						{
							seqEnd = 1;
							trackEnd = 1;
							j += 2;
						}

						/*Transpose (global)*/
						else if (command[0] == 0x69)
						{
							globalTranspose = (signed char)command[1];
							if (curTrack == 0)
							{
								switchPoint[switchNum][0] = masterDelay;
								switchPoint[switchNum][1] = globalTranspose;
								switchNum++;
							}
							seqPos += 2;
						}

						/*End of sequence*/
						else if (command[0] == 0x6A)
						{
							if (jump2Pos == 0)
							{
								seqEnd = 1;
								j += 2;
							}
							else if (jump2Pos == 1)
							{
								seqPos = jumpRet;
								jump2Pos = 0;
							}
						}

						/*Manually set length of next note OR rest*/
						else if (command[0] == 0x6C)
						{
							curChanSpeed = command[1] * 5;

							seqPos += 2;
						}

						/*Set up channel 4 speed and offset*/
						else if (command[0] == 0x6D)
						{
							curDrumSpeed = command[1] * 5;
							drumOfs = ReadLE16(&romData[seqPos + 2 - bankAmt]);

							drumPos = drumOfs - bankAmt;
							drumDelay = masterDelay - drumPosDelay;
							drumPosDelay += drumDelay;

							while (romData[drumPos] <= 0xD0)
							{
								if (romData[drumPos] < 0x30)
								{
									curDrum = romData[drumPos] + 28;

									if (curDrum != 24)
									{
										tempPos = WriteNoteEvent(drumMidData, drumMidPos, curDrum, curDrumSpeed, drumDelay, firstDrumNote, 3, curInst);
										firstDrumNote = 0;
										drumMidPos = tempPos;
										drumPosDelay += curDrumSpeed;
										drumDelay = 0;
										drumPos++;
									}
									else if (curDrum == 24)
									{
										drumPosDelay += curDrumSpeed;
										drumDelay += curDrumSpeed;
										drumPos++;
									}
								}
								else if (romData[drumPos] >= 0x30)
								{
									drumPos++;
								}

							}
							drumDelay = 0;
							seqPos += 4;
						}

						/*Stop channel 4?*/
						else if (command[0] == 0x6E)
						{
							seqPos++;
						}

						/*Effect/decay options*/
						else if (command[0] >= 0x6F && command[0] < 0x75)
						{
							seqPos++;
						}

						/*Set channel parameters?*/
						else if (command[0] == 0x75)
						{
							if (command[2] > 0x00)
							{
								seqPos += 4;
							}
							else
							{
								seqPos += 3;
							}
						}

						/*Unknown channel 3 setting 76*/
						else if (command[0] == 0x76)
						{
							seqPos += 2;
						}

						/*Unknown command 77*/
						else if (command[0] == 0x77)
						{
							seqPos += 2;
						}

						/*Switch waveform*/
						else if (command[0] == 0x7A)
						{
							seqPos += 3;
						}

						/*Transpose (current channel)*/
						else if (command[0] == 0x7D)
						{
							transpose = (signed char)command[1];
							if (offset == 0x4018 && transpose == 10)
							{
								transpose = 12;
							}
							seqPos += 2;
						}

						/*Load routine*/
						else if (command[0] == 0x7E)
						{
							seqPos += 3;
						}

						/*Unknown command 7F*/
						else if (command[0] == 0x7F)
						{
							seqPos++;
						}

						/*Go to loop point*/
						else if (command[0] == 0x80)
						{
							if (hasPatLoop[curTrack] == 1)
							{
								seqPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
							}
							else
							{
								seqEnd = 1;
								j += 2;
							}

						}

						/*Jump to position*/
						else if (command[0] == 0x81)
						{
							jump2Pos = 1;
							jumpPos = ReadLE16(&romData[seqPos + 1 - bankAmt]);
							jumpRet = seqPos + 3;
							seqPos = jumpPos;
						}

						/*Load routine (2)*/
						else if (command[0] == 0x82)
						{
							seqPos += 3;
						}

						/*Unknown command 84*/
						else if (command[0] == 0x84)
						{
							seqPos += 2;
						}

						/*Switch instrument*/
						else if (command[0] >= 0x85 && command[0] < 0x90)
						{
							seqPos++;
						}

						/*Unknown command 9E*/
						else if (command[0] == 0x9E)
						{
							seqPos++;
						}

						/*Effect*/
						else if (command[0] >= 0x90 && command[0] < 0xA9)
						{
							seqPos++;
						}

						/*Unknown command 9E*/
						else if (command[0] == 0x9E)
						{
							seqPos += 4;
						}

						/*Unknown command BD*/
						else if (command[0] == 0xBD)
						{
							seqPos += 4;
						}

						/*Set up waveform/volume envelope*/
						else if (command[0] >= 0xC0 && command[0] < speedStart)
						{
							seqPos += 4;
						}

						/*Change channel speed*/
						else if (command[0] >= speedStart && command[0] < repStart)
						{
							curChanSpeed = (command[0] - (speedStart - 1)) * 5;
							seqPos++;
						}

						/*Repeat*/
						else if (command[0] >= repStart)
						{
							repeat = command[0] - (repStart - 2);
							repeatStart = seqPos + 1;
							seqPos++;

						}

						/*Unknown command*/
						else
						{
							seqPos++;
						}


					}

					/*Format 6 (Pop Up)*/
					else if (format == 6)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos - bankAmt + 1];
						command[2] = romData[seqPos - bankAmt + 2];
						command[3] = romData[seqPos - bankAmt + 3];


						/*Transpose check*/
						if (curTrack != 0)
						{
							for (switchNum = 0; switchNum < 90; switchNum++)
							{
								if (switchPoint[switchNum][0] == masterDelay)
								{
									globalTranspose = switchPoint[switchNum][1];
								}
							}
						}

						if (usFlag != 1)
						{
							if (command[0] < 0x60)
							{
								curNote = command[0] + 12 + transpose + globalTranspose;

								if (curNote >= 0x80)
								{
									curNote = 0x7F;
								}
								if (command[1] != 0x6B)
								{
									curNoteLen = curChanSpeed;
								}
								else if (command[1] == 0x6B)
								{
									curNoteLen = curChanSpeed;
									extLen = 1;
									while (romData[seqPos + extLen - bankAmt] == 0x6B)
									{
										curNoteLen += curChanSpeed;
										extLen++;
									}
									seqPos += extLen - 1;
								}

								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos++;
							}

							/*No note/rest for a bar*/
							else if (command[0] == 0x6B)
							{
								curDelay += curChanSpeed;
								masterDelay += curChanSpeed;
								seqPos++;
							}

							/*No note/rest for a bar*/
							else if (command[0] == 0x6C)
							{
								curDelay += curChanSpeed;
								masterDelay += curChanSpeed;
								seqPos++;
							}

							/*Transpose (global)*/
							else if (command[0] == 0x72)
							{
								globalTranspose = (signed char)command[1];
								if (curTrack == 0)
								{
									switchPoint[switchNum][0] = masterDelay;
									switchPoint[switchNum][1] = globalTranspose;
									switchNum++;
								}
								seqPos += 2;
							}

							/*End of sequence*/
							else if (command[0] == 0x73)
							{
								if (jump2Pos == 0)
								{
									seqEnd = 1;
									j += 2;
								}
								else if (jump2Pos == 1)
								{
									seqPos = jumpRet;
									jump2Pos = 0;
								}
							}

							/*Manually set length of next note OR rest*/
							else if (command[0] == 0x76)
							{
								curChanSpeed = command[1] * 5;
								seqPos += 2;
							}

							/*Hold note for a bar*/
							else if (command[0] == 0x77)
							{
								curDelay += curChanSpeed;
								masterDelay += curChanSpeed;
								seqPos++;
							}

							/*Effect*/
							else if (command[0] >= 0x90 && command[0] < 0xA0)
							{
								seqPos++;
							}

							/*Change channel speed*/
							else if (command[0] >= 0xE0)
							{
								curChanSpeed = (command[0] - 0xDE) * 5;
								seqPos++;
							}

							/*Unknown command*/
							else
							{
								seqPos++;
							}
						}

						else if (usFlag == 1)
						{
							if (command[0] < 0x60)
							{
								curNote = command[0] + 12 + transpose + globalTranspose;

								if (curNote >= 0x80)
								{
									curNote = 0x7F;
								}
								if (command[1] != 0x6B)
								{
									curNoteLen = curChanSpeed;
								}
								else if (command[1] == 0x6B)
								{
									curNoteLen = curChanSpeed;
									extLen = 1;
									while (romData[seqPos + extLen - bankAmt] == 0x6B)
									{
										curNoteLen += curChanSpeed;
										extLen++;
									}
									seqPos += extLen - 1;
								}

								masterDelay += curNoteLen;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								seqPos++;
							}

							/*No note/rest for a bar*/
							else if (command[0] == 0x6B)
							{
								curDelay += curChanSpeed;
								masterDelay += curChanSpeed;
								seqPos++;
							}

							/*No note/rest for a bar*/
							else if (command[0] == 0x6C)
							{
								curDelay += curChanSpeed;
								masterDelay += curChanSpeed;
								seqPos++;
							}

							/*Transpose (global)*/
							else if (command[0] == 0x70)
							{
								globalTranspose = (signed char)command[1];
								if (curTrack == 0)
								{
									switchPoint[switchNum][0] = masterDelay;
									switchPoint[switchNum][1] = globalTranspose;
									switchNum++;
								}
								seqPos += 2;
							}

							/*End of sequence*/
							else if (command[0] == 0x71)
							{
								if (jump2Pos == 0)
								{
									seqEnd = 1;
									j += 2;
								}
								else if (jump2Pos == 1)
								{
									seqPos = jumpRet;
									jump2Pos = 0;
								}
							}

							/*Manually set length of next note OR rest*/
							else if (command[0] == 0x74)
							{
								curChanSpeed = command[1] * 5;
								seqPos += 2;
							}

							/*Hold note for a bar*/
							else if (command[0] == 0x75)
							{
								curDelay += curChanSpeed;
								masterDelay += curChanSpeed;
								seqPos++;
							}

							/*Effect*/
							else if (command[0] >= 0x8E && command[0] < 0x9E)
							{
								seqPos++;
							}

							/*Change channel speed*/
							else if (command[0] >= 0xDE)
							{
								curChanSpeed = (command[0] - 0xDC) * 5;
								seqPos++;
							}

							/*Unknown command*/
							else
							{
								seqPos++;
							}
						}

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

		/*End of drum track*/
		WriteBE32(&drumMidData[drumMidPos], 0xFF2F00);
		drumMidPos += 4;

		/*Calculate MIDI channel size*/
		trackSize = drumMidPos - drumMidTrackBase;
		WriteBE16(&drumMidData[drumMidTrackBase - 2], trackSize);

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		fwrite(drumMidData, drumMidPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		free(drumMidData);
		fclose(mid);
	}
}
