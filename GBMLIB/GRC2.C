/*Graphic Research (2nd driver)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "GRC2.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
int i, j;
char outfile[1000000];
int songNum;
int songPtr;
long curPtr;
long bankAmt;
int curVol;
int drvVers;
long tablePtrLoc;
long tableOffset;
int firstPtr;
int foundTable;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;

unsigned char* ctrlMidData;

long midLength;

const unsigned char GRC2MagicBytes[6] = { 0x19, 0x2A, 0x66, 0x6F, 0x19, 0xC9 };
int GRC2noteVals[16] = { 0, 2, 4, 5, 7, 9, 11, 255, 1, 3, 6, 7, 8, 10, 12, 255 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void GRC2song2mid(int songNum, long songPtr);

void GRC2Proc(int bank)
{
	drvVers = GRC2_VER_STD;
	curVol = 120;

	if (bank < 0x02)
	{
		bankAmt = 0x0000;
		bank = 0x02;
	}
	else
	{
		bankAmt = bankSize;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for base table*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], GRC2MagicBytes, 6) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;
		firstPtr = tableOffset + ReadLE16(&romData[i]);

		while (i < firstPtr)
		{
			if (ReadLE16(&romData[i]) != 0x0000)
			{
				songPtr = tableOffset + ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				GRC2song2mid(songNum, songPtr);
			}
			else
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X (invalid, skipping)\n", songNum, songPtr);
			}
			i += 2;
			songNum++;


		}
		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}

}

/*Convert the song data to MIDI*/
void GRC2song2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int ticks = 120;
	int curNoteLenTab = 0;
	int k = 0;
	int repeatStart = 0;
	int repeatTimes = 0;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curNoteLen = 0;
	int tempNoteLen = 0;
	int lastNote = 0;
	int octave = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 150;

	int curInst = 0;

	int tie = 0;

	unsigned long seqPos = 0;

	unsigned char command[3];

	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	unsigned long seqPtrs[4];

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int inMacro = 0;
	int macros[8][2];

	int tempoPos = 0;

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

		switch (drvVers)
		{
		case GRC2_VER_STD:
			/*Fall-through*/
		default:
			GRC2_STATUS_NOTE_MIN = 0x00;
			GRC2_STATUS_NOTE_MAX = 0x7F;
			GRC2_STATUS_OCTAVE_MIN = 0x90;
			GRC2_STATUS_OCTAVE_MAX = 0x9F;
			EventMap[0xE9] = GRC2_EVENT_UNKNOWN1;
			EventMap[0xEA] = GRC2_EVENT_UNKNOWN1;
			EventMap[0xEB] = GRC2_EVENT_REPEAT_END;
			EventMap[0xEC] = GRC2_EVENT_REPEAT_START;
			EventMap[0xED] = GRC2_EVENT_PROG_CHANGE;
			EventMap[0xEE] = GRC2_EVENT_SWEEP;
			EventMap[0xEF] = GRC2_EVENT_TUNING;
			EventMap[0xF0] = GRC2_EVENT_RESET;
			EventMap[0xF1] = GRC2_EVENT_NOP;
			EventMap[0xF2] = GRC2_EVENT_NOP;
			EventMap[0xF3] = GRC2_EVENT_NOP;
			EventMap[0xF4] = GRC2_EVENT_NOP;
			EventMap[0xF5] = GRC2_EVENT_NOP;
			EventMap[0xF6] = GRC2_EVENT_NOP;
			EventMap[0xF7] = GRC2_EVENT_NOP;
			EventMap[0xF8] = GRC2_EVENT_RETURN;
			EventMap[0xF9] = GRC2_EVENT_CALL;
			EventMap[0xFA] = GRC2_EVENT_JUMP;
			EventMap[0xFB] = GRC2_EVENT_VIBRATO;
			EventMap[0xFC] = GRC2_EVENT_ENV_SEQ;
			EventMap[0xFD] = GRC2_EVENT_NOTE_LEN;
			EventMap[0xFE] = GRC2_EVENT_TIE;
			EventMap[0xFF] = GRC2_EVENT_STOP;
			break;
		}

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

		/*Get track pointers*/
		romPos = songPtr;
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (ReadLE16(&romData[romPos + 1]) != 0x0000)
			{
				seqPtrs[curTrack] = tableOffset + ReadLE16(&romData[romPos + 1]);
				romPos += 3;
			}
			else
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos + 1]);
				romPos += 3;
			}
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			transpose = 0;
			inMacro = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			tempNoteLen = 0;
			curVol = 120;
			repeatTimes = -1;
			octave = 0;

			if (curTrack == 3)
			{
				octave = 3;
			}

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

			romPos = songPtr + (curTrack * 3);
			seqPos = seqPtrs[curTrack];
			if (romData[romPos] == 0)
			{
				seqEnd = 1;
			}
			if (seqPtrs[curTrack] == 0x0000)
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				if (command[0] >= GRC2_STATUS_NOTE_MIN && command[0] <= GRC2_STATUS_NOTE_MAX)
				{
					curNote = command[0] & 0x0F;

					if (curNote == 0x07 || curNote == 0x0F)
					{
						if ((command[0] & 0x10) != 0x00)
						{
							tempNoteLen = command[1] * 5;
							curDelay += tempNoteLen;
							ctrlDelay += tempNoteLen;
							masterDelay += tempNoteLen;
							seqPos++;
						}
						else
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
					}
					else
					{
						curNote = GRC2noteVals[(command[0] & 0x0F)] + (octave * 12) + transpose;
						if (curTrack != 2)
						{
							curNote += 12;
						}
						if ((command[0] & 0x10) != 0x00)
						{
							tempNoteLen = command[1] * 5;
							seqPos++;
							tempPos = WriteNoteEvent(midData, midPos, curNote, tempNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						else
						{
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
					}
					seqPos++;
				}

				else if (command[0] >= GRC2_STATUS_OCTAVE_MIN && command[0] <= GRC2_STATUS_OCTAVE_MAX)
				{
					octave = command[0] & 0x0F;
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_UNKNOWN0 || EventMap[command[0]] == GRC2_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_UNKNOWN1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_REPEAT_END)
				{
					if (repeatTimes > 0)
					{
						seqPos = repeatStart;
						repeatTimes--;
					}
					else
					{
						repeatTimes = -1;
						seqPos++;
					}
				}

				else if (EventMap[command[0]] == GRC2_EVENT_REPEAT_START)
				{
					repeatStart = seqPos + 2;
					repeatTimes = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_PROG_CHANGE)
				{
					curInst = command[1];
					firstNote = 1;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_RESET)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_RETURN)
				{
					if (inMacro > 0)
					{
						seqPos = macros[inMacro][1];
						inMacro--;
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == GRC2_EVENT_CALL)
				{
					if (inMacro >= 7)
					{
						seqEnd = 1;
					}
					else
					{
						inMacro++;
						macros[inMacro][0] = (seqPos + (ReadLE16(&romData[seqPos + 1]))) & 0xFFFF;
						macros[inMacro][1] = seqPos + 3;
						seqPos = macros[inMacro][0];
					}
				}

				else if (EventMap[command[0]] == GRC2_EVENT_JUMP)
				{
					tempPos = (seqPos + (ReadLE16(&romData[seqPos + 1]))) & 0xFFFF;
					if (tempPos > seqPos)
					{
						seqPos = tempPos;
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == GRC2_EVENT_VIBRATO)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_ENV_SEQ)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_NOTE_LEN)
				{
					curNoteLen = command[1] * 5;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_TIE)
				{
					tie = 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC2_EVENT_STOP)
				{
					seqEnd = 1;
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