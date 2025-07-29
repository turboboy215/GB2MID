/*RARE*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "RARE.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long tablePtrLoc;
long tableOffset;
long tempoTablePtrLoc;
long tempoTableOffset;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
long songPtrs[4];
long firstPtrs[4];
long curSpeed;
int curTrack;
long bankAmt;
long nextPtr;
long masterBank;
int cfgPtr;
int exitError;
int fileExit;
int foundTable;
int ptrOverride;
int bankOverride;
long base;
long subBase;
int curTrack;
int tempoPos;
int tempo;
int format;
int curVol;
int curInst;
int ptrFmt;

const unsigned char RAREMagicBytes[7] = { 0x69, 0x26, 0x00, 0x29, 0x29, 0x29, 0x01 };
const unsigned char RAREMagicBytes2[8] = { 0x00, 0x29, 0x29, 0x44, 0x4D, 0x29, 0x09, 0x01 };
const unsigned char RAREMagicBytesTempo[8] = { 0xE0, 0x10, 0x18, 0xBD, 0x4F, 0xAF, 0x47, 0x21 };
const unsigned char RAREMagicBytesTempo2[7] = { 0xE0, 0x10, 0xC9, 0x4F, 0x06, 0x00, 0x21 };
const unsigned char RAREMagicBytesTempo3[8] = { 0xE0, 0x10, 0x18, 0xB9, 0x4F, 0xAF, 0x47, 0x21 };
const unsigned char RAREMagicBytesTempo4[6] = { 0xE0, 0x1C, 0xE0, 0x21, 0x47, 0x21 };
const unsigned char RAREMagicBytesTempo5[6] = { 0xD0, 0xC9, 0x4F, 0x06, 0x00, 0x21 };
const unsigned char RAREMagicBytesTempo6[7] = { 0xDE, 0xCD, 0x32, 0x4A, 0xAF, 0x47, 0x21 };
const unsigned char RAREMagicBytesTempo7[8] = { 0xCD, 0x2C, 0x00, 0xF1, 0x4F, 0xAF, 0x47, 0x21 };
const unsigned char RAREMagicBytesTempo8[8] = { 0x47, 0xEA, 0x03, 0xDE, 0xEA, 0x00, 0xDE, 0x21 };
const unsigned char RAREMagicBytesTempo9[8] = { 0x47, 0xEA, 0x83, 0xCF, 0xEA, 0x84, 0xCF, 0x21 };

const int DKL1NoteVals1[48][2] = {
	/*0xD0-0xD5*/
	{0x04, 0xA2}, {0x04, 0xA0}, {0x04, 0x9D}, {0x04, 0x9E}, {0x04, 0x9B}, {0x04, 0x96},
	/*0xD6-0xDB*/
	{0x08, 0x9B}, {0x08, 0x99}, {0x04, 0xA5}, {0x04, 0x99}, {0x08, 0x8A}, {0x08, 0x96},
	/*0xDC-0xDF*/
	{0x08, 0xA2}, {0x04, 0xA7}, {0x08, 0xC5}, {0x08, 0x88},
	/*0xE0-0xE5*/
	{0x08, 0x8D}, {0x08, 0xA5}, {0x08, 0x94}, {0x08, 0x9E}, {0x08, 0xCD}, {0x08, 0xA0},
	/*0xE6-0xEB*/
	{0x04, 0xA3}, {0x04, 0xA9}, {0x08, 0x8F}, {0x08, 0x86}, {0x08, 0xA9}, {0x04, 0x98},
	/*0xEC-0xEF*/
	{0x08, 0x91}, {0x08, 0xA4}, {0x08, 0x9D}, {0x08, 0x85},
	/*0xF0-0xF5*/
	{0x08, 0x92}, {0x04, 0xC5}, {0x04, 0x94}, {0x04, 0x8A}, {0x04, 0x86}, {0x08, 0xA7},
	/*0xF6-0xFB*/
	{0x08, 0x9A}, {0x04, 0xC0}, {0x04, 0xAE}, {0x04, 0xAA}, {0x04, 0x9A}, {0x03, 0x96},
	/*0xFC-0xFF*/
	{0x04, 0x83}, {0x02, 0x96}, {0x04, 0x91}, {0x04, 0x92}
};

const int DKL1NoteVals2[64][2] = {
	/*0x50-0x55*/
	{0x08, 0x9F}, {0x08, 0x98}, {0x04, 0xA6}, {0x08, 0xAA}, {0x04, 0xAC}, {0x05, 0xC3},
	/*0x56-0x5B*/
	{0x08, 0xA6}, {0x08, 0xA3}, {0x05, 0xCD}, {0x0C, 0xA3}, {0x08, 0x89}, {0x04, 0xA4},
	/*0x5C-0x5F*/
	{0x05, 0x9E}, {0x10, 0x94}, {0x0C, 0xA0}, {0x0C, 0x9E},
	/*0x60-0x65*/
	{0x05, 0x96}, {0x04, 0x8F}, {0x04, 0x8D}, {0x02, 0x9B}, {0x01, 0xC5}, {0x0C, 0xA2},
	/*0x66-0x6B*/
	{0x0C, 0x96}, {0x08, 0xA8}, {0x08, 0x9C}, {0x08, 0x97}, {0x08, 0x8C}, {0x04, 0x97},
	/*0x6C-0x6F*/
	{0x02, 0xA0}, {0x10, 0x8D}, {0x0C, 0xA7}, {0x08, 0x8B},
	/*0x70-0x75*/
	{0x06, 0xA0}, {0x05, 0xA2}, {0x05, 0x9C}, {0x10, 0xC5}, {0x10, 0xA5}, {0x08, 0x95},
	/*0x76-0x7B*/
	{0x05, 0xC2}, {0x03, 0x9B}, {0x04, 0xC1}, {0x10, 0x92}, {0x0C, 0x99}, {0x0C, 0x8D},
	/*0x7C-0x7F*/
	{0x08, 0xC1}, {0x08, 0xA1}, {0x08, 0x81}, {0x04, 0xCD}
};

char string1[100];
char string2[100];
char RAREcheckStrings[6][100] = { "masterBank=", "numSongs=", "bank=", "base=", "auto", "format=" };
unsigned char* romData;
unsigned char* exRomData;
unsigned char* cfgData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void RAREsong2mid(int songNum, long ptrList[4]);

void RAREProc(char parameters[4][100])
{
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;
	foundTable = 0;
	ptrOverride = 0;
	bankOverride = 0;
	base = 0;
	subBase = 0;
	tempoPos = 0;
	tempo = 0;
	format = 1;
	curVol = 100;
	curInst = 0;
	ptrFmt = 1;
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the "master bank" value*/
		fgets(string1, 12, cfg);

		if (memcmp(string1, RAREcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		masterBank = strtol(string1, NULL, 16);
		printf("Master bank: %01X\n", masterBank);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, RAREcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		numSongs = strtod(string1, NULL);
		printf("Total # of songs: %i\n", numSongs);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the format number*/
		fgets(string1, 8, cfg);

		if (memcmp(string1, RAREcheckStrings[5], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		format = strtod(string1, NULL);
		printf("Format: %i\n", format);

		/*Copy the current bank's ROM data*/
		if (masterBank != 1)
		{
			bankAmt = bankSize;
			fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
			romData = (unsigned char*)malloc(bankSize);
			fread(romData, 1, bankSize, rom);
		}

		else
		{
			bankAmt = 0;
			fseek(rom, ((masterBank - 1) * bankSize * 2), SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize * 2, rom);
		}

		/*Try to search the bank for base table*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], RAREMagicBytes, 7)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 7;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				break;
			}
		}

		/*Another format*/
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], RAREMagicBytes2, 8)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				ptrFmt = 2;
				break;
			}
		}

		/*Now search for the tempo table*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo, 8))
			{
				tempoTablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo2, 7))
			{
				tempoTablePtrLoc = bankAmt + i + 7;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo3, 8))
			{
				tempoTablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo4, 6))
			{
				tempoTablePtrLoc = bankAmt + i + 6;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo5, 6))
			{
				tempoTablePtrLoc = bankAmt + i + 6;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo6, 7))
			{
				tempoTablePtrLoc = bankAmt + i + 7;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo7, 8))
			{
				tempoTablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo8, 8))
			{
				tempoTablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		/*Another method*/
		for (i = 0; i < bankSize; i++)
		{
			if (!memcmp(&romData[i], RAREMagicBytesTempo9, 8))
			{
				tempoTablePtrLoc = bankAmt + i + 8;
				printf("Found pointer to tempo table at address 0x%04x!\n", tempoTablePtrLoc);
				tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
				printf("Tempo table starts at 0x%04x...\n", tempoTableOffset);
				break;
			}
		}

		if (ptrFmt == 2)
		{
			if (masterBank == 0xD1)
			{
				masterBank = 0xD2;
				bankAmt = bankSize;
				fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
				romData = (unsigned char*)malloc(bankSize);
				fread(romData, 1, bankSize, rom);
			}
		}

		i = tableOffset - bankAmt;
		songNum = 1;
		tempoPos = tempoTableOffset - bankAmt;

		while (fileExit == 0 && exitError == 0)
		{
			bankOverride = 0;
			ptrOverride = 0;

			if (songNum > numSongs)
			{
				fileExit = 1;
			}
			if (fileExit == 0)
			{
				/*Skip new line*/
				fgets(string1, 2, cfg);
				/*Skip the first line*/
				fgets(string1, 10, cfg);

				/*Now look for the "bank"*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, RAREcheckStrings[2], 1))
				{
					exitError = 1;
				}
				/*Check if the parameter is "auto" or not*/
				fgets(string1, 5, cfg);
				if (memcmp(string1, RAREcheckStrings[4], 1))
				{
					bankOverride = 1;
					bank = strtol(string1, NULL, 16);
				}
				/*Skip new line*/
				fgets(string1, 2, cfg);

				/*Now look for the "base"*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, RAREcheckStrings[3], 1))
				{
					exitError = 1;
				}
				/*Check if the parameter is "auto" or not*/
				fgets(string1, 5, cfg);
				if (memcmp(string1, RAREcheckStrings[4], 1))
				{
					ptrOverride = 1;
					base = strtol(string1, NULL, 16);
				}

				if (bankOverride == 0)
				{
					bank = masterBank;
				}
				printf("Song %i bank: %01X\n", songNum, bank);

				fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize);
				fread(exRomData, 1, bankSize, rom);

				if (ptrOverride == 0)
				{
					base = 0x4000;
					subBase = 0x4000;
				}
				else if (ptrOverride == 1)
				{
					base = strtol(string1, NULL, 16);
					fgets(string1, 2, cfg);
					fgets(string1, 5, cfg);
					subBase = strtol(string1, NULL, 16);
				}
				printf("Song %i base: %01X\n", songNum, base);

				/*Now get the pointers*/

				if (ptrFmt == 1)
				{
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						songPtrs[curTrack] = ReadLE16(&romData[i]);
						if (ptrOverride == 1)
						{
							songPtrs[curTrack] = songPtrs[curTrack] - base + subBase;
						}
						printf("Song %i channel %i: %04X\n", songNum, (curTrack + 1), songPtrs[curTrack]);
						i += 2;
					}
				}
				else if (ptrFmt == 2)
				{
					if (masterBank == 0xD2)
					{
						for (curTrack = 0; curTrack < 4; curTrack++)
						{
							i += 1;
							songPtrs[curTrack] = ReadLE16(&exRomData[i]);
							if (ptrOverride == 1)
							{
								songPtrs[curTrack] = songPtrs[curTrack] - base + subBase;
							}
							printf("Song %i channel %i: %04X\n", songNum, (curTrack + 1), songPtrs[curTrack]);
							i += 2;
						}
					}
					else
					{
						for (curTrack = 0; curTrack < 4; curTrack++)
						{
							i += 1;
							songPtrs[curTrack] = ReadLE16(&romData[i]);
							if (ptrOverride == 1)
							{
								songPtrs[curTrack] = songPtrs[curTrack] - base + subBase;
							}
							printf("Song %i channel %i: %04X\n", songNum, (curTrack + 1), songPtrs[curTrack]);
							i += 2;
						}
					}

				}


				tempo = romData[tempoPos];
				printf("Song %i tempo: %01X\n", songNum, tempo);
				tempoPos++;

				if (songPtrs[0] != 0x0000 && songPtrs[0] < 0x8000 && songPtrs[0] >= bankAmt)
				{
					RAREsong2mid(songNum, songPtrs);
				}

				songNum++;
			}

		}

		if (exitError == 1)
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}

		songNum = 1;

		free(romData);
	}
}

/*Convert the song data to MIDI*/
void RAREsong2mid(int songNum, long ptrList[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int trackCnt = 4;
	int ticks = 120;
	int songTempo = 120;
	long command[5];
	long romPos = 0;
	long seqPos = 0;
	int endSeq = 0;
	int curTempo = tempo;
	int transpose1 = 0;
	int transpose2 = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curDelay = 0;
	int tempVal = 0;
	int tempPos = 0;
	int valSize = 0;
	long trackSize = 0;
	long jumpPos = 0;
	long macro1aPos = 0;
	long macro1aRet = 0;
	long macro1bPos = 0;
	long macro1bRet = 0;
	long macro1cPos = 0;
	long macro1cRet = 0;
	long macro1dPos = 0;
	long macro1dRet = 0;
	long macro1ePos = 0;
	long macro1eRet = 0;
	int macro1aTimes = 0;
	int macro1bTimes = 0;
	int macro1cTimes = 0;
	int macro1dTimes = 0;
	int macro1eTimes = 0;
	long macro2aPos = 0;
	long macro2aRet = 0;
	long macro2bPos = 0;
	long macro2bRet = 0;
	long macro2cPos = 0;
	long macro2cRet = 0;
	long macro2dPos = 0;
	long macro2dRet = 0;
	long macro2ePos = 0;
	long macro2eRet = 0;
	int macro2aTimes = 0;
	int macro2bTimes = 0;
	int macro2cTimes = 0;
	int macro2dTimes = 0;
	int macro2eTimes = 0;
	long macro3aPos = 0;
	long macro3aRet = 0;
	long macro3bPos = 0;
	long macro3bRet = 0;
	long macro3cPos = 0;
	long macro3cRet = 0;
	long macro3dPos = 0;
	long macro3dRet = 0;
	long macro3ePos = 0;
	long macro3eRet = 0;
	int macro3aTimes = 0;
	int macro3bTimes = 0;
	int macro3cTimes = 0;
	int macro3dTimes = 0;
	int macro3eTimes = 0;
	int curVol = 0;
	int automaticCtrl = 0;
	int automaticSpeed = 0;
	int octaveFlag = 0;
	long midPos = 0;
	long ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int firstNote = 1;
	int inMacro1 = 0;
	int inMacro2 = 0;
	int inMacro3 = 0;
	int curTrack = 0;
	int repeatTimes = 0;
	int repeatPt = 0;
	int backPos = 0;
	int k = 0;
	int ctrlDelay = 0;

	sprintf(outfile, "song%d.mid", songNum);
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

		if (tempo > 1)
		{
			songTempo = tempo * 0.9;
		}
		else if (tempo == 0)
		{
			songTempo = 120;
		}

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / songTempo);
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
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;
			transpose1 = 0;
			transpose2 = 0;
			firstNote = 1;
			inMacro1 = 0;
			inMacro2 = 0;
			inMacro3 = 0;
			curDelay = 0;
			curVol = 100;
			ctrlDelay = 0;

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

			if (bank > 1)
			{
				bankAmt = 0x4000;
			}
			else
			{
				bank = 0x0000;
			}
			seqPos = ptrList[curTrack] - bankAmt;
			endSeq = 0;
			automaticCtrl = 0;

			/*DKL1*/
			if (format == 1)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*-base + subBase - bankAmt*/

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadBE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						seqPos = jumpPos;
					}

					/*Go to loop point*/
					else if (command[0] == 0x02)
					{
						endSeq = 1;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (v1) (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Go to macro (v1) 1 time*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro (v1) 2 times*/
					else if (command[0] == 0x06)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro (v1) 3 times*/
					else if (command[0] == 0x07)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 3;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 3;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 3;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 3;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 3;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro (v1) 4 times*/
					else if (command[0] == 0x08)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 4;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 4;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 4;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 4;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 4;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro (v1) 6 times*/
					else if (command[0] == 0x09)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 6;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 6;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 6;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 6;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 6;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro (v1) 8 times*/
					else if (command[0] == 0x0A)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 8;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 8;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 8;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 8;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 8;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro (v1) 16 times*/
					else if (command[0] == 0x0B)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 16;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 16;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 16;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 16;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 16;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Return from macro*/
					else if (command[0] == 0x0C)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0D)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn on automatic note length: 1*/
					else if (command[0] == 0x0E)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						seqPos++;
					}

					/*Turn on automatic note length: 2*/
					else if (command[0] == 0x0F)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						seqPos++;
					}

					/*Turn on automatic note length: 4*/
					else if (command[0] == 0x10)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						seqPos++;
					}

					/*Turn on automatic note length: 6*/
					else if (command[0] == 0x11)
					{
						automaticCtrl = 1;
						automaticSpeed = 6;
						seqPos++;
					}

					/*Turn on automatic note length: 8*/
					else if (command[0] == 0x12)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x13)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x14)
					{
						seqPos += 4;
					}

					/*Turn off vibrato*/
					else if (command[0] == 0x15)
					{
						seqPos++;
					}

					/*Turn off transpose*/
					else if (command[0] == 0x16)
					{
						transpose1 = 0;
						seqPos++;
					}

					/*Set transpose: +1 octave*/
					else if (command[0] == 0x17)
					{
						transpose1 = 12;
						seqPos++;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x18)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x19)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose: +2*/
					else if (command[0] == 0x1A)
					{
						transpose1 += 2;
						seqPos++;
					}

					/*Set transpose: -2*/
					else if (command[0] == 0x1B)
					{
						transpose1 -= 2;
						seqPos++;
					}

					/*Set transpose: +5*/
					else if (command[0] == 0x1C)
					{
						transpose1 += 5;
						seqPos++;
					}

					/*Set transpose: -5*/
					else if (command[0] == 0x1D)
					{
						transpose1 -= 5;
						seqPos++;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x1E)
					{
						seqPos += 5;
					}

					/*Set volume: 10*/
					else if (command[0] == 0x1F)
					{
						curVol = 25;
						seqPos++;
					}

					/*Set volume: 20*/
					else if (command[0] == 0x20)
					{
						curVol = 35;
						seqPos++;
					}

					/*Set volume: 30*/
					else if (command[0] == 0x21)
					{
						curVol = 40;
						seqPos++;
					}

					/*Set volume: 40*/
					else if (command[0] == 0x22)
					{
						curVol = 50;
						seqPos++;
					}

					/*Set volume: 50*/
					else if (command[0] == 0x23)
					{
						curVol = 70;
						seqPos++;
					}

					/*Set volume: 60*/
					else if (command[0] == 0x24)
					{
						curVol = 80;
						seqPos++;
					}

					/*Set volume: 70*/
					else if (command[0] == 0x25)
					{
						curVol = 100;
						seqPos++;
					}

					/*Switch to duty 1/envelope*/
					else if (command[0] == 0x26)
					{
						seqPos += 2;
					}

					/*Switch to duty 2/envelope*/
					else if (command[0] == 0x27)
					{
						seqPos += 2;
					}

					/*Switch to duty 3/envelope*/
					else if (command[0] == 0x28)
					{
						seqPos += 2;
					}

					/*Set channel 3 envelope (v2)*/
					else if (command[0] == 0x29)
					{
						seqPos += 3;
					}

					/*Go to macro (v2) 1 time*/
					else if (command[0] == 0x2A)
					{
						if (inMacro2 == 0)
						{
							macro2aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro2aRet = seqPos + 3;
							macro2aTimes = 1;
							seqPos = macro2aPos;
							inMacro2 = 1;
						}
						else if (inMacro2 == 1)
						{
							macro2bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro2bRet = seqPos + 3;
							macro2bTimes = 1;
							seqPos = macro2bPos;
							inMacro2 = 2;
						}
						else if (inMacro2 == 2)
						{
							macro2cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro2cRet = seqPos + 3;
							macro2cTimes = 1;
							seqPos = macro2cPos;
							inMacro2 = 3;
						}
						else if (inMacro2 == 3)
						{
							macro2dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro2dRet = seqPos + 3;
							macro2dTimes = 1;
							seqPos = macro2dPos;
							inMacro2 = 4;
						}
						else if (inMacro2 == 4)
						{
							macro2ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro2eRet = seqPos + 3;
							macro2eTimes = 1;
							seqPos = macro2ePos;
							inMacro2 = 5;
						}
					}

					/*Return from macro (v1) and turn off automatic control*/
					else if (command[0] == 0x2B)
					{
						automaticCtrl = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Switch waveform*/
					else if (command[0] == 0x2C)
					{
						seqPos += 2;
					}

					/*Switch to duty 4/envelope*/
					else if (command[0] == 0x2D)
					{
						seqPos += 2;
					}

					/*Set note size*/
					else if (command[0] >= 0x2E && command[0] < 0x3C)
					{
						seqPos++;
					}

					/*Set note size (manual)*/
					else if (command[0] == 0x3C)
					{
						seqPos += 2;
					}

					/*Return from macro (v2)*/
					else if (command[0] == 0x3D)
					{
						if (inMacro2 == 1)
						{
							if (macro2aTimes > 1)
							{
								seqPos = macro2aPos;
								macro2aTimes--;
							}
							else
							{
								seqPos = macro2aRet;
								inMacro2 = 0;
							}

						}
						else if (inMacro2 == 2)
						{
							if (macro2bTimes > 1)
							{
								seqPos = macro2bPos;
								macro2bTimes--;
							}
							else
							{
								seqPos = macro2bRet;
								inMacro2 = 1;
							}
						}
						else if (inMacro2 == 3)
						{
							if (macro2cTimes > 1)
							{
								seqPos = macro2cPos;
								macro2cTimes--;
							}
							else
							{
								seqPos = macro2cRet;
								inMacro2 = 2;
							}
						}
						else if (inMacro2 == 4)
						{
							if (macro2dTimes > 1)
							{
								seqPos = macro2dPos;
								macro2dTimes--;
							}
							else
							{
								seqPos = macro2dRet;
								inMacro2 = 3;
							}
						}
						else if (inMacro2 == 5)
						{
							if (macro2eTimes > 1)
							{
								seqPos = macro2ePos;
								macro2eTimes--;
							}
							else
							{
								seqPos = macro2eRet;
								inMacro2 = 4;
							}
						}
					}

					/*Set repeat amount (manual)*/
					else if (command[0] == 0x3E)
					{
						repeatTimes = command[1];
						repeatPt = seqPos + 2;
						seqPos += 2;
					}

					/*Set repeat times: 2*/
					else if (command[0] == 0x3F)
					{
						repeatTimes = 2;
						repeatPt = seqPos + 1;
						seqPos++;
					}

					/*Set repeat times: 3*/
					else if (command[0] == 0x40)
					{
						repeatTimes = 3;
						repeatPt = seqPos + 1;
						seqPos++;
					}

					/*Set repeat times: 4*/
					else if (command[0] == 0x41)
					{
						repeatTimes = 4;
						repeatPt = seqPos + 1;
						seqPos++;
					}

					/*End of repeat point*/
					else if (command[0] == 0x42)
					{
						if (repeatTimes > 1)
						{
							repeatTimes--;
							seqPos = repeatPt;
						}
						else
						{
							seqPos++;
						}
					}

					/*Return from macro (v3) and turn off transpose*/
					else if (command[0] == 0x43)
					{
						transpose1 = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}


					/*Go to macro (v3) 1 time*/
					else if (command[0] == 0x44)
					{
						if (songNum == 14)
						{
							octaveFlag = ReadLE16(&exRomData[seqPos + 1]);
						}
						if (inMacro3 == 0)
						{
							macro3aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro3aRet = seqPos + 3;
							macro3aTimes = 1;
							seqPos = macro3aPos;
							inMacro3 = 1;
						}
						else if (inMacro3 == 1)
						{
							macro3bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro3bRet = seqPos + 3;
							macro3bTimes = 1;
							seqPos = macro3bPos;
							inMacro3 = 2;
						}
						else if (inMacro3 == 2)
						{
							macro3cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro3cRet = seqPos + 3;
							macro3cTimes = 1;
							seqPos = macro3cPos;
							inMacro3 = 3;
						}
						else if (inMacro3 == 3)
						{
							macro3dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro3dRet = seqPos + 3;
							macro3dTimes = 1;
							seqPos = macro3dPos;
							inMacro3 = 4;
						}
						else if (inMacro3 == 4)
						{
							macro3ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro3eRet = seqPos + 3;
							macro3eTimes = 1;
							seqPos = macro3ePos;
							inMacro3 = 5;
						}
					}

					/*Return from macro (v3)*/
					else if (command[0] == 0x45)
					{
						if (inMacro3 == 1)
						{
							if (macro3aTimes > 1)
							{
								seqPos = macro3aPos;
								macro3aTimes--;
							}
							else
							{
								seqPos = macro3aRet;
								inMacro3 = 0;
							}

						}
						else if (inMacro3 == 2)
						{
							if (macro3bTimes > 1)
							{
								seqPos = macro3bPos;
								macro3bTimes--;
							}
							else
							{
								seqPos = macro3bRet;
								inMacro3 = 1;
							}
						}
						else if (inMacro3 == 3)
						{
							if (macro3cTimes > 1)
							{
								seqPos = macro3cPos;
								macro3cTimes--;
							}
							else
							{
								seqPos = macro3cRet;
								inMacro3 = 2;
							}
						}
						else if (inMacro3 == 4)
						{
							if (macro3dTimes > 1)
							{
								seqPos = macro3dPos;
								macro3dTimes--;
							}
							else
							{
								seqPos = macro3dRet;
								inMacro3 = 3;
							}
						}
						else if (inMacro3 == 5)
						{
							if (macro3eTimes > 1)
							{
								seqPos = macro3ePos;
								macro3eTimes--;
							}
							else
							{
								seqPos = macro3eRet;
								inMacro3 = 4;
							}
						}
					}

					/*Return from macro (v3) and turn off automatic control*/
					else if (command[0] == 0x46)
					{
						automaticCtrl = 0;
						if (inMacro3 == 1)
						{
							if (macro3aTimes > 1)
							{
								seqPos = macro3aPos;
								macro3aTimes--;
							}
							else
							{
								seqPos = macro3aRet;
								inMacro3 = 0;
							}

						}
						else if (inMacro3 == 2)
						{
							if (macro3bTimes > 1)
							{
								seqPos = macro3bPos;
								macro3bTimes--;
							}
							else
							{
								seqPos = macro3bRet;
								inMacro3 = 1;
							}
						}
						else if (inMacro3 == 3)
						{
							if (macro3cTimes > 1)
							{
								seqPos = macro3cPos;
								macro3cTimes--;
							}
							else
							{
								seqPos = macro3cRet;
								inMacro3 = 2;
							}
						}
						else if (inMacro3 == 4)
						{
							if (macro3dTimes > 1)
							{
								seqPos = macro3dPos;
								macro3dTimes--;
							}
							else
							{
								seqPos = macro3dRet;
								inMacro3 = 3;
							}
						}
						else if (inMacro3 == 5)
						{
							if (macro3eTimes > 1)
							{
								seqPos = macro3ePos;
								macro3eTimes--;
							}
							else
							{
								seqPos = macro3eRet;
								inMacro3 = 4;
							}
						}
					}

					/*Return from macro (v3) and turn on automatic control*/
					else if (command[0] == 0x47)
					{
						automaticCtrl = 0;
						if (inMacro3 == 1)
						{
							if (macro3aTimes > 1)
							{
								seqPos = macro3aPos;
								macro3aTimes--;
							}
							else
							{
								seqPos = macro3aRet;
								inMacro3 = 0;
							}

						}
						else if (inMacro3 == 2)
						{
							if (macro3bTimes > 1)
							{
								seqPos = macro3bPos;
								macro3bTimes--;
							}
							else
							{
								seqPos = macro3bRet;
								inMacro3 = 1;
							}
						}
						else if (inMacro3 == 3)
						{
							if (macro3cTimes > 1)
							{
								seqPos = macro3cPos;
								macro3cTimes--;
							}
							else
							{
								seqPos = macro3cRet;
								inMacro3 = 2;
							}
						}
						else if (inMacro3 == 4)
						{
							if (macro3dTimes > 1)
							{
								seqPos = macro3dPos;
								macro3dTimes--;
							}
							else
							{
								seqPos = macro3dRet;
								inMacro3 = 3;
							}
						}
						else if (inMacro3 == 5)
						{
							if (macro3eTimes > 1)
							{
								seqPos = macro3ePos;
								macro3eTimes--;
							}
							else
							{
								seqPos = macro3eRet;
								inMacro3 = 4;
							}
						}
					}

					/*Change note size (v2)?*/
					else if (command[0] == 0x48)
					{
						seqPos += 2;
					}

					/*Play note from table (1)*/
					else if (command[0] >= 0x50 && command[0] < 0x80)
					{
						curNote = DKL1NoteVals2[command[0] - 0x50][1] - 0x80 + 23 + transpose1;
						curNoteLen = DKL1NoteVals2[command[0] - 0x50][0] * 8;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play note from table (2)*/
					else if (command[0] >= 0xD0)
					{
						curNote = DKL1NoteVals1[command[0] - 0xD0][1] - 0x80 + 23 + transpose1;
						curNoteLen = DKL1NoteVals1[command[0] - 0xD0][0] * 8;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
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
			}

			/*DKL2/3*/
			else if (format == 2)
			{
				while (endSeq == 0 && seqPos <= bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							endSeq = 1;
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Go to loop point*/
					else if (command[0] == 0x02)
					{
						endSeq = 1;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set panning 1*/
					else if (command[0] == 0x06)
					{
						seqPos++;
					}

					/*Set panning 2*/
					else if (command[0] == 0x07)
					{
						seqPos++;
					}

					/*Set panning 3*/
					else if (command[0] == 0x08)
					{
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x0B)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 4;
					}

					/*Turn off vibrato*/
					else if (command[0] == 0x0D)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						seqPos += 6;
					}

					/*Pitch bend effect*/
					else if (command[0] == 0x10)
					{
						seqPos += 4;
					}

					/*Set tempo (absolute?)*/
					else if (command[0] == 0x11)
					{
						songTempo = command[1] * 0.9;
						tempo = songTempo;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set tempo (add to value)*/
					else if (command[0] == 0x12)
					{
						songTempo = (signed char)command[1] * 0.9;
						tempo += songTempo;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x14)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x16)
					{
						seqPos += 5;
					}

					/*Set volume: 10*/
					else if (command[0] == 0x17)
					{
						curVol = 25;
						seqPos++;
					}

					/*Set volume: 20*/
					else if (command[0] == 0x18)
					{
						curVol = 35;
						seqPos++;
					}

					/*Set volume: 30*/
					else if (command[0] == 0x19)
					{
						curVol = 40;
						seqPos++;
					}

					/*Set volume: 40*/
					else if (command[0] == 0x1A)
					{
						curVol = 45;
						seqPos++;
					}

					/*Set volume: 50*/
					else if (command[0] == 0x1B)
					{
						curVol = 50;
						seqPos++;
					}

					/*Set volume: 60*/
					else if (command[0] == 0x1C)
					{
						curVol = 55;
						seqPos++;
					}

					/*Set volume: 70*/
					else if (command[0] == 0x1D)
					{
						curVol = 60;
						seqPos++;
					}

					/*Set volume: 80*/
					else if (command[0] == 0x1E)
					{
						curVol = 65;
						seqPos++;
					}

					/*Set volume: 90*/
					else if (command[0] == 0x1F)
					{
						curVol = 70;
						seqPos++;
					}

					/*Set volume: A0*/
					else if (command[0] == 0x20)
					{
						curVol = 75;
						seqPos++;
					}

					/*Set volume: B0*/
					else if (command[0] == 0x21)
					{
						curVol = 80;
						seqPos++;
					}

					/*Set volume: C0*/
					else if (command[0] == 0x22)
					{
						curVol = 85;
						seqPos++;
					}

					/*Set volume: D0*/
					else if (command[0] == 0x23)
					{
						curVol = 90;
						seqPos++;
					}

					/*Set volume: E0*/
					else if (command[0] == 0x24)
					{
						curVol = 95;
						seqPos++;
					}

					/*Set volume: F0*/
					else if (command[0] == 0x25)
					{
						curVol = 100;
						seqPos++;
					}

					/*Go to macro 1 time*/
					else if (command[0] == 0x26)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 2 times*/
					else if (command[0] == 0x27)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 4 times*/
					else if (command[0] == 0x28)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 4;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 4;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 4;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 4;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 4;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 6 times*/
					else if (command[0] == 0x29)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 6;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 6;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 6;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 6;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 6;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Set channel envelope (v2)*/
					else if (command[0] == 0x2A)
					{
						seqPos += 3;
					}

					/*Turn on automatic note length: 2*/
					else if (command[0] == 0x2B)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						seqPos++;
					}

					/*Turn on automatic note length: 1*/
					else if (command[0] == 0x2C)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						seqPos++;
					}

					/*Go to macro 8 times*/
					else if (command[0] == 0x2D)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 8;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 8;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 8;
							seqPos = macro1cPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 8;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 8;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Turn on automatic note length: 8*/
					else if (command[0] == 0x2E)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Go to macro 16 times*/
					else if (command[0] == 0x2F)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 16;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 16;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 16;
							seqPos = macro1cPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 16;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 16;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Invalid command 30*/
					else if (command[0] == 0x30)
					{
						seqPos++;
					}

					/*Go to macro 3 times*/
					else if (command[0] == 0x31)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 3;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 3;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 3;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 3;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 3;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Turn off automatic length and end of macro*/
					else if (command[0] == 0x32)
					{
						automaticCtrl = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Switch to duty 1/envelope*/
					else if (command[0] == 0x33)
					{
						seqPos += 2;
					}

					/*Switch to duty 2/envelope*/
					else if (command[0] == 0x34)
					{
						seqPos += 2;
					}

					/*Switch to duty 3/envelope*/
					else if (command[0] == 0x35)
					{
						seqPos += 2;
					}

					/*Switch to duty 1/envelope*/
					else if (command[0] == 0x36)
					{
						seqPos += 2;
					}

					/*Turn off echo/envelope effect*/
					else if (command[0] == 0x37)
					{
						seqPos++;
					}

					/*Turn on automatic note length: 2*/
					else if (command[0] == 0x38)
					{
						automaticCtrl = 1;
						automaticSpeed = 6;
						seqPos++;
					}

					/*Switch waveform*/
					else if (command[0] == 0x3B)
					{
						seqPos += 2;
					}

					/*Change note size*/
					else if (command[0] == 0x3F)
					{
						seqPos += 2;
					}

					/*Change note size (v2)?*/
					else if (command[0] == 0x40)
					{
						seqPos += 2;
					}

					/*Turn on automatic note length: 4*/
					else if (command[0] == 0x41)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						seqPos++;
					}

					/*Turn off transpose*/
					else if (command[0] == 0x4F)
					{
						transpose1 = 0;
						seqPos++;
					}

					/*Transpose +12 (1 octave)*/
					else if (command[0] == 0x50)
					{
						transpose1 = 12;
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
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
			}

			/*Monster Max*/
			else if (format == 3)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							endSeq = 1;
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Switch waveform*/
					else if (command[0] == 0x02)
					{
						seqPos += 2;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set panning 1*/
					else if (command[0] == 0x06)
					{
						seqPos++;
					}

					/*Set panning 2*/
					else if (command[0] == 0x07)
					{
						seqPos++;
					}

					/*Set panning 3*/
					else if (command[0] == 0x08)
					{
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x0B)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 4;
					}

					/*Reset noise?*/
					else if (command[0] == 0x0D)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						seqPos += 6;
					}

					/*Manually set envelope*/
					else if (command[0] == 0x10)
					{
						seqPos += 2;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x14)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Set distortion pitch effect?*/
					else if (command[0] == 0x15)
					{
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x16)
					{
						seqPos += 5;
					}

					/*Turn off vibrato?*/
					else if (command[0] == 0x17)
					{
						seqPos++;
					}

					/*Set volume: 50*/
					else if (command[0] == 0x18)
					{
						curVol = 55;
						seqPos++;
					}

					/*Set volume: 30*/
					else if (command[0] == 0x19)
					{
						curVol = 40;
						seqPos++;
					}

					/*Set volume: 80*/
					else if (command[0] == 0x1A)
					{
						curVol = 65;
						seqPos++;
					}

					/*Set volume: 70*/
					else if (command[0] == 0x1B)
					{
						curVol = 60;
						seqPos++;
					}

					/*Set volume: 20*/
					else if (command[0] == 0x1C)
					{
						curVol = 35;
						seqPos++;
					}

					/*Go to macro 1 time*/
					else if (command[0] == 0x1D)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;

							if (macro1aPos == 2428)
							{
								octaveFlag = 0;
							}
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 2 times*/
					else if (command[0] == 0x1E)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 3 times*/
					else if (command[0] == 0x1F)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 3;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 3;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 3;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 3;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 3;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 4 times*/
					else if (command[0] == 0x20)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 4;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 4;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 4;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 4;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 4;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 8 times*/
					else if (command[0] == 0x21)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 8;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 8;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 8;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 8;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 8;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 6 times*/
					else if (command[0] == 0x22)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 6;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 6;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 6;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 6;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 6;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 7 times*/
					else if (command[0] == 0x23)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 7;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 7;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 7;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 7;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 7;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 5 times*/
					else if (command[0] == 0x24)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 5;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 5;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 5;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 5;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 5;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Set channel envelope/duty*/
					else if (command[0] >= 0x25 && command[0] < 0x29)
					{
						seqPos += 2;
					}

					/*Set duty to 0?*/
					else if (command[0] == 0x29)
					{
						seqPos += 2;
					}

					/*Return from macro and turn off automatic note length mode*/
					else if (command[0] == 0x2A)
					{
						automaticCtrl = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set duty effect?*/
					else if (command[0] == 0x2B)
					{
						seqPos += 2;
					}

					/*Turn off vibrato (v2?)*/
					else if (command[0] == 0x2C)
					{
						seqPos += 2;
					}

					/*Set volume: 90*/
					else if (command[0] == 0x2E)
					{
						curVol = 95;
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
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

			}

			/*Battletoads 2/3*/
			else if (format == 4)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							endSeq = 1;
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Switch waveform*/
					else if (command[0] == 0x02)
					{
						seqPos += 2;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set panning 1*/
					else if (command[0] == 0x06)
					{
						seqPos++;
					}

					/*Set panning 2*/
					else if (command[0] == 0x07)
					{
						seqPos++;
					}

					/*Set panning 3*/
					else if (command[0] == 0x08)
					{
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x0B)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 4;
					}

					/*Reset noise?*/
					else if (command[0] == 0x0D)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						seqPos += 6;
					}

					/*Manually set envelope*/
					else if (command[0] == 0x10)
					{
						seqPos += 2;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x14)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Set distortion pitch effect?*/
					else if (command[0] == 0x15)
					{
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x16)
					{
						seqPos += 5;
					}

					/*Set volume: 40*/
					else if (command[0] == 0x17)
					{
						curVol = 42;
						seqPos++;
					}

					/*Set volume: 20*/
					else if (command[0] == 0x18)
					{
						curVol = 15;
						seqPos++;
					}

					/*Set volume: 10*/
					else if (command[0] == 0x19)
					{
						curVol = 25;
						seqPos++;
					}

					/*Set volume: 30*/
					else if (command[0] == 0x1A)
					{
						curVol = 35;
						seqPos++;
					}

					/*Set volume: 50*/
					else if (command[0] == 0x1B)
					{
						curVol = 50;
						seqPos++;
					}

					/*Set volume: 60*/
					else if (command[0] == 0x1C)
					{
						curVol = 60;
						seqPos++;
					}

					/*Set volume: 70*/
					else if (command[0] == 0x1D)
					{
						curVol = 70;
						seqPos++;
					}

					/*Set volume: 80*/
					else if (command[0] == 0x1E)
					{
						curVol = 75;
						seqPos++;
					}

					/*Set volume: 90*/
					else if (command[0] == 0x1F)
					{
						curVol = 80;
						seqPos++;
					}

					/*Turn off transpose*/
					else if (command[0] == 0x20)
					{
						transpose1 = 0;
						seqPos++;
					}

					/*Set transpose to +1 octave*/
					else if (command[0] == 0x21)
					{
						transpose1 = 12;
						seqPos++;
					}

					/*Set note size to 40*/
					else if (command[0] == 0x22)
					{
						seqPos++;
					}

					/*Set automatic note speed to 4*/
					else if (command[0] == 0x23)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						seqPos++;
					}

					/*Set volume: A0*/
					else if (command[0] == 0x24)
					{
						curVol = 90;
						seqPos++;
					}

					/*Set volume: B0*/
					else if (command[0] == 0x25)
					{
						curVol = 95;
						seqPos++;
					}

					/*Go to macro 1 time*/
					else if (command[0] == 0x26)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;

							if (macro1aPos == 2428)
							{
								octaveFlag = 0;
							}
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 2 times*/
					else if (command[0] == 0x27)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 4 times*/
					else if (command[0] == 0x28)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 4;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 4;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 4;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 4;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 4;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 6 times*/
					else if (command[0] == 0x29)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 6;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 6;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 6;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 6;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 6;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Set note size to F0*/
					else if (command[0] == 0x2A)
					{
						seqPos++;
					}

					/*Set automatic note speed to 2*/
					else if (command[0] == 0x2B)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						seqPos++;
					}

					/*Set automatic note speed to 1*/
					else if (command[0] == 0x2C)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						seqPos++;
					}

					/*Go to macro 8 times*/
					else if (command[0] == 0x2D)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 8;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 8;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 8;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 8;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 8;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Set automatic note speed to 8*/
					else if (command[0] == 0x2E)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Go to macro 16 times*/
					else if (command[0] == 0x2F)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 16;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 16;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 16;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 16;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 16;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Set volume: C0*/
					else if (command[0] == 0x30)
					{
						curVol = 100;
						seqPos++;
					}

					/*Go to macro 3 times*/
					else if (command[0] == 0x31)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 3;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 3;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 3;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 3;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 3;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Return from macro and turn on automatic note length mode*/
					else if (command[0] == 0x32)
					{
						automaticCtrl = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set channel envelope/volume*/
					else if (command[0] == 0x33)
					{
						seqPos += 2;
					}

					/*Change note size*/
					else if (command[0] == 0x34)
					{
						seqPos += 2;
					}

					/*Set duty to 80 and change note size*/
					else if (command[0] == 0x35)
					{
						seqPos += 2;
					}

					/*Change note size with echo effect?*/
					else if (command[0] == 0x36)
					{
						seqPos += 2;
					}

					/*Turn off volume effect*/
					else if (command[0] == 0x37)
					{
						seqPos++;
					}

					/*Set automatic note speed to 6*/
					else if (command[0] == 0x38)
					{
						automaticCtrl = 6;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Return from macro and turn off automatic note length mode (v2?)*/
					else if (command[0] == 0x39)
					{
						automaticCtrl = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Unknown (set stereo flag?)*/
					else if (command[0] == 0x3A)
					{
						seqPos++;
					}

					/*Go back amount of bytes (signed)*/
					else if (command[0] == 0x3B)
					{
						/*Workarounds for specific songs*/
						if (songNum == 3 && seqPos == 0x15D4)
						{
							backPos = (signed char)command[1] + 1;
							seqPos += backPos;
						}
						else if (songNum == 7 && seqPos == 0x1C9D)
						{
							backPos = (signed char)command[1] + 1;
							seqPos = 0x1BEA;
						}
						else if (songNum == 13 && seqPos == 0x10AC)
						{
							backPos = (signed char)command[1] + 1;
							seqPos = 0x105D;
						}
						else if (songNum == 16 && seqPos == 0xA4D)
						{
							backPos = (signed char)command[1] + 1;
							seqPos = 0xA38;
						}
						else if (songNum == 16 && seqPos == 0xA54)
						{
							backPos = (signed char)command[1] + 1;
							seqPos = 0xA38;
						}

						else if (songNum == 5 && seqPos == 0x2C88)
						{
							seqPos = 0x2C2A;
						}
						else if (songNum == 5 && seqPos == 0x2DA2)
						{
							seqPos = 0x2D30;
						}
						else if (songNum == 12 && seqPos == 0x1C49)
						{
							seqPos = 0x1C08;
						}
						else
						{
							endSeq = 1;
						}

					}

					/*Set channel envelope (v2)?*/
					else if (command[0] == 0x3F)
					{
						seqPos += 2;
					}

					/*Set duty?*/
					else if (command[0] == 0x40)
					{
						seqPos += 2;
					}

					/*Set volume: E0*/
					else if (command[0] == 0x41)
					{
						curVol = 100;
						seqPos++;
					}

					/*Set volume: F0*/
					else if (command[0] == 0x42)
					{
						curVol = 100;
						seqPos++;
					}

					/*Turn off effects*/
					else if (command[0] == 0x43)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Repeat notes*/
					else if (command[0] == 0x44)
					{
						curNote = command[1] - 0x80 + 32;
						repeatTimes = command[2];
						for (k = 0; k < repeatTimes; k++)
						{
							curNoteLen = automaticSpeed * 8;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						seqPos += 3;
					}

					/*Turn on automatic note length mode with speed (v2?)*/
					else if (command[0] == 0x45)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
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
			}

			/*Battletoads 1*/
			else if (format == 5)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							/*Workarounds for specific channels in songs*/
							if (inMacro1 != 0)
							{
								seqPos = jumpPos;
							}
							else
							{
								endSeq = 1;
							}
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Switch waveform*/
					else if (command[0] == 0x02)
					{
						seqPos += 2;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set panning 1*/
					else if (command[0] == 0x06)
					{
						seqPos++;
					}

					/*Set panning 2*/
					else if (command[0] == 0x07)
					{
						seqPos++;
					}

					/*Set panning 3*/
					else if (command[0] == 0x08)
					{
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x0B)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 4;
					}

					/*Reset noise?*/
					else if (command[0] == 0x0D)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						seqPos += 6;
					}

					/*Manually set envelope*/
					else if (command[0] == 0x10)
					{
						seqPos += 2;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x14)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Set distortion pitch effect?*/
					else if (command[0] == 0x15)
					{
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x16)
					{
						seqPos += 5;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x17)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x18)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Go to macro 1 time*/
					else if (command[0] == 0x19)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 2 times*/
					else if (command[0] == 0x1A)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Turn off effects*/
					else if (command[0] == 0x1B)
					{
						seqPos++;
					}

					/*Go to macro 3 times*/
					else if (command[0] == 0x1C)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 3;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 3;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 3;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 3;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 3;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 4 times*/
					else if (command[0] == 0x1D)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 4;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 4;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 4;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 4;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 4;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x1E)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						seqPos++;
					}

					/*Turn off transpose*/
					else if (command[0] == 0x1F)
					{
						transpose1 = 0;
						seqPos++;
					}

					/*Set transpose to +7*/
					else if (command[0] == 0x20)
					{
						transpose1 = 7;
						seqPos++;
					}

					/*Set transpose to +1 octave*/
					else if (command[0] == 0x20)
					{
						transpose1 = 12;
						seqPos++;
					}

					/*Go to macro 8 times*/
					else if (command[0] == 0x22)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 8;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 8;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 8;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 8;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 8;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x23)
					{
						automaticCtrl = 1;
						automaticSpeed = 12;
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x24)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x25)
					{
						automaticCtrl = 1;
						automaticSpeed = 3;
						seqPos++;
					}

					/*Go to macro 16 times*/
					else if (command[0] == 0x26)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 16;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 16;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 16;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 16;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 16;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 13 times*/
					else if (command[0] == 0x27)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 13;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 13;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 13;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 13;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 13;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Set decay?*/
					else if (command[0] == 0x28)
					{
						seqPos += 2;
					}

					/*Go to macro 7 times*/
					else if (command[0] == 0x29)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 7;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 7;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 7;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 7;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 7;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Load duty with value 80*/
					else if (command[0] == 0x2A)
					{
						seqPos += 2;
					}

					/*Load duty with value 40*/
					else if (command[0] == 0x2B)
					{
						seqPos += 2;
					}

					/*Go to macro 3 times*/
					else if (command[0] == 0x2C)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 6;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 6;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 6;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 6;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 6;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Load duty with value D0*/
					else if (command[0] == 0x2D)
					{
						seqPos += 2;
					}

					/*Switch wave instrument?*/
					else if (command[0] == 0x2E || command[0] == 0x2F)
					{
						seqPos += 2;
					}

					/*Return from macro and turn off automatic note length mode*/
					else if (command[0] == 0x30)
					{
						automaticCtrl = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Return from macro and turn on automatic note length mode?*/
					else if (command[0] == 0x31)
					{
						automaticCtrl = 0;
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Change envelope volume*/
					else if (command[0] > 0x31 && command[0] < 0x80)
					{
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0 && command[0] < 0xE0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Rest?*/
					else if (command[0] == 0xE0)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}
			}

			/*WWF Superstars*/
			else if (format == 6)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							/*Workarounds for specific channels in songs*/
							if (inMacro1 != 0)
							{
								seqPos = jumpPos;
							}
							else
							{
								endSeq = 1;
							}
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Switch waveform*/
					else if (command[0] == 0x02)
					{
						seqPos += 2;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set panning 1*/
					else if (command[0] == 0x06)
					{
						seqPos++;
					}

					/*Set panning 2*/
					else if (command[0] == 0x07)
					{
						seqPos++;
					}

					/*Set panning 3*/
					else if (command[0] == 0x08)
					{
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x0B)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 4;
					}

					/*Reset noise?*/
					else if (command[0] == 0x0D)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						seqPos += 6;
					}

					/*Manually set envelope*/
					else if (command[0] == 0x10)
					{
						seqPos += 2;
					}

					/*Set tempo (add to value)*/
					else if (command[0] == 0x12)
					{
						songTempo = (signed char)command[1] * 0.9;
						tempo += songTempo;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x14)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Set distortion pitch effect?*/
					else if (command[0] == 0x15)
					{
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x16)
					{
						seqPos += 5;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x17)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						seqPos++;
					}

					/*Set volume to 40*/
					else if (command[0] == 0x18)
					{
						curVol = 50;
						seqPos++;
					}

					/*Set volume to 80*/
					else if (command[0] == 0x19)
					{
						curVol = 100;
						seqPos++;
					}

					/*Set volume to 20*/
					else if (command[0] == 0x1A)
					{
						curVol = 30;
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x1B)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Go to macro 1 time*/
					else if (command[0] == 0x1C)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 2 times*/
					else if (command[0] == 0x1D)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Turn off vibrato?*/
					else if (command[0] == 0x1E)
					{
						seqPos++;
					}

					/*Set volume to 70*/
					else if (command[0] == 0x1F)
					{
						curVol = 90;
						seqPos++;
					}

					/*Set volume to 30*/
					else if (command[0] == 0x20)
					{
						curVol = 40;
						seqPos++;
					}

					/*Change envelope volume*/
					else if (command[0] > 0x20 && command[0] < 0x80)
					{
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0 && command[0] < 0xE0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Rest?*/
					else if (command[0] == 0xFF)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}
			}

			/*Beetlejuice*/
			else if (format == 7)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							/*Workarounds for specific channels in songs*/
							if (inMacro1 != 0)
							{
								seqPos = jumpPos;
							}
							else
							{
								endSeq = 1;
							}
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Switch waveform*/
					else if (command[0] == 0x02)
					{
						seqPos += 2;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set panning 1*/
					else if (command[0] == 0x06)
					{
						seqPos++;
					}

					/*Set panning 2*/
					else if (command[0] == 0x07)
					{
						seqPos++;
					}

					/*Set panning 3*/
					else if (command[0] == 0x08)
					{
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x0B)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 4;
					}

					/*Reset noise?*/
					else if (command[0] == 0x0D)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						seqPos += 6;
					}

					/*Manually set envelope*/
					else if (command[0] == 0x10)
					{
						seqPos += 2;
					}

					/*Set tempo (absolute?)*/
					else if (command[0] == 0x11)
					{
						songTempo = (signed char)command[1] * 0.9;
						tempo = songTempo;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set tempo (add to value)*/
					else if (command[0] == 0x12)
					{
						songTempo = (signed char)command[1] * 0.9;
						tempo += songTempo;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x14)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Set distortion pitch effect?*/
					else if (command[0] == 0x15)
					{
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x16)
					{
						seqPos += 5;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x17)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						seqPos++;
					}

					/*Set volume to 40*/
					else if (command[0] == 0x18)
					{
						curVol = 50;
						seqPos++;
					}

					/*Set volume to 80*/
					else if (command[0] == 0x19)
					{
						curVol = 90;
						seqPos++;
					}

					/*Set volume to 20*/
					else if (command[0] == 0x1A)
					{
						curVol = 30;
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x1B)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Go to macro 1 time*/
					else if (command[0] == 0x1C)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 2 times*/
					else if (command[0] == 0x1D)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Turn off effects*/
					else if (command[0] == 0x1E)
					{
						seqPos++;
					}

					/*Set volume to 70*/
					else if (command[0] == 0x1F)
					{
						curVol = 80;
						seqPos++;
					}

					/*Set volume to 30*/
					else if (command[0] == 0x20)
					{
						curVol = 40;
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x21)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						seqPos++;
					}

					/*Set volume to 50*/
					else if (command[0] == 0x22)
					{
						curVol = 60;
						seqPos++;
					}

					/*Set volume to 90*/
					else if (command[0] == 0x23)
					{
						curVol = 100;
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x24)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						seqPos++;
					}

					/*Change envelope volume?*/
					else if (command[0] > 0x24 && command[0] < 0x80)
					{
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0 && command[0] < 0xE0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Rest?*/
					else if (command[0] == 0xE0)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}
			}

			/*Sneaky Snakes*/
			else if (format == 8)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							/*Workarounds for specific channels in songs*/
							if (inMacro1 != 0)
							{
								seqPos = jumpPos;
							}
							else
							{
								endSeq = 1;
							}
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Switch waveform*/
					else if (command[0] == 0x02)
					{
						seqPos += 2;
					}

					/*Set channel 3 envelope*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Go to macro (manual times)*/
					else if (command[0] == 0x04)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x05)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Set panning 1*/
					else if (command[0] == 0x06)
					{
						seqPos++;
					}

					/*Set panning 2*/
					else if (command[0] == 0x07)
					{
						seqPos++;
					}

					/*Set panning 3*/
					else if (command[0] == 0x08)
					{
						seqPos++;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0A)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x0B)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on vibrato (v2)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 4;
					}

					/*Reset noise?*/
					else if (command[0] == 0x0D)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x0E || command[0] == 0x0F)
					{
						seqPos += 6;
					}

					/*Manually set envelope*/
					else if (command[0] == 0x10)
					{
						seqPos += 2;
					}

					/*Set tempo (absolute?)*/
					else if (command[0] == 0x11)
					{
						songTempo = (signed char)command[1] * 0.9;
						tempo = songTempo;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set tempo (add to value)*/
					else if (command[0] == 0x12)
					{
						songTempo = (signed char)command[1] * 0.9;
						tempo += songTempo;
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x13)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x14)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Set distortion pitch effect?*/
					else if (command[0] == 0x15)
					{
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x16)
					{
						seqPos += 5;
					}

					/*Set duty values*/
					else if (command[0] >= 0x17 && command[0] <= 0x1A)
					{
						seqPos++;
					}

					/*Change envelope volume?*/
					else if (command[0] > 0x1A && command[0] < 0x80)
					{
						seqPos++;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xC0)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0 && command[0] < 0xE0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Rest?*/
					else if (command[0] == 0xE0)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							ctrlDelay += command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
							ctrlDelay += automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}
			}

			/*Croc*/
			else if (format == 9)
			{
				while (endSeq == 0 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];

					/*End of sequence*/
					if (command[0] == 0x00)
					{
						endSeq = 1;
					}

					/*Jump to position*/
					else if (command[0] == 0x01)
					{
						jumpPos = ReadBE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
						if ((jumpPos - bankAmt) < seqPos)
						{
							endSeq = 1;
						}
						else
						{
							seqPos = jumpPos;
						}
					}

					/*Go to loop point*/
					else if (command[0] == 0x02)
					{
						endSeq = 1;
					}

					/*Set channel 3 envelope (v1)*/
					else if (command[0] == 0x03)
					{
						seqPos += 4;
					}

					/*Set channel 3 envelope (v2)*/
					else if (command[0] == 0x04)
					{
						seqPos += 3;
					}

					/*Switch waveform*/
					else if (command[0] == 0x05)
					{
						seqPos += 2;
					}


					/*Go to macro (manual times)*/
					else if (command[0] == 0x06)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 4;
							macro1aTimes = exRomData[seqPos + 1];
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 4;
							macro1bTimes = exRomData[seqPos + 1];
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 4;
							macro1cTimes = exRomData[seqPos + 1];
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 4;
							macro1dTimes = exRomData[seqPos + 1];
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 2]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 4;
							macro1eTimes = exRomData[seqPos + 1];
							seqPos = macro1ePos;
							inMacro1 = 5;
						}

					}

					/*Return from macro*/
					else if (command[0] == 0x07)
					{
						if (inMacro1 == 1)
						{
							if (macro1aTimes > 1)
							{
								seqPos = macro1aPos;
								macro1aTimes--;
							}
							else
							{
								seqPos = macro1aRet;
								inMacro1 = 0;
							}

						}
						else if (inMacro1 == 2)
						{
							if (macro1bTimes > 1)
							{
								seqPos = macro1bPos;
								macro1bTimes--;
							}
							else
							{
								seqPos = macro1bRet;
								inMacro1 = 1;
							}
						}
						else if (inMacro1 == 3)
						{
							if (macro1cTimes > 1)
							{
								seqPos = macro1cPos;
								macro1cTimes--;
							}
							else
							{
								seqPos = macro1cRet;
								inMacro1 = 2;
							}
						}
						else if (inMacro1 == 4)
						{
							if (macro1dTimes > 1)
							{
								seqPos = macro1dPos;
								macro1dTimes--;
							}
							else
							{
								seqPos = macro1dRet;
								inMacro1 = 3;
							}
						}
						else if (inMacro1 == 5)
						{
							if (macro1eTimes > 1)
							{
								seqPos = macro1ePos;
								macro1eTimes--;
							}
							else
							{
								seqPos = macro1eRet;
								inMacro1 = 4;
							}
						}
					}

					/*Turn on automatic note length mode*/
					else if (command[0] == 0x08)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						seqPos++;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x09)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Change note size (v1)*/
					else if (command[0] == 0x0A)
					{
						seqPos += 2;
					}

					/*Change note size (v2)*/
					else if (command[0] == 0x0B)
					{
						seqPos += 2;
					}

					/*Change note size (v3)*/
					else if (command[0] == 0x0C)
					{
						seqPos += 2;
					}

					/*Turn on automatic note length mode with speed*/
					else if (command[0] == 0x0D)
					{
						automaticCtrl = 1;
						automaticSpeed = command[1];
						seqPos += 2;
					}

					/*Effect, parameters?*/
					else if (command[0] == 0x0E)
					{
						seqPos += 4;
					}

					/*Turn off effect*/
					else if (command[0] == 0x0F)
					{
						seqPos++;
					}

					/*Portamento*/
					else if (command[0] == 0x10)
					{
						seqPos += 6;
					}

					/*Pitch bend effect*/
					else if (command[0] == 0x11)
					{
						seqPos += 4;
					}

					/*Set transpose (v2)*/
					else if (command[0] == 0x12)
					{
						transpose1 = (signed char)command[1];
						seqPos += 2;
					}

					/*Set transpose (v1)*/
					else if (command[0] == 0x13)
					{
						transpose1 += (signed char)command[1];
						seqPos += 2;
					}

					/*Turn on vibrato*/
					else if (command[0] == 0x14)
					{
						seqPos += 5;
					}

					/*Switch to duty 1/envelope*/
					else if (command[0] == 0x15)
					{
						seqPos += 2;
					}

					/*Switch to duty 2/envelope*/
					else if (command[0] == 0x16)
					{
						seqPos += 2;
					}

					/*Switch to duty 3/envelope*/
					else if (command[0] == 0x17)
					{
						seqPos += 2;
					}

					/*Set volume: 00*/
					else if (command[0] == 0x18)
					{
						curVol = 00;
						seqPos++;
					}

					/*Set volume: 10*/
					else if (command[0] == 0x19)
					{
						curVol = 25;
						seqPos++;
					}

					/*Set volume: 20*/
					else if (command[0] == 0x1A)
					{
						curVol = 35;
						seqPos++;
					}

					/*Set volume: 30*/
					else if (command[0] == 0x1B)
					{
						curVol = 40;
						seqPos++;
					}

					/*Set volume: 40*/
					else if (command[0] == 0x1C)
					{
						curVol = 45;
						seqPos++;
					}

					/*Set volume: 50*/
					else if (command[0] == 0x1D)
					{
						curVol = 50;
						seqPos++;
					}

					/*Set volume: 60*/
					else if (command[0] == 0x1E)
					{
						curVol = 55;
						seqPos++;
					}

					/*Set volume: 70*/
					else if (command[0] == 0x1F)
					{
						curVol = 60;
						seqPos++;
					}

					/*Set volume: 80*/
					else if (command[0] == 0x20)
					{
						curVol = 65;
						seqPos++;
					}

					/*Set volume: 90*/
					else if (command[0] == 0x21)
					{
						curVol = 70;
						seqPos++;
					}

					/*Set volume: A0*/
					else if (command[0] == 0x22)
					{
						curVol = 75;
						seqPos++;
					}

					/*Set volume: B0*/
					else if (command[0] == 0x23)
					{
						curVol = 80;
						seqPos++;
					}

					/*Set volume: C0*/
					else if (command[0] == 0x24)
					{
						curVol = 85;
						seqPos++;
					}

					/*Set volume: D0*/
					else if (command[0] == 0x25)
					{
						curVol = 90;
						seqPos++;
					}

					/*Set volume: E0*/
					else if (command[0] == 0x26)
					{
						curVol = 95;
						seqPos++;
					}

					/*Set volume: F0*/
					else if (command[0] == 0x27)
					{
						curVol = 100;
						seqPos++;
					}

					/*Turn off automatic note length mode*/
					else if (command[0] == 0x28)
					{
						automaticCtrl = 0;
						seqPos++;
					}

					/*Turn on automatic note length: 1*/
					else if (command[0] == 0x29)
					{
						automaticCtrl = 1;
						automaticSpeed = 1;
						seqPos++;
					}

					/*Turn on automatic note length: 2*/
					else if (command[0] == 0x2A)
					{
						automaticCtrl = 1;
						automaticSpeed = 2;
						seqPos++;
					}

					/*Turn on automatic note length: 3*/
					else if (command[0] == 0x2B)
					{
						automaticCtrl = 1;
						automaticSpeed = 3;
						seqPos++;
					}

					/*Turn on automatic note length: 4*/
					else if (command[0] == 0x2C)
					{
						automaticCtrl = 1;
						automaticSpeed = 4;
						seqPos++;
					}

					/*Turn on automatic note length: 5*/
					else if (command[0] == 0x2D)
					{
						automaticCtrl = 1;
						automaticSpeed = 5;
						seqPos++;
					}

					/*Turn on automatic note length: 6*/
					else if (command[0] == 0x2E)
					{
						automaticCtrl = 1;
						automaticSpeed = 6;
						seqPos++;
					}

					/*Turn on automatic note length: 7*/
					else if (command[0] == 0x2F)
					{
						automaticCtrl = 1;
						automaticSpeed = 7;
						seqPos++;
					}

					/*Turn on automatic note length: 8*/
					else if (command[0] == 0x30)
					{
						automaticCtrl = 1;
						automaticSpeed = 8;
						seqPos++;
					}

					/*Go to macro 1 time*/
					else if (command[0] == 0x31)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 1;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 1;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 1;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 1;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 1;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 2 times*/
					else if (command[0] == 0x32)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 2;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 2;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 2;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 2;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 2;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 3 times*/
					else if (command[0] == 0x33)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 3;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 3;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 3;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 3;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 3;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 4 times*/
					else if (command[0] == 0x34)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 4;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 4;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 4;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 4;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 4;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 5 times*/
					else if (command[0] == 0x35)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 5;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 5;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 5;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 5;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 5;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 6 times*/
					else if (command[0] == 0x36)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 6;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 6;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 6;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 6;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 6;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 8 times*/
					else if (command[0] == 0x37)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 8;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 8;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 8;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 8;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 8;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Go to macro 16 times*/
					else if (command[0] == 0x38)
					{
						if (inMacro1 == 0)
						{
							macro1aPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1aRet = seqPos + 3;
							macro1aTimes = 16;
							seqPos = macro1aPos;
							inMacro1 = 1;
						}
						else if (inMacro1 == 1)
						{
							macro1bPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1bRet = seqPos + 3;
							macro1bTimes = 16;
							seqPos = macro1bPos;
							inMacro1 = 2;
						}
						else if (inMacro1 == 2)
						{
							macro1cPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1cRet = seqPos + 3;
							macro1cTimes = 16;
							seqPos = macro1cPos;
							inMacro1 = 3;
						}
						else if (inMacro1 == 3)
						{
							macro1dPos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1dRet = seqPos + 3;
							macro1dTimes = 16;
							seqPos = macro1dPos;
							inMacro1 = 4;
						}
						else if (inMacro1 == 4)
						{
							macro1ePos = ReadLE16(&exRomData[seqPos + 1]) - base + subBase - bankAmt;
							macro1eRet = seqPos + 3;
							macro1eTimes = 16;
							seqPos = macro1ePos;
							inMacro1 = 5;
						}
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] > 0x80 && command[0] < 0xBE)
					{
						curNote = command[0] - 0x80 + 23 + transpose1;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Play drum note*/
					else if (command[0] >= 0xC0)
					{
						curNote = command[0] - 0xC0 + 32;
						if (automaticCtrl == 0)
						{
							curNoteLen = command[1] * 8;
							seqPos++;
						}
						else if (automaticCtrl == 1)
						{
							curNoteLen = automaticSpeed * 8;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
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