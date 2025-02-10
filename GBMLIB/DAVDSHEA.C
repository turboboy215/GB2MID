/*David Shea (Coyote Developments)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DAVDSHEA.H"

#define bankSize 16384

FILE* rom, * mid;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtr;
int chanMask;
long bankAmt;
int foundTable;
long firstPtr;
int curInst;
int curVol;
int stopCvt;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;
long midLength;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char DShMagicBytes[7] = { 0x3E, 0x80, 0xE0, 0x23, 0xC9, 0x24, 0x00 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void DShsong2mid(int songNum, long ptr);

void DShProc(int bank)
{
	foundTable = 0;
	firstPtr = 0;
	curInst = 0;
	curVol = 100;
	stopCvt = 0;
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

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], DShMagicBytes, 7)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 7;
			printf("Found song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[i + 7]) + (tablePtrLoc - 2);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;
		firstPtr = ReadLE16(&romData[i]) + tableOffset;
		while ((ReadLE16(&romData[i]) + tableOffset) < 0x8000 && stopCvt == 0)
		{
			if (i < (firstPtr - bankAmt))
			{
				songPtr = ReadLE16(&romData[i]) + tableOffset;
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				DShsong2mid(songNum, songPtr);
				i += 2;
				songNum++;
			}
			else
			{
				stopCvt = 1;
			}

		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		free(romData);
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void DShsong2mid(int songNum, long ptr)
{
	int numChan = 0;
	int chanPtr = 0;
	long romPos = 0;
	long seqPos = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int curTrack = 0;
	int noteSize = 0;
	int restTime = 0;
	int tempo = 0;
	int ticks = 120;
	int noteOn = 0;
	int repeat1Times = 0;
	int repeat1Pos = 0;
	int repeat2Times = 0;
	int repeat2Pos = 0;
	unsigned char command[8];
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int firstNote = 1;
	int timeVal = 0;
	int holdNote = 0;
	int lastNote = 0;

	int valSize = 0;

	int tempByte = 0;
	long tempPos = 0;
	long trackSize = 0;
	long delayPos = 0;
	int sfxMode = 0;

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
		romPos = ptr - bankAmt;
		numChan = ReadLE16(&romData[romPos]);
		romPos += 2;

		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], numChan + 1);
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

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / 150);
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

		for (curTrack == 0; curTrack < numChan; curTrack++)
		{
			firstNote = 1;

			curDelay = 0;
			ctrlDelay = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;

			masterDelay = 0;

			repeat1Times = -1;
			repeat1Pos = -1;
			repeat2Times = -1;
			repeat2Pos = -1;
			delayPos = 0;
			sfxMode = 0;

			/*Add track header*/
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			chanPtr = ReadLE16(&romData[romPos]);

			seqPos = ptr + chanPtr - bankAmt;
			seqEnd = 0;
			noteOn = 1;
			curInst = 0;
			curVol = 100;
			noteSize = 400;
			if (curTrack == 1 && songNum == 1)
			{
				curTrack = 1;
			}

			while (seqEnd == 0 && seqPos < bankAmt)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];
				command[4] = romData[seqPos + 4];
				command[5] = romData[seqPos + 5];
				command[6] = romData[seqPos + 6];
				command[7] = romData[seqPos + 7];

				/*Play note OR delay*/
				if (command[0] < 0x80)
				{
					if (noteOn == 0)
					{
						if (seqPos != delayPos)
						{
							curDelay += command[0] * 3;
						}
						seqPos++;
						noteOn = 1;
					}
					else if (noteOn == 1)
					{
						curNote = command[0] - 12;

						if (curNote >= 0x80)
						{
							curNote = 20;
						}
						if (sfxMode == 0)
						{
							delayPos = seqPos + 1;
						}
						else if (sfxMode == 1)
						{
							noteSize = command[1] * 10;
							delayPos = seqPos + 2;
						}

						if (command[1] < 0x80)
						{
							if (sfxMode == 0)
							{
								curNoteLen = command[1] * 3;
							}
							else if (sfxMode == 1)
							{
								if (command[2] < 0x80)
								{
									curNoteLen = command[2] * 3;
								}
								else
								{
									curNoteLen = (ReadBE16(&romData[seqPos + 2]) - 0x8000) * 3;
								}


								/*
								if (curNoteLen < 2)
								{
									curNoteLen = 520;
								}
								*/
							}

						}
						else
						{
							if (sfxMode == 0)
							{
								curNoteLen = (ReadBE16(&romData[seqPos + 1]) - 0x8000) * 3;
							}
							else if (sfxMode == 1)
							{
								curNoteLen = (ReadBE16(&romData[seqPos + 2]) - 0x8000) * 3;
							}

						}

						if (curNoteLen >= noteSize)
						{
							tempPos = WriteNoteEventGen(midData, midPos, curNote, noteSize, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = curNoteLen - noteSize;
							midPos = tempPos;
						}

						else
						{
							tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							curDelay = 0;
							midPos = tempPos;
						}
						if (sfxMode == 1)
						{
							seqPos++;
						}
						seqPos++;
						noteOn = 0;
					}
				}

				/*Delay (long)*/
				else if (command[0] >= 0x80 && noteOn == 0)
				{
					if (seqPos != delayPos)
					{
						curDelay += ((ReadBE16(&romData[seqPos]) - 0x8000) * 3);
					}
					seqPos += 2;
					noteOn = 1;
				}

				/*Turn off channel*/
				else if (command[0] == 0x80)
				{
					seqEnd = 1;
				}

				/*Turn on channel*/
				else if (command[0] == 0x81)
				{
					noteOn = 0;
					seqPos++;
				}

				/*Set tempo*/
				else if (command[0] == 0x82)
				{
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					tempo = command[1] * 1.66;
					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
					ctrlMidPos += 2;
					seqPos += 2;
					noteOn = 0;
				}

				/*Set envelope bits (1)*/
				else if (command[0] == 0x83)
				{
					noteOn = 0;
					seqPos++;
				}

				/*Set envelope bits (2)*/
				else if (command[0] == 0x84)
				{
					noteOn = 0;
					sfxMode = 1;
					seqPos++;
				}

				/*Set envelope bits (3)*/
				else if (command[0] == 0x85)
				{
					noteOn = 0;
					sfxMode = 0;
					seqPos++;
				}

				/*Set SFX volume (1)?*/
				else if (command[0] == 0x86)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Set SFX volume (2)?*/
				else if (command[0] == 0x87)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Set SFX volume (3)?*/
				else if (command[0] == 0x88)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Set SFX effect?*/
				else if (command[0] == 0x89)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Invalid command*/
				else if (command[0] > 0x89 && command[0] < 0x90)
				{
					noteOn = 0;
					seqPos++;
				}

				/*Set envelope size*/
				else if (command[0] == 0x90)
				{
					noteSize = command[1] * 10;
					noteOn = 0;
					seqPos += 2;
				}

				/*Set pitch bend?*/
				else if (command[0] == 0x91)
				{
					noteOn = 0;
					seqPos += 3;
				}

				/*Set note speed?*/
				else if (command[0] == 0x92)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Unknown command 93*/
				else if (command[0] == 0x93)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Set instrument*/
				else if (command[0] == 0x94)
				{
					curInst = command[1];
					firstNote = 1;
					seqPos += 2;
					noteOn = 0;
				}

				/*Unknown command 95*/
				else if (command[0] == 0x95)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Set tuning*/
				else if (command[0] == 0x96)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Unknown command 97*/
				else if (command[0] == 0x97)
				{
					noteOn = 0;
					seqPos += 2;
				}

				/*Turn off noise channel?*/
				else if (command[0] == 0x98)
				{
					seqEnd = 1;
				}

				/*Turn on noise channel?*/
				else if (command[0] == 0x99)
				{
					noteOn = 0;
					seqPos++;
				}

				/*Invalid command*/
				else if (command[0] > 0x99 && command[0] < 0xA0)
				{
					noteOn = 0;
					seqPos++;
				}

				/*Set volume/envelope settings?*/
				else if (command[0] >= 0xA0 && command[0] < 0xB0)
				{
					noteOn = 0;
					seqPos++;
				}

				/*Song loop point*/
				else if (command[0] == 0xB0)
				{
					repeat2Pos = seqPos + 2;
					repeat2Times = command[1];
					noteOn = 0;
					seqPos += 2;
				}

				/*Repeat point*/
				else if (command[0] == 0xB1)
				{
					repeat1Pos = seqPos + 2;
					repeat1Times = command[1];
					noteOn = 0;
					seqPos += 2;
				}

				/*Invalid command*/
				else if (command[0] == 0xB2 || command[0] == 0xB3)
				{
					noteOn = 0;
					seqPos++;
				}

				/*Go to song loop point*/
				else if (command[0] == 0xB4)
				{
					seqEnd = 1;
				}

				/*Go to repeat point*/
				else if (command[0] == 0xB5)
				{
					if (repeat1Times > 1)
					{
						seqPos = repeat1Pos;
						repeat1Times--;
						noteOn = 0;
					}
					else
					{
						seqPos++;
						noteOn = 0;
						repeat1Times = -1;
					}
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

			romPos += 2;
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