/*TOSE*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

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
long songPtr;
int numChan;
long songPtrs[4];
long curSpeed;
long bankAmt;
long nextPtr;
long masterBank;
int cfgPtr;
int exitError;
int fileExit;
int foundTable;
long base;
int curTrack;
int format;
int curInst;
int curReptPt;
int endPoint;
int speedVal;
long jumpPos;
long jumpRet;
int jumpTimes;
int hasSub;
int sub;
long firstSub;

long repeatPts[100][2];
long repeatList[100];

char string1[100];
char string2[100];
char TOSEcheckStrings[8][100] = { "numSongs=", "format=", "bank=", "addr=", "chan=", "speedval=", "sub=", "base="};
unsigned char* romData;
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
void TOSEsong2mid(int songNum, long ptr, int chanNums);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);

void TOSEProc(char parameters[4][100])
{
	exitError = 0;
	fileExit = 0;
	foundTable = 0;
	base = 0;
	format = 1;
	curInst = 0;
	curReptPt = 0;
	endPoint = 0;
	speedVal = 5;
	jumpPos = 0;
	jumpRet = 0;
	jumpTimes = 0;
	hasSub = 0;
	sub = 1;
	firstSub = 0;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, TOSEcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 4, cfg);
		numSongs = strtod(string1, NULL);
		printf("Total # of songs: %i\n", numSongs);

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the format number*/
		fgets(string1, 8, cfg);

		if (memcmp(string1, TOSEcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		format = strtod(string1, NULL);
		printf("Format: %i\n", format);

		/*Sanrio Timenet has dynamic sub-songs*/
		if (format == 8)
		{
			hasSub = 1;
			format = 3;
		}

		/*Skip new line*/
		fgets(string1, 2, cfg);
		/*Get the base speed value*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, TOSEcheckStrings[5], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		speedVal = strtod(string1, NULL);
		printf("Base speed value: %i\n", speedVal);

		if (format == 0 || format == 10)
		{
			/*Skip new line*/
			fgets(string1, 2, cfg);
			/*Get the base offset value*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, TOSEcheckStrings[7], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);
			base = strtol(string1, NULL, 16);
			printf("Base offset: 0x%04X\n", base);
		}



		for (songNum = 1; songNum <= numSongs; songNum++)
		{
			/*Skip new line*/
			fgets(string1, 2, cfg);

			/*Skip song number*/
			fgets(string1, 11, cfg);

			/*Get the bank number*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, TOSEcheckStrings[2], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 4, cfg);
			bank = strtol(string1, NULL, 16);

			/*Copy the current bank's ROM data*/
			if (format != 0 && format != 10 && format != 89)
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
					if (format == 7)
					{
						/*Banks 1 and 6 are used together for Play Action Football*/
						bankAmt = 0;
						fseek(rom, 0, SEEK_SET);
						romData = (unsigned char*)malloc(bankSize * 2);
						fread(romData, 1, bankSize, rom);
						fseek(rom, (5 * bankSize), SEEK_SET);
						fread(romData + bankSize, 1, bankSize, rom);
					}

					else
					{
						bankAmt = 0;
						fseek(rom, ((bank - 1) * bankSize * 2), SEEK_SET);
						romData = (unsigned char*)malloc(bankSize * 2);
						fread(romData, 1, bankSize * 2, rom);
					}

				}
			}

			/*NES version*/
			else if (format == 0 || format == 10)
			{
				bankAmt = base * 2;

				/*+0x10 = Header*/
				fseek(rom, (((bank - 1) * bankSize) + 0x10), SEEK_SET);
				romData = (unsigned char*)malloc(bankSize);
				fread(romData, 1, bankSize, rom);
			}

			/*Used for Mani 4-in-1 (Tennis)*/
			else if (format == 89)
			{
				fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
				romData = (unsigned char*)malloc(bankSize * 2);
				fread(romData, 1, bankSize * 2, rom);
				fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
			}


			/*Skip new line*/
			fgets(string1, 2, cfg);

			/*Get the hex address*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, TOSEcheckStrings[3], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 5, cfg);
			songPtr = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 2, cfg);

			/*Get the number of channels*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, TOSEcheckStrings[4], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 3, cfg);
			numChan = strtol(string1, NULL, 16);

			/*Special case for Sanrio Timenet sub-songs*/
			if (hasSub == 1)
			{
				/*Skip new line*/
				fgets(string1, 2, cfg);

				/*Get the number of channels*/
				fgets(string1, 5, cfg);

				if (memcmp(string1, TOSEcheckStrings[6], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);

				}
				fgets(string1, 3, cfg);
				sub = strtol(string1, NULL, 16);
			}

			printf("Song %i: bank %01X, address 0x%04X, channels: %i\n", songNum, bank, songPtr, numChan);

			TOSEsong2mid(songNum, songPtr, numChan);
			free(romData);
		}
	}
}

void TOSEsong2mid(int songNum, long ptr, int chanNums)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int curTrack = 0;
	int ptrs[4] = { 0, 0, 0, 0 };
	long command[5];
	int chanMask = 0;
	int chanNum = 0;
	long romPos = 0;
	long seqPos = 0;
	int seqEnd = 0;
	int chanSpeed = 0;
	int chanVol = 0;
	int parameters[4] = { 0, 0, 0, 0 };
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int octave = 0;
	int repeat0Times = -1;
	int repeat0Pos = 0;
	int repeat1Times = -1;
	int repeat1Pos = 0;
	int repeat2Times = -1;
	int repeat2Pos = 0;
	int repeat3Times = -1;
	int repeat3Pos = 0;
	long macroPos = 0;
	long macroRet = 0;
	int macroTimes = 0;
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
	int holdNote = 0;
	long startPos = 0;
	long repeatPt = 0;
	int tempoVal = 0;
	int rep9Times = 0;
	int rep9Rem = 0;
	long originalPos = 0;
	int repPos = 0;


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

	if (bank == 1)
	{
		bankAmt = 0;
	}
	else
	{
		/*WonderSwan version*/
		if (format == 11)
		{
			bankAmt = 0;
		}
		else if (format != 0 && format != 10 && format != 89)
		{
			bankAmt = 0x4000;
		}
		else if (format == 0 || format == 10)
		{
			bankAmt = base;
		}
		else if (format == 89)
		{
			bankAmt = 0;
		}

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
		if (format != 11)
		{
			for (curTrack = 0; curTrack < chanNums; curTrack++)
			{
				chanMask = romData[romPos];
				chanNum = romData[romPos + 1];
				ptrs[chanNum] = ReadLE16(&romData[romPos + 2]);
				romPos += 4;
			}
		}
		else if (format == 11)
		{
			for (curTrack = 0; curTrack < chanNums; curTrack++)
			{
				chanMask = romData[romPos];
				chanNum = romData[romPos + 2];
				ptrs[chanNum] = ReadLE16(&romData[romPos + 4]);
				romPos += 6;
			}
		}


		for (curTrack = 0; curTrack < 4; curTrack++)
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
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1Times = -1;
			repeat2Times = -1;
			repeat3Times = -1;
			repeat0Times = -1;
			repPos = 0;
			firstSub = 0;
			for (k = 0; k < 100; k++)
			{
				repeatList[k] = 0;
			}
			for (k = 0; k < 100; k++)
			{
				repeatPts[k][0] = 0;
				repeatPts[k][1] = 0;
			}
			curReptPt = 0;

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

			if (ptrs[curTrack] != 0 && ptrs[curTrack] >= bankAmt)
			{
				seqPos = ptrs[curTrack] - bankAmt;
				parameters[0] = romData[seqPos];
				parameters[1] = romData[seqPos + 1];
				parameters[2] = romData[seqPos + 2];
				parameters[3] = romData[seqPos + 3];
				highNibble = (parameters[0] & 15);

				if (format == 0 || format == 1 || format == 7 || format == 10 || format == 89)
				{
					if (highNibble < 0x10)
					{
						chanSpeed = (highNibble + 1) * speedVal;
					}
					else
					{
						chanSpeed = speedVal;
					}
				}
				else if (format == 2 || format == 3 || format == 6 || format == 11 || format == 89)
				{
					if (chanSpeed == 0)
					{
						speedVal = 5;
					}
					else
					{
						chanSpeed = speedVal + (highNibble * 0.5);
					}

				}

				else if (format == 4)
				{
					chanSpeed = 5;
					if (curTrack == 0)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = parameters[0] * 2;
						if (tempo == 0)
						{
							tempo = 150;
						}
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}

				}

				else
				{
					if (highNibble < 4)
					{
						chanSpeed = 6;
					}
					else
					{
						chanSpeed = highNibble + 3;
					}

				}

				seqPos += 4;

				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
					command[4] = romData[seqPos + 4];

					/*Play note or rest*/
					if (command[0] < 0xA0)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);
						octave = lowNibble;
						curNoteLen = command[1] * chanSpeed;

						/*Rest*/
						if (highNibble >= 0x0C)
						{
							if (curTrack != 3)
							{
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}
							else if (curTrack == 3)
							{
								if (highNibble == 0x0F)
								{
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
								}
								else
								{
									curNote = highNibble + 48;

									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
								}
							}

						}
						/*Play note*/
						else
						{
							if (curTrack != 3)
							{
								curNote = highNibble + (octave * 12) + 12;
								if (curTrack == 0 || curTrack == 1)
								{
									curNote += 12;
								}
							}
							else
							{
								curNote = highNibble + 48;
							}

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos += 2;
					}

					/*Set envelope length*/
					else if (command[0] == 0xA0)
					{
						seqPos += 2;
					}

					/*Set panning (v1)?*/
					else if (command[0] == 0xA1)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] == 0xA2)
					{
						seqPos += 2;
					}

					/*Set panning (v2)?*/
					else if (command[0] == 0xA3)
					{
						seqPos += 2;
					}

					/*Set panning (v3)?*/
					else if (command[0] == 0xA4)
					{
						seqPos += 2;
					}

					/*Set envelope?*/
					else if (command[0] == 0xA5)
					{
						seqPos += 2;
					}

					/*Set global volume*/
					else if (command[0] == 0xA6)
					{
						seqPos += 2;
					}

					/*Set delay*/
					else if (command[0] == 0xA7)
					{
						curDelay += command[1] * chanSpeed;
						seqPos += 2;
					}

					/*Go to macro*/
					else if (command[0] == 0xA8)
					{
						if (format == 4)
						{
							macroTimes = command[1];
							macroPos = ReadLE16(&romData[seqPos + 2]) - bankAmt;
							macroRet = seqPos += 4;
							seqPos = macroPos;
						}
						else
						{
							seqPos += 2;
						}

					}

					/*Repeat macro/Return from macro*/
					else if (command[0] == 0xA9)
					{
						if (format == 4)
						{
							if (macroTimes > 1)
							{
								seqPos = macroPos;
								macroTimes--;
							}
							else
							{
								seqPos = macroRet;
							}
						}
						else if (format == 3 || format == 5 || format == 6 || format == 11)
						{
							if (songNum == 2 && curTrack == 1)
							{
								songNum = 2;
							}
							if (command[1] == 0xFF)
							{
								originalPos = ReadLE16(&romData[seqPos + 2]);
								if (hasSub == 1 && originalPos == firstSub)
								{
									seqEnd = 1;
								}
								for (k = 0; k < 100; k++)
								{
									if (repeatPts[k][0] == seqPos)
									{
										repeatPts[k][1]++;
										break;
									}
								}
								if (k == 100)
								{
									repeatPts[curReptPt][0] = seqPos;
									curReptPt++;
								}
								else
								{
									repPos = ReadLE16(&romData[seqPos + ((repeatPts[k][1] - 1) * 2) + 2]);

									seqPos = ptrs[curTrack] - bankAmt + (repPos * 2);

								}
							}
							else if (command[1] == 0xFE)
							{
								if (firstSub == 0)
								{
									firstSub = ReadLE16(&romData[seqPos + (2 * sub)]);
								}
								seqPos = ptrs[curTrack] - bankAmt + (ReadLE16(&romData[seqPos + (2 * sub)]) * 2);
							}
							else if (command[1] == 0xF0)
							{
								if (format == 6)
								{
									repeat1Times = -1;
									repeat2Times = -1;
									repeat3Times = -1;
								}
								seqPos += 2;
							}
							else if (command[1] == 0x00)
							{
								repeat1Times = -1;
								repeat0Times = -1;

								for (k = 0; k < 100; k++)
								{
									repeatPts[k][0] = 0;
									repeatPts[k][1] = 0;
								}
								seqPos += 2;
							}
							else
							{
								seqPos += 2;
							}
						}

						else
						{
							seqPos += 2;
						}

					}

					/*Jump to position*/
					else if (command[0] == 0xAC)
					{
						jumpTimes = command[1];
						jumpPos = ReadLE16(&romData[seqPos + 2]) + ptrs[curTrack] - bankAmt;
						jumpRet = seqPos + 4;
						seqPos = jumpPos;
					}

					/*Return from jump*/
					else if (command[0] == 0xAD)
					{
						if (jumpTimes > 1)
						{
							seqPos = jumpPos;
							jumpTimes--;
						}
						else
						{
							seqPos = jumpRet;
						}
					}

					/*Set tuning*/
					else if (command[0] == 0xAE)
					{
						seqPos += 2;
					}

					/*Set channel speed*/
					else if (command[0] == 0xAF)
					{
						highNibble = (command[1] & 15);
						if (format == 0 || format == 1 || format == 7 || format == 89)
						{
							if (highNibble < 0x10)
							{
								chanSpeed = (highNibble + 1) * speedVal;
							}
							else
							{
								chanSpeed = speedVal;
							}
						}
						else if (format == 2 || format == 3 || format == 6 || format == 11)
						{
							chanSpeed = speedVal + (highNibble * 0.5);
						}

						else if (format == 4)
						{
							tempoVal = command[1] * 2;
							if (tempoVal != tempo && curTrack == 0)
							{
								tempo = tempoVal;
								ctrlMidPos++;
								valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
								ctrlDelay = 0;
								ctrlMidPos += valSize;
								WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
								ctrlMidPos += 3;
								WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
								ctrlMidPos += 2;
							}
						}

						else
						{
							if (highNibble < 4)
							{
								chanSpeed = 6;
							}
							else
							{
								chanSpeed = highNibble + 3;
							}
						}

						seqPos += 2;
					}

					/*Go to loop*/
					else if (command[0] == 0xB0)
					{
						if (format == 0 || format == 1 || format == 2 || format == 7 || format == 10 || format == 89)
						{
							seqEnd = 1;
						}
						else
						{
							if (command[1] >= 0xF0)
							{
								seqEnd = 1;
							}
							else
							{
								seqPos += 2;
							}
						}


					}

					/*Repeat section*/
					else if (command[0] > 0xB0 && command[0] < 0xC0)
					{
						if (format == 0 || format == 1 || format == 2 || format == 7 || format == 10 || format == 89)
						{
							if (command[1] == 0 || (command[1] == 0xFE && format == 0))
							{
								if (format == 0 && command[1] == 0)
								{
									seqEnd = 1;
								}
								if (repeat1Times == -1)
								{
									repeat1Times = command[0] - 0xB0;
									repeat1Pos = repeatPt;
								}
								else if (repeat1Times > 0)
								{
									seqPos = repeat1Pos;
									repeat1Times--;
								}
								else
								{
									seqPos += 2;
									repeat1Times = -1;
								}

							}
							else
							{
								if (repeat2Times == -1)
								{
									repeat2Times = command[0] - 0xB0;
									repeat2Pos = ptrs[curTrack] - bankAmt + (command[1] * 2);
								}
								else if (repeat2Times > 0)
								{
									seqPos = repeat2Pos;
									repeat2Times--;
								}
								else
								{
									seqPos += 2;
									repeat2Times = -1;
								}
							}
						}

						else if (format == 3 || format == 4 || format == 5 || format == 6 || format == 11)
						{
							if ((command[0] - 0xB0) == 0x00)
							{
								seqEnd = 1;
							}
							else
							{
								if (command[1] == 0xF1)
								{
									if (repeat1Times == -1)
									{
										repeat1Times = command[0] - 0xB0;
									}
									else if (repeat1Times > 0)
									{
										seqPos = repeatList[1];
										repeat1Times--;
									}
									else
									{
										seqPos += 2;
										repeat1Times = -1;
									}
								}
								else if (command[1] == 0xF2)
								{
									if (repeat2Times == -1)
									{
										repeat2Times = command[0] - 0xB0;
									}
									else if (repeat2Times > 0)
									{
										seqPos = repeatList[command[1] - 0xF0];
										repeat2Times--;
									}
									else
									{
										seqPos += 2;
										repeat2Times = -1;
									}
								}
								else if (command[1] == 0xF3)
								{
									if (repeat3Times == -1)
									{
										repeat3Times = command[0] - 0xB0;
									}
									else if (repeat3Times > 0)
									{
										seqPos = repeatList[command[1] - 0xF0];
										repeat3Times--;
									}
									else
									{
										seqPos += 2;
										repeat3Times = -1;
									}
								}
								else if (command[1] == 0xF0)
								{
									if (repeat0Times == -1)
									{
										repeat0Times = command[0] - 0xB0;
									}
									else if (repeat0Times > 0)
									{
										seqPos = repeatList[command[1] - 0xF0];
										repeat0Times--;
									}
									else
									{
										seqPos += 2;
										repeat0Times = -1;
									}
								}
								else if (command[1] == 0xFC)
								{
									if (repeat1Times == -1)
									{
										repeat1Times = command[0] - 0xB0;
									}
									else if (repeat1Times > 0)
									{
										repeat1Pos = ReadLE16(&romData[seqPos + 2]);
										seqPos = ptrs[curTrack] - bankAmt + (repeat1Pos * 2);
										repeat1Times--;
									}
									else
									{
										seqPos += 4;
										repeat1Times = -1;
									}
								}

								else if (command[1] == 0x00)
								{
									if (repeat0Times == -1)
									{
										repeat0Times = command[0] - 0xB0;
										repeat0Pos = repeatList[14];
									}
									else if (repeat0Times > 0)
									{
										seqPos = repeat0Pos;
										repeat0Times--;
									}
									else
									{
										seqPos += 2;
										repeat0Times = -1;
									}

								}
								else
								{
									seqEnd = 1;
								}


							}
						}

					}

					/*Noise/effect settings*/
					else if (command[0] >= 0xC0 && command[0] < 0xFD)
					{
						seqPos += 2;
					}

					/*Start of repeat section*/
					else if (command[0] == 0xFD)
					{
						if (format == 0 || format == 1 || format == 2 || format == 7 || format == 10 || format == 89)
						{
							repeatPt = seqPos;
						}
						else if (format == 3 || format == 4 || format == 5 || format == 6 || format == 11)
						{
							repeatList[command[1] - 0xF0] = seqPos + 2;
						}

						seqPos += 2;
					}

					/*Skip?*/
					else if (command[0] == 0xFE)
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