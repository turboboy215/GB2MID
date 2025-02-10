/*Beam Software*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "BEAM.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long patTablePtrLoc;
long patTableOffset;
long seqTablePtrLoc;
long seqTableOffset;
long seqDataOffset;
long seqPTables[4];
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long songSeqPtrs;
long songSeqData;
int chanMask;
long bankAmt;
int foundTable1;
int foundTable2;
int foundTable3;
long firstPtr;
int curInst;
long tempoTablePtrLoc;
long tempoTableOffset;
int songTempo;

unsigned static char* romData;
unsigned static char* midData;
unsigned static char* ctrlMidData;

long midLength;

const char BeamMagicBytesA[8] = { 0x19, 0x86, 0x18, 0x0D, 0x79, 0xD6, 0x04, 0x07 };
const char BeamMagicBytesB[8] = { 0x5F, 0x16, 0x00, 0x3E, 0x00, 0xE0, 0x25, 0x21 };
const char BeamMagicBytesC[4] = { 0x07, 0x07, 0x07, 0x07 };
const int BeamNoteLenTab[46] = { 0x03, 0x06, 0x09, 0x0C, 0x12, 0x15, 0x18, 0x24, 0x2A, 0x30, 0x48, 0x54, 0x60,
0x90, 0xA8, 0x02, 0x03, 0x04, 0x06, 0x07, 0x08, 0x0C, 0x0E, 0x10, 0x18, 0x1C,
0x20, 0x30, 0x38, 0xFF, 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F, 0x01,
0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Beamsong2mid(int songNum, long ptrs[4], long seqPtrs, long seqData);

void BeamProc(int bank)
{
	foundTable1 = 0;
	foundTable2 = 0;
	foundTable3 = 0;
	firstPtr = 0;
	curInst = 0;
	songTempo = 0;

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
		if ((!memcmp(&romData[i], BeamMagicBytesA, 8)) && foundTable1 != 1)
		{
			patTablePtrLoc = bankAmt + i - 2;
			printf("Found pointer to song pattern tables at address 0x%04x!\n", patTablePtrLoc);
			patTableOffset = ReadLE16(&romData[patTablePtrLoc - bankAmt]);
			printf("Song pattern tables start at 0x%04x...\n", patTableOffset);
			foundTable1 = 1;
		}
	}

	/*Now search for the sequence data*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], BeamMagicBytesB, 8)) && foundTable2 != 1)
		{
			seqTablePtrLoc = bankAmt + i + 8;
			printf("Found pointer to sequence tables at address 0x%04x!\n", seqTablePtrLoc);
			seqTableOffset = ReadLE16(&romData[seqTablePtrLoc - bankAmt + 12]);
			printf("Sequence pointer tables start at 0x%04x...\n", seqTableOffset);
			seqDataOffset = ReadLE16(&romData[seqTablePtrLoc - bankAmt]);
			printf("Sequence data tables start at 0x%04x...\n", seqDataOffset);
			foundTable2 = 1;
		}
	}

	/*Finally, look for the song tempo values*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], BeamMagicBytesC, 4)) && foundTable3 != 1 && romData[i - 3] == 0xFA)
		{
			tempoTablePtrLoc = bankAmt + i + 16;
			printf("Found pointer to song tempo table at address 0x%04x!\n", tempoTablePtrLoc);
			tempoTableOffset = ReadLE16(&romData[tempoTablePtrLoc - bankAmt]);
			printf("Tempo table is at 0x%04x...\n", tempoTableOffset);
			foundTable3 = 1;
		}
	}

	if (foundTable1 == 1 && foundTable2 == 1)
	{
		/*Get the locations of the actual channel pointers*/
		seqPTables[0] = ReadLE16(&romData[patTableOffset - bankAmt]);
		seqPTables[1] = ReadLE16(&romData[patTableOffset - bankAmt + 2]);
		seqPTables[2] = ReadLE16(&romData[patTableOffset - bankAmt + 4]);
		seqPTables[3] = ReadLE16(&romData[patTableOffset - bankAmt + 6]);

		/*Now convert the song data*/
		songNum = 1;
		i = seqPTables[0] - bankAmt;

		while ((i + bankAmt) < seqPTables[1])
		{
			songPtrs[0] = ReadLE16(&romData[seqPTables[0] + (2 * (songNum - 1)) - bankAmt]);
			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			songPtrs[1] = ReadLE16(&romData[seqPTables[1] + (2 * (songNum - 1)) - bankAmt]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			songPtrs[2] = ReadLE16(&romData[seqPTables[2] + (2 * (songNum - 1)) - bankAmt]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			songPtrs[3] = ReadLE16(&romData[seqPTables[3] + (2 * (songNum - 1)) - bankAmt]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			songSeqPtrs = ReadLE16(&romData[seqTableOffset + (2 * (songNum - 1)) - bankAmt]);
			printf("Song %i sequence pointers: 0x%04X\n", songNum, songSeqPtrs);
			songSeqData = ReadLE16(&romData[seqDataOffset + (2 * (songNum - 1)) - bankAmt]);
			printf("Song %i sequence data: 0x%04X\n", songNum, songSeqData);
			songTempo = romData[tempoTableOffset + (songNum - 1) - bankAmt];
			printf("Song %i tempo: %01X\n", songNum, songTempo);
			Beamsong2mid(songNum, songPtrs, songSeqPtrs, songSeqData);
			i += 2;
			songNum++;
		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		free(romData);
		exit(2);
	}
}

void Beamsong2mid(int songNum, long ptrs[4], long seqPtrs, long seqData)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[2];
	unsigned char patCom[3];
	long romPos = 0;
	long ptrPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int curSeq = 0;
	int patTimes = 0;
	int patEnd = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
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
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long curSeqPos = 0;

	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int amtBytes = 0;
	int maskVal = 0;

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

		/*Set initial tempo*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		if (songTempo > 1)
		{
			switch (songTempo)
			{
			case 0x00:
				tempo = 145;
				break;
			case 0x01:
				tempo = 10;
				break;
			case 0x02:
				tempo = 75;
				break;
			case 0x03:
				tempo = 96;
				break;
			case 0x04:
				tempo = 115;
				break;
			case 0x05:
				tempo = 115;
				break;
			case 0x06:
				tempo = 120;
				break;
			case 0x07:
				tempo = 130;
				break;
			case 0x08:
				tempo = 130;
				break;
			case 0x09:
				tempo = 130;
				break;
			case 0x0A:
				tempo = 130;
				break;
			case 0x0B:
				tempo = 140;
				break;
			case 0x0C:
				tempo = 140;
				break;
			case 0x0D:
				tempo = 140;
				break;
			case 0x0E:
				tempo = 145;
				break;
			case 0x0F:
				tempo = 145;
				break;
			default:
				tempo = 145;
				break;
			}

		}
		else
		{
			tempo = 145;
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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;
			curDelay = 0;
			ctrlDelay = 0;
			patTimes = -1;

			if (ptrs[curTrack] != 0x0000)
			{
				romPos = ptrs[curTrack];
				patEnd = 0;
				seqEnd = 1;
			}
			else
			{
				patEnd = 1;
			}

			while (patEnd == 0)
			{
				patCom[0] = romData[romPos - bankAmt];
				patCom[1] = romData[romPos + 1 - bankAmt];
				patCom[2] = romData[romPos + 2 - bankAmt];

				/*End song without loop*/
				if (patCom[0] == 0x00)
				{
					patEnd = 1;
				}

				/*Play sequence*/
				else if (patCom[0] == 0x01)
				{
					amtBytes = 2;
					curSeq = patCom[1];
					ptrPos = songSeqPtrs - bankAmt;
					curSeqPos = ReadLE16(&romData[ptrPos + (curSeq * 2)]) + ReadLE16(&romData[seqDataOffset + ((songNum - 1) * 2) - bankAmt]) - bankAmt;
					seqPos = curSeqPos;
					seqEnd = 0;
				}

				/*Go to pattern loop*/
				else if (patCom[0] == 0x02)
				{
					patEnd = 1;
				}

				/*Jump to pattern position multiple times*/
				else if (patCom[0] == 0x03)
				{
					if (patTimes == -1)
					{
						patTimes = patCom[2];
					}
					else if (patTimes > 0)
					{
						romPos = ptrs[curTrack] + (patCom[1]);
						patTimes--;
					}
					else
					{
						romPos += 3;
						patTimes = -1;
					}
				}

				while (seqEnd == 0 && midPos < 48000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];

					/*Rest*/
					if (command[0] == 0x00)
					{
						maskVal = command[1] & 0x1F;
						curNoteLen = BeamNoteLenTab[maskVal] * 5;
						curDelay += curNoteLen;
						seqPos += 2;
					}

					/*Play note*/
					else if (command[0] < 0x80)
					{
						curNote = command[0] - 1;

						maskVal = command[1] & 0x1F;
						curNoteLen = BeamNoteLenTab[maskVal] * 5;
						tempPos = WriteNoteEventGen(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos += 2;
					}

					/*Change instrument*/
					else if (command[0] >= 0x80 && command[0] < 0xFA)
					{
						curInst = command[0] - 0x80;
						firstNote = 1;
						seqPos++;
					}

					/*End of sequence*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
						romPos += amtBytes;
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