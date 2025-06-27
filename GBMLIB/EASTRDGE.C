/*Nick Eastridge*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "EASTRDGE.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long c1Offset;
long c2Offset;
long c3Offset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long seqPtrs[4];
long songPtr;
int numSongs;
int songBank;
long bankAmt;
int curInst;
long firstPtr;
int drvVers;
int cfgPtr;
int exitError;
int fileExit;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* multiMidData[4];
unsigned char* ctrlMidData;

long midLength;

char* argv3;
char string1[100];
char string2[100];
char NEcheckStrings[5][100] = { "version=", "numSongs=", "bank=", "songList=", "start=" };

unsigned int NEnoteLens[16] = { 0x06, 0x0C, 0x18, 0x30, 0x60, 0x24, 0x09, 0x12, 0x07, 0x0E, 0x1C, 0x38, 0x70, 0x2A, 0x0A, 0x15 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void NEsong2mid(int songNum, long ptrs[4]);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);

void NEProc(int bank, char parameters[4][50])
{
	curInst = 0;
	firstPtr = 0;
	cfgPtr = 0;
	exitError = 0;
	fileExit = 0;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the driver version value*/
		fgets(string1, 9, cfg);
		if (memcmp(string1, NEcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		drvVers = strtod(string1, NULL);

		fgets(string1, 3, cfg);
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, NEcheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtod(string1, NULL);

		fgets(string1, 3, cfg);
		if (drvVers == 1)
		{
			printf("Version: %01X\n", drvVers);
			/*Get the song bank*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, NEcheckStrings[2], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 3, cfg);

			masterBank = strtod(string1, NULL);
			printf("Bank: %01X\n", masterBank);

			fgets(string1, 3, cfg);
			/*Get the song list*/
			fgets(string1, 10, cfg);
			if (memcmp(string1, NEcheckStrings[3], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);

			}
			fgets(string1, 5, cfg);
			tableOffset = strtol(string1, NULL, 16);
			printf("Song list: 0x%04X\n", tableOffset);

			/*Copy the ROM data*/
			if (masterBank != 1)
			{
				bankAmt = bankSize;
			}
			else
			{
				bankAmt = 0;
			}

			if (masterBank != 1)
			{
				fseek(rom, 0, SEEK_SET);
				romData = (unsigned char*)malloc(bankSize * 2);
				fread(romData, 1, bankSize, rom);
				fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
				fread(romData + bankSize, 1, bankSize, rom);
			}
			else if (masterBank == 1)
			{
				fseek(rom, ((masterBank - 1) * bankSize * 2), SEEK_SET);
				romData = (unsigned char*)malloc(bankSize * 2);
				fread(romData, 1, bankSize * 2, rom);
			}


			/*Process each song*/
			songNum = 1;
			i = tableOffset;
			while (songNum <= numSongs)
			{
				songPtrs[0] = ReadLE16(&romData[i]);
				printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
				songPtrs[1] = ReadLE16(&romData[i + (numSongs * 2)]);
				printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
				songPtrs[2] = ReadLE16(&romData[i + (numSongs * 4)]);
				printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
				songPtrs[3] = 0;
				NEsong2mid(songNum, songPtrs);
				i += 2;
				songNum++;
			}


		}
		else if (drvVers == 2 || drvVers == 3)
		{
			printf("Version: %01X\n", drvVers);
			songNum = 1;
			while (songNum <= numSongs)
			{
				/*Skip the first line*/
				fgets(string1, 10, cfg);

				/*Get the song bank*/
				fgets(string1, 6, cfg);
				if (memcmp(string1, NEcheckStrings[2], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);

				songBank = strtol(string1, NULL, 16);

				fgets(string1, 3, cfg);

				/*Get the start position of the song*/
				fgets(string1, 7, cfg);
				if (memcmp(string1, NEcheckStrings[4], 1))
				{
					printf("ERROR: Invalid CFG data!\n");
					exit(1);
				}
				fgets(string1, 5, cfg);

				songPtrs[0] = strtol(string1, NULL, 16);

				printf("Song %i: 0x%04X, bank: %01X\n", songNum, songPtrs[0], songBank);

				fgets(string1, 3, cfg);

				fseek(rom, 0, SEEK_SET);
				romData = (unsigned char*)malloc(bankSize * 2);
				fread(romData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(romData + bankSize, 1, bankSize, rom);

				NEsong2mid(songNum, songPtrs);


				songNum++;
			}
		}
		else
		{
			printf("ERROR: Invalid driver version!\n");
			exit(1);
		}

		if (drvVers == 1)
		{
			free(romData);
		}
	}
}

/*Convert the song data to MIDI*/
void NEsong2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int trackSizes[4];
	int onOff[4];
	int transpose = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	unsigned char command[8];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int firstNote = 1;
	int firstNotes[4];
	unsigned int midPos = 0;
	unsigned int midPosM[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	long ctrlTrackSize = 0;
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	int rowTime = 0;
	int holdNotes[4];
	int delayTime[4];
	int prevNotes[4][2];

	if (drvVers == 1)
	{
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

			if (tableOffset == 0x6882)
			{
				tempo = 180;
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

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				firstNote = 1;
				holdNote = 0;
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;

				curDelay = 0;
				seqEnd = 0;

				curNote = 0;
				curNoteLen = 0;
				repeat1 = -1;
				repeat2 = -1;
				transpose = 0;


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

				if (songPtrs[curTrack] != 0x0000)
				{
					seqEnd = 0;
					seqPos = songPtrs[curTrack];

					while (seqEnd == 0)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];
						command[3] = romData[seqPos + 3];
						command[4] = romData[seqPos + 4];
						command[5] = romData[seqPos + 5];
						command[6] = romData[seqPos + 6];
						command[7] = romData[seqPos + 7];

						/*Rest*/
						if (command[0] == 0x00)
						{
							if (command[1] == 0xFF)
							{
								seqEnd = 1;
							}
							else
							{
								curNoteLen = NEnoteLens[command[1]] * 5;
								curDelay += curNoteLen;
								seqPos += 2;
							}

						}

						/*End of channel*/
						else if (command[0] == 0xFF || command[1] == 0xFF)
						{
							seqEnd = 1;
						}

						/*Play note*/
						else
						{
							curNote = command[0] + 11;
							curNoteLen = NEnoteLens[command[1]] * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
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
	else if (drvVers == 2)
	{

		midLength = 0x10000;
		for (j = 0; j < 4; j++)
		{
			multiMidData[j] = (unsigned char*)malloc(midLength);
		}

		ctrlMidData = (unsigned char*)malloc(midLength);

		for (j = 0; j < midLength; j++)
		{
			for (k = 0; k < 4; k++)
			{
				multiMidData[k][j] = 0;
			}

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

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				midPosM[curTrack] = 0;
				firstNotes[curTrack] = 1;
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
				midPosM[curTrack] += 8;
				curNoteLens[curTrack] = 0;
				curDelays[curTrack] = 0;
				seqEnd = 0;
				midTrackBase = midPosM[curTrack];

				/*Calculate MIDI channel size*/
				trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
				WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);

				onOff[curTrack] = 0;
				holdNotes[curTrack] = 0;
				delayTime[curTrack] = 0;
				prevNotes[curTrack][0] = 0;
				prevNotes[curTrack][1] = 0;
			}

			/*Convert the sequence data*/
			seqPos = ptrs[0];
			while (seqEnd == 0)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];
				command[4] = romData[seqPos + 4];
				command[5] = romData[seqPos + 5];
				command[6] = romData[seqPos + 6];
				command[7] = romData[seqPos + 7];

				/*Play note*/
				if (command[0] >= 0x10 && command[0] <= 0x13)
				{
					curTrack = command[0] & 0x0F;
					curNotes[curTrack] = command[1] + 24;
					curNoteLens[curTrack] = command[3] * 5;
					onOff[curTrack] = 1;
					seqPos += 4;
				}

				/*Row time*/
				else if (command[0] == 0x20)
				{
					rowTime = command[1] * 5;
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (holdNotes[curTrack] == 1)
						{
							delayTime[curTrack] += rowTime;
							if (onOff[curTrack] == 1)
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								holdNotes[curTrack] = 0;
								delayTime[curTrack] = 0;
							}
							else if (delayTime[curTrack] >= prevNotes[curTrack][1])
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], delayTime[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] -= delayTime[curTrack];
								delayTime[curTrack] = 0;
							}
						}
						if (onOff[curTrack] == 1)
						{
							if (curNoteLens[curTrack] <= rowTime)
							{
								tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = rowTime - curNoteLens[curTrack];
							}
							else
							{
								tempPos = WriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = rowTime;
								delayTime[curTrack] += rowTime;
								holdNotes[curTrack] = 1;
								prevNotes[curTrack][0] = curNotes[curTrack];
								prevNotes[curTrack][1] = curNoteLens[curTrack];
							}
							firstNotes[curTrack] = 0;
							onOff[curTrack] = 0;
						}
						else
						{
							curDelays[curTrack] += rowTime;
						}
					}
					seqPos += 2;
				}

				/*End of song (no loop)?*/
				else if (command[0] == 0x30)
				{
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (holdNotes[curTrack] == 1)
						{
							if (onOff[curTrack] == 1)
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								holdNotes[curTrack] = 0;
								delayTime[curTrack] = 0;
							}
						}
						if (onOff[curTrack] == 1)
						{
							tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = curNoteLens[curTrack];
						}
					}
					seqEnd = 1;
				}

				/*End of song (go to loop)*/
				else if (command[0] == 0x40)
				{
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (holdNotes[curTrack] == 1)
						{
							if (onOff[curTrack] == 1)
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								holdNotes[curTrack] = 0;
								delayTime[curTrack] = 0;
							}
						}
						if (onOff[curTrack] == 1)
						{
							tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = curNoteLens[curTrack];
						}
					}


					seqEnd = 1;
				}

				/*Unknown command*/
				else
				{
					seqPos++;
				}
			}

			/*End of track*/
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
				midPosM[curTrack] += 4;
				firstNotes[curTrack] = 0;

				/*Calculate MIDI channel size*/
				trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
				WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
			}

			/*End of control track*/
			ctrlMidPos++;
			WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
			ctrlMidPos += 4;

			/*Calculate MIDI channel size*/
			ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
			WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

			sprintf(outfile, "song%d.mid", songNum);
			fwrite(ctrlMidData, ctrlMidPos, 1, mid);
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
			}

			free(multiMidData[0]);
			free(romData);
			fclose(mid);
		}

	}

	else if (drvVers == 3)
	{

		midLength = 0x10000;
		for (j = 0; j < 4; j++)
		{
			multiMidData[j] = (unsigned char*)malloc(midLength);
		}

		ctrlMidData = (unsigned char*)malloc(midLength);

		for (j = 0; j < midLength; j++)
		{
			for (k = 0; k < 4; k++)
			{
				multiMidData[k][j] = 0;
			}

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

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				midPosM[curTrack] = 0;
				firstNotes[curTrack] = 1;
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
				midPosM[curTrack] += 8;
				curNoteLens[curTrack] = 0;
				curDelays[curTrack] = 0;
				seqEnd = 0;
				midTrackBase = midPosM[curTrack];

				/*Calculate MIDI channel size*/
				trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
				WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);

				onOff[curTrack] = 0;
				holdNotes[curTrack] = 0;
				delayTime[curTrack] = 0;
				prevNotes[curTrack][0] = 0;
				prevNotes[curTrack][1] = 0;
			}

			/*Convert the sequence data*/
			seqPos = ptrs[0];
			while (seqEnd == 0)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];
				command[4] = romData[seqPos + 4];
				command[5] = romData[seqPos + 5];
				command[6] = romData[seqPos + 6];
				command[7] = romData[seqPos + 7];

				/*Play note (channel 1-3)*/
				if (command[0] < 0x30)
				{
					curTrack = (command[0] & 0xF0) / 0x10;
					curNotes[curTrack] = command[1] + 24;
					curNoteLens[curTrack] = command[2] * 5;
					onOff[curTrack] = 1;
					seqPos += 3;
				}

				/*End of song (no loop)?*/
				else if (command[0] == 0x30)
				{
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (holdNotes[curTrack] == 1)
						{
							if (onOff[curTrack] == 1)
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								holdNotes[curTrack] = 0;
								delayTime[curTrack] = 0;
							}
						}
						if (onOff[curTrack] == 1)
						{
							tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = curNoteLens[curTrack];
						}
					}
					seqEnd = 1;
				}

				/*End of song (go to loop)*/
				else if (command[0] == 0x40)
				{
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (holdNotes[curTrack] == 1)
						{
							if (onOff[curTrack] == 1)
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								holdNotes[curTrack] = 0;
								delayTime[curTrack] = 0;
							}
						}
						if (onOff[curTrack] == 1)
						{
							tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = curNoteLens[curTrack];
						}
					}


					seqEnd = 1;
				}

				/*Play note (channel 4)*/
				else if (command[0] >= 0x50 && command[0] <= 0x7F)
				{
					curNotes[3] = command[0] - 0x50 + 36;
					onOff[3] = 1;
					seqPos++;
				}

				/*Row time*/
				else if (command[0] >= 0x80)
				{
					rowTime = (command[0] - 0x80) * 5;
					curNoteLens[3] = rowTime;
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						if (holdNotes[curTrack] == 1)
						{
							delayTime[curTrack] += rowTime;
							if (onOff[curTrack] == 1)
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = 0;
								holdNotes[curTrack] = 0;
								delayTime[curTrack] = 0;
							}
							else if (delayTime[curTrack] >= prevNotes[curTrack][1])
							{
								tempPos = WriteNoteEventOff(multiMidData[curTrack], midPosM[curTrack], prevNotes[curTrack][0], curNoteLens[curTrack], delayTime[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] -= delayTime[curTrack];
								delayTime[curTrack] = 0;
							}
						}
						if (onOff[curTrack] == 1)
						{
							if (curNoteLens[curTrack] <= rowTime)
							{
								tempPos = WriteNoteEvent(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = rowTime - curNoteLens[curTrack];
							}
							else
							{
								tempPos = WriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInst);
								midPosM[curTrack] = tempPos;
								curDelays[curTrack] = rowTime;
								delayTime[curTrack] += rowTime;
								holdNotes[curTrack] = 1;
								prevNotes[curTrack][0] = curNotes[curTrack];
								prevNotes[curTrack][1] = curNoteLens[curTrack];
							}
							firstNotes[curTrack] = 0;
							onOff[curTrack] = 0;
						}
						else
						{
							curDelays[curTrack] += rowTime;
						}
					}
					seqPos++;
				}

				/*Unknown command*/
				else
				{
					seqPos++;
				}
			}

			/*End of track*/
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
				midPosM[curTrack] += 4;
				firstNotes[curTrack] = 0;

				/*Calculate MIDI channel size*/
				trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
				WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
			}

			/*End of control track*/
			ctrlMidPos++;
			WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
			ctrlMidPos += 4;

			/*Calculate MIDI channel size*/
			ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
			WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

			sprintf(outfile, "song%d.mid", songNum);
			fwrite(ctrlMidData, ctrlMidPos, 1, mid);
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
			}

			free(multiMidData[0]);
			free(romData);
			fclose(mid);
		}

	}


}