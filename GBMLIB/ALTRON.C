/*Altron*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "ALTRON.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
long stepPtrLoc;
long stepOffset;
long stepTab;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
int drvVers;
int multiBanks;
int curBank;

char folderName[100];

const unsigned char AltronMagicBytes[7] = { 0x87, 0x87, 0x87, 0x5F, 0x16, 0x00, 0x21 };
const unsigned char AltronTableMagicBytes[7] = { 0xE5, 0x07, 0xE9, 0x07, 0xEC, 0x07, 0xEE };
unsigned char NoteLens[10];

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
void Altronsong2mid(int songNum, long ptrs[4]);

void AltronProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	curInst = 0;
	drvVers = 1;

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
	}

	if (bank < 2)
	{
		bank = 2;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], AltronMagicBytes, 7)) && foundTable != 1)
		{
			tablePtrLoc = i + 7;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Now look for start of note length data*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], AltronTableMagicBytes, 7)) && foundTable == 1)
		{
			stepOffset = i + 7;
			printf("Note length data starts at 0x%04x...\n", stepOffset);
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		while (ReadLE16(&romData[i]) >= 0x4000 && ReadLE16(&romData[i]) < 0x8000)
		{
			seqPtrs[0] = ReadLE16(&romData[i]);
			seqPtrs[1] = ReadLE16(&romData[i + 2]);
			seqPtrs[2] = ReadLE16(&romData[i + 4]);
			seqPtrs[3] = ReadLE16(&romData[i + 6]);

			printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
			printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
			printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);

			if (drvVers == ALTRON_VER_ROX)
			{
				printf("Song %i note lengths: 0x%04X\n", songNum, seqPtrs[3]);
			}
			else
			{
				printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[0]);
			}

			Altronsong2mid(songNum, seqPtrs);
			i += 8;
			songNum++;
		}

		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void Altronsong2mid(int songNum, long ptrs[4])
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
	int curNoteLen = 0;
	int autoLen = 0;
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
	int octave = 0;

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
			autoLen = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
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

			if (seqPtrs[curTrack] >= bankSize && seqPtrs[curTrack] < (bankSize * 2))
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];

				if (curTrack == 3 && drvVers == ALTRON_VER_ROX)
				{
					seqEnd = 1;
				}
			}
			else
			{
				seqEnd = 1;
			}

			/*Early driver*/
			if (drvVers == ALTRON_VER_ROX)
			{
				AltronEventMap[0x3C] = ALTRON_EVENT_OCTAVE_DOWN;
				AltronEventMap[0x3E] = ALTRON_EVENT_OCTAVE_UP;
				AltronEventMap[0x48] = ALTRON_EVENT_RESTART;
				AltronEventMap[0x4C] = ALTRON_EVENT_NOTE_LEN;
				AltronEventMap[0x4F] = ALTRON_EVENT_OCTAVE;
				AltronEventMap[0x52] = ALTRON_EVENT_REST;
				AltronEventMap[0x53] = ALTRON_EVENT_END;

				AltronEventMap[0x43] = ALTRON_EVENT_NOTEC;
				AltronEventMap[0x44] = ALTRON_EVENT_NOTED;
				AltronEventMap[0x45] = ALTRON_EVENT_NOTEE;
				AltronEventMap[0x46] = ALTRON_EVENT_NOTEF;
				AltronEventMap[0x47] = ALTRON_EVENT_NOTEG;
				AltronEventMap[0x41] = ALTRON_EVENT_NOTEA;
				AltronEventMap[0x42] = ALTRON_EVENT_NOTEB;
				AltronEventMap[0x55] = ALTRON_EVENT_NOTECS;
				AltronEventMap[0x57] = ALTRON_EVENT_NOTEDS;
				AltronEventMap[0x58] = ALTRON_EVENT_NOTEFS;
				AltronEventMap[0x59] = ALTRON_EVENT_NOTEGS;
				AltronEventMap[0x5A] = ALTRON_EVENT_NOTEAS;

				ALTRON_STATUS_NOTE_LEN_MIN = 0x30;
				ALTRON_STATUS_NOTE_LEN_MAX = 0x39;

				/*Get note length values*/
				stepTab = stepOffset + (romData[seqPtrs[3]] * 10);

				for (j = 0; j < 10; j++)
				{
					NoteLens[j] = romData[stepTab + j];
				}

			}
			/*Later driver*/
			else if (drvVers == ALTRON_VER_LM)
			{
				ALTRON_STATUS_NOTE_MIN = 0x00;
				ALTRON_STATUS_NOTE_MAX = 0xFC;

				AltronEventMap[0xFD] = ALTRON_EVENT_REST;
				AltronEventMap[0xFE] = ALTRON_EVENT_RESTART;
				AltronEventMap[0xFF] = ALTRON_EVENT_END;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				if (drvVers == ALTRON_VER_LM && command[0] >= ALTRON_STATUS_NOTE_MIN && command[0] <= ALTRON_STATUS_NOTE_MAX)
				{
					curNote = command[0] - ALTRON_STATUS_NOTE_MIN;
					curNoteLen = command[1] * 5;
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos += 2;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_REST)
				{
					if (drvVers == ALTRON_VER_ROX)
					{
						curDelay += (NoteLens[(command[1] - 0x30)] * 5);
						ctrlDelay += (NoteLens[(command[1] - 0x30)] * 5);
					}
					else
					{
						curDelay += (command[1] * 5);
						ctrlDelay += (command[1] * 5);
					}
					seqPos += 2;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_RESTART)
				{
					seqEnd = 1;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_END)
				{
					seqEnd = 1;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_OCTAVE_DOWN)
				{
					octave--;
					if (octave < 0)
					{
						octave = 0;
					}
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_OCTAVE_UP)
				{
					octave++;
					if (octave > 7)
					{
						octave = 0;
					}
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTE_LEN)
				{
					autoLen = NoteLens[(command[1] - 0x30)] * 5;
					seqPos += 2;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_OCTAVE)
				{
					octave = command[1] - 0x30;

					if (octave > 7 || octave < 0)
					{
						octave = 0;
					}

					seqPos += 2;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEC)
				{
					curNote = (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTED)
				{
					curNote = 2 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEE)
				{
					curNote = 4 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEF)
				{
					curNote = 5 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEG)
				{
					curNote = 7 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEA)
				{
					curNote = 9 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;

				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEB)
				{
					curNote = 11 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTECS)
				{
					curNote = 1 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEDS)
				{
					curNote = 3 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEFS)
				{
					curNote = 6 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEGS)
				{
					curNote = 8 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

				else if (AltronEventMap[command[0]] == ALTRON_EVENT_NOTEAS)
				{
					curNote = 10 + (octave * 12);

					if (command[1] >= ALTRON_STATUS_NOTE_LEN_MIN && command[1] <= ALTRON_STATUS_NOTE_LEN_MAX)
					{
						curNoteLen = NoteLens[command[1] - 0x30] * 5;
						seqPos++;
					}
					else
					{
						curNoteLen = autoLen;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos++;
				}

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