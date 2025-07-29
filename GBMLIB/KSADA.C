/*Kyouhei Sada (Pure Sound)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "KSADA.H"

#define bankSize 16384

FILE* rom, * mid;
long masterBank;
long songBank;
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
int oldMode;
int oldModePtrSet;
int drvVers;
int curInst;
int curVol;
int romSize;
int numBanks;
int orderFix;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* headerData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char SadaMagicBytes[14] = { 0x16, 0x00, 0x5F, 0x26, 0x00, 0x6F, 0xCB, 0x25, 0xCB, 0x14, 0x19, 0x54, 0x5D, 0x21 };
const long PalamedesPtrs[7] = { 0x0A0B, 0x0A69, 0x0ABD, 0x0B57, 0x0ECA, 0x11C1, 0x1283 };
const long PalamedesPtrsJ[7] = { 0x09F9, 0x0A57, 0x0AAB, 0x0B45, 0x0EB8, 0x11AF, 0x1271 };
const int PalamedesLns[58] = { 0x60, 0x00, 0x0C, 0x0B, 0x06, 0x05, 0x06, 0x00, 0x18, 0x00, 0x18, 0x17, 0x24,
0x22, 0x24, 0x18, 0x0C, 0x0D, 0x30, 0x20, 0x10, 0x0F, 0x08, 0x07, 0x20, 0x1F, 0x0C, 0x00, 0x48,
0x00, 0x12, 0x11, 0x06, 0x07, 0x30, 0x2E, 0x04, 0x04, 0x0C, 0x06, 0x30, 0x31, 0x0C, 0x06, 0x30,
0x2F, 0x30, 0x00, 0x60, 0x5F, 0x54, 0x53, 0x18, 0x0C, 0x18, 0x19, 0x60, 0x61 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Sadasong2mid(int songNum, long ptr, int bank);
void Sadasong2midOld(int songNum, long ptr);

int SadaProc(int bank, char parameters[4][100])
{
	masterBank = bank;
	drvVers = 1;
	foundTable = 0;
	oldMode = 0;
	curInst = 0;
	curVol = 100;
	orderFix = 0;

	if (parameters[0][0] != 0)
	{
		drvVers = strtoul(parameters[0], NULL, 16);
	}

	if (drvVers == 4)
	{
		drvVers = 2;
		orderFix = 1;
	}

	else if (drvVers == 5)
	{
		drvVers = 2;
		orderFix = 2;
	}

	else if (drvVers == 6)
	{
		drvVers = 2;
		orderFix = 3;
	}

	if (masterBank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
		oldMode = 1;
	}

	fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Get ROM size*/
	fseek(rom, 0, SEEK_SET);
	headerData = (unsigned char*)malloc(bankSize);
	fread(headerData, 1, bankSize, rom);
	romSize = headerData[0x148];

	switch (romSize)
	{
	case 0x00:
		numBanks = 2;
		break;
	case 0x01:
		numBanks = 4;
		break;
	case 0x02:
		numBanks = 8;
		break;
	case 0x03:
		numBanks = 16;
		break;
	case 0x04:
		numBanks = 32;
		break;
	case 0x05:
		numBanks = 64;
		break;
	case 0x06:
		numBanks = 128;
		break;
	case 0x07:
		numBanks = 256;
		break;
	case 0x08:
		numBanks = 512;
		break;
	default:
		numBanks = 2;
		break;
	}

	free(headerData);

	if (oldMode != 1)
	{
		/*Try to search the bank for song table loader*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], SadaMagicBytes, 14)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 14;
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
			while (ReadLE16(&romData[i + 1]) >= bankAmt && ReadLE16(&romData[i + 1]) < 0x8000)
			{
				songBank = romData[i] + masterBank;
				songPtr = ReadLE16(&romData[i + 1]);
				printf("Song %i: 0x%04X, bank %01X\n", songNum, songPtr, songBank);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize);
				fread(exRomData, 1, bankSize, rom);

				if (songBank <= numBanks && orderFix == 0)
				{
					Sadasong2mid(songNum, songPtr, songBank);
				}
				else if (songBank <= numBanks && orderFix == 1)
				{
					if (songNum != 159)
					{
						Sadasong2mid(songNum, songPtr, songBank);
					}
				}
				else if (songBank <= numBanks && orderFix == 2)
				{
					if (songNum != 46)
					{
						Sadasong2mid(songNum, songPtr, songBank);
					}
				}
				else if (songBank <= numBanks && orderFix == 3)
				{
					if (songNum != 20)
					{
						Sadasong2mid(songNum, songPtr, songBank);
					}
				}
				i += 3;
				songNum++;
			}

			free(romData);
		}
		else
		{
			printf("ERROR: Magic bytes not found!\n");
			exit(-1);
		}
	}
	else if (oldMode == 1)
	{
		if (romData[0x014A] == 0x01)
		{
			oldModePtrSet = 0;
		}
		else
		{
			oldModePtrSet = 1;
		}

		if (oldModePtrSet == 0)
		{
			for (songNum = 1; songNum <= 7; songNum++)
			{
				songPtr = PalamedesPtrs[songNum - 1];
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Sadasong2midOld(songNum, songPtr);
			}

			free(romData);
		}

		else
		{
			for (songNum = 1; songNum <= 7; songNum++)
			{
				songPtr = PalamedesPtrsJ[songNum - 1];
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Sadasong2midOld(songNum, songPtr);
			}

			free(romData);
		}
	}


}

void Sadasong2mid(int songNum, long ptr, int bank)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int chanPtrs[4];
	int chanMask;
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
	long jumpPos = 0;
	long jumpPosRet = 0;
	unsigned char command[4];
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
	romPos = ptr - bankAmt;
	int f0Com = 0;
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

		if (drvVers == 1)
		{
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				chanPtrs[curTrack] = 0;
			}

			/*Get the channel pointers*/
			while (romData[romPos] != 0xFF)
			{
				chanMask = romData[romPos];
				highNibble = (romData[romPos] & 15);
				switch (highNibble)
				{
				case 0x00:
					curTrack = 0;
					break;
				case 0x01:
					curTrack = 1;
					break;
				case 0x02:
					curTrack = 0;
					break;
				case 0x03:
					curTrack = 1;
					break;
				case 0x04:
					curTrack = 2;
					break;
				case 0x05:
					curTrack = 2;
					break;
				case 0x06:
					curTrack = 3;
					break;
				case 0x07:
					curTrack = 3;
					break;
				default:
					curTrack = 0;
					break;
				}

				chanPtrs[curTrack] = ReadLE16(&romData[romPos + 1]);
				romPos += 3;
			}
		}
		else
		{
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				chanPtrs[curTrack] = 0;
			}

			/*Get the channel pointers*/
			while (exRomData[romPos] != 0xFF)
			{
				chanMask = exRomData[romPos];
				highNibble = (exRomData[romPos] & 15);

				if (drvVers == 2)
				{
					switch (highNibble)
					{
					case 0x00:
						curTrack = 0;
						break;
					case 0x01:
						curTrack = 1;
						break;
					case 0x02:
						curTrack = 0;
						break;
					case 0x03:
						curTrack = 2;
						break;
					case 0x04:
						curTrack = 3;
						break;
					case 0x05:
						curTrack = 1;
						break;
					case 0x06:
						curTrack = 2;
						break;
					case 0x07:
						curTrack = 3;
						break;
					default:
						curTrack = 0;
						break;
					}
				}
				else
				{
					switch (highNibble)
					{
					case 0x00:
						curTrack = 0;
						break;
					case 0x01:
						curTrack = 1;
						break;
					case 0x02:
						curTrack = 0;
						break;
					case 0x03:
						curTrack = 1;
						break;
					case 0x04:
						curTrack = 2;
						break;
					case 0x05:
						curTrack = 2;
						break;
					case 0x06:
						curTrack = 3;
						break;
					case 0x07:
						curTrack = 3;
						break;
					default:
						curTrack = 0;
						break;
					}
				}
				chanPtrs[curTrack] = ReadLE16(&exRomData[romPos + 1]);
				romPos += 3;
			}
		}

		/*Now do the conversion*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;
			f0Com = 0;

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

			if (chanPtrs[curTrack] != 0 && chanPtrs[curTrack] > bankAmt && chanPtrs[curTrack] < 0x8000)
			{
				seqPos = chanPtrs[curTrack] - bankAmt;

				while (seqEnd == 0 && seqPos < bankSize && midPos < 48000)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];

					/*Play note*/
					if (command[0] < 0x60)
					{
						if (f0Com == 0)
						{
							curNote = command[0] + 12;
							curNoteLen = command[1] * 5;
							seqPos += 2;
						}
						else
						{
							curNote = command[0] + 12;
							curNoteLen = command[2] * 5;
							seqPos += 3;
						}

						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						midPos = tempPos;
					}

					/*Rest (v1)*/
					else if (command[0] == 0x60)
					{
						curNoteLen = command[1] * 5;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Rest (v2)*/
					else if (command[0] == 0x61)
					{
						curNoteLen = command[1] * 5;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Additional notes (CH4)?*/
					else if (command[0] > 0x62 && command[0] < 0x80)
					{
						curNote = command[0] - 0x62 + 24;
						curNoteLen = command[1] * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						midPos = tempPos;
						seqPos += 2;
					}

					/*Set volume*/
					else if (command[0] >= 0xD0 && command[0] < 0xE0)
					{
						highNibble = (command[0] & 15);

						switch (highNibble)
						{
						case 0x00:
							curVol = 5;
							break;
						case 0x01:
							curVol = 60;
							break;
						case 0x02:
							curVol = 50;
							break;
						case 0x03:
							curVol = 60;
							break;
						case 0x04:
							curVol = 60;
							break;
						case 0x05:
							curVol = 80;
							break;
						case 0x06:
							curVol = 80;
							break;
						case 0x07:
							curVol = 80;
							break;
						case 0x08:
							curVol = 80;
							break;
						case 0x09:
							curVol = 85;
							break;
						case 0x0A:
							curVol = 90;
							break;
						case 0x0B:
							curVol = 95;
							break;
						case 0x0C:
							curVol = 95;
							break;
						case 0x0D:
							curVol = 96;
							break;
						case 0x0E:
							curVol = 98;
							break;
						case 0x0F:
							curVol = 100;
							break;
						}
						seqPos++;
					}

					/*Set envelope*/
					else if (command[0] >= 0xE0 && command[0] < 0xF0)
					{
						seqPos += 2;
					}

					/*Turn on sweep/module mode*/
					else if (command[0] == 0xF0)
					{
						if (curTrack < 2)
						{
							f0Com = 1;
						}
						seqPos += 2;
					}

					/*Unknown commands F1-F3*/
					else if (command[0] >= 0xF1 && command[0] <= 0xF3)
					{
						seqPos++;
					}

					/*Unknown commands F4-F5*/
					else if (command[0] >= 0xF4 && command[0] <= 0xF5)
					{
						seqEnd = 1;
					}

					/*Set duty*/
					else if (command[0] == 0xF6)
					{
						seqPos += 2;
					}

					/*Unknown command F7*/
					else if (command[0] == 0xF7)
					{
						seqEnd = 1;
					}

					/*Set wave counter 1*/
					else if (command[0] == 0xF8)
					{
						seqPos += 2;
					}

					/*Set vibrato*/
					else if (command[0] == 0xF9)
					{
						seqPos += 2;
					}

					/*Unknown commands FA-FC*/
					else if (command[0] >= 0xFA && command[0] <= 0xFC)
					{
						seqEnd = 1;
					}

					/*Set wave counter 2*/
					else if (command[0] == 0xFD)
					{
						seqPos += 2;
					}

					/*Jump to position*/
					else if (command[0] == 0xFE)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]);
						seqEnd = 1;
					}

					/*Stop channel*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					/*Unknown command*/
					else
					{
						seqPos += 2;
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
		free(exRomData);
		fclose(mid);
	}
}
void Sadasong2midOld(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int chanPtrs[4];
	int chanMask;
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
	long jumpPos = 0;
	long jumpPosRet = 0;
	unsigned char command[4];
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
	romPos = ptr - bankAmt;
	int f0Com = 0;
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

		for (curTrack == 0; curTrack < 4; curTrack++)
		{
			chanPtrs[curTrack] = 0;
		}

		/*Get the channel pointers*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			chanPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
		}

		/*Now do the conversion*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;
			f0Com = 0;

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

			if (chanPtrs[curTrack] != 0 && chanPtrs[curTrack] > bankAmt && chanPtrs[curTrack] < 0x8000)
			{
				seqPos = chanPtrs[curTrack] - bankAmt;

				while (seqEnd == 0 && seqPos < bankSize && midPos < 48000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Play note*/
					if (command[0] < 0x60)
					{
						if (f0Com == 0)
						{
							if (PalamedesLns[(command[1] * 2) + 1] != 0x00)
							{
								curNote = command[0] + 11;
								curNoteLen = PalamedesLns[command[1] * 2] * 5;
								seqPos += 2;
							}
							else
							{
								seqPos += 2;
							}

						}
						else
						{
							curNote = command[0] + 11;
							curNoteLen = PalamedesLns[command[2] * 2] * 5;
							seqPos += 3;
						}

						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						midPos = tempPos;
					}

					/*Rest (v1)*/
					else if (command[0] == 0x60)
					{
						curNoteLen = PalamedesLns[command[1] * 2] * 5;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Rest (v2)*/
					else if (command[0] == 0x61)
					{
						curNoteLen = PalamedesLns[command[1] * 2] * 5;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos += 2;
					}

					/*Additional notes (CH4)?*/
					else if (command[0] > 0x62 && command[0] < 0x80)
					{
						curNote = command[0] - 0x62 + 24;
						curNoteLen = PalamedesLns[command[1] * 2] * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						midPos = tempPos;
						seqPos += 2;
					}

					/*Set volume*/
					else if (command[0] >= 0xD0 && command[0] < 0xE0)
					{
						highNibble = (command[0] & 15);

						switch (highNibble)
						{
						case 0x00:
							curVol = 5;
							break;
						case 0x01:
							curVol = 60;
							break;
						case 0x02:
							curVol = 50;
							break;
						case 0x03:
							curVol = 60;
							break;
						case 0x04:
							curVol = 60;
							break;
						case 0x05:
							curVol = 80;
							break;
						case 0x06:
							curVol = 80;
							break;
						case 0x07:
							curVol = 80;
							break;
						case 0x08:
							curVol = 80;
							break;
						case 0x09:
							curVol = 85;
							break;
						case 0x0A:
							curVol = 90;
							break;
						case 0x0B:
							curVol = 95;
							break;
						case 0x0C:
							curVol = 95;
							break;
						case 0x0D:
							curVol = 96;
							break;
						case 0x0E:
							curVol = 98;
							break;
						case 0x0F:
							curVol = 100;
							break;
						}
						seqPos++;
					}

					/*Set envelope*/
					else if (command[0] >= 0xE0 && command[0] < 0xF0)
					{
						seqPos += 2;
					}

					/*Turn on sweep/module mode*/
					else if (command[0] == 0xF0)
					{
						if (curTrack < 2)
						{
							f0Com = 1;
						}
						seqPos += 2;
					}

					/*Unknown commands F1-F3*/
					else if (command[0] >= 0xF1 && command[0] <= 0xF3)
					{
						seqPos++;
					}

					/*Unknown commands F4-F5*/
					else if (command[0] >= 0xF4 && command[0] <= 0xF5)
					{
						seqEnd = 1;
					}

					/*Set duty*/
					else if (command[0] == 0xF6)
					{
						seqPos += 2;
					}

					/*Unknown command F7*/
					else if (command[0] == 0xF7)
					{
						seqEnd = 1;
					}

					/*Set wave counter 1*/
					else if (command[0] == 0xF8)
					{
						seqPos += 2;
					}

					/*Set vibrato*/
					else if (command[0] == 0xF9)
					{
						seqPos += 2;
					}

					/*Unknown commands FA-FC*/
					else if (command[0] >= 0xFA && command[0] <= 0xFC)
					{
						seqEnd = 1;
					}

					/*Set wave counter 2*/
					else if (command[0] == 0xFD)
					{
						seqPos += 2;
					}

					/*Jump to position*/
					else if (command[0] == 0xFE)
					{
						jumpPos = ReadLE16(&romData[seqPos + 1]);
						seqEnd = 1;
					}

					/*Stop channel*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					/*Unknown command*/
					else
					{
						seqPos += 2;
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