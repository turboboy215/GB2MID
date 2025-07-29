/*Kemco*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "KEMCO.H"

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
int drvVers;
int ptrOverride;
int sysMode;
char string1[4];
int usePALTempo;
char* argv5;
int multiBanks;
int curBank;

char folderName[100];

const unsigned char KemcoMagicBytesA[5] = { 0xD6, 0x01, 0xCB, 0x27, 0x21 };
const unsigned char KemcoMagicBytesB[6] = { 0xCB, 0x27, 0x4F, 0x06, 0x00, 0x21 };
const unsigned char KemcoMagicBytesC[5] = { 0x87, 0x4F, 0x06, 0x00, 0x21 };
const unsigned char KemcoNoteLens[] = { 0xC0, 0x00, 0x60, 0x00, 0x30, 0x00, 0x18, 0x00, 0x0C, 0x00,
0x06, 0x00, 0x03, 0x00, 0x20, 0x01, 0x90, 0x00, 0x48, 0x00, 0x24, 0x00, 0x12, 0x00, 0x09, 0x00,
0x80, 0x00, 0x40, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04, 0x00 };

unsigned char* romData;
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
void Kemcosong2mid(int songNum, long ptr);
void Kemcosong2midNES(int songNum, long ptr);

void KemcoProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	sysMode = 2;
	curInst = 0;
	curVol = 100;
	drvVers = 1;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	/*Check for NES ROM header*/
	fgets(string1, 4, rom);
	if (!memcmp(string1, "NES", 1))
	{
		fseek(rom, (((bank - 1) * bankSize)) + 0x10, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
		bankAmt = 0x8000;
		sysMode = 1;
	}
	else
	{
		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);
		sysMode = 2;
	}

	if (parameters[1][0] != 0)
	{
		ptrOverride = 1;
		tableOffset = strtol(parameters[1], NULL, 16);
		drvVers = strtol(parameters[0], NULL, 16);
		foundTable = 1;
	}
	else if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
	}


	if (drvVers < 1 && drvVers > 2)
	{
		printf("ERROR: Invalid version number!\n");
		exit(2);
	}

	if (ptrOverride != 1)
	{
		/*Try to search the bank for song table loader - Method 1*/
		for (i = 0; i < bankSize * 2; i++)
		{
			if ((!memcmp(&romData[i], KemcoMagicBytesA, 5)) && foundTable != 1)
			{
				tablePtrLoc = i + 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}

		/*Method 2*/
		if (drvVers < 2)
		{
			for (i = 0; i < bankSize * 2; i++)
			{
				if ((!memcmp(&romData[i], KemcoMagicBytesB, 6)) && foundTable != 1)
				{
					tablePtrLoc = i + 6;
					printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
					tableOffset = ReadLE16(&romData[tablePtrLoc]);
					printf("Song table starts at 0x%04x...\n", tableOffset);
					foundTable = 1;
				}
			}
		}
		else if (drvVers >= 2)
		{
			for (i = bankSize; i < bankSize * 2; i++)
			{
				if ((!memcmp(&romData[i], KemcoMagicBytesB, 6)) && foundTable != 1)
				{
					tablePtrLoc = i + 6;
					printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
					tableOffset = ReadLE16(&romData[tablePtrLoc]);
					printf("Song table starts at 0x%04x...\n", tableOffset);
					foundTable = 1;
				}
			}

			/*Method 3*/
			if (drvVers == 3)
			{
				for (i = bankAmt; i < bankSize * 2; i++)
				{
					if ((!memcmp(&romData[i], KemcoMagicBytesC, 5)) && foundTable != 1)
					{
						tablePtrLoc = i + 5;
						printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
						tableOffset = ReadLE16(&romData[tablePtrLoc]);
						printf("Song table starts at 0x%04x...\n", tableOffset);
						foundTable = 1;
						drvVers = 3;
					}
				}
			}
		}

	}


	if (foundTable == 1)
	{
		if (sysMode == 1)
		{
			i = tableOffset - bankAmt;
			songNum = 1;

			while (ReadLE16(&romData[i]) > bankAmt && ReadLE16(&romData[i]) < 0xC000)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Kemcosong2midNES(songNum, songPtr);
				i += 2;
				songNum++;
			}
		}
		else
		{
			i = tableOffset;
			songNum = 1;

			/*Fix for Snoopy 2*/
			firstPtr = ReadLE16(&romData[i]);
			if (drvVers == 2 && ptrOverride == 1 && tableOffset == 0x5F00)
			{
				while (i < firstPtr)
				{
					songPtr = ReadLE16(&romData[i]);
					if (songPtr != 0x0000)
					{
						printf("Song %i: 0x%04X\n", songNum, songPtr);
						Kemcosong2mid(songNum, songPtr);
					}
					else if (songPtr == 0x0000)
					{
						printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
					}
					i += 2;
					songNum++;
				}
			}
			else
			{
				while (ReadLE16(&romData[i]) > bankAmt && ReadLE16(&romData[i]) < 0x8000)
				{
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Kemcosong2mid(songNum, songPtr);
					i += 2;
					songNum++;
				}
			}
		}

		free(romData);
	}
	else
	{
		free(romData);
		printf("ERROR: Magic bytes not found!\n");
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void Kemcosong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
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
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	int inMacro = 0;
	int repeat1 = -1;
	long repeat1Pos = 0;
	int repeat2 = -1;
	long repeat2Pos = 0;
	unsigned char command[4];
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	long startPos = 0;
	int chanSpeed = 0;
	int connect = 0;
	int connectAmt = 0;
	int tempLen = 0;
	int isLoop = 0;
	int repeat1Times = 0;
	int repeat2Times = 0;
	int transpose = 0;
	int newRepeat1 = 0;
	long newRepeat1Pos = 0;
	int newRepeat2 = 0;
	long newRepeat2Pos = 0;
	int skip = 0;


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

	for (curTrack = 0; curTrack < 4; curTrack++)
	{
		seqPtrs[curTrack] = 0;
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

		if (drvVers == 1)
		{
			tempo = romData[ptr + 9] * 7.5;
		}

		else if (drvVers >= 2)
		{
			tempo = romData[ptr + 1] * 7.5;
		}


		/*Avoid crash due to "divide by zero"*/
		if (tempo < 2)
		{
			tempo = 150;
		}

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

		romPos = ptr + 1;

		if (drvVers >= 2)
		{
			romPos++;
		}
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
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
			inMacro = 0;
			repeat1 = -1;
			repeat2 = -1;
			connect = 0;
			connectAmt = 0;
			repeat1Times = -1;
			repeat2Times = -1;
			newRepeat1 = 0;
			newRepeat2 = 0;
			transpose = 0;
			skip = 0;

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

			if (seqPtrs[curTrack] >= bankAmt && seqPtrs[curTrack] < 0x8000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];

				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Set channel volumes?*/
					if (command[0] <= 0x05)
					{
						seqPos++;

						if (command[0] == 0x01 && drvVers >= 2)
						{
							seqPos++;
						}
					}

					else if (command[0] == 0x06 && drvVers >= 2)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x07 && drvVers >= 2)
					{
						if (drvVers == 2)
						{
							seqPos++;
						}
						else
						{
							if (newRepeat1 == -1)
							{
								newRepeat1 = 2;
								newRepeat1Pos = seqPos + 1;
								seqPos++;
							}
							else
							{
								newRepeat2 = 2;
								newRepeat2Pos = seqPos + 1;
								seqPos++;
							}
						}

					}

					/*Transpose*/
					else if (command[0] == 0x08 && drvVers >= 2)
					{
						if (drvVers == 2)
						{
							transpose = (signed char)command[1];
							seqPos += 2;
						}
						else if (drvVers == 3)
						{
							if (newRepeat2 == -1)
							{
								if (newRepeat1 == 2)
								{
									seqPos++;
								}
								else
								{
									seqPos++;
									while (romData[seqPos] != 0x09)
									{
										seqPos++;
									}
								}
							}
							else if (newRepeat2 > -1)
							{
								if (newRepeat2 == 2)
								{
									seqPos++;
								}
								else
								{
									seqPos++;
									while (romData[seqPos] != 0x09)
									{
										seqPos++;
									}
								}
							}
							else
							{
								seqEnd = 1;
							}
						}
						else
						{
							seqPos += 2;
						}
					}

					else if (command[0] == 0x09 && drvVers == 3)
					{
						if (newRepeat2 == -1)
						{
							if (newRepeat1 > 1)
							{
								seqPos = newRepeat1Pos;
								newRepeat1--;
							}
							else if (newRepeat1 <= 1)
							{
								newRepeat1 = -1;
								seqPos++;
							}
						}
						else if (newRepeat2 > -1)
						{
							if (newRepeat2 > 1)
							{
								seqPos = newRepeat2Pos;
								newRepeat2--;
							}
							else if (newRepeat2 <= 1)
							{
								newRepeat2 = -1;
								seqPos++;
							}
						}
					}

					/*Portamento?*/
					else if (command[0] == 0x0A && drvVers >= 2)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0B && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0C && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0D && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0E && drvVers == 3)
					{
						seqPos++;
					}

					else if (command[0] == 0x0F && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x7D && drvVers == 3)
					{
						curDelay += curNoteLen;

						if (connect == 1)
						{
							curDelay += connectAmt;
							connect = 0;
							connectAmt = 0;
						}
						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x05 && command[0] <= 0x7D)
					{
						curNote = command[0] - 4 + 12;
						curNote += transpose;

						if (curTrack == 2 && curNote >= 12)
						{
							curNote -= 12;
						}

						if (connect == 1)
						{
							curNoteLen += connectAmt;
							connect = 0;
							connectAmt = 0;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curNoteLen = chanSpeed * 2;
						curDelay = 0;
						seqPos++;
					}

					/*Connect note length*/
					else if (command[0] == 0x7E)
					{
						if (drvVers < 2)
						{
							connect = 1;
							connectAmt += curNoteLen;
						}



						seqPos++;
					}

					/*Hold note*/
					else if (command[0] == 0x7F)
					{
						curDelay += curNoteLen;

						if (connect == 1)
						{
							curDelay += connectAmt;
							connect = 0;
							connectAmt = 0;
						}
						seqPos++;
					}

					/*Set note size*/
					else if (command[0] >= 0x80 && command[0] <= 0xBF)
					{
						chanSpeed = command[0] - 0x80;
						chanSpeed *= 2;
						chanSpeed = KemcoNoteLens[chanSpeed];
						curNoteLen = chanSpeed * 2;
						seqPos++;
					}

					/*Repeat point*/
					else if (command[0] >= 0xC0 && command[0] <= 0xC9 && drvVers < 2)
					{
						if (command[0] == 0xC0)
						{
							seqEnd = 1;
						}
						else
						{
							if (repeat1 == -1)
							{
								repeat1 = command[0] - 0xC0;
								repeat1Pos = seqPos + 1;
								repeat1Times = 1;
								seqPos++;
							}
							else
							{
								repeat2 = command[0] - 0xC0;
								repeat2Pos = seqPos + 1;
								repeat2Times = 1;
								seqPos++;
							}
						}

					}

					/*Repeat point (later version)*/
					else if (command[0] >= 0xC0 && command[0] <= 0xC9 && drvVers >= 2)
					{
						if (command[0] == 0xC0)
						{
							seqEnd = 1;
						}
						else
						{
							if (repeat1 == -1)
							{
								repeat1 = command[0] - 0xC0;
								repeat1Pos = seqPos + 1;
								repeat1Times = 1;
								seqPos++;
							}
							else
							{
								repeat2 = command[0] - 0xC0;
								repeat2Pos = seqPos + 1;
								repeat2Times = 1;
								seqPos++;
							}
						}

					}

					/*Song loop point*/
					else if (command[0] == 0xCA && drvVers < 2)
					{
						isLoop = 1;
						seqPos++;
					}

					/*Song loop point (later version)*/
					else if ((command[0] == 0xCE || command[0] == 0xCA) && drvVers >= 2)
					{
						isLoop = 1;
						seqPos++;
					}

					/*Go to repeat point/loop point*/
					else if (command[0] == 0xCF)
					{
						if (drvVers != 4)
						{
							if (repeat2 == -1)
							{
								if (repeat1 > 1)
								{
									seqPos = repeat1Pos;
									repeat1--;
									repeat1Times++;
								}
								else if (repeat1 <= 1)
								{
									repeat1 = -1;
									repeat1Times = 0;
									seqPos++;
								}
							}
							else if (repeat2 > -1)
							{
								if (repeat2 > 1)
								{
									seqPos = repeat2Pos;
									repeat2--;
									repeat2Times++;
								}
								else if (repeat2 <= 1)
								{
									repeat2 = -1;
									repeat2Times = 0;
									seqPos++;
								}
							}
							else
							{
								seqEnd = 1;
							}
						}
						else if (drvVers == 3)
						{
							seqEnd = 1;
						}

					}

					/*Only play segment if on repeat time*/
					else if (command[0] >= 0xD0 && command[0] <= 0xDF)
					{
						tempByte = command[0] - 0xD0;
						if (repeat2 == -1)
						{
							if (repeat1Times == tempByte)
							{
								seqPos++;
							}
							else
							{
								if (drvVers < 2)
								{
									while (romData[seqPos] != 0xCF)
									{
										seqPos++;
									}
									seqPos++;
								}
								else if (drvVers >= 2)
								{
									seqPos++;
									skip = 1;
									while (skip == 1)
									{
										if (romData[seqPos] == 0xCF)
										{
											skip = 0;
										}
										else if (romData[seqPos] >= 0xD0 && romData[seqPos] <= 0xDF)
										{
											skip = 0;
										}
										if (skip == 1)
										{
											seqPos++;
										}

									}
								}


							}
						}
						else if (repeat2 > -1)
						{
							if (repeat2Times == tempByte)
							{
								seqPos++;
							}
							else
							{
								if (drvVers < 2)
								{
									while (romData[seqPos] != 0xCF)
									{
										seqPos++;
									}
									seqPos++;
								}
								else if (drvVers >= 2)
								{
									seqPos++;
									while (romData[seqPos] < 0xCF)
									{
										seqPos++;
									}
								}

							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Set instrument*/
					else if (command[0] >= 0xE0 && command[0] <= 0xEF)
					{
						seqPos += 3;
						if (drvVers >= 2)
						{
							curInst = command[1];
							if (curInst >= 0x80)
							{
								curInst = 0;
							}
							firstNote = 1;
							seqPos--;
						}
					}

					/*End of channel*/
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

/*Convert the song data to MIDI (NES version)*/
void Kemcosong2midNES(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
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
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	int inMacro = 0;
	int repeat1 = -1;
	long repeat1Pos = 0;
	int repeat2 = -1;
	long repeat2Pos = 0;
	unsigned char command[4];
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	long startPos = 0;
	int chanSpeed = 0;
	int connect = 0;
	int connectAmt = 0;
	int tempLen = 0;
	int isLoop = 0;
	int repeat1Times = 0;
	int repeat2Times = 0;
	int transpose = 0;
	int newRepeat1 = 0;
	long newRepeat1Pos = 0;
	int newRepeat2 = 0;
	long newRepeat2Pos = 0;
	int skip = 0;


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

	for (curTrack = 0; curTrack < 4; curTrack++)
	{
		seqPtrs[curTrack] = 0;
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

		if (drvVers == 1)
		{
			tempo = romData[ptr + 11 - bankAmt] * 7.5;

			if (usePALTempo == 1)
			{
				tempo *= 0.83;
			}
		}

		else if (drvVers >= 2)
		{
			tempo = romData[ptr + 1 - bankAmt] * 7.5;

			if (usePALTempo == 1)
			{
				tempo *= 0.83;
			}
		}


		/*Avoid crash due to "divide by zero"*/
		if (tempo < 2)
		{
			tempo = 150;
		}

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

		romPos = ptr + 1 - bankAmt;

		if (drvVers >= 2)
		{
			romPos++;
		}
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
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
			inMacro = 0;
			repeat1 = -1;
			repeat2 = -1;
			connect = 0;
			connectAmt = 0;
			repeat1Times = -1;
			repeat2Times = -1;
			newRepeat1 = 0;
			newRepeat2 = 0;
			transpose = 0;
			skip = 0;

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

			if (seqPtrs[curTrack] >= bankAmt && seqPtrs[curTrack] < 0xC000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack] - bankAmt;

				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					/*Set channel volumes?*/
					if (command[0] <= 0x05)
					{
						seqPos++;

						if (command[0] == 0x01 && drvVers >= 2)
						{
							seqPos++;
						}
					}

					else if (command[0] == 0x06 && drvVers >= 2)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x07 && drvVers >= 2)
					{
						if (drvVers == 2)
						{
							seqPos++;
						}
						else
						{
							if (newRepeat1 == -1)
							{
								newRepeat1 = 2;
								newRepeat1Pos = seqPos + 1;
								seqPos++;
							}
							else
							{
								newRepeat2 = 2;
								newRepeat2Pos = seqPos + 1;
								seqPos++;
							}
						}

					}

					/*Transpose*/
					else if (command[0] == 0x08 && drvVers >= 2)
					{
						if (drvVers == 2)
						{
							transpose = (signed char)command[1];
							seqPos += 2;
						}
						else if (drvVers == 3)
						{
							if (newRepeat2 == -1)
							{
								if (newRepeat1 == 2)
								{
									seqPos++;
								}
								else
								{
									seqPos++;
									while (romData[seqPos] != 0x09)
									{
										seqPos++;
									}
								}
							}
							else if (newRepeat2 > -1)
							{
								if (newRepeat2 == 2)
								{
									seqPos++;
								}
								else
								{
									seqPos++;
									while (romData[seqPos] != 0x09)
									{
										seqPos++;
									}
								}
							}
							else
							{
								seqEnd = 1;
							}
						}
						else
						{
							seqPos += 2;
						}
					}

					else if (command[0] == 0x09 && drvVers == 3)
					{
						if (newRepeat2 == -1)
						{
							if (newRepeat1 > 1)
							{
								seqPos = newRepeat1Pos;
								newRepeat1--;
							}
							else if (newRepeat1 <= 1)
							{
								newRepeat1 = -1;
								seqPos++;
							}
						}
						else if (newRepeat2 > -1)
						{
							if (newRepeat2 > 1)
							{
								seqPos = newRepeat2Pos;
								newRepeat2--;
							}
							else if (newRepeat2 <= 1)
							{
								newRepeat2 = -1;
								seqPos++;
							}
						}
					}

					/*Portamento?*/
					else if (command[0] == 0x0A && drvVers >= 2)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0B && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0C && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0D && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x0E && drvVers == 3)
					{
						seqPos++;
					}

					else if (command[0] == 0x0F && drvVers == 3)
					{
						seqPos += 2;
					}

					else if (command[0] == 0x7D && drvVers == 3)
					{
						curDelay += curNoteLen;

						if (connect == 1)
						{
							curDelay += connectAmt;
							connect = 0;
							connectAmt = 0;
						}
						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x05 && command[0] <= 0x7D)
					{
						curNote = command[0] - 4 + 12;
						curNote += transpose;

						if (curTrack == 2 && curNote >= 12)
						{
							curNote -= 12;
						}

						if (connect == 1)
						{
							curNoteLen += connectAmt;
							connect = 0;
							connectAmt = 0;
						}
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curNoteLen = chanSpeed * 2;
						curDelay = 0;
						seqPos++;
					}

					/*Connect note length*/
					else if (command[0] == 0x7E)
					{
						if (drvVers < 2)
						{
							connect = 1;
							connectAmt += curNoteLen;
						}



						seqPos++;
					}

					/*Hold note*/
					else if (command[0] == 0x7F)
					{
						curDelay += curNoteLen;

						if (connect == 1)
						{
							curDelay += connectAmt;
							connect = 0;
							connectAmt = 0;
						}
						seqPos++;
					}

					/*Set note size*/
					else if (command[0] >= 0x80 && command[0] <= 0xBF)
					{
						chanSpeed = command[0] - 0x80;
						chanSpeed *= 2;
						chanSpeed = KemcoNoteLens[chanSpeed];
						curNoteLen = chanSpeed * 2;
						seqPos++;
					}

					/*Repeat point*/
					else if (command[0] >= 0xC0 && command[0] <= 0xC9 && drvVers < 2)
					{
						if (command[0] == 0xC0)
						{
							seqEnd = 1;
						}
						else
						{
							if (repeat1 == -1)
							{
								repeat1 = command[0] - 0xC0;
								repeat1Pos = seqPos + 1;
								repeat1Times = 1;
								seqPos++;
							}
							else
							{
								repeat2 = command[0] - 0xC0;
								repeat2Pos = seqPos + 1;
								repeat2Times = 1;
								seqPos++;
							}
						}

					}

					/*Repeat point (later version)*/
					else if (command[0] >= 0xC0 && command[0] <= 0xC9 && drvVers >= 2)
					{
						if (command[0] == 0xC0)
						{
							seqEnd = 1;
						}
						else
						{
							if (repeat1 == -1)
							{
								repeat1 = command[0] - 0xC0;
								repeat1Pos = seqPos + 1;
								repeat1Times = 1;
								seqPos++;
							}
							else
							{
								repeat2 = command[0] - 0xC0;
								repeat2Pos = seqPos + 1;
								repeat2Times = 1;
								seqPos++;
							}
						}

					}

					/*Song loop point*/
					else if (command[0] == 0xCA && drvVers < 2)
					{
						isLoop = 1;
						seqPos++;
					}

					/*Song loop point (later version)*/
					else if ((command[0] == 0xCE || command[0] == 0xCA) && drvVers >= 2)
					{
						isLoop = 1;
						seqPos++;
					}

					/*Go to repeat point/loop point*/
					else if (command[0] == 0xCF)
					{
						if (drvVers != 4)
						{
							if (repeat2 == -1)
							{
								if (repeat1 > 1)
								{
									seqPos = repeat1Pos;
									repeat1--;
									repeat1Times++;
								}
								else if (repeat1 <= 1)
								{
									repeat1 = -1;
									repeat1Times = 0;
									seqPos++;
								}
							}
							else if (repeat2 > -1)
							{
								if (repeat2 > 1)
								{
									seqPos = repeat2Pos;
									repeat2--;
									repeat2Times++;
								}
								else if (repeat2 <= 1)
								{
									repeat2 = -1;
									repeat2Times = 0;
									seqPos++;
								}
							}
							else
							{
								seqEnd = 1;
							}
						}
						else if (drvVers == 3)
						{
							seqEnd = 1;
						}

					}

					/*Only play segment if on repeat time*/
					else if (command[0] >= 0xD0 && command[0] <= 0xDF)
					{
						tempByte = command[0] - 0xD0;
						if (repeat2 == -1)
						{
							if (repeat1Times == tempByte)
							{
								seqPos++;
							}
							else
							{
								if (drvVers < 2)
								{
									while (romData[seqPos] != 0xCF)
									{
										seqPos++;
									}
									seqPos++;
								}
								else if (drvVers >= 2)
								{
									seqPos++;
									skip = 1;
									while (skip == 1)
									{
										if (romData[seqPos] == 0xCF)
										{
											skip = 0;
										}
										else if (romData[seqPos] >= 0xD0 && romData[seqPos] <= 0xDF)
										{
											skip = 0;
										}
										if (skip == 1)
										{
											seqPos++;
										}

									}
								}


							}
						}
						else if (repeat2 > -1)
						{
							if (repeat2Times == tempByte)
							{
								seqPos++;
							}
							else
							{
								if (drvVers < 2)
								{
									while (romData[seqPos] != 0xCF)
									{
										seqPos++;
									}
									seqPos++;
								}
								else if (drvVers >= 2)
								{
									seqPos++;
									while (romData[seqPos] < 0xCF)
									{
										seqPos++;
									}
								}

							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Set instrument*/
					else if (command[0] >= 0xE0 && command[0] <= 0xEF)
					{
						seqPos += 3;
						if (drvVers >= 2)
						{
							curInst = command[1];
							if (curInst >= 0x80)
							{
								curInst = 0;
							}
							firstNote = 1;
							seqPos--;
						}
					}

					/*End of channel*/
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