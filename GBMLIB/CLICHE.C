/*Studio Cliche*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CLICHE.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
int i, j;
char outfile[1000000];
int songNum;
int songBank;
int songPtr;
int numSongs;
long curPtr;
long bankAmt;
int curVol;
int drvVers;
long tableOffset;
int firstPtr;

int multiBanks;
int curBank;

char folderName[100];

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
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Clichesong2mid(int songNum, long songPtr);

void ClicheProc(int bank, char parameters[4][100])
{
	drvVers = CLICHE_VER_STD;
	curVol = 120;

	tableOffset = strtol(parameters[0], NULL, 16);
	numSongs = strtol(parameters[1], NULL, 16);

	if (parameters[2][0] != 0)
	{
		drvVers = strtol(parameters[2], NULL, 16);
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	i = tableOffset;
	songNum = 1;

	if (drvVers == CLICHE_VER_STD)
	{
		while (songNum <= numSongs)
		{
			songBank = romData[i + 1];
			songPtr = ReadLE16(&romData[i + 2]);

			/*Copy the current bank data from the ROM*/
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, (songBank * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);

			printf("Song %i, bank %02X: 0x%04X\n", songNum, songBank, songPtr);
			Clichesong2mid(songNum, songPtr);
			free(exRomData);
			i += 4;
			songNum++;
		}
	}
	else
	{
		songBank = bank - 1;
		while (songNum <= numSongs)
		{
			songPtr = i + 2 + ReadLE16(&romData[i]);

			/*Copy the current bank data from the ROM*/
			fseek(rom, 0, SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData, 1, bankSize, rom);
			fseek(rom, (songBank * bankSize), SEEK_SET);
			fread(exRomData + bankSize, 1, bankSize, rom);

			printf("Song %i, bank %02X: 0x%04X\n", songNum, songBank, songPtr);
			Clichesong2mid(songNum, songPtr);
			free(exRomData);
			i += 2;
			songNum++;
		}
	}

}

/*Convert the song data to MIDI*/
void Clichesong2mid(int songNum, long songPtr)
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
		case CLICHE_VER_STD:
			/*Fall-through*/
		case CLICHE_VER_DM:
		case CLICHE_VER_MMB:
		default:
			CLICHE_STATUS_NOTE_MIN = 0x00;
			CLICHE_STATUS_NOTE_MAX = 0x7E;
			CLICHE_STATUS_REST = 0x7F;
			EventMap[0xF0] = CLICHE_EVENT_JUMP;
			EventMap[0xF1] = CLICHE_EVENT_CALL;
			EventMap[0xF2] = CLICHE_EVENT_RETURN;
			EventMap[0xF3] = CLICHE_EVENT_REPEAT_START;
			EventMap[0xF4] = CLICHE_EVENT_REPEAT_END;
			EventMap[0xF5] = CLICHE_EVENT_VIBRATO;
			EventMap[0xF6] = CLICHE_EVENT_PROG_CHANGE;
			EventMap[0xF7] = CLICHE_EVENT_ENV_SEQ;
			EventMap[0xF8] = CLICHE_EVENT_NOP;
			EventMap[0xF9] = CLICHE_EVENT_TRANSPOSE;
			EventMap[0xFA] = CLICHE_EVENT_TUNING;
			EventMap[0xFB] = CLICHE_EVENT_PAN;
			EventMap[0xFC] = CLICHE_EVENT_UNKNOWN1;
			EventMap[0xFD] = CLICHE_EVENT_UNKNOWN0;
			EventMap[0xFE] = CLICHE_EVENT_STOP;
			EventMap[0xFF] = CLICHE_EVENT_STOP;
			break;
		}

		if (drvVers == CLICHE_VER_MMB)
		{
			EventMap[0xFD] = CLICHE_EVENT_UNKNOWN1;
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

		if (drvVers != CLICHE_VER_MMB)
		{
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				if (ReadLE16(&exRomData[romPos + 1]) != 0x0000)
				{
					seqPtrs[curTrack] = romPos + 3 + ReadLE16(&exRomData[romPos + 1]);
					romPos += 3;
				}
				else
				{
					seqPtrs[curTrack] = ReadLE16(&exRomData[romPos + 1]);
					romPos += 3;
				}
			}
		}
		else
		{
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				seqPtrs[curTrack] = 0x0000;
			}
			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				if (exRomData[romPos] == 0x00)
				{
					break;
				}
				else
				{
					seqPtrs[curTrack] = romPos + 3 + ReadLE16(&exRomData[romPos + 1]);
					romPos += 3;
				}
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
			if (exRomData[romPos] == 0)
			{
				seqEnd = 1;
			}
			if (seqPtrs[curTrack] == 0x0000)
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];

				if (command[0] >= CLICHE_STATUS_NOTE_MIN && command[0] <= CLICHE_STATUS_NOTE_MAX)
				{
					if (songNum == 140)
					{
						songNum = 140;
					}
					curNote = command[0] + transpose + 24;
					if (curTrack < 2)
					{
						curNote += 12;
					}
					curNoteLen = command[1] * 5;

					if (command[2] == 0xF8)
					{
						seqPos++;
					}
					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					seqPos += 2;
				}

				else if (command[0] == CLICHE_STATUS_REST)
				{
					curNoteLen = command[1] * 5;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_UNKNOWN0 || EventMap[command[0]] == CLICHE_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_UNKNOWN1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_JUMP)
				{
					tempPos = (seqPos + 3 + (ReadLE16(&exRomData[seqPos + 1]))) & 0xFFFF;
					if (tempPos > seqPos)
					{
						seqPos = tempPos;
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_CALL)
				{
					if (inMacro >= 7)
					{
						seqEnd = 1;
					}
					else
					{
						inMacro++;
						macros[inMacro][0] = (seqPos + 3 + (ReadLE16(&exRomData[seqPos + 1]))) & 0xFFFF;
						macros[inMacro][1] = seqPos + 3;
						seqPos = macros[inMacro][0];
					}
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_RETURN)
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

				else if (EventMap[command[0]] == CLICHE_EVENT_REPEAT_START)
				{
					repeatStart = seqPos + 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_REPEAT_END)
				{
					if (repeatTimes == -1)
					{
						repeatTimes = command[1];
					}
					if (repeatTimes > 1)
					{
						seqPos = repeatStart;
						repeatTimes--;
					}
					else
					{
						repeatTimes = -1;
						seqPos += 2;
					}
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_VIBRATO)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_PROG_CHANGE)
				{
					curInst = command[1];
					firstNote = 1;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_ENV_SEQ)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_TRANSPOSE)
				{
					if (songNum == 2343)
					{
						songNum = 2343;
					}
					transpose += (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == CLICHE_EVENT_STOP)
				{
					seqEnd = 1;
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
