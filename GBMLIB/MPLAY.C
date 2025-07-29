/*MPlay (Thomas E. Petersen)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MPLAY.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long masterBank;
long seqBanks[4];
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long songPtrs[4];
int chanMask;
long bankAmt;
int numBanks;
int curBank;
int numSongs;
int tempNumSongs;
int drvVers;
long songList;
long patData;
long seqList;
long bankMap;
long tempoMap;
int exitError;
int fileExit;
int firstSong;
int curInst;
int curVol;
int bankSong;

unsigned char* romData;
unsigned char* exRomData;
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
void WriteLE16(unsigned char* buffer, unsigned int value);
void WriteLE24(unsigned char* buffer, unsigned long value);
void WriteLE32(unsigned char* buffer, unsigned long value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void MPlaysong2mid(int songNum, long songPtr, long songPtrs[4]);

char string1[100];
char string2[100];
char MPlaycheckStrings[9][100] = { "masterBank=", "numSongs=", "format", "songList=", "patData=", "seqList=", "bankMap=", "tempoMap=", "numBanks=" };

void MPlayProc(int bank, char parameters[4][100])
{
	exitError = 0;
	fileExit = 0;
	songNum = 1;
	firstSong = 1;
	tempNumSongs = 0;
	curInst = 0;
	curVol = 100;
	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{

		/*Get the total number of banks*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, MPlaycheckStrings[8], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);
		numBanks = strtod(string1, NULL);
		printf("Number of banks: %01X\n", numBanks);

		for (curBank = 1; curBank <= numBanks; curBank++)
		{
			bankSong = 0;
			/*Skip new line*/
			fgets(string1, 2, cfg);
			/*Skip next line*/
			fgets(string1, 10, cfg);

			/*Get the "master bank" value*/
			fgets(string1, 12, cfg);

			if (memcmp(string1, MPlaycheckStrings[0], 1))
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

			if (memcmp(string1, MPlaycheckStrings[1], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 3, cfg);
			numSongs = strtod(string1, NULL);
			tempNumSongs += numSongs;
			printf("Total # of songs: %i\n", numSongs);

			/*Skip new line*/
			fgets(string1, 2, cfg);
			/*Get the version number*/
			fgets(string1, 8, cfg);

			if (memcmp(string1, MPlaycheckStrings[2], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 3, cfg);
			drvVers = strtod(string1, NULL);
			printf("Version: %i\n", drvVers);

			/*Copy the current bank's ROM data*/
			bankAmt = bankSize;
			fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
			romData = (unsigned char*)malloc(bankSize);
			fread(romData, 1, bankSize, rom);

			if (drvVers == 1)
			{
				/*Get the song pointer list*/
				fgets(string1, 3, cfg);

				fgets(string1, 10, cfg);
				if (memcmp(string1, MPlaycheckStrings[3], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				songList = strtol(string1, NULL, 16);
				printf("Song list: 0x%04X\n", songList);

				/*Get the sequence list*/
				fgets(string1, 3, cfg);

				fgets(string1, 9, cfg);
				if (memcmp(string1, MPlaycheckStrings[5], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				seqList = strtol(string1, NULL, 16);
				printf("Sequence list: 0x%04X\n", seqList);

				/*Get the tempo mapping*/
				fgets(string1, 3, cfg);

				fgets(string1, 10, cfg);
				if (memcmp(string1, MPlaycheckStrings[7], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				tempoMap = strtol(string1, NULL, 16);
				printf("Tempo mapping: 0x%04X\n", tempoMap);

				/*Process each song*/
				i = songList - bankAmt;
				for (songNum = firstSong; songNum <= tempNumSongs; songNum++)
				{
					for (j = 0; j < 3; j++)
					{
						songPtrs[j] = ReadLE16(&romData[i]);
						i += 2;
						printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), songPtrs[j]);
					}
					MPlaysong2mid(songNum, songPtr, songPtrs);
					bankSong++;
				}

			}
			else if (drvVers == 2)
			{
				/*Get the song pointer list*/
				fgets(string1, 3, cfg);

				fgets(string1, 10, cfg);
				if (memcmp(string1, MPlaycheckStrings[3], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				songList = strtol(string1, NULL, 16);
				printf("Song list: 0x%04X\n", songList);

				/*Get the sequence list*/
				fgets(string1, 3, cfg);

				fgets(string1, 9, cfg);
				if (memcmp(string1, MPlaycheckStrings[5], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				seqList = strtol(string1, NULL, 16);
				printf("Sequence list: 0x%04X\n", seqList);

				/*Get the tempo mapping*/
				fgets(string1, 3, cfg);

				fgets(string1, 10, cfg);
				if (memcmp(string1, MPlaycheckStrings[7], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				tempoMap = strtol(string1, NULL, 16);
				printf("Tempo mapping: 0x%04X\n", tempoMap);

				/*Process each song*/
				i = songList - bankAmt;
				for (songNum = firstSong; songNum <= tempNumSongs; songNum++)
				{
					for (j = 0; j < 3; j++)
					{
						songPtrs[j] = ReadLE16(&romData[i]);
						i += 2;
						printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), songPtrs[j]);
					}
					MPlaysong2mid(songNum, songPtr, songPtrs);
					bankSong++;
				}
			}
			else if (drvVers == 3)
			{
				/*Get the song pointer list*/
				fgets(string1, 3, cfg);

				fgets(string1, 10, cfg);
				if (memcmp(string1, MPlaycheckStrings[3], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				songList = strtol(string1, NULL, 16);
				printf("Song list: 0x%04X\n", songList);

				/*Get the start of the pattern data*/
				fgets(string1, 3, cfg);

				fgets(string1, 9, cfg);
				if (memcmp(string1, MPlaycheckStrings[4], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				patData = strtol(string1, NULL, 16);
				printf("Pattern data: 0x%04X\n", patData);

				/*Get the sequence list*/
				fgets(string1, 3, cfg);

				fgets(string1, 9, cfg);
				if (memcmp(string1, MPlaycheckStrings[5], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				seqList = strtol(string1, NULL, 16);
				printf("Sequence list: 0x%04X\n", seqList);

				/*Get the bank mapping*/
				fgets(string1, 3, cfg);

				fgets(string1, 9, cfg);
				if (memcmp(string1, MPlaycheckStrings[6], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				bankMap = strtol(string1, NULL, 16);
				printf("Bank mapping: 0x%04X\n", bankMap);


				/*Process each song*/
				i = songList - bankAmt;
				for (songNum = 1; songNum <= numSongs; songNum++)
				{
					songPtr = patData + romData[i];
					i++;
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					MPlaysong2mid(songNum, songPtr, songPtrs);
				}

			}
			else if (drvVers == 4)
			{
				/*Get the song pointer list*/
				fgets(string1, 3, cfg);

				fgets(string1, 10, cfg);
				if (memcmp(string1, MPlaycheckStrings[3], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				songList = strtol(string1, NULL, 16);
				printf("Song list: 0x%04X\n", songList);

				/*Get the sequence list*/
				fgets(string1, 3, cfg);

				fgets(string1, 9, cfg);
				if (memcmp(string1, MPlaycheckStrings[5], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				seqList = strtol(string1, NULL, 16);
				printf("Sequence list: 0x%04X\n", seqList);

				/*Get the bank mapping*/
				fgets(string1, 3, cfg);

				fgets(string1, 9, cfg);
				if (memcmp(string1, MPlaycheckStrings[6], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);
				bankMap = strtol(string1, NULL, 16);
				printf("Bank mapping: 0x%04X\n", bankMap);


				/*Process each song*/
				i = songList - bankAmt;
				for (songNum = 1; songNum <= numSongs; songNum++)
				{
					songPtr = ReadLE16(&romData[i]);
					i += 2;
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					MPlaysong2mid(songNum, songPtr, songPtrs);
				}
			}
			else
			{
				printf("ERROR: Invalid version number!\n");
				exit(1);
			}
			firstSong += numSongs;
			free(romData);
		}

	}
}

void MPlaysong2mid(int songNum, long songPtr, long songPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int seqEnd = 0;
	int patEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curPat = 0;
	int transpose = 0;
	int tempo = 180;
	int rowsUsed = 0;
	long seqPtrs[4];
	int parameter = 0;
	int paramVal = 0;
	long ptrBase = 0;
	long ptrBankBase = 0;
	curInst = 0;
	unsigned char command[8];
	unsigned char patCom = 0;

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
	int curTempo = 0;

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

		if (drvVers == 1 || drvVers == 2)
		{
			trackCnt = 3;
		}
		else if (drvVers == 3 || drvVers == 4)
		{
			trackCnt = 4;
		}

		if (drvVers == 1 || drvVers == 2)
		{
			romPos = bankSong + tempoMap - bankAmt;
			tempo = romData[romPos];

			if (tempo == 0x01)
			{
				tempo = 500;
			}

			else if (tempo == 0x02)
			{
				tempo = 400;
			}

			else if (tempo == 0x03)
			{
				tempo = 300;
			}

			else if (tempo == 0x04)
			{
				tempo = 220;
			}

			else if (tempo == 0x05)
			{
				tempo = 200;
			}

			else if (tempo == 0x06)
			{
				tempo = 150;
			}

			else if (tempo == 0x07)
			{
				tempo = 120;
			}

			else if (tempo == 0x08)
			{
				tempo = 110;
			}

			else if (tempo == 0x09)
			{
				tempo = 100;
			}

			else if (tempo == 0x0A)
			{
				tempo = 90;
			}

			else if (tempo == 0x0B)
			{
				tempo = 80;
			}

			else if (tempo == 0x0C)
			{
				tempo = 70;
			}

			else
			{
				tempo = 70;
			}
		}

		else
		{
			tempo = 180;
			curTempo = 180;
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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			transpose = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
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

			if (drvVers == 2 && masterBank == 0x20)
			{
				ptrBase = ReadLE16(&romData[seqList + ((songNum - 20) * 2) - bankAmt]);
			}
			else if (drvVers == 2)
			{
				ptrBase = ReadLE16(&romData[seqList + ((songNum - 1) * 2) - bankAmt]);
			}

			if (drvVers == 1 || drvVers == 2)
			{
				romPos = (songNum - 1) + tempoMap - bankAmt;
				tempo = romData[romPos] * 50;


				romPos = songPtrs[curTrack] - bankAmt;
				patEnd = 0;

				while (patEnd == 0)
				{
					patCom = romData[romPos];

					/*Play pattern*/
					if (patCom < 0x80)
					{
						curPat = romData[romPos];
						seqEnd = 0;

						if (drvVers == 1)
						{
							seqPos = (ReadLE16(&romData[(seqList - bankAmt) + (curPat * 2)])) - bankAmt;
						}
						else if (drvVers == 2)
						{
							seqPos = (ReadLE16(&romData[ptrBase + (curPat * 2) - bankAmt])) - bankAmt;
						}

						while (seqEnd == 0)
						{

							if (curPat == 0x26)
							{
								curPat = 0x26;
							}
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];
							command[2] = romData[seqPos + 2];
							command[3] = romData[seqPos + 3];

							mask = command[0];
							highNibble = (mask & 31);

							/*End of sequence*/
							if (mask == 0xFF)
							{
								seqEnd = 1;
							}

							else
							{
								rowsUsed = highNibble + 1;

								/*Play note OR rest with instrument change*/
								if (mask & 0x80)
								{
									curNote = command[2];

									/*Rest*/
									if (curNote == 0xFF)
									{
										curDelay += (rowsUsed * 30);
									}

									/*Play note*/
									else
									{
										curNote = curNote + 12 + transpose;
										curNoteLen = rowsUsed * 30;
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
									}

									if (command[1] != curInst)
									{
										curInst = command[1];
										firstNote = 1;
									}

									seqPos += 3;
								}
								/*Play note OR rest, no instrument change*/
								else
								{
									curNote = command[1];

									/*Rest*/
									if (curNote == 0xFF)
									{
										curDelay += (rowsUsed * 30);
									}

									/*Play note*/
									else
									{
										curNote = curNote + 12 + transpose;
										curNoteLen = rowsUsed * 30;
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
									}
									seqPos += 2;
								}
							}
						}
						romPos++;
					}

					/*Set transpose*/
					else if (patCom >= 0x80 && patCom < 0xFE)
					{
						transpose = patCom - 0x80;
						romPos++;
					}

					/*End of channel (no loop)*/
					else if (patCom == 0xFE)
					{
						patEnd = 1;
					}

					/*End of channel (loop)*/
					else if (patCom == 0xFF)
					{
						patEnd = 1;
					}

				}

				/*End of track*/
				WriteBE32(&midData[midPos], 0xFF2F00);
				midPos += 4;

				/*Calculate MIDI channel size*/
				trackSize = midPos - midTrackBase;
				WriteBE16(&midData[midTrackBase - 2], trackSize);
			}
			else if (drvVers == 3 || drvVers == 4)
			{
				romPos = songPtr - bankAmt;
				patEnd = 0;

				if (drvVers == 4)
				{
					ptrBase = ReadLE16(&romData[seqList + ((songNum - 1) * 2) - bankAmt]);
					ptrBankBase = ReadLE16(&romData[bankMap + ((songNum - 1) * 2) - bankAmt]);
				}

				while (patEnd == 0)
				{
					patCom = romData[romPos];

					/*Play pattern*/
					if (patCom < 0xFE)
					{
						curPat = patCom;

						if (drvVers == 3)
						{
							seqPtrs[curTrack] = ReadLE16(&romData[(seqList - bankAmt) + (curPat * 8) + (curTrack * 2)]);
							seqBanks[curTrack] = masterBank + romData[(bankMap - bankAmt) + (curPat * 4) + curTrack];
							fseek(rom, ((seqBanks[curTrack] - 1) * bankSize), SEEK_SET);
						}
						else if (drvVers == 4)
						{
							seqPtrs[curTrack] = ReadLE16(&romData[(ptrBase - bankAmt) + (curPat * 8) + (curTrack * 2)]);
							seqBanks[curTrack] = masterBank + romData[(ptrBankBase - bankAmt) + (curPat * 4) + curTrack];
							fseek(rom, ((seqBanks[curTrack] - 1) * bankSize), SEEK_SET);
						}

						exRomData = (unsigned char*)malloc(bankSize);
						fread(exRomData, 1, bankSize, rom);
						seqPos = seqPtrs[curTrack] - bankAmt;
						seqEnd = 0;

						while (seqEnd == 0)
						{
							command[0] = exRomData[seqPos];
							command[1] = exRomData[seqPos + 1];
							command[2] = exRomData[seqPos + 2];
							command[3] = exRomData[seqPos + 3];
							command[4] = exRomData[seqPos + 4];

							/*End of sequence*/
							if (command[0] >= 0x80)
							{
								seqEnd = 1;
							}
							else
							{
								mask = command[0];
								lowNibble = (mask >> 4);
								highNibble = (mask & 15);

								/*Get the parameter*/
								parameter = highNibble;

								/*Parameter: none*/
								if (highNibble == 0x00)
								{
									;
								}
								/*Parameter: portamento up*/
								else if (highNibble == 0x01)
								{
									tempPos = WriteDeltaTime(midData, midPos, curDelay);
									midPos += tempPos;
									Write8B(&midData[midPos], (0xE0 | curTrack));
									Write8B(&midData[midPos + 1], 0);
									Write8B(&midData[midPos + 2], 0x40);
									Write8B(&midData[midPos + 3], 0);
									curDelay = 0;
									firstNote = 1;
									midPos += 3;
								}
								/*Parameter: portamento down*/
								else if (highNibble == 0x02)
								{
									tempPos = WriteDeltaTime(midData, midPos, curDelay);
									midPos += tempPos;
									Write8B(&midData[midPos], (0xE0 | curTrack));
									Write8B(&midData[midPos + 1], 0);
									Write8B(&midData[midPos + 2], 0x40);
									Write8B(&midData[midPos + 3], 0);
									curDelay = 0;
									firstNote = 1;
									midPos += 3;
								}
								/*Parameter: volume slide*/
								else if (highNibble == 0x03)
								{
									;
								}
								/*Parameter: set portamento?*/
								else if (highNibble == 0x04)
								{
									tempPos = WriteDeltaTime(midData, midPos, curDelay);
									midPos += tempPos;
									Write8B(&midData[midPos], (0xE0 | curTrack));
									Write8B(&midData[midPos + 1], 0);
									Write8B(&midData[midPos + 2], 0x40);
									Write8B(&midData[midPos + 3], 0);
									curDelay = 0;
									firstNote = 1;
									midPos += 3;
								}
								/*Parameter: sweep*/
								else if (highNibble == 0x0B)
								{
									;
								}
								/*Parameter: set volume*/
								else if (highNibble == 0x0C)
								{
									;
								}
								/*Parameter: set tempo*/
								else if (highNibble == 0x0F)
								{
									;
								}
								/*Parameter: unknown*/
								else
								{
									;
								}

								/*Empty row*/
								if (lowNibble == 0x00)
								{
									curDelay += ((command[1] + 1) * 30);
									seqPos += 2;
								}

								/*Play note*/
								else if (lowNibble == 0x01)
								{
									curNote = command[1] + 24;
									curNoteLen = (command[2] + 1) * 30;

									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;

									seqPos += 3;
								}

								/*Play note and set instrument*/
								else if (lowNibble == 0x03)
								{
									curNote = command[1] + 24;
									if (command[2] != curInst)
									{
										curInst = command[2];
										firstNote = 1;
									}
									curNoteLen = (command[3] + 1) * 30;

									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;

									seqPos += 4;
								}

								/*Set parameter*/
								else if (lowNibble == 0x04)
								{
									paramVal = command[1];
									curDelay += ((command[2] + 1) * 30);

									if (highNibble == 0x0F)
									{
										tempo = paramVal;

										if (tempo == 0x01)
										{
											tempo = 500;
										}

										else if (tempo == 0x02)
										{
											tempo = 400;
										}

										else if (tempo == 0x03)
										{
											tempo = 300;
										}

										else if (tempo == 0x04)
										{
											tempo = 220;
										}

										else if (tempo == 0x05)
										{
											tempo = 200;
										}

										else if (tempo == 0x06)
										{
											tempo = 150;
										}

										else if (tempo == 0x07)
										{
											tempo = 120;
										}

										else if (tempo == 0x08)
										{
											tempo = 110;
										}

										else if (tempo == 0x09)
										{
											tempo = 100;
										}

										else if (tempo == 0x0A)
										{
											tempo = 90;
										}

										else if (tempo == 0x0B)
										{
											tempo = 80;
										}

										else if (tempo == 0x0C)
										{
											tempo = 70;
										}

										else
										{
											tempo = 70;
										}

										ctrlMidPos++;
										valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
										ctrlDelay = 0;
										ctrlMidPos += valSize;
										WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
										ctrlMidPos += 3;
										WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
										ctrlMidPos += 2;
									}

									seqPos += 3;
								}

								/*Play note and set parameter*/
								else if (lowNibble == 0x05)
								{
									paramVal = command[2];

									if (highNibble == 0x0F)
									{
										tempo = paramVal;
										if (tempo == 0x01)
										{
											tempo = 500;
										}

										else if (tempo == 0x02)
										{
											tempo = 400;
										}

										else if (tempo == 0x03)
										{
											tempo = 300;
										}

										else if (tempo == 0x04)
										{
											tempo = 220;
										}

										else if (tempo == 0x05)
										{
											tempo = 200;
										}

										else if (tempo == 0x06)
										{
											tempo = 150;
										}

										else if (tempo == 0x07)
										{
											tempo = 120;
										}

										else if (tempo == 0x08)
										{
											tempo = 110;
										}

										else if (tempo == 0x09)
										{
											tempo = 100;
										}

										else if (tempo == 0x0A)
										{
											tempo = 90;
										}

										else if (tempo == 0x0B)
										{
											tempo = 80;
										}

										else if (tempo == 0x0C)
										{
											tempo = 70;
										}

										else
										{
											tempo = 70;
										}

										ctrlMidPos++;
										valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
										ctrlDelay = 0;
										ctrlMidPos += valSize;
										WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
										ctrlMidPos += 3;
										WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
										ctrlMidPos += 2;
									}

									curNote = command[1] + 24;
									curNoteLen = (command[3] + 1) * 30;

									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;

									seqPos += 4;
								}

								/*Play note, set instrument, and set parameter*/
								else if (lowNibble == 0x07)
								{
									curNote = command[1] + 24;
									if (command[2] != curInst)
									{
										curInst = command[2];
										firstNote = 1;
									}
									paramVal = command[3];

									if (highNibble == 0x0F)
									{
										tempo = paramVal;
										if (tempo == 0x01)
										{
											tempo = 500;
										}

										else if (tempo == 0x02)
										{
											tempo = 400;
										}

										else if (tempo == 0x03)
										{
											tempo = 300;
										}

										else if (tempo == 0x04)
										{
											tempo = 220;
										}

										else if (tempo == 0x05)
										{
											tempo = 200;
										}

										else if (tempo == 0x06)
										{
											tempo = 150;
										}

										else if (tempo == 0x07)
										{
											tempo = 120;
										}

										else if (tempo == 0x08)
										{
											tempo = 110;
										}

										else if (tempo == 0x09)
										{
											tempo = 100;
										}

										else if (tempo == 0x0A)
										{
											tempo = 90;
										}

										else if (tempo == 0x0B)
										{
											tempo = 80;
										}

										else if (tempo == 0x0C)
										{
											tempo = 70;
										}

										else
										{
											tempo = 70;
										}

										ctrlMidPos++;
										valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
										ctrlDelay = 0;
										ctrlMidPos += valSize;
										WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
										ctrlMidPos += 3;
										WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
										ctrlMidPos += 2;
									}

									curNoteLen = (command[4] + 1) * 30;

									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 0;
									midPos = tempPos;
									curDelay = 0;

									seqPos += 5;
								}
							}
						}
						free(exRomData);
						romPos++;
					}

					/*End of channel (no loop)*/
					else if (patCom == 0xFE)
					{
						patEnd = 1;
					}

					/*End of channel (loop)*/
					else if (patCom == 0xFF)
					{
						patEnd = 1;
					}
				}

				/*End of track*/
				WriteBE32(&midData[midPos], 0xFF2F00);
				midPos += 4;

				/*Calculate MIDI channel size*/
				trackSize = midPos - midTrackBase;
				WriteBE16(&midData[midTrackBase - 2], trackSize);
			}

			else
			{
				printf("ERROR: Invalid driver version!\n");
				exit(0);
			}
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