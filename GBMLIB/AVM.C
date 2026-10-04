/*Audio Visual Magic (AVM)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "AVM.H"

#define bankSize 16384

FILE* mod, * rom, * cfg;
long offset;
long headerOffset;
int i, j;
char outfile[1000000];
long patList;
long patDir;
int numInst;
unsigned static char* modData;
unsigned static char* romData;
unsigned static char* cfgData;
long modLength;
int mode;
int songNum;
int numSongs;
int fileExit;
int exitError;
int bank;
int songBank;
long bankAmt;
long songPtr;
int drvVers;

/*Our sample*/
const unsigned char AVMsineWave[64] = { 0x00, 0x00, 0x00, 0x03, 0x06, 0x0A, 0x0E, 0x11, 0x15, 0x17, 0x1A,
0x1D, 0x20, 0x23, 0x27, 0x2A, 0x2C, 0x30, 0x33, 0x35, 0x38, 0x3B, 0x3E, 0x41, 0x43, 0x46, 0x4A, 0x4C,
0x4F, 0x52, 0x55, 0x58, 0x59, 0x59, 0x54, 0x4F, 0x4B, 0x48, 0x44, 0x41, 0x3E, 0x3B, 0x39, 0x37, 0x35,
0x33, 0x30, 0x2E, 0x2C, 0x29, 0x26, 0x22, 0x1F, 0x1B, 0x18, 0x12, 0x0B, 0x05, 0xFE, 0xF8, 0xF2, 0xEC,
0xE7, 0xE4 };

/*ProTracker periods, C-1 to B-3, finetune 0*/
const unsigned short AVMmodPeriods[36] = { 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 };

/*Strings to check in CFG*/
char string1[100];
char string2[100];
char AVMcheckStrings[3][100] = { "numSongs=", "bank=", "start=" };


/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void AVMsong2mod(int songNum, long songPtr);

void AVMProc(int bank, char parameters[4][100])
{
	drvVers = AVM_VER_STD;

	if ((cfg = fopen(parameters[0], "rb")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		fileExit = 0;
		exitError = 0;
		songNum = 1;
		bankAmt = bankSize;

		/*Get the total number of songs*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, AVMcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		numSongs = strtod(string1, NULL);
		printf("Total # of songs: %i\n", numSongs);

		/*Repeat for every song*/
		while (fileExit == 0 && exitError == 0)
		{
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

				/*Get the bank of each song*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, AVMcheckStrings[1], 1))
				{
					exitError = 1;
				}
				fgets(string1, 5, cfg);
				{
					songBank = strtol(string1, NULL, 16);
				}

				/*Copy the ROM's bank data into RAM*/
				if (songBank < 0x02)
				{
					songBank = 0x02;
				}

				fseek(rom, 0, SEEK_SET);
				romData = (unsigned char*)malloc(bankSize * 2);
				fread(romData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(romData + bankSize, 1, bankSize, rom);


				/*Skip new line*/
				fgets(string1, 2, cfg);

				/*Get the "start" of the module*/
				fgets(string1, 7, cfg);
				if (memcmp(string1, AVMcheckStrings[2], 1))
				{
					exitError = 1;
				}
				fgets(string1, 5, cfg);
				songPtr = strtol(string1, NULL, 16);
				printf("Song %i: 0x%04lX, bank: %01X\n", songNum, songPtr, songBank);

				/*Skip new line*/
				fgets(string1, 2, cfg);

				AVMsong2mod(songNum, songPtr);
				free(romData);
				songNum++;

			}
		}
		fclose(cfg);
	}
}

/*Convert the song data to MOD*/
void AVMsong2mod(int songNum, long songPtr)
{
	long romPos = 0;
	long modPos = 0;
	long curPtr;
	long patStart;
	int highestPos = 0;
	int numPats = 0;
	int curPat = 0;
	int curRow = 0;
	int curVoice = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int transpose = 0;
	int lowNote = 999;
	int highNote = -999;
	int noteOfs = 0;
	int sampLen = 64;
	int folded = 0;

	/*Per-voice reading state: the three voices are read side by side*/
	long voicePos[AVM_VOICES];		/*ROM position of each voice's stream*/
	int voiceSkip[AVM_VOICES];		/*empty rows still owed by each voice*/
	int voiceIns[AVM_VOICES];		/*last instrument number of each voice*/
	int insUsed[32];

	unsigned char b0, b1, b2;
	int note, ins, fx, param;
	int modNote, modIns, modFx, modParam;
	int period;
	long cell;

	modLength = 0x100000;
	modData = ((unsigned char*)malloc(modLength));

	for (i = 0; i < modLength; i++)
	{
		modData[i] = 0;
	}
	for (i = 0; i < 32; i++)
	{
		insUsed[i] = 0;
	}
	sprintf(outfile, "song%i.mod", songNum);
	if ((mod = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mod!\n", songNum);
		exit(2);
	}
	else
	{
		/*Song header: optional $FF + transpose*/
		romPos = songPtr;
		if (romData[romPos] == 0xFF)
		{
			transpose = (signed char)romData[romPos + 1];
			romPos += 2;
		}
		patStart = romPos;
		numPats = romData[romPos];
		patList = romPos + 6;
		for (j = 0; j < numPats; j++)
		{
			if (romData[patList + j] > highestPos)
			{
				highestPos = romData[patList + j];
			}
		}

		/*Pass 1: find the lowest and highest note of the whole song (row notes + song
		  transpose only; instrument data is never read) and which instruments occur*/
		for (curPat = 0; curPat <= highestPos; curPat++)
		{
			curPtr = patStart + ReadLE16(&romData[patStart + 0x86 + (curPat * 2)]);
			voicePos[0] = curPtr + 4;
			voicePos[1] = curPtr + 4 + ReadLE16(&romData[curPtr]);
			voicePos[2] = curPtr + 4 + ReadLE16(&romData[curPtr + 2]);
			for (curVoice = 0; curVoice < AVM_VOICES; curVoice++)
			{
				voiceSkip[curVoice] = 0;
			}

			for (curRow = 0; curRow < AVM_ROWS; curRow++)
			{
				for (curVoice = 0; curVoice < AVM_VOICES; curVoice++)
				{
					if (voiceSkip[curVoice] > 0)
					{
						voiceSkip[curVoice]--;
						continue;
					}
					b0 = romData[voicePos[curVoice]];
					if (b0 & 0x80)
					{
						k = b0 & 0x7F;
						voiceSkip[curVoice] = (k > 1) ? k - 1 : 0;
						voicePos[curVoice]++;
						continue;
					}
					b1 = romData[voicePos[curVoice] + 1];
					voicePos[curVoice] += ((b1 & 0x0F) == 7) ? 2 : 3;
					insUsed[b1 >> 4] = 1;
					if ((b0 & 0x3F) != 0x3F)
					{
						note = (b0 & 0x3F) + transpose;
						if (note < lowNote) lowNote = note;
						if (note > highNote) highNote = note;
					}
				}
			}
		}

		/*One sample for the whole song. A 64-byte one-cycle sample at period 856 (C-1)
		  plays GB note 0 (C2); 32 bytes = C3, 16 bytes = C4, 8 bytes = C5. Pick the
		  longest sample that keeps the highest note inside C-1..B-3.*/
		if (highNote >= lowNote)
		{
			k = highNote - 35;
			if (k < 0)
			{
				k = 0;
			}
			noteOfs = ((k + 11) / 12) * 12;
			if (noteOfs > 36)
			{
				noteOfs = 36;
			}
			sampLen = 64 >> (noteOfs / 12);
			printf("  notes %i-%i, sample length %i\n", lowNote, highNote, sampLen);
		}

		/*Write MOD header*/
		modPos = 0;
		sprintf((char*)&modData[modPos], "GB2MOD conversion");
		/*The song title is 20 bytes*/
		modPos += 20;

		/*Every instrument number that occurs gets the same sample*/
		for (i = 1; i <= 31; i++)
		{
			if (insUsed[i])
			{
				sprintf((char*)&modData[modPos], "sinewave");
				modPos += 22;
				/*Sample length (in words)*/
				WriteBE16(&modData[modPos], sampLen / 2);
				modPos += 2;
				/*Fine tune: +1 (1/8 semitone), the PAL period table is about 18 cents flat here*/
				Write8B(&modData[modPos], 1);
				modPos++;
				/*Sample volume*/
				Write8B(&modData[modPos], 64);
				modPos++;
				/*Loop start*/
				WriteBE16(&modData[modPos], 0);
				modPos += 2;
				/*Loop length (in words)*/
				WriteBE16(&modData[modPos], sampLen / 2);
				modPos += 2;
			}
			else
			{
				/*Unused slot: empty sample, loop length 1 as ProTracker writes it*/
				modPos += 22;
				WriteBE16(&modData[modPos], 0);
				modPos += 2;
				modPos += 2;
				WriteBE16(&modData[modPos], 0);
				modPos += 2;
				WriteBE16(&modData[modPos], 1);
				modPos += 2;
			}
		}

		/*Number of song positions*/
		Write8B(&modData[modPos], numPats);
		modPos++;
		/*Restart byte: 127 as ProTracker writes it*/
		Write8B(&modData[modPos], 127);
		modPos++;
		/*Fill in the pattern table*/
		for (j = 0; j < numPats; j++)
		{
			Write8B(&modData[modPos + j], romData[patList + j]);
		}
		modPos += 128;
		/*Put in the initials M.K.*/
		memcpy(&modData[modPos], "M.K.", 4);
		modPos += 4;

		/*Pass 2: the pattern data. The three voice streams are read in lockstep: for
		  every row, each voice either pays off one row of a pending empty run or reads
		  its next event. Channel 4 is never written, so it stays empty.*/
		for (curVoice = 0; curVoice < AVM_VOICES; curVoice++)
		{
			voiceIns[curVoice] = 0;
		}
		for (curPat = 0; curPat <= highestPos; curPat++)
		{
			curPtr = patStart + ReadLE16(&romData[patStart + 0x86 + (curPat * 2)]);
			voicePos[0] = curPtr + 4;
			voicePos[1] = curPtr + 4 + ReadLE16(&romData[curPtr]);
			voicePos[2] = curPtr + 4 + ReadLE16(&romData[curPtr + 2]);
			for (curVoice = 0; curVoice < AVM_VOICES; curVoice++)
			{
				voiceSkip[curVoice] = 0;
			}

			for (curRow = 0; curRow < AVM_ROWS; curRow++)
			{
				for (curVoice = 0; curVoice < AVM_VOICES; curVoice++)
				{
					cell = modPos + (curPat * MOD_PAT_SIZE) + (curRow * MOD_ROW_SIZE) + (curVoice * 4);

					/*Still inside a run of empty rows: leave the cell empty*/
					if (voiceSkip[curVoice] > 0)
					{
						voiceSkip[curVoice]--;
						continue;
					}

					b0 = romData[voicePos[curVoice]];

					/*1nnnnnnn = n empty rows, this one included ($80 and $81 both mean one)*/
					if (b0 & 0x80)
					{
						k = b0 & 0x7F;
						voiceSkip[curVoice] = (k > 1) ? k - 1 : 0;
						voicePos[curVoice]++;
						continue;
					}

					/*0?nnnnnn iiiieeee [pp]: effect 7 rows have no parameter byte*/
					b1 = romData[voicePos[curVoice] + 1];
					fx = b1 & 0x0F;
					if (fx == 7)
					{
						b2 = 0;
						voicePos[curVoice] += 2;
					}
					else
					{
						b2 = romData[voicePos[curVoice] + 2];
						voicePos[curVoice] += 3;
					}
					note = b0 & 0x3F;
					ins = b1 >> 4;
					param = b2;
					if (ins != 0)
					{
						voiceIns[curVoice] = ins;
					}

					modNote = -1;
					modIns = 0;
					if (note != 0x3F)
					{
						modNote = note + transpose - noteOfs;
						while (modNote < 0)
						{
							modNote += 12;
							folded++;
						}
						while (modNote > 35)
						{
							modNote -= 12;
							folded++;
						}
						/*Repeat the instrument number: the driver resets the volume on every note-on*/
						modIns = voiceIns[curVoice];
					}
					else
					{
						modIns = ins;
					}

					/*Effects: the driver uses ProTracker numbering, so most pass straight through*/
					modFx = 0;
					modParam = 0;
					switch (fx)
					{
					case 0x00:	/*Arpeggio*/
					case 0x0B:	/*Position jump*/
					case 0x0C:	/*Volume, already 0-64*/
						modFx = fx;
						modParam = param;
						break;
					case 0x01:	/*Portamento: GB period units -> Amiga period units*/
					case 0x02:
						modFx = fx;
						modParam = (int)((param * 3546895.0) / (sampLen * 131072.0) + 0.5);
						if (modParam < 1) modParam = 1;
						if (modParam > 255) modParam = 255;
						break;
					case 0x08:	/*Stop music -> F00 (stop in ProTracker)*/
						modFx = 0x0F;
						modParam = 0;
						break;
					case 0x0D:	/*Pattern break (the driver ignores the parameter)*/
						modFx = 0x0D;
						modParam = 0;
						break;
					case 0x0F:	/*Speed*/
						modFx = 0x0F;
						modParam = param ? param : 1;
						break;
					default:	/*7 and anything the driver ignores*/
						break;
					}

					/*MOD cell: iiiiPPPP PPPPPPPP iiiieeee pppppppp*/
					period = (modNote >= 0) ? AVMmodPeriods[modNote] : 0;
					modData[cell + 0] = (modIns & 0x10) | ((period >> 8) & 0x0F);
					modData[cell + 1] = period & 0xFF;
					modData[cell + 2] = ((modIns & 0x0F) << 4) | (modFx & 0x0F);
					modData[cell + 3] = modParam;
				}
			}

			for (curVoice = 0; curVoice < AVM_VOICES; curVoice++)
			{
				if (voiceSkip[curVoice] != 0)
				{
					printf("WARNING: pattern %i voice %i: empty-row run crosses the pattern end\n", curPat, curVoice);
				}
			}
		}
		modPos += (highestPos + 1) * MOD_PAT_SIZE;

		if (folded)
		{
			printf("WARNING: song spans more than 3 octaves, %i notes were moved by an octave\n", folded);
		}

		/*Sample data: the 64-byte wave resampled to the chosen length, once per used instrument*/
		for (i = 1; i <= 31; i++)
		{
			if (!insUsed[i])
			{
				continue;
			}
			for (j = 0; j < sampLen; j++)
			{
				modData[modPos + j] = AVMsineWave[(j * 64) / sampLen];
			}
			modPos += sampLen;
		}

		fwrite(modData, modPos, 1, mod);
		free(modData);
		fclose(mod);
	}
}
