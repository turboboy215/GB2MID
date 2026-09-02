/*Gremlin*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "GREMLIN.H"
#include "ALIDEC.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtr;
long songPtrs[4];
int foundTable;
int curInst;
long firstPtr;
int numSongs;
long seqTab;
int songTempo;
int songTranspose;
int comp = 0;
int compDatStart;
int compDatEnd;
int compBank;
int ramStart;

long bankAmt;

int curVol;
int drvVers;

const char GremlinMagicBytes[6] = { 0xCB, 0x37, 0x4F, 0x06, 0x00, 0x21 };
unsigned int GremlindecompVals[6][3] = { 0x05, 0x75AA, 0xC158,
								  0x05, 0x7B1E, 0xC158,
								  0x05, 0x7AA7, 0xC158,
								  0x05, 0x7B63, 0xC158,
								  0x05, 0x75AA, 0xC158,
};

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;
unsigned char* bankRomData;
unsigned char* compData;
unsigned char* decompData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void GremlincopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd);
void Gremlinsong2mid(int songNum, long songPtrs[4]);

void GremlinProc(int bank)
{
	drvVers = GREMLIN_VER_STD;
	curVol = 120;
	foundTable = 0;
	if (bank < 0x02)
	{
		bank = 0x02;
	}
	bankAmt = bankSize;

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)calloc(0x10000, 1);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], GremlinMagicBytes, 6)) && foundTable != 1)
		{
			tablePtrLoc = i + 6;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		if (bank == 0x02)
		{
			comp = 1;
		}

		i = tableOffset;
		songNum = 1;

		if (comp != 1)
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
			{
				songPtrs[0] = ReadLE16(&romData[i]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				songPtrs[2] = ReadLE16(&romData[i + 4]);
				songPtrs[3] = ReadLE16(&romData[i + 6]);
				seqTab = ReadLE16(&romData[i + 8]);
				songTempo = romData[i + 12];
				songTranspose = (signed char)romData[i + 14] / 4;
				printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
				printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
				printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
				printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
				printf("Song %i sequence table: 0x%04X\n", songNum, seqTab);
				printf("Song %i tempo: %i\n", songNum, songTempo);
				printf("Song %i transpose (tuning): %i\n", songNum, songTranspose);
				Gremlinsong2mid(songNum, songPtrs);
				i += 16;
				songNum++;
			}
		}
		else
		{
			/*Song 1 (empty)*/
			songPtrs[0] = ReadLE16(&romData[i]);
			songPtrs[1] = ReadLE16(&romData[i + 2]);
			songPtrs[2] = ReadLE16(&romData[i + 4]);
			songPtrs[3] = ReadLE16(&romData[i + 6]);
			seqTab = ReadLE16(&romData[i + 8]);
			songTempo = romData[i + 12];
			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			printf("Song %i sequence table: 0x%04X\n", songNum, seqTab);
			printf("Song %i tempo: %i\n", songNum, songTempo);
			printf("Song %i transpose (tuning): %i\n", songNum, songTranspose);
			Gremlinsong2mid(songNum, songPtrs);
			i += 16;
			songNum++;

			/*Songs 2-5 are compressed, each has its own block in bank 5
			   The GBS table at 0x20 is [num][BE ptr] where num is stored at 0xDE76
			   and is *not* decompressed length but an index/transpose. For ROM,
			   GremlindecompVals gives the correct ROM offset and RAM destination per song.
			   The ROM uses hard-coded RAM 0xC158 for all, but GBS uses num as index. */
			while (songNum <= 5)
			{
				compDatStart = (GremlindecompVals[songNum - 2][1]) & 0x3FFF;
				compBank = GremlindecompVals[songNum - 2][0];
				ramStart = GremlindecompVals[songNum - 2][2];
				if (!decompData) decompData = (unsigned char*)calloc(0x10000, 1);
				bankRomData = (unsigned char*)malloc(bankSize);
				fseek(rom, compBank * bankSize, SEEK_SET);
				fread(bankRomData, 1, bankSize, rom);
				unsigned char* prefix = bankRomData;
				size_t prefix_len = compDatStart;
				compData = (unsigned char*)malloc(0x10000);
				size_t compDatEnd = decompress(bankRomData, compDatStart, (uint8_t*)compData, 0x10000, prefix, prefix_len);
				printf("Decompressed %zu bytes from bank %d offset 0x%04X to RAM 0x%04X\n", compDatEnd, compBank, compDatStart, ramStart);
				GremlincopyData(compData, decompData, ramStart, compDatEnd);
				memcpy(romData + ramStart, decompData + ramStart, compDatEnd);
				free(bankRomData);
				bankRomData = NULL;

				songPtrs[0] = ReadLE16(&romData[i]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				songPtrs[2] = ReadLE16(&romData[i + 4]);
				songPtrs[3] = ReadLE16(&romData[i + 6]);
				seqTab = ReadLE16(&romData[i + 8]);
				songTempo = romData[i + 12];
				printf("Song %i (compressed) channel 1: 0x%04X\n", songNum, songPtrs[0]);
				printf("Song %i (compressed) channel 2: 0x%04X\n", songNum, songPtrs[1]);
				printf("Song %i (compressed) channel 3: 0x%04X\n", songNum, songPtrs[2]);
				printf("Song %i (compressed) channel 4: 0x%04X\n", songNum, songPtrs[3]);
				printf("Song %i (compressed) sequence table: 0x%04X\n", songNum, seqTab);
				printf("Song %i (compressed) tempo: %i\n", songNum, songTempo);
				printf("Song %i (compressed) transpose (tuning): %i\n", songNum, songTranspose);
				Gremlinsong2mid(songNum, songPtrs);
				i += 16;
				songNum++;
			}
		}
		free(romData);
		fclose(rom);
		printf("The operation was successfully completed!\n");
		exit(0);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}

}

void GremlincopyData(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd)
{
	/* source = temp decompressed buffer(compData), dest is ignored, use global decompData*/
	/* dataStart = ramStart(0xC158), dataEnd = decompressed length*/
	(void)dest;
	if (!decompData) decompData = (unsigned char*)calloc(0x10000, 1);
	if (!source) return;
	if (dataEnd > 0x10000 - dataStart) dataEnd = 0x10000 - dataStart;
	memcpy(decompData + dataStart, source, dataEnd);
	free(source);
	/* source was compData(global), clear global to avoid double free*/
	compData = NULL;
}


/*Convert the song data to MIDI*/
void Gremlinsong2mid(int songNum, long songPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	long startSeq = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int trackEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int transpose = 0;
	int repeat = 0;
	unsigned char command[8];
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
	int masterDelay = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int noteDur = 0;

	midPos = 0;
	ctrlMidPos = 0;

	tempo = songTempo * 2.5;

	if (tempo < 2)
	{
		tempo = 150;
	}

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
		case GREMLIN_VER_STD:
			/*Fall-through*/
		default:
			GREMLIN_STATUS_NOTE_MIN = 0x00;
			GREMLIN_STATUS_NOTE_MAX = 0x7F;
			GREMLIN_STATUS_PROG_CHANGE_MIN = 0x80;
			GREMLIN_STATUS_PROG_CHANGE_MAX = 0xFC;
			EventMap[0xFD] = GREMLIN_EVENT_VOL;
			EventMap[0xFF] = GREMLIN_EVENT_STOP;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;
			trackEnd = 0;

			curNote = 0;
			curNoteLen = 0;

			repeat = -1;

			/*Add track header*/

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			romPos = songPtrs[curTrack];

			if (comp == 1 && songNum == 1)
			{
				trackEnd = 1;
			}

			while (trackEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				if (comp == 1 && songNum != 1)
				{
					if (decompData[romPos] != 0xFF)
					{
						seqEnd = 0;
						startSeq = ReadLE16(&decompData[seqTab + (decompData[romPos] * 2)]);
						seqPos = startSeq;
					}
					else
					{
						seqEnd = 1;
						trackEnd = 1;
					}
				}
				else
				{
					if (romData[romPos] != 0xFF)
					{
						seqEnd = 0;
						startSeq = ReadLE16(&romData[seqTab + (romData[romPos] * 2)]);
						seqPos = startSeq;
					}
					else
					{
						seqEnd = 1;
						trackEnd = 1;
					}
				}


				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos)
				{
					if (comp != 1)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];
						command[3] = romData[seqPos + 3];
					}
					else
					{
						if (songNum == 1)
						{
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];
							command[2] = romData[seqPos + 2];
							command[3] = romData[seqPos + 3];
						}
						else
						{
							command[0] = decompData[seqPos];
							command[1] = decompData[seqPos + 1];
							command[2] = decompData[seqPos + 2];
							command[3] = decompData[seqPos + 3];
						}

					}


					if (command[0] >= GREMLIN_STATUS_NOTE_MIN && command[0] <= GREMLIN_STATUS_NOTE_MAX)
					{
						curNote = command[0] - GREMLIN_STATUS_NOTE_MIN + songTranspose + 35;
						curNoteLen = command[1] * 20;

						/*Rest*/
						if (command[0] == 0x00)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						/*Play note*/
						else
						{
							tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}

						seqPos += 2;
					}

					else if (command[0] >= GREMLIN_STATUS_PROG_CHANGE_MIN && command[0] <= GREMLIN_STATUS_PROG_CHANGE_MAX)
					{
						curInst = command[0] - GREMLIN_STATUS_PROG_CHANGE_MIN;
						firstNote = 1;
						seqPos++;
					}

					else if (EventMap[command[0]] == GREMLIN_EVENT_UNKNOWN0)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == GREMLIN_EVENT_VOL)
					{
						curVol = (command[1] * 3);

						if (curVol > 120)
						{
							curVol = 120;
						}

						if (curVol < 1)
						{
							curVol = 1;
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == GREMLIN_EVENT_STOP)
					{
						seqEnd = 1;
						romPos++;
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}