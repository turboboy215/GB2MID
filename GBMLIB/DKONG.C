/*Taisuke Araki (Donkey Kong/Wave Race/etc.)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DKONG.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
int songBank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long stepPtr;
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int songBank;
int drvVers;
long masterBank;
int multiBanks;
int curBank;

char folderName[100];

const char DKongMagicBytesA[6] = { 0xCD, 0x2A, 0x41, 0x21, 0x8B, 0xD8 };
const char DKongMagicBytesB[6] = { 0xCD, 0x73, 0x41, 0x21, 0xC8, 0xD8 };
const char DKongMagicBytesC[6] = { 0xCB, 0x27, 0x5F, 0x16, 0x00, 0x19 };

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
void DKongsong2mid(int songNum, long ptr);

void DKongProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	drvVers = 3;
	masterBank = bank;

	if (masterBank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Try to search the bank for song table loader - Method 1: Wave Race*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], DKongMagicBytesA, 6)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			drvVers = 1;
		}
	}

	/*Try to search the bank for song table loader - Method 2: Top Ranking Tennis*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], DKongMagicBytesB, 6)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			drvVers = 2;
		}
	}

	/*Try to search the bank for song table loader - Method 3: Donkey Kong*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], DKongMagicBytesC, 6)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;

			if (masterBank <= 4)
			{
				drvVers = 3;
			}
			else
			{
				drvVers = 4;
			}

		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;

		if (drvVers < 4)
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000 && ReadLE16(&romData[i + 2]) != 0x0000)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				DKongsong2mid(songNum, songPtr);
				i += 2;
				songNum++;
			}
		}
		else if (drvVers >= 4)
		{

			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000 && ReadLE16(&romData[i + 2]) != 0x0000)
			{
				songPtr = ReadLE16(&romData[i]);
				if (masterBank == 0x3E)
				{
					if (songNum < 0x17)
					{
						songBank = masterBank + 1;
					}
					else if (songNum >= 0x17 && songNum < 0x32)
					{
						songBank = masterBank + 2;
					}
					else
					{
						songBank = masterBank + 3;
					}
				}
				else if (masterBank == 0x08)
				{
					if (songNum < 0x19)
					{
						songBank = masterBank + 1;
					}
					else if (songNum >= 0x19 && songNum < 0x2A)
					{
						songBank = masterBank + 2;
					}
					else
					{
						songBank = masterBank + 3;
					}
				}
				else if (masterBank == 0x09)
				{
					if (songNum < 0x1C)
					{
						songBank = masterBank + 1;
					}
					else if (songNum >= 0x1C && songNum < 0x2A)
					{
						songBank = masterBank + 2;
					}
					else
					{
						songBank = masterBank + 3;
					}
				}

				else if (masterBank == 0x0F)
				{

					if (songNum < 0x17)
					{
						songBank = 0x0E;
					}
					else if (songNum >= 0x17 && songNum < 0x26)
					{
						songBank = 0x0D;
					}
					else if (songNum >= 0x26 && songNum < 0x39)
					{
						songBank = 0x0C;
					}
					else
					{
						songBank = 0x0D;
					}
					if (ReadLE16(&romData[i]) == 0x4B3E)
					{
						i += 6;
					}
				}

				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize);
				fread(exRomData, 1, bankSize, rom);

				printf("Song %i: 0x%04X\n", songNum, songPtr);
				DKongsong2mid(songNum, songPtr);
				i += 2;
				songNum++;
			}

		}
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		free(romData);
		fclose(rom);
		exit(2);
	}
}

/*Convert the song data to MIDI*/
void DKongsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	long romPos = 0;
	long seqPos = 0;
	long curSeq = 0;
	long stepPtr = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;

	if (drvVers == 1)
	{
		tempo = 180;
	}

	if (drvVers >= 4 && masterBank != 0x0F)
	{
		tempo = exRomData[ptr - bankAmt + 2] * 1.2;
	}
	int k = 0;
	int patEnd = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned char chanSpeed = 0;
	int repeat1 = 0;
	long repeat1Pt = 0;
	unsigned char command[6];
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

	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int hasJumped = 0;

	long tempPtr = 0;


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

		if (tempo < 2)
		{
			tempo = 150;
		}

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

		/*Get each channel's pattern info*/
		romPos = ptr - bankAmt;

		if (drvVers == 1 || drvVers == 2)
		{
			romPos++;
		}

		if (drvVers <= 3)
		{
			stepPtr = ReadLE16(&romData[romPos]);
			romPos += 2;

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				activeChan[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}
		}
		else if (drvVers > 3)
		{
			stepPtr = ReadLE16(&exRomData[romPos]);
			romPos += 3;

			if (masterBank == 0x0F)
			{
				romPos--;
			}

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				activeChan[curTrack] = ReadLE16(&exRomData[romPos]);
				romPos += 2;
			}
		}



		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			patEnd = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;

			repeat1 = -1;
			hasJumped = 0;

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

			if (activeChan[curTrack] >= bankAmt && activeChan[curTrack] < 0x8000)
			{
				patEnd = 0;

				romPos = activeChan[curTrack] - bankAmt;
				while (patEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
				{

					if (drvVers < 4)
					{
						tempPtr = ReadLE16(&romData[romPos]);
					}
					else
					{
						tempPtr = ReadLE16(&exRomData[romPos]);
					}

					if (tempPtr != 0x0000 && tempPtr != 0xFFFF)
					{
						if (drvVers < 4)
						{
							seqPos = ReadLE16(&romData[romPos]) - bankAmt;
						}
						else
						{
							seqPos = ReadLE16(&exRomData[romPos]) - bankAmt;
						}

						seqEnd = 0;
						holdNote = 0;

						if (drvVers == 1)
						{
							while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
							{
								command[0] = romData[seqPos];
								command[1] = romData[seqPos + 1];
								command[2] = romData[seqPos + 2];
								command[3] = romData[seqPos + 3];
								command[4] = romData[seqPos + 4];
								command[5] = romData[seqPos + 5];


								/*End of phrase*/
								if (command[0] == 0x00)
								{
									seqEnd = 1;
									romPos += 2;
								}

								/*Rest*/
								else if (command[0] == 0x01)
								{
									curDelay += (chanSpeed * 10);
									ctrlDelay += (chanSpeed * 10);
									seqPos++;
								}

								/*Play note*/
								else if (command[0] >= 0x02 && command[0] <= 0x9A)
								{
									curNote = (command[0] / 2) + 23;
									curNoteLen = chanSpeed * 10;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									seqPos++;
								}

								/*Repeat the following section*/
								else if (command[0] == 0x9B)
								{
									repeat1 = command[1];
									repeat1Pt = seqPos + 2;
									seqPos += 2;
								}

								/*End of repeat*/
								else if (command[0] == 0x9C)
								{
									if (repeat1 > 1)
									{
										seqPos = repeat1Pt;
										repeat1--;
									}
									else
									{
										seqPos++;
										repeat1 = -1;
									}
								}

								/*Set panning/waveform info?*/
								else if (command[0] == 0x9D)
								{
									seqPos += 4;
									if (curTrack == 2)
									{
										seqPos += 2;
									}
								}

								/*Unknown command 9E*/
								else if (command[0] == 0x9E)
								{
									seqPos++;
								}

								/*Unknown command 9F*/
								else if (command[0] == 0x9F)
								{
									seqPos++;
								}

								/*Set note size*/
								else if (command[0] >= 0xA0 && command[0] <= 0xAF)
								{
									chanSpeed = romData[stepPtr - bankAmt + (command[0] - 0xA0)];
									seqPos++;
								}

								else if (command[0] == 0xB0)
								{
									seqPos += 4;
									curDelay += (command[1] * 10);
									if (command[2] == 0x41)
									{
										seqPos++;
									}
								}

								/*Set pitch bend pointer*/
								else if (command[0] == 0xB1)
								{
									seqPos += 3;
								}

								/*Play sound effect*/
								else if (command[0] == 0xB3)
								{
									seqPos += 4;
								}

								/*Unknown command*/
								else
								{
									seqPos++;
								}
							}
						}
						else
						{
							while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
							{
								if (seqPos < 0 || seqPos >= 0x8000)
								{
									seqEnd = 1;
								}

								if (drvVers < 4)
								{
									command[0] = romData[seqPos];
									command[1] = romData[seqPos + 1];
									command[2] = romData[seqPos + 2];
									command[3] = romData[seqPos + 3];
									command[4] = romData[seqPos + 4];
									command[5] = romData[seqPos + 5];
								}
								else if (drvVers >= 4)
								{
									if (masterBank == 0x0F)
									{
										if (seqEnd == 0)
										{
											command[0] = exRomData[seqPos];
											command[1] = exRomData[seqPos + 1];
											command[2] = exRomData[seqPos + 2];
											command[3] = exRomData[seqPos + 3];
											command[4] = exRomData[seqPos + 4];
											command[5] = exRomData[seqPos + 5];
										}
									}
									else
									{
										command[0] = exRomData[seqPos];
										command[1] = exRomData[seqPos + 1];
										command[2] = exRomData[seqPos + 2];
										command[3] = exRomData[seqPos + 3];
										command[4] = exRomData[seqPos + 4];
										command[5] = exRomData[seqPos + 5];
									}

								}

								/*End of phrase*/
								if (command[0] == 0x00)
								{
									if (holdNote == 1)
									{
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
										ctrlDelay += curNoteLen;

									}
									seqEnd = 1;
									romPos += 2;
								}

								/*Rest*/
								else if (command[0] == 0x01)
								{
									if (holdNote == 1)
									{
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
										ctrlDelay += curNoteLen;

									}
									curDelay += (chanSpeed * 5);
									ctrlDelay += (chanSpeed * 5);
									seqPos++;
								}

								/*Play note*/
								else if (command[0] >= 0x02 && command[0] <= 0xC9)
								{
									if (holdNote == 1)
									{
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
										ctrlDelay += curNoteLen;

									}
									curNote = (command[0] / 2) + 23;
									curNoteLen = chanSpeed * 5;
									holdNote = 1;
									seqPos++;
								}

								/*Extend note length*/
								else if (command[0] == 0xCA)
								{
									if (holdNote == 1)
									{
										curNoteLen += (chanSpeed * 5);
									}
									else
									{
										curDelay += (chanSpeed * 5);
										ctrlDelay += (chanSpeed * 5);
									}

									seqPos++;
								}

								/*Set channel speed*/
								else if (command[0] >= 0xD0 && command[0] <= 0xEA)
								{
									if (drvVers < 4)
									{
										chanSpeed = romData[stepPtr - bankAmt + (command[0] - 0xD0)];
									}
									else
									{
										chanSpeed = exRomData[stepPtr - bankAmt + (command[0] - 0xD0)];
									}

									seqPos++;
								}

								/*Repeat the following section*/
								else if (command[0] == 0xEB)
								{
									repeat1 = command[1];
									repeat1Pt = seqPos + 2;
									seqPos += 2;
								}

								/*End of repeat*/
								else if (command[0] == 0xEC)
								{
									if (repeat1 > 1)
									{
										seqPos = repeat1Pt;
										repeat1--;
									}
									else
									{
										seqPos++;
										repeat1 = -1;
									}
								}

								/*Set panning/waveform info?*/
								else if (command[0] == 0xED)
								{
									seqPos += 4;
									if (curTrack == 2)
									{
										seqPos += 2;
									}
								}

								/*Unknown command EE*/
								else if (command[0] == 0xEE)
								{
									seqPos += 3;
									if (drvVers > 3)
									{
										seqPos++;
									}
								}

								/*Set instrument parameters*/
								else if (command[0] == 0xF0)
								{
									seqPos += 4;
									curDelay += (command[1] * 5);
									if (command[2] == 0x41)
									{
										seqPos++;
									}
									else if (command[2] == 0x55 && drvVers > 3)
									{
										seqPos += 3;
									}
									else if (command[2] == 0x14 && drvVers > 3)
									{
										seqPos++;
									}
									else if (command[2] == 0x54 && drvVers > 3)
									{
										seqPos += 2;
									}
								}

								/*Set pitch bend pointer*/
								else if (command[0] == 0xF1)
								{
									seqPos += 3;
								}

								/*Disable pitch bend*/
								else if (command[0] == 0xF2)
								{
									seqPos++;
								}

								/*Set vibrato effect*/
								else if (command[0] == 0xF3)
								{
									seqPos += 4;
								}

								/*Delay*/
								else if (command[0] == 0xF4)
								{
									if (holdNote == 1)
									{
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
										ctrlDelay += curNoteLen;

									}
									curDelay += (command[1] * 5);
									ctrlDelay += (command[1] * 5);
									seqPos += 2;
								}

								/*Set volume envelope pointer?*/
								else if (command[0] == 0xF5)
								{
									seqPos += 3;
								}

								/*Set "slow" tempo?*/
								else if (command[0] == 0xF6 && drvVers > 3)
								{
									seqPos++;
								}

								/*Jump to start of pattern*/
								else if (command[0] == 0xFF)
								{
									if (holdNote == 1)
									{
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
										ctrlDelay += curNoteLen;

									}
									if (hasJumped == 0)
									{
										seqPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
										hasJumped = 1;
									}
									else
									{
										seqEnd = 1;
										patEnd = 1;
									}

								}

								/*Unknown command*/
								else
								{
									seqPos++;
								}
							}
						}
					}
					else
					{
						patEnd = 1;
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
		free(exRomData);
		fclose(mid);
	}
}