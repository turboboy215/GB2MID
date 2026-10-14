/*AudioArts*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "AUDIOART.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long seqPtrLoc;
long seqOffset;
int i, j;
char outfile[1000000];
int format = 0;
int songNum;
long songPtrs[4];
long songTempo;
long nextPtr;
long endPtr;
long bankAmt;
int multiBanks;
int curVol;
int curInst;
int drvVers;
int foundTable;
int trackFmt;
int chanMask;
long morphWave;
long wave;
int maskArray[4];
int songBase;
int progBank;

char folderName[100];

const char Table1Find[15] = { 0x16, 0x00, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19 };
const char Table2Find[12] = { 0x1A, 0x22, 0x13, 0x1A, 0x13, 0x73, 0x2C, 0x72, 0x5F, 0x16, 0x00, 0x21 };

/*Alternate method for another variation of the driver (e.g. Extreme Ghostbusters, Barca, Wendy)*/
const char Table2FindAlt[4] = { 0x72, 0x59, 0x57, 0x21 };

/*Alternate code to look for in Carmageddon (early driver?)*/
const char Table1FindCarma[9] = { 0x16, 0x00, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x2A };
const char Table2FindCarma[13] = { 0x1A, 0x77, 0x13, 0x1A, 0x13, 0x23, 0x73, 0x23, 0x72, 0x5F, 0x16, 0x00, 0x21 };

/*Look for these bytes since there isn't a standard table in the US version of Casper*/
const char Table1FindCU[10] = { 0xE0, 0x66, 0xCA, 0x4A, 0x36, 0x4B, 0xA2, 0x4B, 0x0E, 0x4C };

unsigned long seqList[500];
int totalSeqs;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOnPB(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst, int progBank);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void AAsong2mid(int songNum, long ptrs[4], int speed);

void AAProc(int bank)
{
	drvVers = AA_VER_OLD;
	curVol = 120;
	curInst = 0;
	foundTable = 0;
	trackFmt = AA_TRACK_VER8;

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

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for song pattern table loader*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Table1Find, 15))
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			if (romData[i + 15] == 0x2A)
			{
				drvVers = AA_VER_OLD;
				printf("Detected old table format.\n");
			}
			else if (romData[i + 15] == 0x19)
			{
				drvVers = AA_VER_NEW;
				printf("Detected new table format.\n");
			}
			printf("Song table starts at 0x%04X...\n", tableOffset);
			break;
		}
	}

	/*Alternate method for Carmageddon*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Table1FindCarma, 9) && bank == 0x42)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			drvVers = AA_VER_CARMA;
			printf("Detected old (Carmageddon) table format.\n");
			printf("Song table starts at 0x%04X...\n", tableOffset);
			break;
		}
	}

	/*Now try to search the bank for sequence table loader*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Table2Find, 12))
		{
			seqPtrLoc = i + 12;
			printf("Found pointer to sequence table at address 0x%04X!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc]);
			printf("Sequence table starts at 0x%04X...\n", seqOffset);
			break;
		}
	}

	/*Alternate method for Carmageddon*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Table2FindCarma, 13) && drvVers != AA_VER_OLD && drvVers != AA_VER_NEW)
		{
			seqPtrLoc = i + 13;
			printf("Found pointer to sequence table at address 0x%04X!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc]);
			printf("Sequence table starts at 0x%04X...\n", seqOffset);
			break;
		}
	}

	/*Alternate method for Total Soccer 2000/Barca/Wendy*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Table2FindAlt, 4))
		{
			seqPtrLoc = i + 4;
			printf("Found pointer to sequence table at address 0x%04X!\n", seqPtrLoc);
			seqOffset = ReadLE16(&romData[seqPtrLoc]);
			printf("Sequence table starts at 0x%04X...\n", seqOffset);
			trackFmt = AA_TRACK_VER16;
			break;
		}
	}

	/*Workaround for Casper (US)*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Table1FindCU, 10) && bank == 0x20)
		{
			tablePtrLoc = 0x4949;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = 0x4949;
			drvVers = AA_VER_CASPU;
			trackFmt = AA_TRACK_VER16;
			printf("Detected old table format.\n");
			printf("Song table starts at 0x%04X...\n", tableOffset);
			break;
		}
	}

	if (tableOffset != NULL && seqOffset != NULL)
	{
		songNum = 1;
		i = tableOffset;

		if (drvVers == AA_VER_OLD)
		{
			if (bank != 0x20)
			{
				while (i < seqOffset)
				{
					songPtrs[0] = ReadLE16(&romData[i]);
					printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
					songPtrs[1] = ReadLE16(&romData[i + 2]);
					printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
					songPtrs[2] = ReadLE16(&romData[i + 4]);
					printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
					songPtrs[3] = ReadLE16(&romData[i + 6]);
					printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
					songTempo = romData[i + 8];
					printf("Song %i tempo: %02X\n", songNum, songTempo);
					morphWave = ReadLE16(&romData[i + 9]);
					printf("Song %i waveform (morph): 0x%04X\n", songNum, morphWave);
					wave = ReadLE16(&romData[i + 11]);
					printf("Song %i waveform (normal): 0x%04X\n", songNum, wave);
					AAsong2mid(songNum, songPtrs, songTempo);

					i += 13;
					songNum++;
				}
			}
			/*Workaround for Casper*/
			else
			{
				while (songNum <= 4)
				{
					songPtrs[0] = ReadLE16(&romData[i]);
					printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
					songPtrs[1] = ReadLE16(&romData[i + 2]);
					printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
					songPtrs[2] = ReadLE16(&romData[i + 4]);
					printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
					songPtrs[3] = ReadLE16(&romData[i + 6]);
					printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
					songTempo = romData[i + 8];
					printf("Song %i tempo: %02X\n", songNum, songTempo);
					morphWave = ReadLE16(&romData[i + 9]);
					printf("Song %i waveform (morph): 0x%04X\n", songNum, morphWave);
					wave = ReadLE16(&romData[i + 11]);
					printf("Song %i waveform (normal): 0x%04X\n", songNum, wave);
					AAsong2mid(songNum, songPtrs, songTempo);

					i += 13;
					songNum++;
				}
			}

		}

		else if (drvVers == AA_VER_NEW)
		{
			while (i < seqOffset)
			{
				songBase = i;
				songTempo = romData[i];
				printf("Song %i tempo: %02X\n", songNum, songTempo);
				chanMask = romData[i + 1];

				printf("Song %i channel mask: %02X\n", songNum, chanMask);
				i += 2;


				if ((chanMask & 0x01) != 0x00)
				{
					songPtrs[0] = ReadLE16(&romData[i]);
					printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
					i += 2;
				}
				else
				{
					songPtrs[0] = 0x0000;
					printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
				}

				if ((chanMask & 0x02) != 0x00)
				{
					songPtrs[1] = ReadLE16(&romData[i]);
					printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
					i += 2;
				}
				else
				{
					songPtrs[1] = 0x0000;
					printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
				}

				if ((chanMask & 0x04) != 0x00)
				{
					songPtrs[2] = ReadLE16(&romData[i]);
					printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
					i += 2;
				}
				else
				{
					songPtrs[2] = 0x0000;
					printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
				}

				if ((chanMask & 0x08) != 0x00)
				{
					songPtrs[3] = ReadLE16(&romData[i]);
					printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
					i += 2;
				}
				else
				{
					songPtrs[3] = 0x0000;
					printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
				}

				i = songBase;

				morphWave = ReadLE16(&romData[i + 10]);
				printf("Song %i waveform (morph): 0x%04X\n", songNum, morphWave);
				wave = ReadLE16(&romData[i + 12]);
				printf("Song %i waveform (normal): 0x%04X\n", songNum, wave);
				AAsong2mid(songNum, songPtrs, songTempo);

				i += 14;
				songNum++;
			}
		}

		else if (drvVers == AA_VER_CARMA)
		{
			while (songNum <= 3)
			{
				songPtrs[0] = ReadLE16(&romData[i]);
				printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
				songPtrs[3] = ReadLE16(&romData[i + 4]);
				printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
				songTempo = 7;
				AAsong2mid(songNum, songPtrs, songTempo);

				i += 6;
				songNum++;
			}
		}

		else if (drvVers == AA_VER_CASPU)
		{
			while (songNum <= 4)
			{
				songPtrs[0] = ReadLE16(&romData[i]);
				printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
				songPtrs[2] = ReadLE16(&romData[i + 4]);
				printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
				songPtrs[3] = ReadLE16(&romData[i + 6]);
				printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
				songTempo = romData[i + 8];
				printf("Song %i tempo: %02X\n", songNum, songTempo);
				wave = ReadLE16(&romData[i + 9]);
				printf("Song %i waveform: 0x%04X\n", songNum, wave);
				AAsong2mid(songNum, songPtrs, songTempo);

				i += 11;
				songNum++;
			}
		}

		free(romData);

	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
}

void AAsong2mid(int songNum, long ptrs[4], int speed)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curVol = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 0;

	int curInst = 0;

	unsigned long patPos = 0;
	unsigned long seqPos = 0;

	unsigned char command[3];

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;

	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;

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

		tempo = 150;

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
		case AA_VER_CARMA:
			/*Fall-through*/
		case AA_VER_OLD:
		case AA_VER_NEW:
		case AA_VER_CASPU:
		default:
			AA_STATUS_END = 0x00;
			AA_STATUS_NOTE_MIN = 0x00;
			AA_STATUS_NOTE_MAX = 0xFC;
			AA_STATUS_JUMP = 0xFF;
			EventMap[0xFF] = AA_EVENT_HOLD;
			EventMap[0xFE] = AA_EVENT_KEY_OFF;
			EventMap[0xFD] = AA_EVENT_KEY_ON;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			holdNote = 0;
			curVol = 120;
			progBank = 0;

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
			patPos = ptrs[curTrack];
			if (ptrs[curTrack] == 0)
			{
				trackEnd = 1;
			}

			while (trackEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				seqEnd = 0;
				/*Get current sequence and transpose*/
				if (trackFmt == AA_TRACK_VER8)
				{
					transpose = (signed char)romData[patPos];
					curSeq = romData[patPos + 1];
				}
				else
				{
					transpose = (signed char)ReadLE16(&romData[patPos]);
					curSeq = ReadLE16(&romData[patPos + 2]);
				}

				if (curSeq == 0xFF)
				{
					trackEnd = 1;
				}


				/*Go to the current sequence*/
				seqPos = ReadLE16(&romData[seqOffset + (curSeq * 2)]);
				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];

					if (command[0] == AA_STATUS_JUMP)
					{
						seqEnd = 1;
						trackEnd = 1;
					}
					else if (command[0] == AA_STATUS_END)
					{
						seqEnd = 1;

						if (trackFmt == AA_TRACK_VER8)
						{
							patPos += 2;
						}
						else
						{
							patPos += 4;
						}
					}

					else if (command[1] >= AA_STATUS_NOTE_MIN && command[1] <= AA_STATUS_NOTE_MAX)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						curNote = command[1] + transpose + 24;

						if (curTrack < 2)
						{
							curNote += 12;
						}

						if (curNote == 37)
						{
							curNote = 37;
						}

						curNoteLen = command[0] * speed * 5;

						if (curInst != command[2])
						{
							curInst = command[2];
							firstNote = 1;
						}
						tempPos = WriteNoteEventOnPB(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst, progBank);
						firstNote = 0;
						midPos = tempPos;
						curDelay = curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						holdNote = 1;
						seqPos += 3;
					}

					else if (EventMap[command[1]] == AA_EVENT_HOLD)
					{
						curDelay += (command[0] * speed * 5);
						ctrlDelay += (command[0] * speed * 5);
						seqPos += 2;
					}

					else if (EventMap[command[1]] == AA_EVENT_KEY_OFF)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						curDelay += (command[0] * speed * 5);
						ctrlDelay += (command[0] * speed * 5);
						seqPos += 2;
					}

					else if (EventMap[command[1]] == AA_EVENT_KEY_ON)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						curNoteLen = command[0] * speed * 5;
						tempPos = WriteNoteEventOnPB(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst, progBank);
						firstNote = 0;
						midPos = tempPos;
						curDelay = curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						holdNote = 1;
						seqPos += 2;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}

				}

			}

			if (holdNote == 1)
			{
				tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
				holdNote = 0;
				midPos = tempPos;
				curDelay = 0;
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