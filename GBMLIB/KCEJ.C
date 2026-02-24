/*Konami (KCE Japan)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "KCEJ.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtr;
long bankAmt;
int curInst;
int drvVers;
int songBank;
int curVol;
unsigned int curInsts[4];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* multiMidData[4];
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void KCEJsong2mid(int songNum, long songPtr);

void KCEJProc(int bank, char parameters[4][100])
{
	curInst = 0;
	drvVers = KCEJ_VER_HXH;
	bankAmt = bankSize;

	if (parameters[1][0] != 0)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
		drvVers = strtol(parameters[1], NULL, 16);
	}
	else if (parameters[0][0] != 0)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
	}

	if (drvVers != KCEJ_VER_HXH && drvVers != KCEJ_VER_YGO)
	{
		printf("ERROR: Invalid version number!\n");
		exit(2);
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	i = tableOffset;
	songNum = 1;

	if (drvVers == KCEJ_VER_HXH)
	{
		while (ReadLE16(&romData[i]) >= bankSize && ReadLE16(&romData[i]) < (bankSize * 2) && ReadLE16(&romData[i + 2]) < 0x0100)
		{
			songPtr = ReadLE16(&romData[i]);
			songBank = ReadLE16(&romData[i + 2]);

			printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, songBank);
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, (songBank * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);
			KCEJsong2mid(songNum, songPtr);
			free(exRomData);
			i += 4;
			songNum++;
		}
	}

	else
	{
		while (ReadLE16(&romData[i + 2]) >= bankSize && ReadLE16(&romData[i + 2]) < (bankSize * 2) && ReadLE16(&romData[i]) < 0x0100)
		{
			songPtr = ReadLE16(&romData[i + 2]);
			songBank = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, songBank);
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, (songBank * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);
			KCEJsong2mid(songNum, songPtr);
			free(exRomData);
			i += 4;
			songNum++;
		}
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void KCEJsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long seqPos = 0;
	int curDelay[4];
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int curNote[4];
	int curNoteLen[4];
	int rowTime = 0;
	int seqEnd = 0;
	int k = 0;
	int curTrack = 0;
	unsigned int command[3];
	int firstNote[16];
	int midPos[4];
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 250;
	long tempPos = 0;
	int trackSize[4];
	int ctrlTrackSize = 0;
	int onOff[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int valSize = 0;

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
		case KCEJ_VER_HXH:
			/*Fall-through*/
		case KCEJ_VER_YGO:
		default:
			KCEJ_STATUS_DUR_MIN = 0x00;
			KCEJ_STATUS_DUR_MAX = 0x7F;
			for (k = 0; k < 16; k++)
			{
				EventMap[0x80 + k] = KCEJ_EVENT_NOTE_ON;
			}
			for (k = 0; k < 16; k++)
			{
				EventMap[0x90 + k] = KCEJ_EVENT_NOTE_OFF;
			}
			for (k = 0; k < 16; k++)
			{
				EventMap[0xA0 + k] = KCEJ_EVENT_VOLUME;
			}
			EventMap[0xC0] = KCEJ_EVENT_NOP;
			EventMap[0xC8] = KCEJ_EVENT_NOP;
			EventMap[0xF0] = KCEJ_EVENT_SONG_LOOP;
			EventMap[0xFE] = KCEJ_EVENT_STOP;
			EventMap[0xFF] = KCEJ_EVENT_GOTO_LOOP;

		}

		for (k = 0; k < 4; k++)
		{
			curInsts[k] = 0;
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

			if (ReadLE16(&exRomData[songPtr]) == 0x0000)
			{
				seqEnd = 1;
			}
		}

		/*Convert the sequence data*/
		while (seqEnd == 0)
		{
			command[0] = exRomData[seqPos];
			command[1] = exRomData[seqPos + 1];
			command[2] = exRomData[seqPos + 2];

			if (command[0] >= KCEJ_STATUS_DUR_MIN && command[0] <= KCEJ_STATUS_DUR_MAX)
			{
				rowTime = command[0];

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
				seqPos++;
			}

			else if (EventMap[command[0]] == KCEJ_EVENT_NOTE_ON)
			{
				curTrack = (command[0] & 7);

				if (curTrack > 3)
				{
					curTrack -= 4;
				}

				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				else
				{
					curNote[curTrack] = command[1];

					if (curTrack != 3)
					{
						curNote[curTrack] += 24;

						if (curTrack == 2)
						{
							curNote[curTrack] -= 12;
						}
					}
					onOff[curTrack] = 1;
					curNoteLen[curTrack] = 0;

					seqPos += 2;
				}
			}

			else if (EventMap[command[0]] == KCEJ_EVENT_NOTE_OFF)
			{
				curTrack = (command[0] & 7);

				if (curTrack > 3)
				{
					curTrack -= 4;
				}

				if (onOff[curTrack] == 1)
				{
					tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
					firstNote[curTrack] = 0;
					midPos[curTrack] = tempPos;
					curDelay[curTrack] = 0;
					onOff[curTrack] = 0;
				}
				seqPos++;
			}

			else if (EventMap[command[0]] == KCEJ_EVENT_VOLUME)
			{
				curTrack = (command[0] & 7);

				if (curTrack > 3)
				{
					curTrack -= 4;
				}

				seqPos += 2;
			}

			else if (EventMap[command[0]] == KCEJ_EVENT_SONG_LOOP)
			{
				seqPos++;
			}

			else if (EventMap[command[0]] == KCEJ_EVENT_STOP)
			{
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					if (onOff[curTrack] == 1)
					{
						tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
						firstNote[curTrack] = 0;
						midPos[curTrack] = tempPos;
						curDelay[curTrack] = 0;
						onOff[curTrack] = 0;
					}

				}
				seqEnd = 1;
			}

			else if (EventMap[command[0]] == KCEJ_EVENT_GOTO_LOOP)
			{
				for (curTrack = 0; curTrack < trackCnt; curTrack++)
				{
					if (onOff[curTrack] == 1)
					{
						tempPos = WriteNoteEvent(multiMidData[curTrack], midPos[curTrack], curNote[curTrack], curNoteLen[curTrack], curDelay[curTrack], firstNote[curTrack], curTrack, curInsts[curTrack]);
						firstNote[curTrack] = 0;
						midPos[curTrack] = tempPos;
						curDelay[curTrack] = 0;
						onOff[curTrack] = 0;
					}
				}
				seqEnd = 1;
			}

			/*Unknown event*/
			else
			{
				seqPos++;
			}
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