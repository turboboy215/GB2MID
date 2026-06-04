/*Ubi Soft*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "UBISOFT.H"

#define bankSize 16384

FILE* rom, * mid, * cfg;
long bank;
long songBank;
long songPtr;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
long bankAmt;
int masterBank;
int exitError;
int curTrack;
int curInst;
int drvVers;
int curVol;

char string1[100];
char string2[100];
char UbiSoftCheckStrings[3][100] = { "numSongs=", "bank=", "offset=" };
unsigned char* romData;
unsigned char* exRomData;
unsigned char* cfgData;
unsigned char* midData;
unsigned char* multiMidData[4];
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
void UbiSoftsong2mid(int songNum, long ptrs);

int gbFreq2Note(unsigned int freq);

void UbiSoftProc(char parameters[4][100])
{
	curInst = 0;
	drvVers = UBISOFT_VER_STD;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);

		if (memcmp(string1, UbiSoftCheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 4, cfg);
		numSongs = strtod(string1, NULL);
		printf("Total # of songs: %i\n", numSongs);

		/*Skip new line*/
		fgets(string1, 2, cfg);

		songNum = 1;

		while (songNum <= numSongs && exitError == 0)
		{
			/*Skip new line*/
			fgets(string1, 2, cfg);
			/*Skip the first line*/
			fgets(string1, 10, cfg);

			/*Now look for the "bank"*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, UbiSoftCheckStrings[1], 1))
			{
				exitError = 1;
			}

			fgets(string1, 5, cfg);
			songBank = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 2, cfg);

			/*Now look for the "offset"*/
			fgets(string1, 8, cfg);
			if (memcmp(string1, UbiSoftCheckStrings[2], 1))
			{
				exitError = 1;
			}

			fgets(string1, 5, cfg);
			songPtr = strtol(string1, NULL, 16);

			/*Skip new line*/
			fgets(string1, 2, cfg);

			printf("Song %i: Bank %04X, Offset 0x%04X\n", songNum, songBank, songPtr);

			/*Copy the current bank data from the ROM*/
			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);

			UbiSoftsong2mid(songNum, songPtr);
			free(romData);
			songNum++;

		}

		fclose(cfg);
		return 0;
	}

}

/*Convert the song data to MIDI*/

void UbiSoftsong2mid(int songNum, long ptrs)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	long romPos = 0;
	int curDelay[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int curNote[4];
	int curNoteLen[4];
	int rowTime = 0;
	int songEnd = 0;
	int curSeq = 0;
	long seqPtr = 0;
	int seqEnd = 0;
	int k = 0;
	int curTrack = 0;
	unsigned int command[3];
	int curInsts[4];
	int firstNote[16];
	int midPos[4];
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	long tempPos = 0;
	int trackSize[4];
	int ctrlTrackSize = 0;
	int onOff[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int valSize = 0;
	long songStart = 0;
	long patOffset = 0;
	int numSeqs = 0;
	int ctrlDelay = 0;
	int curVols[4];
	long curFreq;

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

	for (k = 0; k < 4; k++)
	{
		onOff[k] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		seqPos = songPtr;
		seqEnd = 0;

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

		switch (drvVers)
		{
		case UBISOFT_VER_STD:
			/*Fall-through*/
		default:
			UBISOFT_STATUS_JUMP_MIN = 0x80;
			UBISOFT_STATUS_JUMP_MAX = 0xFF;

			EventMap[0x00] = UBISOFT_EVENT_NOTE_ON1;
			EventMap[0x02] = UBISOFT_EVENT_NOTE_OFF1;
			EventMap[0x04] = UBISOFT_EVENT_PROG_CHANGE1;
			EventMap[0x06] = UBISOFT_EVENT_NOP;
			EventMap[0x08] = UBISOFT_EVENT_NOP;
			EventMap[0x0A] = UBISOFT_EVENT_NOTE_ON2;
			EventMap[0x0C] = UBISOFT_EVENT_NOTE_OFF2;
			EventMap[0x0E] = UBISOFT_EVENT_PROG_CHANGE2;
			EventMap[0x10] = UBISOFT_EVENT_NOP;
			EventMap[0x12] = UBISOFT_EVENT_NOP;
			EventMap[0x14] = UBISOFT_EVENT_NOTE_ON3;
			EventMap[0x16] = UBISOFT_EVENT_NOTE_OFF3;
			EventMap[0x18] = UBISOFT_EVENT_PROG_CHANGE3;
			EventMap[0x1A] = UBISOFT_EVENT_NOP;
			EventMap[0x1C] = UBISOFT_EVENT_NOP;
			EventMap[0x1E] = UBISOFT_EVENT_NOTE_ON4;
			EventMap[0x20] = UBISOFT_EVENT_NOTE_OFF4;
			EventMap[0x22] = UBISOFT_EVENT_NOP;
			EventMap[0x24] = UBISOFT_EVENT_NOP;
			EventMap[0x26] = UBISOFT_EVENT_NOP;
			EventMap[0xFC] = UBISOFT_EVENT_EXIT;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPos[curTrack] = 0;
			firstNote[curTrack] = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPos[curTrack]], 0x4D54726B);
			midPos[curTrack] += 8;
			curNoteLen[curTrack] = 0;
			curDelay[curTrack] = 0;
			curVols[curTrack] = 120;
			seqEnd = 0;
			midTrackBase = midPos[curTrack];

			/*Add track header*/
			valSize = WriteDeltaTime(multiMidData[curTrack], midPos[curTrack], 0);
			midPos[curTrack] += valSize;
			WriteBE16(&multiMidData[curTrack][midPos[curTrack]], 0xFF03);
			midPos[curTrack] += 2;
			Write8B(&multiMidData[curTrack][midPos[curTrack]], strlen(TRK_NAMES[curTrack]));
			midPos[curTrack]++;
			sprintf((char*)&multiMidData[curTrack][midPos[curTrack]], TRK_NAMES[curTrack]);
			midPos[curTrack] += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSize[curTrack]);

			curInsts[curTrack] = 0;
		}

		/*Process song header*/
		songStart = ReadLE16(&romData[songPtr + 7]);
		patOffset = songStart + ReadLE16(&romData[songStart]);
		numSeqs = ReadLE16(&romData[songStart + 5]);

		romPos = patOffset;
		curSeq = 0;
		songEnd = 0;


		while (curSeq < numSeqs && songEnd == 0)
		{
			seqPos = songStart + ReadLE16(&romData[romPos]);
			seqEnd = 0;

			while (seqEnd == 0 && midPos[0] < 48000 && ctrlDelay < 110000)
			{
				if (ReadLE16(&romData[seqPos]) != 0)
				{
					rowTime = (ReadLE16(&romData[seqPos]) + 1) * 5;
				}
				else
				{
					rowTime = 0;
				}


				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					if (onOff[curTrack] == 1)
					{
						curNoteLen[curTrack] += rowTime;
					}
					else
					{
						curDelay[curTrack] += rowTime;
					}

				}

				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				if (EventMap[command[2]] == UBISOFT_EVENT_NOP)
				{
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_ON1)
				{
					curTrack = 0;
					curInst = curInsts[curTrack];
					curFreq = ReadBE16(&romData[seqPos + 3]) & 0x07FF;
					curVol = curVols[curTrack];
					curNote[curTrack] = gbFreq2Note(curFreq);
					onOff[curTrack] = 1;

					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_OFF1)
				{
					curTrack = 0;
					curInst = curInsts[curTrack];
					curVol = curVols[curTrack];
					tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInst);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
					curNoteLen[curTrack] = 0;
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_PROG_CHANGE1)
				{
					curTrack = 0;
					curInsts[curTrack] = ReadLE16(&romData[seqPos + 3]) / 4;
					firstNote[curTrack] = 1;
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_ON2)
				{
					curTrack = 1;
					curInst = curInsts[curTrack];
					curFreq = ReadBE16(&romData[seqPos + 3]) & 0x07FF;
					curVol = curVols[curTrack];
					curNote[curTrack] = gbFreq2Note(curFreq);
					onOff[curTrack] = 1;

					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_OFF2)
				{
					curTrack = 1;
					curInst = curInsts[curTrack];
					curVol = curVols[curTrack];
					tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInst);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
					curNoteLen[curTrack] = 0;
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_PROG_CHANGE2)
				{
					curTrack = 1;
					curInsts[curTrack] = ReadLE16(&romData[seqPos + 3]) / 4;
					firstNote[curTrack] = 1;
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_ON3)
				{
					curTrack = 2;
					curInst = curInsts[curTrack];
					curFreq = ReadBE16(&romData[seqPos + 3]) & 0x07FF;
					curVol = curVols[curTrack];
					curNote[curTrack] = gbFreq2Note(curFreq) - 12;
					onOff[curTrack] = 1;

					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_OFF3)
				{
					curTrack = 2;
					curInst = curInsts[curTrack];
					curVol = curVols[curTrack];
					tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInst);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
					curNoteLen[curTrack] = 0;
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_PROG_CHANGE3)
				{
					curTrack = 2;
					curInsts[curTrack] = ReadLE16(&romData[seqPos + 3]) / 4;
					firstNote[curTrack] = 1;
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_ON4)
				{
					curTrack = 3;
					curInst = curInsts[curTrack];
					curNote[curTrack] = ((ReadLE16(&romData[seqPos + 3])) & 0x0FFF) / 4;
					curVols[curTrack] = (romData[seqPos + 4] & 0xF0) / 2;

					if (curVols[curTrack] == 0)
					{
						curVols[curTrack] = 1;
					}
					curVol = curVols[curTrack];

					if (curNote[curTrack] > 127)
					{
						curNote[curTrack] -= 127;
					}
					onOff[curTrack] = 1;

					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_NOTE_OFF4)
				{
					curTrack = 3;
					curInst = curInsts[curTrack];
					curVol = curVols[curTrack];
					tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInst);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
					curNoteLen[curTrack] = 0;
					seqPos += 5;
				}
				else if (EventMap[command[2]] == UBISOFT_EVENT_EXIT)
				{
					seqEnd = 1;
					romPos += 2;
				}
				else if (EventMap[command[2]] >= UBISOFT_STATUS_JUMP_MIN && EventMap[command[2]] <= UBISOFT_STATUS_JUMP_MAX)
				{
					seqEnd = 1;
					songEnd = 1;
				}
				/*Unknown command*/
				else
				{
					seqPos += 5;
				}
			}
			curSeq++;
		}

		/*End of track*/
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			/*WriteDeltaTime(DE1midData[curTrack], midPos[curTrack], 0);
			midPos[curTrack]++;*/
			WriteBE32(&multiMidData[curTrack][midPos[curTrack]], 0xFF2F00);
			midPos[curTrack] += 4;
			firstNote[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSize[curTrack] = midPos[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSize[curTrack]);
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
			fwrite(multiMidData[curTrack], midPos[curTrack], 1, mid);
		}

		for (k = 0; k < trackCnt; k++)
		{
			free(multiMidData[k]);
		}
		free(ctrlMidData);
		fclose(mid);

	}
}