/*Graphic Research (1st driver)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "GRC1.H"

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
long lenTablePtrLoc;
long lenTableOffset;
int firstPtr;
int foundTable;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;

unsigned char* ctrlMidData;

long midLength;

const unsigned char GRC1MagicBytes[4] = { 0x6F, 0x3E, 0x00, 0xCE };
const unsigned char GRC1MagicBytesLen[7] = { 0x85, 0x6F, 0x3E, 0x00, 0x8C, 0x67, 0x7E };

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
void GRC1song2mid(int songNum, long ptr);


void GRC1Proc(int bank)
{
	drvVers = GRC1_VER_STD;
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
		if (!memcmp(&romData[i], GRC1MagicBytes, 4) && foundTable != 1)
		{
			tablePtrLoc = i - 1;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc] + (romData[tablePtrLoc + 5] * 0x100);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Now find the note length directory table*/

	if (foundTable == 1)
	{
		for (i = 0; i < (bankSize * 2); i++)
		{
			if (!memcmp(&romData[i], GRC1MagicBytesLen, 7) && foundTable == 1)
			{
				lenTablePtrLoc = i - 2;
				printf("Found pointer to note length list table at address 0x%04X!\n", lenTablePtrLoc);
				lenTableOffset = ReadLE16(&romData[lenTablePtrLoc]);
				printf("Note length list table starts at 0x%04X...\n", lenTableOffset);
				foundTable = 2;
			}
		}
	}

	if (foundTable == 2)
	{
		i = tableOffset;
		songNum = 1;
		firstPtr = ReadLE16(&romData[i]);
		while (ReadLE16(&romData[i]) < (bankSize * 2) && i != firstPtr)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			GRC1song2mid(songNum, songPtr);
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
		exit(1);
	}

}

/*Convert the song data to MIDI*/
void GRC1song2mid(int songNum, long ptr)
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
		case GRC1_VER_STD:
			/*Fall-through*/
		default:
			for (j = 0x00; j < 0x1F; j++)
			{
				EventMap[j] = GRC1_EVENT_NOTE_LEN;
			}
			for (j = 0x20; j < 0x2F; j++)
			{
				EventMap[j] = GRC1_EVENT_WAVEFORM;
			}
			for (j = 0x30; j < 0x3F; j++)
			{
				EventMap[j] = GRC1_EVENT_MASK;
			}
			for (j = 0x40; j < 0x5F; j++)
			{
				EventMap[j] = GRC1_EVENT_REST_WITH_LEN;
			}
			for (j = 0x60; j < 0x6F; j++)
			{
				EventMap[j] = GRC1_EVENT_OCTAVE;
			}
			for (j = 0x70; j < 0x7F; j++)
			{
				EventMap[j] = GRC1_EVENT_NOTE_LEN_TABLE;
			}
			for (j = 0x80; j < 0x8F; j++)
			{
				EventMap[j] = GRC1_EVENT_NOTE;
			}
			for (j = 0x90; j < 0x9F; j++)
			{
				EventMap[j] = GRC1_EVENT_NOTE_WITH_LEN;
			}
			for (j = 0xA0; j < 0xAF; j++)
			{
				EventMap[j] = GRC1_EVENT_UNKNOWN1;
			}
			for (j = 0xB0; j < 0xCF; j++)
			{
				EventMap[j] = GRC1_EVENT_ENV_SEQ;
			}
			for (j = 0xD0; j < 0xDF; j++)
			{
				EventMap[j] = GRC1_EVENT_DUTY;
			}
			for (j = 0xE0; j < 0xEF; j++)
			{
				EventMap[j] = GRC1_EVENT_VIBRATO;
			}
			GRC1_STATUS_NOTE_LEN_MIN = 0x00;
			GRC1_STATUS_NOTE_LEN_MAX = 0x1F;
			GRC1_STATUS_REST_WITH_LEN_MIN = 0x40;
			GRC1_STATUS_REST_WITH_LEN_MAX = 0x5F;
			GRC1_STATUS_ENV_SEQ_MIN = 0xB0;
			GRC1_STATUS_ENV_SEQ_MAX = 0xCF;
			EventMap[0xF0] = GRC1_EVENT_SWEEP_ON;
			EventMap[0xF1] = GRC1_EVENT_SWEEP_OFF;
			EventMap[0xF2] = GRC1_EVENT_TUNING;
			EventMap[0xF3] = GRC1_EVENT_REST;
			EventMap[0xF4] = GRC1_EVENT_SINGLE_SHOT;
			EventMap[0xF5] = GRC1_EVENT_DETUNE;
			EventMap[0xF6] = GRC1_EVENT_TRANSPOSE;
			EventMap[0xF7] = GRC1_EVENT_NOP;
			EventMap[0xF8] = GRC1_EVENT_NOP;
			EventMap[0xF9] = GRC1_EVENT_NOP;
			EventMap[0xFA] = GRC1_EVENT_JUMP;
			EventMap[0xFB] = GRC1_EVENT_REPEAT_START;
			EventMap[0xFC] = GRC1_EVENT_REPEAT_END;
			EventMap[0xFD] = GRC1_EVENT_CALL;
			EventMap[0xFE] = GRC1_EVENT_RETURN;
			EventMap[0xFF] = GRC1_EVENT_STOP;
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
			seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 3;
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
			curNoteLenTab = ReadLE16(&romData[lenTableOffset]);

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
			if (romData[romPos + 2] == 0)
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

				highNibble = (command[0] & 15);

				if (EventMap[command[0]] == GRC1_EVENT_UNKNOWN0 || EventMap[command[0]] == GRC1_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_UNKNOWN1)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_NOTE_LEN)
				{
					curNoteLen = romData[curNoteLenTab + (command[0] - GRC1_STATUS_NOTE_LEN_MIN)] * 5;
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_WAVEFORM)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_MASK)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_REST_WITH_LEN)
				{
					tempNoteLen = romData[curNoteLenTab + (command[0] - GRC1_STATUS_REST_WITH_LEN_MIN)] * 5;
					curDelay += tempNoteLen;
					ctrlDelay += tempNoteLen;
					masterDelay += tempNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_OCTAVE)
				{
					octave = highNibble;
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_NOTE_LEN_TABLE)
				{
					curNoteLenTab = lenTableOffset + (highNibble * 20);
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_NOTE)
				{
					curNote = (octave * 12) + highNibble + transpose + 11;
					if (curTrack < 2)
					{
						curNote += 12;
					}
					if (curTrack == 3)
					{
						curNote += 24;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_NOTE_WITH_LEN)
				{
					tempNoteLen = romData[curNoteLenTab + command[1]] * 5;
					curNote = (octave * 12) + highNibble + transpose + 11;
					if (curTrack < 2)
					{
						curNote += 12;
					}
					if (curTrack == 3)
					{
						curNote += 24;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, tempNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_ENV_SEQ)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_DUTY)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_VIBRATO)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_SWEEP_ON)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_SWEEP_OFF)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_REST)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_SINGLE_SHOT)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_DETUNE)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_JUMP)
				{
					if (ReadLE16(&romData[seqPos + 1]) > seqPos)
					{
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == GRC1_EVENT_REPEAT_START)
				{
					repeatStart = seqPos + 2;
					repeatTimes = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == GRC1_EVENT_REPEAT_END)
				{
					if (repeatTimes > 1)
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

				else if (EventMap[command[0]] == GRC1_EVENT_CALL)
				{
					if (inMacro >= 8)
					{
						seqEnd = 1;
					}
					else
					{
						inMacro++;
						macros[inMacro][0] = ReadLE16(&romData[seqPos + 1]);
						macros[inMacro][1] = seqPos + 3;
						seqPos = macros[inMacro][0];
					}
				}

				else if (EventMap[command[0]] == GRC1_EVENT_RETURN)
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

				else if (EventMap[command[0]] == GRC1_EVENT_STOP)
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