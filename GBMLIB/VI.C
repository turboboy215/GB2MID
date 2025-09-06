/*Visual Impact*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "VI.H"
#include "PUCRUNCH.H"
#include "RNC.H"
#include "T2KDEC.H"

#define bankSize 16384

typedef struct {
	uint8_t accumulator; /* 8 - bit accumulator*/
	uint8_t carry;      /* Carry flag(0 or 1)*/
} CPU;

FILE* rom, * xm, * data, * cfg;
long bank;
long offset;
int i, j, a, c;
char outfile[1000000];
long bankAmt;
int numSongs;
int songNum;
int songStart;
int songEnd;
int songLen;
int cmpLen;
int patRows;
int songBank;
long songTable;
long insTable;
long patTable;
int totalSeqs;
int drvVers;
int compressed;
int curInst;
int carry;
CPU VITempCPU = { 0, 0 };

unsigned char* romData;
unsigned char* songData;
unsigned char* compData;
unsigned char* xmData;
unsigned char* endData;
long xmLength;

char string1[100];
char string2[100];
char VIcheckStrings[6][100] = { "numSongs=", "format=", "bank=", "start=", "end=", "compressed=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
void VIcopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd);
void VIsong2xm(int songNum, int songLen);
void SLA(CPU* cpu);
void SRA(CPU* cpu);

void VIcopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd)
{
	int k = dataStart;
	int l = 0;

	if (compressed == 0)
	{
		songLen = dataEnd - dataStart;
		songData = (unsigned char*)malloc(bankSize);

		while (k <= dataEnd)
		{
			songData[l] = romData[k];
			k++;
			l++;
		}
	}
	else
	{
		cmpLen = dataEnd - dataStart;
		compData = (unsigned char*)malloc(cmpLen);
		songData = (unsigned char*)malloc(bankSize);

		while (k <= dataEnd)
		{
			compData[l] = romData[k];
			k++;
			l++;
		}
	}
	free(romData);
}

void VIProc(int bank, char parameters[4][100])
{
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, VIcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtol(string1, NULL, 16);

		printf("Number of songs: %i\n", numSongs);
		fgets(string1, 3, cfg);

		/*Get the driver version*/
		fgets(string1, 8, cfg);
		if (memcmp(string1, VIcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		drvVers = strtol(string1, NULL, 16);

		if (drvVers != 0 && drvVers != 1)
		{
			printf("ERROR: Invalid version number!\n");
			exit(2);
		}

		printf("Version: %i\n", drvVers);

		/*Process each song*/
		songNum = 1;

		while (songNum <= numSongs)
		{
			/*Skip new line*/
			fgets(string1, 3, cfg);

			/*Skip the first line*/
			fgets(string1, 13, cfg);

			/*Get the song bank*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, VIcheckStrings[2], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songBank = strtol(string1, NULL, 16);

			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			/*Get the start of the song*/
			fgets(string1, 7, cfg);
			if (memcmp(string1, VIcheckStrings[3], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songStart = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			/*Get the end of the song*/
			fgets(string1, 5, cfg);
			if (memcmp(string1, VIcheckStrings[4], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			songEnd = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 3, cfg);

			/*Get the compression type*/
			fgets(string1, 12, cfg);
			if (memcmp(string1, VIcheckStrings[5], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}

			fgets(string1, 5, cfg);

			compressed = strtol(string1, NULL, 16);

			songLen = songEnd - songStart;

			printf("Song %i: 0x%04X (bank: %02X, size: 0x%04X, compression: %i)\n", songNum, songStart, songBank, songLen, compressed);

			/*Uncompressed*/
			if (compressed == 0)
			{
				VIcopyData(romData, songData, songStart, songEnd);
				VIsong2xm(songNum, songLen);
			}

			/*RNC*/
			else if (compressed == 1)
			{
				VIcopyData(romData, compData, songStart, songEnd);
				cmpLen = songEnd - songStart;
				procRNC(compData, songData, cmpLen);
				VIsong2xm(songNum, songLen);
			}
			/*Pucrunch*/
			else if (compressed == 2)
			{
				VIcopyData(romData, compData, songStart, songEnd);
				cmpLen = songEnd - songStart;
				PCProc(compData, songData, cmpLen);
				VIsong2xm(songNum, songLen);
			}
			/*Tyrian 2000*/
			else if (compressed == 3)
			{
				VIcopyData(romData, compData, songStart, songEnd);
				cmpLen = songEnd - songStart;
				T2KProc(compData, songData, cmpLen);
				VIsong2xm(songNum, songLen);
			}
			songNum++;
		}

		fclose(cfg);
	}
}

/*Convert the song data to XM*/
void VIsong2xm(int songNum, int songLen)
{
	int curPat = 0;
	long pattern[4];
	unsigned char command[2];
	long curPos = 0;
	int index = 0;
	int curSeq = 0;
	long c1Pos = 0;
	long c2Pos = 0;
	long c3Pos = 0;
	long c4Pos = 0;
	long songPos = 0;
	long xmPos = 0;
	long patPos = 0;
	int channels = 4;
	int defTicks = 6;
	int bpm = 150;
	long packPos = 0;
	long tempPos = 0;
	int rowsLeft = 0;
	int curChan = 0;
	int numPats = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	long patSize = 0;
	int curNote;
	int freqVal;
	int instChan;

	int l = 0;

	xmLength = 0x10000;
	xmData = ((unsigned char*)malloc(xmLength));

	for (l = 0; l < xmLength; l++)
	{
		xmData[l] = 0;
	}

	sprintf(outfile, "song%d.xm", songNum);
	if ((xm = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.xm!\n", songNum);
		exit(2);
	}
	else
	{

		if (drvVers == 1)
		{
			/*Get the number of patterns for the XM*/
			songPos = 132;
			while (numPats < 64)
			{
				if (songData[songPos] == 0x00 && songData[songPos + 1] == 0x00 && songData[songPos + 2] == 0x00 && songData[songPos + 3] == 0x00)
				{
					break;
				}
				numPats++;
				songPos += 4;
			}

			xmPos = 0;
			/*Write the header*/
			sprintf((char*)&xmData[xmPos], "Extended Module: ");
			xmPos += 17;
			sprintf((char*)&xmData[xmPos], "                     ");
			xmPos += 20;
			Write8B(&xmData[xmPos], 0x1A);
			xmPos++;
			sprintf((char*)&xmData[xmPos], "FastTracker v2.00   ");
			xmPos += 20;
			WriteBE16(&xmData[xmPos], 0x0401);
			xmPos += 2;

			/*Header size: 20 + number of patterns (256)*/
			WriteLE32(&xmData[xmPos], 276);
			xmPos += 4;

			/*Song length*/
			WriteLE16(&xmData[xmPos], numPats);
			xmPos += 2;

			/*Restart position*/
			WriteLE16(&xmData[xmPos], 0);
			xmPos += 2;

			/*Number of channels*/
			WriteLE16(&xmData[xmPos], channels);
			xmPos += 2;

			/*Number of patterns*/
			WriteLE16(&xmData[xmPos], numPats);
			xmPos += 2;

			/*Number of instruments*/
			WriteLE16(&xmData[xmPos], 32);
			xmPos += 2;

			/*Flags: Linear frequency*/
			WriteLE16(&xmData[xmPos], 1);
			xmPos += 2;

			/*Default tempo (ticks)*/
			defTicks = songData[0x01];
			WriteLE16(&xmData[xmPos], defTicks);
			xmPos += 2;

			/*Default tempo (BPM), always the same for our case*/
			WriteLE16(&xmData[xmPos], bpm);
			xmPos += 2;

			/*Pattern table*/
			for (l = 0; l < numPats; l++)
			{
				Write8B(&xmData[xmPos], l);
				xmPos++;
			}
			xmPos += (256 - l);

			songPos = 132;

			for (curPat = 0; curPat < numPats; curPat++)
			{
				/*First, pattern header*/
				/*Pattern header length*/
				WriteLE32(&xmData[xmPos], 9);
				xmPos += 4;

				/*Packing type = 0*/
				Write8B(&xmData[xmPos], 0);
				xmPos++;

				/*Number of rows*/
				WriteLE16(&xmData[xmPos], patRows);
				xmPos += 2;

				/*Packed pattern data - fill in later*/
				packPos = xmPos;
				WriteLE16(&xmData[xmPos], 0);
				xmPos += 2;

				/*Now the actual pattern data...*/
				rowsLeft = 64;
				patSize = 0;

				for (index = 0; index < 4; index++)
				{
					pattern[index] = songData[songPos + index];
				}

				/*Get channel information*/
				c1Pos = 392 + (pattern[0] * 128);
				c2Pos = 392 + (pattern[1] * 128);
				c3Pos = 392 + (pattern[2] * 128);
				c4Pos = 392 + (pattern[3] * 128);
				songPos += 4;

				for (rowsLeft = 64; rowsLeft > 0; rowsLeft--)
				{
					for (curChan = 0; curChan < 4; curChan++)
					{
						if (curChan == 0)
						{
							command[0] = songData[c1Pos];
							command[1] = songData[c1Pos + 1];
						}

						else if (curChan == 1)
						{
							command[0] = songData[c2Pos];
							command[1] = songData[c2Pos + 1];
						}

						else if (curChan == 2)
						{
							command[0] = songData[c3Pos];
							command[1] = songData[c3Pos + 1];
						}

						else if (curChan == 3)
						{
							command[0] = songData[c4Pos];
							command[1] = songData[c4Pos + 1];
						}

						/*Empty row*/
						if (command[0] == 0x00 && command[1] == 0x00)
						{
							Write8B(&xmData[xmPos], 0x80);
							if (curChan == 0)
							{
								c1Pos += 2;
							}
							else if (curChan == 1)
							{
								c2Pos += 2;
							}
							else if (curChan == 2)
							{
								c3Pos += 2;
							}
							else if (curChan == 3)
							{
								c4Pos += 2;
							}
							xmPos++;
							patSize++;
						}

						/*Command*/
						else if (command[1] == 0xFF)
						{
							/*Mute channel note*/
							if (command[0] <= 0x03)
							{
								Write8B(&xmData[xmPos], 0x81);
								Write8B(&xmData[xmPos + 1], 97);

								xmPos += 2;
								patSize += 2;
							}

							/*Pattern break?*/
							else if (command[0] == 0x05)
							{
								Write8B(&xmData[xmPos], 0x88);
								Write8B(&xmData[xmPos + 1], 0x0D);

								xmPos += 2;
								patSize += 2;
							}

							if (curChan == 0)
							{
								c1Pos += 2;
							}
							else if (curChan == 1)
							{
								c2Pos += 2;
							}
							else if (curChan == 2)
							{
								c3Pos += 2;
							}
							else if (curChan == 3)
							{
								c4Pos += 2;
							}
						}

						/*Play note with instrument*/
						else
						{
							/*Get the instrument number*/
							VITempCPU.accumulator = command[0];
							SLA(&VITempCPU);
							SLA(&VITempCPU);
							SLA(&VITempCPU);
							curInst = (VITempCPU.accumulator / 8) + 1;
							instChan = songData[4 + VITempCPU.accumulator];

							/*Get the note frequency*/
							curNote = command[0] & 0xE0;
							curNote = ((curNote & 0x0F) << 4 | (curNote & 0xF0) >> 4);
							VITempCPU.accumulator = curNote;
							SRA(&VITempCPU);
							freqVal = command[1] + (VITempCPU.accumulator * 0x100);

							if (freqVal <= 0x0020)
							{
								curNote = 12;
							}
							else if (freqVal >= 0x0020 && freqVal < 0x0095)
							{
								curNote = 12;
							}
							else if (freqVal >= 0x0095 && freqVal < 0x00FD)
							{
								curNote = 13;
							}
							else if (freqVal >= 0x00FD && freqVal < 0x016B)
							{
								curNote = 14;
							}
							else if (freqVal >= 0x0160 && freqVal < 0x01C0)
							{
								curNote = 15;
							}
							else if (freqVal >= 0x01C0 && freqVal < 0x021E)
							{
								curNote = 16;
							}
							else if (freqVal >= 0x021E && freqVal < 0x0270)
							{
								curNote = 17;
							}
							else if (freqVal >= 0x0270 && freqVal < 0x02C0)
							{
								curNote = 18;
							}
							else if (freqVal >= 0x02C0 && freqVal < 0x0310)
							{
								curNote = 19;
							}
							else if (freqVal >= 0x0310 && freqVal < 0x0350)
							{
								curNote = 20;
							}
							else if (freqVal >= 0x0350 && freqVal < 0x0390)
							{
								curNote = 21;
							}
							else if (freqVal >= 0x0390 && freqVal < 0x03D0)
							{
								curNote = 22;
							}
							else if (freqVal >= 0x03D0 && freqVal < 0x0410)
							{
								curNote = 23;
							}
							else if (freqVal >= 0x0410 && freqVal < 0x0440)
							{
								curNote = 24;
							}
							else if (freqVal >= 0x0440 && freqVal < 0x0480)
							{
								curNote = 25;
							}
							else if (freqVal >= 0x0480 && freqVal < 0x04B5)
							{
								curNote = 26;
							}
							else if (freqVal >= 0x04B0 && freqVal < 0x04E0)
							{
								curNote = 27;
							}
							else if (freqVal >= 0x04E0 && freqVal < 0x0510)
							{
								curNote = 28;
							}
							else if (freqVal >= 0x0510 && freqVal < 0x053B)
							{
								curNote = 29;
							}
							else if (freqVal >= 0x053B && freqVal < 0x0563)
							{
								curNote = 30;
							}
							else if (freqVal >= 0x0563 && freqVal < 0x0589)
							{
								curNote = 31;
							}
							else if (freqVal >= 0x0589 && freqVal < 0x05AC)
							{
								curNote = 32;
							}
							else if (freqVal >= 0x05AC && freqVal < 0x05CE)
							{
								curNote = 33;
							}
							else if (freqVal >= 0x05CE && freqVal < 0x05ED)
							{
								curNote = 34;
							}
							else if (freqVal >= 0x05ED && freqVal < 0x060B)
							{
								curNote = 35;
							}
							else if (freqVal >= 0x060B && freqVal < 0x0627)
							{
								curNote = 36;
							}
							else if (freqVal >= 0x0627 && freqVal < 0x0642)
							{
								curNote = 37;
							}
							else if (freqVal >= 0x0642 && freqVal < 0x065B)
							{
								curNote = 38;
							}
							else if (freqVal >= 0x065B && freqVal < 0x0672)
							{
								curNote = 39;
							}
							else if (freqVal >= 0x0672 && freqVal < 0x0689)
							{
								curNote = 40;
							}
							else if (freqVal >= 0x0689 && freqVal < 0x069E)
							{
								curNote = 41;
							}
							else if (freqVal >= 0x069E && freqVal < 0x06B2)
							{
								curNote = 42;
							}
							else if (freqVal >= 0x06B2 && freqVal < 0x06C4)
							{
								curNote = 43;
							}
							else if (freqVal >= 0x06C4 && freqVal < 0x06D6)
							{
								curNote = 44;
							}
							else if (freqVal >= 0x06D6 && freqVal < 0x06E7)
							{
								curNote = 45;
							}
							else if (freqVal >= 0x06E7 && freqVal < 0x06F7)
							{
								curNote = 46;
							}
							else if (freqVal >= 0x06F7 && freqVal < 0x0706)
							{
								curNote = 47;
							}
							else if (freqVal >= 0x0706 && freqVal < 0x0714)
							{
								curNote = 48;
							}
							else if (freqVal >= 0x0714 && freqVal < 0x0721)
							{
								curNote = 49;
							}
							else if (freqVal >= 0x0721 && freqVal < 0x072D)
							{
								curNote = 50;
							}
							else if (freqVal >= 0x072D && freqVal < 0x0739)
							{
								curNote = 51;
							}
							else if (freqVal >= 0x0739 && freqVal < 0x0744)
							{
								curNote = 52;
							}
							else if (freqVal >= 0x0744 && freqVal < 0x074F)
							{
								curNote = 53;
							}
							else if (freqVal >= 0x074F && freqVal < 0x0759)
							{
								curNote = 54;
							}
							else if (freqVal >= 0x0759 && freqVal < 0x0762)
							{
								curNote = 55;
							}
							else if (freqVal >= 0x0762 && freqVal < 0x076B)
							{
								curNote = 56;
							}
							else if (freqVal >= 0x076B && freqVal < 0x0773)
							{
								curNote = 57;
							}
							else if (freqVal >= 0x0773 && freqVal < 0x077B)
							{
								curNote = 58;
							}
							else if (freqVal >= 0x077B && freqVal < 0x0783)
							{
								curNote = 59;
							}
							else if (freqVal >= 0x0783 && freqVal < 0x078A)
							{
								curNote = 60;
							}
							else if (freqVal >= 0x078A && freqVal < 0x0790)
							{
								curNote = 61;
							}
							else if (freqVal >= 0x0790 && freqVal < 0x0797)
							{
								curNote = 62;
							}
							else if (freqVal >= 0x0797 && freqVal < 0x079D)
							{
								curNote = 63;
							}
							else if (freqVal >= 0x079D && freqVal < 0x07A2)
							{
								curNote = 64;
							}
							else if (freqVal >= 0x07A2 && freqVal < 0x07A7)
							{
								curNote = 65;
							}
							else if (freqVal >= 0x07A7 && freqVal < 0x07AC)
							{
								curNote = 66;
							}
							else if (freqVal >= 0x07AC && freqVal < 0x07B1)
							{
								curNote = 67;
							}
							else if (freqVal >= 0x07B1 && freqVal < 0x07B6)
							{
								curNote = 68;
							}
							else if (freqVal >= 0x07B6 && freqVal < 0x07BA)
							{
								curNote = 69;
							}
							else if (freqVal >= 0x07BA && freqVal < 0x07BE)
							{
								curNote = 70;
							}
							else if (freqVal >= 0x07BE && freqVal < 0x07C1)
							{
								curNote = 71;
							}
							else if (freqVal >= 0x07C1 && freqVal < 0x07C5)
							{
								curNote = 72;
							}
							else if (freqVal >= 0x07C5 && freqVal < 0x07C8)
							{
								curNote = 73;
							}
							else if (freqVal >= 0x07C8 && freqVal < 0x07CB)
							{
								curNote = 74;
							}
							else if (freqVal >= 0x07CB && freqVal < 0x07CE)
							{
								curNote = 75;
							}
							else if (freqVal >= 0x07CE && freqVal < 0x07D1)
							{
								curNote = 76;
							}
							else if (freqVal >= 0x07D1 && freqVal < 0x07D4)
							{
								curNote = 77;
							}
							else if (freqVal >= 0x07D4 && freqVal < 0x07D6)
							{
								curNote = 78;
							}
							else if (freqVal >= 0x07D6 && freqVal < 0x07D9)
							{
								curNote = 79;
							}
							else if (freqVal >= 0x07D9 && freqVal < 0x07DB)
							{
								curNote = 80;
							}
							else if (freqVal >= 0x07DB && freqVal < 0x07DD)
							{
								curNote = 81;
							}
							else if (freqVal >= 0x07DD && freqVal < 0x07DF)
							{
								curNote = 82;
							}
							else if (freqVal >= 0x07DF && freqVal < 0x07E1)
							{
								curNote = 83;
							}
							else if (freqVal >= 0x07E1 && freqVal < 0x07E2)
							{
								curNote = 84;
							}
							else if (freqVal >= 0x07E2 && freqVal < 0x07E4)
							{
								curNote = 85;
							}
							else if (freqVal >= 0x07E4 && freqVal < 0x07E6)
							{
								curNote = 86;
							}
							else if (freqVal >= 0x07E6 && freqVal < 0x07E7)
							{
								curNote = 87;
							}
							else if (freqVal >= 0x07E7 && freqVal < 0x07E9)
							{
								curNote = 88;
							}
							else if (freqVal >= 0x07E9 && freqVal < 0x07EA)
							{
								curNote = 89;
							}
							else if (freqVal >= 0x07EA && freqVal < 0x07EB)
							{
								curNote = 90;
							}
							else if (freqVal >= 0x07EB && freqVal < 0x07EC)
							{
								curNote = 91;
							}
							else if (freqVal >= 0x07EC && freqVal < 0x07ED)
							{
								curNote = 92;
							}
							else if (freqVal >= 0x07ED && freqVal < 0x07EE)
							{
								curNote = 93;
							}
							else if (freqVal >= 0x07EE && freqVal < 0x07EF)
							{
								curNote = 94;
							}
							else if (freqVal >= 0x07EF && freqVal < 0x07F0)
							{
								curNote = 95;
							}
							else if (freqVal >= 0x07F0 && freqVal < 0x07F1)
							{
								curNote = 96;
							}
							else if (freqVal >= 0x07F1 && freqVal < 0x07F2)
							{
								curNote = 97;
							}
							else if (freqVal >= 0x07F2 && freqVal < 0x07F2)
							{
								curNote = 98;
							}
							else if (freqVal >= 0x07F3 && freqVal < 0x07F4)
							{
								curNote = 99;
							}
							else if (freqVal >= 0x07F4 && freqVal < 0x07F5)
							{
								curNote = 100;
							}
							else if (freqVal >= 0x07F4 && freqVal < 0x07F5)
							{
								curNote = 101;
							}
							else if (freqVal >= 0x07F5 && freqVal < 0x07F6)
							{
								curNote = 102;
							}
							else
							{
								curNote = 103;
							}


							curNote += 13;

							if (instChan == 3)
							{
								curNote = command[1];

								if (curNote > 96)
								{
									curNote -= 96;
								}
							}
							Write8B(&xmData[xmPos], 0x83);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);

							xmPos += 3;
							patSize += 3;

							if (curChan == 0)
							{
								c1Pos += 2;
							}
							else if (curChan == 1)
							{
								c2Pos += 2;
							}
							else if (curChan == 2)
							{
								c3Pos += 2;
							}
							else if (curChan == 3)
							{
								c4Pos += 2;
							}

						}
					}
				}

				WriteLE16(&xmData[packPos], patSize);
			}
		}
		else
		{
			/*Get the number of patterns for the XM*/
			songPos = 130;
			while (numPats < 64)
			{
				if (songData[songPos] == 0x00 && songData[songPos + 1] == 0x00 && songData[songPos + 2] == 0x00 && songData[songPos + 3] == 0x00)
				{
					break;
				}
				numPats++;
				songPos += 4;
			}

			xmPos = 0;
			/*Write the header*/
			sprintf((char*)&xmData[xmPos], "Extended Module: ");
			xmPos += 17;
			sprintf((char*)&xmData[xmPos], "                     ");
			xmPos += 20;
			Write8B(&xmData[xmPos], 0x1A);
			xmPos++;
			sprintf((char*)&xmData[xmPos], "FastTracker v2.00   ");
			xmPos += 20;
			WriteBE16(&xmData[xmPos], 0x0401);
			xmPos += 2;

			/*Header size: 20 + number of patterns (256)*/
			WriteLE32(&xmData[xmPos], 276);
			xmPos += 4;

			/*Song length*/
			WriteLE16(&xmData[xmPos], numPats);
			xmPos += 2;

			/*Restart position*/
			WriteLE16(&xmData[xmPos], 0);
			xmPos += 2;

			/*Number of channels*/
			WriteLE16(&xmData[xmPos], channels);
			xmPos += 2;

			/*Number of patterns*/
			WriteLE16(&xmData[xmPos], numPats);
			xmPos += 2;

			/*Number of instruments*/
			WriteLE16(&xmData[xmPos], 32);
			xmPos += 2;

			/*Flags: Linear frequency*/
			WriteLE16(&xmData[xmPos], 1);
			xmPos += 2;

			/*Default tempo (ticks)*/
			defTicks = songData[0x80];
			WriteLE16(&xmData[xmPos], defTicks);
			xmPos += 2;

			/*Default tempo (BPM), always the same for our case*/
			WriteLE16(&xmData[xmPos], bpm);
			xmPos += 2;

			/*Pattern table*/
			for (l = 0; l < numPats; l++)
			{
				Write8B(&xmData[xmPos], l);
				xmPos++;
			}
			xmPos += (256 - l);

			songPos = 130;

			for (curPat = 0; curPat < numPats; curPat++)
			{
				/*First, pattern header*/
				/*Pattern header length*/
				WriteLE32(&xmData[xmPos], 9);
				xmPos += 4;

				/*Packing type = 0*/
				Write8B(&xmData[xmPos], 0);
				xmPos++;

				/*Number of rows*/
				WriteLE16(&xmData[xmPos], patRows);
				xmPos += 2;

				/*Packed pattern data - fill in later*/
				packPos = xmPos;
				WriteLE16(&xmData[xmPos], 0);
				xmPos += 2;

				/*Now the actual pattern data...*/
				rowsLeft = 64;
				patSize = 0;

				for (index = 0; index < 4; index++)
				{
					pattern[index] = songData[songPos + index];
				}

				/*Get channel information*/
				c1Pos = 386 + (pattern[0] * 128);
				c2Pos = 386 + (pattern[1] * 128);
				c3Pos = 386 + (pattern[2] * 128);
				c4Pos = 386 + (pattern[3] * 128);
				songPos += 4;

				for (rowsLeft = 64; rowsLeft > 0; rowsLeft--)
				{
					for (curChan = 0; curChan < 4; curChan++)
					{
						if (curChan == 0)
						{
							command[0] = songData[c1Pos];
							command[1] = songData[c1Pos + 1];
						}

						else if (curChan == 1)
						{
							command[0] = songData[c2Pos];
							command[1] = songData[c2Pos + 1];
						}

						else if (curChan == 2)
						{
							command[0] = songData[c3Pos];
							command[1] = songData[c3Pos + 1];
						}

						else if (curChan == 3)
						{
							command[0] = songData[c4Pos];
							command[1] = songData[c4Pos + 1];
						}

						/*Empty row*/
						if (command[0] == 0x00 && command[1] == 0x00)
						{
							Write8B(&xmData[xmPos], 0x80);
							if (curChan == 0)
							{
								c1Pos += 2;
							}
							else if (curChan == 1)
							{
								c2Pos += 2;
							}
							else if (curChan == 2)
							{
								c3Pos += 2;
							}
							else if (curChan == 3)
							{
								c4Pos += 2;
							}
							xmPos++;
							patSize++;
						}

						/*Command*/
						else if (command[1] == 0xFF)
						{
							/*Mute channel note*/
							if (command[0] <= 0x03)
							{
								Write8B(&xmData[xmPos], 0x81);
								Write8B(&xmData[xmPos + 1], 97);

								xmPos += 2;
								patSize += 2;
							}

							/*Pattern break?*/
							else if (command[0] == 0x05)
							{
								Write8B(&xmData[xmPos], 0x88);
								Write8B(&xmData[xmPos + 1], 0x0D);

								xmPos += 2;
								patSize += 2;
							}

							if (curChan == 0)
							{
								c1Pos += 2;
							}
							else if (curChan == 1)
							{
								c2Pos += 2;
							}
							else if (curChan == 2)
							{
								c3Pos += 2;
							}
							else if (curChan == 3)
							{
								c4Pos += 2;
							}
						}

						/*Play note with instrument*/
						else
						{
							/*Get the instrument number*/
							curInst = command[0] + 1;
							instChan = songData[8 * command[0]];

							freqVal = command[1];

							a = freqVal;
							c = a;
							a = 0;
							VITempCPU.accumulator = c;
							SLA(&VITempCPU);
							c = VITempCPU.accumulator;
							a += carry;
							VITempCPU.accumulator = a;
							SLA(&VITempCPU);
							a = VITempCPU.accumulator;
							VITempCPU.accumulator = c;
							SLA(&VITempCPU);
							c = VITempCPU.accumulator;
							a += carry;
							VITempCPU.accumulator = a;
							SLA(&VITempCPU);
							a = VITempCPU.accumulator;
							VITempCPU.accumulator = c;
							SLA(&VITempCPU);
							c = VITempCPU.accumulator;
							a += carry;

							freqVal = c + (a * 0x100);


							if (freqVal <= 0x0020)
							{
								curNote = 12;
							}
							else if (freqVal >= 0x0020 && freqVal < 0x0095)
							{
								curNote = 12;
							}
							else if (freqVal >= 0x0095 && freqVal < 0x00FD)
							{
								curNote = 13;
							}
							else if (freqVal >= 0x00FD && freqVal < 0x0160)
							{
								curNote = 14;
							}
							else if (freqVal >= 0x0160 && freqVal < 0x01C0)
							{
								curNote = 15;
							}
							else if (freqVal >= 0x01C0 && freqVal < 0x021E)
							{
								curNote = 16;
							}
							else if (freqVal >= 0x021E && freqVal < 0x0270)
							{
								curNote = 17;
							}
							else if (freqVal >= 0x0270 && freqVal < 0x02C0)
							{
								curNote = 18;
							}
							else if (freqVal >= 0x02C0 && freqVal < 0x0310)
							{
								curNote = 19;
							}
							else if (freqVal >= 0x0310 && freqVal < 0x0350)
							{
								curNote = 20;
							}
							else if (freqVal >= 0x0350 && freqVal < 0x0390)
							{
								curNote = 21;
							}
							else if (freqVal >= 0x0390 && freqVal < 0x03D0)
							{
								curNote = 22;
							}
							else if (freqVal >= 0x03D0 && freqVal < 0x0410)
							{
								curNote = 23;
							}
							else if (freqVal >= 0x0410 && freqVal < 0x0440)
							{
								curNote = 24;
							}
							else if (freqVal >= 0x0440 && freqVal < 0x0480)
							{
								curNote = 25;
							}
							else if (freqVal >= 0x0480 && freqVal < 0x04B0)
							{
								curNote = 26;
							}
							else if (freqVal >= 0x04B0 && freqVal < 0x04E0)
							{
								curNote = 27;
							}
							else if (freqVal >= 0x04E0 && freqVal < 0x0510)
							{
								curNote = 28;
							}
							else if (freqVal >= 0x0510 && freqVal < 0x053B)
							{
								curNote = 29;
							}
							else if (freqVal >= 0x053B && freqVal < 0x0563)
							{
								curNote = 30;
							}
							else if (freqVal >= 0x0563 && freqVal < 0x0585)
							{
								curNote = 31;
							}
							else if (freqVal >= 0x0585 && freqVal < 0x05AC)
							{
								curNote = 32;
							}
							else if (freqVal >= 0x05AC && freqVal < 0x05CE)
							{
								curNote = 33;
							}
							else if (freqVal >= 0x05CE && freqVal < 0x05ED)
							{
								curNote = 34;
							}
							else if (freqVal >= 0x05ED && freqVal < 0x060B)
							{
								curNote = 35;
							}
							else if (freqVal >= 0x060B && freqVal < 0x0627)
							{
								curNote = 36;
							}
							else if (freqVal >= 0x0627 && freqVal < 0x0642)
							{
								curNote = 37;
							}
							else if (freqVal >= 0x0642 && freqVal < 0x0657)
							{
								curNote = 38;
							}
							else if (freqVal >= 0x0657 && freqVal < 0x0672)
							{
								curNote = 39;
							}
							else if (freqVal >= 0x0672 && freqVal < 0x0689)
							{
								curNote = 40;
							}
							else if (freqVal >= 0x0689 && freqVal < 0x069E)
							{
								curNote = 41;
							}
							else if (freqVal >= 0x069E && freqVal < 0x06B2)
							{
								curNote = 42;
							}
							else if (freqVal >= 0x06B2 && freqVal < 0x06C4)
							{
								curNote = 43;
							}
							else if (freqVal >= 0x06C4 && freqVal < 0x06D6)
							{
								curNote = 44;
							}
							else if (freqVal >= 0x06D6 && freqVal < 0x06E7)
							{
								curNote = 45;
							}
							else if (freqVal >= 0x06E7 && freqVal < 0x06F7)
							{
								curNote = 46;
							}
							else if (freqVal >= 0x06F7 && freqVal < 0x0706)
							{
								curNote = 47;
							}
							else if (freqVal >= 0x0706 && freqVal < 0x0714)
							{
								curNote = 48;
							}
							else if (freqVal >= 0x0714 && freqVal < 0x0721)
							{
								curNote = 49;
							}
							else if (freqVal >= 0x0721 && freqVal < 0x072D)
							{
								curNote = 50;
							}
							else if (freqVal >= 0x072D && freqVal < 0x0739)
							{
								curNote = 51;
							}
							else if (freqVal >= 0x0739 && freqVal < 0x0744)
							{
								curNote = 52;
							}
							else if (freqVal >= 0x0744 && freqVal < 0x074F)
							{
								curNote = 53;
							}
							else if (freqVal >= 0x074F && freqVal < 0x0759)
							{
								curNote = 54;
							}
							else if (freqVal >= 0x0759 && freqVal < 0x0762)
							{
								curNote = 55;
							}
							else if (freqVal >= 0x0762 && freqVal < 0x076B)
							{
								curNote = 56;
							}
							else if (freqVal >= 0x076B && freqVal < 0x0773)
							{
								curNote = 57;
							}
							else if (freqVal >= 0x0773 && freqVal < 0x077B)
							{
								curNote = 58;
							}
							else if (freqVal >= 0x077B && freqVal < 0x0783)
							{
								curNote = 59;
							}
							else if (freqVal >= 0x0783 && freqVal < 0x078A)
							{
								curNote = 60;
							}
							else if (freqVal >= 0x078A && freqVal < 0x0790)
							{
								curNote = 61;
							}
							else if (freqVal >= 0x0790 && freqVal < 0x0797)
							{
								curNote = 62;
							}
							else if (freqVal >= 0x0797 && freqVal < 0x079D)
							{
								curNote = 63;
							}
							else if (freqVal >= 0x079D && freqVal < 0x07A2)
							{
								curNote = 64;
							}
							else if (freqVal >= 0x07A2 && freqVal < 0x07A7)
							{
								curNote = 65;
							}
							else if (freqVal >= 0x07A7 && freqVal < 0x07AC)
							{
								curNote = 66;
							}
							else if (freqVal >= 0x07AC && freqVal < 0x07B1)
							{
								curNote = 67;
							}
							else if (freqVal >= 0x07B1 && freqVal < 0x07B6)
							{
								curNote = 68;
							}
							else if (freqVal >= 0x07B6 && freqVal < 0x07BA)
							{
								curNote = 69;
							}
							else if (freqVal >= 0x07BA && freqVal < 0x07BE)
							{
								curNote = 70;
							}
							else if (freqVal >= 0x07BE && freqVal < 0x07C1)
							{
								curNote = 71;
							}
							else if (freqVal >= 0x07C1 && freqVal < 0x07C5)
							{
								curNote = 72;
							}
							else if (freqVal >= 0x07C5 && freqVal < 0x07C8)
							{
								curNote = 73;
							}
							else if (freqVal >= 0x07C8 && freqVal < 0x07CB)
							{
								curNote = 74;
							}
							else if (freqVal >= 0x07CB && freqVal < 0x07CE)
							{
								curNote = 75;
							}
							else if (freqVal >= 0x07CE && freqVal < 0x07D1)
							{
								curNote = 76;
							}
							else if (freqVal >= 0x07D1 && freqVal < 0x07D4)
							{
								curNote = 77;
							}
							else if (freqVal >= 0x07D4 && freqVal < 0x07D6)
							{
								curNote = 78;
							}
							else if (freqVal >= 0x07D6 && freqVal < 0x07D9)
							{
								curNote = 79;
							}
							else if (freqVal >= 0x07D9 && freqVal < 0x07DB)
							{
								curNote = 80;
							}
							else if (freqVal >= 0x07DB && freqVal < 0x07DD)
							{
								curNote = 81;
							}
							else if (freqVal >= 0x07DD && freqVal < 0x07DF)
							{
								curNote = 82;
							}
							else if (freqVal >= 0x07DF && freqVal < 0x07E1)
							{
								curNote = 83;
							}
							else if (freqVal >= 0x07E1 && freqVal < 0x07E2)
							{
								curNote = 84;
							}
							else if (freqVal >= 0x07E2 && freqVal < 0x07E4)
							{
								curNote = 85;
							}
							else if (freqVal >= 0x07E4 && freqVal < 0x07E6)
							{
								curNote = 86;
							}
							else if (freqVal >= 0x07E6 && freqVal < 0x07E7)
							{
								curNote = 87;
							}
							else if (freqVal >= 0x07E7 && freqVal < 0x07E9)
							{
								curNote = 88;
							}
							else if (freqVal >= 0x07E9 && freqVal < 0x07EA)
							{
								curNote = 89;
							}
							else if (freqVal >= 0x07EA && freqVal < 0x07EB)
							{
								curNote = 90;
							}
							else if (freqVal >= 0x07EB && freqVal < 0x07EC)
							{
								curNote = 91;
							}
							else if (freqVal >= 0x07EC && freqVal < 0x07ED)
							{
								curNote = 92;
							}
							else if (freqVal >= 0x07ED && freqVal < 0x07EE)
							{
								curNote = 93;
							}
							else if (freqVal >= 0x07EE && freqVal < 0x07EF)
							{
								curNote = 94;
							}
							else if (freqVal >= 0x07EF && freqVal < 0x07F0)
							{
								curNote = 95;
							}
							else if (freqVal >= 0x07F0 && freqVal < 0x07F1)
							{
								curNote = 96;
							}
							else if (freqVal >= 0x07F1 && freqVal < 0x07F2)
							{
								curNote = 97;
							}
							else if (freqVal >= 0x07F2 && freqVal < 0x07F2)
							{
								curNote = 98;
							}
							else if (freqVal >= 0x07F3 && freqVal < 0x07F4)
							{
								curNote = 99;
							}
							else if (freqVal >= 0x07F4 && freqVal < 0x07F5)
							{
								curNote = 100;
							}
							else if (freqVal >= 0x07F4 && freqVal < 0x07F5)
							{
								curNote = 101;
							}
							else if (freqVal >= 0x07F5 && freqVal < 0x07F6)
							{
								curNote = 102;
							}
							else
							{
								curNote = 103;
							}


							curNote += 13;

							if (instChan == 3)
							{
								curNote = command[1];

								if (curNote > 96)
								{
									curNote -= 96;
								}
							}

							Write8B(&xmData[xmPos], 0x83);
							Write8B(&xmData[xmPos + 1], curNote);
							Write8B(&xmData[xmPos + 2], curInst);

							xmPos += 3;
							patSize += 3;

							if (curChan == 0)
							{
								c1Pos += 2;
							}
							else if (curChan == 1)
							{
								c2Pos += 2;
							}
							else if (curChan == 2)
							{
								c3Pos += 2;
							}
							else if (curChan == 3)
							{
								c4Pos += 2;
							}

						}
					}
				}

				WriteLE16(&xmData[packPos], patSize);
			}

		}




		fwrite(xmData, xmPos, 1, xm);

		/*Add data to end of XM file*/
		if ((data = fopen("xmdata.dat", "rb")) == NULL)
		{
			printf("ERROR: Unable to open file xmdata.dat!\n");
			exit(1);
		}
		else
		{
			endData = ((unsigned char*)malloc(11744));
			fread(endData, 1, 11744, data);
			fwrite(endData, 11744, 1, xm);
			xmPos += 11744;
		}

		fclose(xm);
		free(songData);
		free(xmData);
		free(endData);
	}
}

/*Shift left with carry*/
void SLA(CPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	carry = (cpu->accumulator & 0x80) >> 7;
	cpu->accumulator = (cpu->accumulator << 1) & 0xFE;
}

/*Shift right with carry*/
void SRA(CPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	carry = (cpu->accumulator & 0x01);
	cpu->accumulator = (cpu->accumulator >> 1) & 0xFF;
}