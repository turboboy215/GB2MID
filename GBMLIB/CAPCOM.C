/*Capcom*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CAPCOM.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long curPtr;
long songPtrs[4];
long songPtr;
long bankAmt;
int curVol;
int drvVers;
int foundTable;
int songTempo;
int curInst;
int progBank;
int isSFX;
int octaveFix;

int multiBanks;
int curBank;

char folderName[100];

const char CapMagicBytes87[3] = { 0x79, 0x3D, 0x21 };
const char CapMagicBytes90[5] = { 0x7C, 0x65, 0x6F, 0xB4, 0xC8 };

/*Early version lengths*/
static const int CapLengths87[8] = { 0, 1, 3, 6, 12, 24, 48, 96 };
static const int CapLengths87Triplet[8] = { 0, 1, 2, 4, 8, 16, 32, 64 };

/*Common version lengths*/
static const int CapLengths90[7] = { 3, 6, 12, 24, 48, 96, 192 };
static const int CapLengths90Triplet[7] = { 2, 4, 8, 16, 32, 64, 128 };
static const int CapLengths90Dotted[7] = { 0, 9, 18, 36, 72, 144, 0 };

unsigned char* romData;
unsigned char* multiMidData[8];
unsigned char* midData;

unsigned char* ctrlMidData;

long midLength;

/*Addresses each track has already played: an endless jump back into them is the song's loop point*/
unsigned char visited[4][0x8000];

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int CapWriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int CapWriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int CapWriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int CapWriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Capsong2mid(int songNum, long songPtr);

unsigned int FlushNote(unsigned char* buffer, unsigned int pos, int* holdNote, unsigned int note, int* delay, int gate, int firstNote, int curChan, int inst);

unsigned int CapWriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	deltaValue = CapWriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0xC0 | curChan);
		}
		else
		{
			Write8B(&buffer[pos], 0xC9);
		}

		Write8B(&buffer[pos + 1], inst);
		Write8B(&buffer[pos + 2], 0);

		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			Write8B(&buffer[pos + 3], 0x99);
		}

		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	deltaValue = CapWriteDeltaTime(buffer, pos, length);
	pos += deltaValue;

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}

int CapWriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value)
{
	unsigned char valSize;
	unsigned char* valData;
	unsigned int tempLen;
	unsigned int curPos;

	/*A value of 0 is written as one $00 byte. (With valSize = 0, nothing was written
	  and "valData[valSize - 1] &= 127" cleared bit 7 of the byte BEFORE the delta.)*/
	valSize = 1;
	tempLen = value >> 7;

	while (tempLen != 0)
	{
		tempLen >>= 7;
		valSize++;
	}

	valData = &buffer[pos];
	curPos = valSize;
	tempLen = value;

	while (curPos > 0)
	{
		curPos--;
		valData[curPos] = 128 | (tempLen & 127);
		tempLen >>= 7;
	}

	valData[valSize - 1] &= 127;

	return valSize;
}

unsigned int CapWriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	int midiBank;
	int midiProg;
	int bankChanged;
	unsigned char bankStatus;
	midiBank = inst / 128;
	midiProg = inst % 128;
	bankChanged = (midiBank != progBank);
	deltaValue = CapWriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1 || bankChanged == 1)
	{
		if (bankChanged == 1)
		{
			if (curChan != 3)
			{
				bankStatus = 0xB0 | curChan;
			}
			else
			{
				bankStatus = 0xB9;
			}

			/*Bank Select MSB (CC0)*/
			Write8B(&buffer[pos], bankStatus);
			Write8B(&buffer[pos + 1], 0x00);
			Write8B(&buffer[pos + 2], midiBank);
			pos += 3;
			/*Delta 0*/
			Write8B(&buffer[pos], 0);
			pos++;

			/*Bank Select LSB (CC32)*/
			Write8B(&buffer[pos], bankStatus);
			Write8B(&buffer[pos + 1], 0x20);
			Write8B(&buffer[pos + 2], 0x00);
			pos += 3;
			/*Delta 0*/
			Write8B(&buffer[pos], 0);
			pos++;

			progBank = midiBank;
		}
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0xC0 | curChan);
		}
		else
		{
			Write8B(&buffer[pos], 0xC9);
		}


		Write8B(&buffer[pos + 1], midiProg);
		Write8B(&buffer[pos + 2], 0);

		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			Write8B(&buffer[pos + 3], 0x99);
		}


		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	return pos;

}

unsigned int CapWriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;

	deltaValue = CapWriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0x90 | curChan);
		}
		else
		{
			Write8B(&buffer[pos], 0x99);
		}
		pos++;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}

/*End the held note of a track. The note-off comes "gate" ticks after the note-on, or at
  the current time if that is earlier; the rest of the elapsed time stays in *delay.*/
unsigned int FlushNote(unsigned char* buffer, unsigned int pos, int* holdNote, unsigned int note, int* delay, int gate, int firstNote, int curChan, int inst)
{
	int offDelay;

	if (*holdNote == 1)
	{
		offDelay = *delay;
		if (gate < offDelay)
		{
			offDelay = gate;
		}
		pos = CapWriteNoteEventOff(buffer, pos, note, 0, offDelay, firstNote, curChan, inst);
		*delay -= offDelay;
		*holdNote = 0;
	}
	return pos;
}

void CapProc(int bank, char parameters[4][100])
{
	drvVers = CAPCOM_VER_90;
	curInst = 0;
	curVol = 120;
	foundTable = 0;
	progBank = 0;
	octaveFix = 0;

	if (parameters[0][0] != 0)
	{
		octaveFix = 1;
	}

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

	/*Try to search the bank for song table loader - common version*/
	for (i = bankSize; i < (bankSize * 2) - 5; i++)
	{
		if ((!memcmp(&romData[i], CapMagicBytes90, 5)) && foundTable != 1)
		{
			tablePtrLoc = i - 5;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			drvVers = CAPCOM_VER_90;
		}
	}

	/*Try to search the bank for song table loader - DuckTales*/
	for (i = bankSize; i < (bankSize * 2) - 3; i++)
	{
		if ((!memcmp(&romData[i], CapMagicBytes87, 3)) && foundTable != 1)
		{
			tablePtrLoc = i + 3;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			drvVers = CAPCOM_VER_87;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		if (drvVers == CAPCOM_VER_87)
		{
			while (ReadLE16(&romData[i]) >= bankSize && ReadLE16(&romData[i]) < 0x8000)
			{
				songPtr = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Capsong2mid(songNum, songPtr);
				i += 2;
				songNum++;
			}
		}
		else
		{
			/*The number of sound ids is stored 5 bytes before the table ($4B40); without
			  this limit the scan ran on into the instrument table*/
			while (songNum <= romData[tableOffset - 5] && ((ReadBE16(&romData[i]) >= bankSize && ReadBE16(&romData[i]) < 0x8000) || ReadBE16(&romData[i]) == 0x0000))
			{
				songPtr = ReadBE16(&romData[i]);
				if (songPtr != 0x0000)
				{
					if (romData[songPtr] == 0x00)
					{
						printf("Song %i: 0x%04X\n", songNum, songPtr);
						Capsong2mid(songNum, songPtr);
					}
					else
					{
						printf("Song %i: 0x%04X (SFX, skipping)\n", songNum, songPtr);
					}
				}
				else
				{
					printf("Song %i: 0x%04X (empty, skipping)\n", songNum, songPtr);
				}

				i += 2;
				songNum++;
			}
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
void Capsong2mid(int songNum, long songPtr)
{

	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char command[4];
	int curTrack = 0;
	int curNote = 0;
	int tempNote = 0;
	unsigned int curNotes[4];
	int curNoteLen = 0;
	int curNoteLens[4];
	int curNoteLenTab;
	int curDelay = 0;
	int curDelays[4];
	int ctrlDelay = 0;
	int masterDelay = 0;
	int masterDelays[4];
	int octave = 4;
	int octaves[4];
	int seqEnd = 0;
	int tracksEnd[4] = { 0, 0, 0, 0 };
	int songEnd = 0;
	unsigned int seqPos = 0;
	unsigned int seqPosM[4];
	unsigned int patPos;
	unsigned int patPosM[4];
	unsigned int noteLensPtr;
	unsigned int romPos = 0;
	unsigned int midPos = 0;
	unsigned int midPosM[4];
	long ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	long tempPos = 0;
	int valSize = 0;
	long trackSize = 0;
	long trackSizes[4];
	long ctrlTrackSize = 0;
	int initTempo = 0;
	int tempo = 150;
	int curVols[4];
	int curSpeeds[4];
	int holdNote = 0;
	int holdNotes[4];
	int transpose = 0;
	int transposes[4];
	int octaveUp[4];
	int trackCnt = 4;
	int ticks = 120;
	int k = 0;
	int baseNotes[4];
	int firstNote = 0;
	int firstNotes[4];
	long seqTime = 0;
	int curInsts[4];
	int repeats[4][5][2];
	int dotted[4];
	int triplet[4];
	int connect[4];
	int connect2[4];
	int tieNum[4];
	int tieLens[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int numChan;
	int envVals[4];
	int progBanks[4];
	int songTempo;
	int half;
	long usPerQuarter;
	int noteGates[4];
	int holds[4];
	int gate;
	int connected;
	int pitch;
	int lenIdx;
	int repSlot;
	long ctrlTime = 0;
	long lastTempo = -1;
	long eventTime;
	int pitchBend[4];

	for (curTrack = 0; curTrack < trackCnt; curTrack++)
	{
		midPosM[curTrack] = 0;
	}

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < trackCnt; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < trackCnt; k++)
		{
			multiMidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < trackCnt; k++)
		{
			multiMidData[k][j] = 0;
		}

		ctrlMidData[j] = 0;
	}

	/*5 MIDI ticks per driver tick. A quarter note is 24 driver ticks in DuckTales (120 MIDI
	  ticks) and 48 in the later version (240 MIDI ticks).*/
	if (drvVers == CAPCOM_VER_90)
	{
		ticks = 240;
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
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
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
		CapWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE16(&ctrlMidData[ctrlMidPos], 0xFF03);
		Write8B(&ctrlMidData[ctrlMidPos + 2], 0);
		ctrlMidPos += 2;

		/*Set initial tempo*/
		CapWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
		ctrlMidPos += 3;

		/*Set time signature*/
		CapWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5804);
		ctrlMidPos += 3;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x04021808);
		ctrlMidPos += 4;

		/*Set key signature (FF 59 02 00 00)*/
		CapWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5902);
		ctrlMidPos += 5;

		switch (drvVers)
		{
		case CAPCOM_VER_87:
			CAPCOM_STATUS_END_TICK_MIN = 0x10;
			CAPCOM_STATUS_END_TICK_MAX = 0x20;
			CAPCOM_STATUS_TIE_MIN = 0x21;
			CAPCOM_STATUS_TIE_MAX = 0x2F;
			CAPCOM_STATUS_TRIPLET = 0x30;
			CAPCOM_STATUS_NOTE_MIN = 0x31;
			CAPCOM_STATUS_NOTE_MAX = 0xFF;
			EventMap[0x00] = CAPCOM_EVENT_NOP1;
			EventMap[0x01] = CAPCOM_EVENT_NOP1;
			EventMap[0x02] = CAPCOM_EVENT_DUTY;
			EventMap[0x03] = CAPCOM_EVENT_VOL;
			EventMap[0x04] = CAPCOM_EVENT_REPEAT1;
			EventMap[0x05] = CAPCOM_EVENT_BASE_NOTE;
			EventMap[0x06] = CAPCOM_EVENT_DOTTED;
			EventMap[0x07] = CAPCOM_EVENT_ENV;
			EventMap[0x08] = CAPCOM_EVENT_VIBRATO;
			EventMap[0x09] = CAPCOM_EVENT_STOP;
			EventMap[0x0A] = CAPCOM_EVENT_NOP1;
			EventMap[0x0B] = CAPCOM_EVENT_NOP1;
			EventMap[0x0C] = CAPCOM_EVENT_NOP1;
			EventMap[0x0D] = CAPCOM_EVENT_NOP1;
			EventMap[0x0E] = CAPCOM_EVENT_NOP1;
			EventMap[0x0F] = CAPCOM_EVENT_NOP1;
			break;
		case CAPCOM_VER_90:
			/*Fall-through*/
		default:
			CAPCOM_STATUS_NOTE_MIN = 0x20;
			CAPCOM_STATUS_NOTE_MAX = 0xFF;
			EventMap[0x00] = CAPCOM_EVENT_TRIPLET;
			EventMap[0x01] = CAPCOM_EVENT_CONNECT;
			EventMap[0x02] = CAPCOM_EVENT_DOTTED;
			EventMap[0x03] = CAPCOM_EVENT_OCTAVE_UP;
			EventMap[0x04] = CAPCOM_EVENT_FLAGS;
			EventMap[0x05] = CAPCOM_EVENT_TEMPO;
			EventMap[0x06] = CAPCOM_EVENT_NOTE_DUR;
			EventMap[0x07] = CAPCOM_EVENT_VOL;
			EventMap[0x08] = CAPCOM_EVENT_PROG_CHANGE;
			EventMap[0x09] = CAPCOM_EVENT_OCTAVE;
			EventMap[0x0A] = CAPCOM_EVENT_GLOBAL_TRANSPOSE;
			EventMap[0x0B] = CAPCOM_EVENT_LOCAL_TRANSPOSE;
			EventMap[0x0C] = CAPCOM_EVENT_TUNING;
			EventMap[0x0D] = CAPCOM_EVENT_PITCH_SLIDE;
			EventMap[0x0E] = CAPCOM_EVENT_REPEAT1;
			EventMap[0x0F] = CAPCOM_EVENT_REPEAT2;
			EventMap[0x10] = CAPCOM_EVENT_REPEAT3;
			EventMap[0x11] = CAPCOM_EVENT_REPEAT4;
			EventMap[0x12] = CAPCOM_EVENT_JUMP_FLAGS1;
			EventMap[0x13] = CAPCOM_EVENT_JUMP_FLAGS2;
			EventMap[0x14] = CAPCOM_EVENT_JUMP_FLAGS3;
			EventMap[0x15] = CAPCOM_EVENT_JUMP_FLAGS4;
			EventMap[0x16] = CAPCOM_EVENT_JUMP;
			EventMap[0x17] = CAPCOM_EVENT_STOP;
			EventMap[0x18] = CAPCOM_EVENT_DUTY;
			EventMap[0x19] = CAPCOM_EVENT_PAN;
			break;
		}

		memset(visited, 0, sizeof(visited));

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			songPtrs[curTrack] = 0x0000;
			for (j = 0; j < 5; j++)
			{
				repeats[curTrack][j][0] = 0x0000;
				repeats[curTrack][j][1] = -1;
			}

		}

		/*Get channel pointers*/
		romPos = songPtr;
		if (drvVers == CAPCOM_VER_87)
		{
			songTempo = romData[romPos];
			half = romData[romPos + 1];
			songPtrs[0] = ReadLE16(&romData[romPos + 2]);
			songPtrs[1] = ReadLE16(&romData[romPos + 4]);
			songPtrs[2] = ReadLE16(&romData[romPos + 6]);
			songPtrs[3] = ReadLE16(&romData[romPos + 8]);
		}
		else
		{
			songPtrs[0] = ReadBE16(&romData[romPos + 1]);
			songPtrs[1] = ReadBE16(&romData[romPos + 3]);
			songPtrs[2] = ReadBE16(&romData[romPos + 5]);
			songPtrs[3] = ReadBE16(&romData[romPos + 7]);
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			curDelays[curTrack] = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			curNotes[curTrack] = 0;
			curNoteLens[curTrack] = 0;
			curInsts[curTrack] = 0;
			tracksEnd[curTrack] = 0;
			transposes[curTrack] = 0;
			curSpeeds[curTrack] = 0;
			envVals[curTrack] = 0xFF;
			octaves[curTrack] = 0;
			octaveUp[curTrack] = 0;
			baseNotes[curTrack] = 0;
			progBanks[curTrack] = 0;
			triplet[curTrack] = 0;
			dotted[curTrack] = 0;
			connect[curTrack] = 0;
			connect2[curTrack] = 0;
			tieNum[curTrack] = 0;
			tieLens[curTrack] = 0;
			noteGates[curTrack] = 0;
			holds[curTrack] = 0;
			pitchBend[curTrack] = 0;
			seqPosM[curTrack] = songPtrs[curTrack];

			if (songPtrs[curTrack] < bankSize || songPtrs[curTrack] >= (bankSize * 2))
			{
				tracksEnd[curTrack] = 1;
			}

		}

		ctrlDelay = 0;
		seqTime = 0;
		transpose = 0;

		if (drvVers == CAPCOM_VER_87)
		{
			if (songTempo < 1)
			{
				songTempo = 1;
			}
			if (songTempo == 1)
			{
				usPerQuarter = (long)(24.0 * 8.0 * 1000000.0 / 59.7275);
			}
			else
			{
				usPerQuarter = (long)(24.0 * 1000000.0 / (59.7275 * (1.0 - 7.0 / (8.0 * songTempo))));
			}

			valSize = CapWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
			ctrlMidPos += valSize;
			WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
			ctrlMidPos += 3;
			WriteBE24(&ctrlMidData[ctrlMidPos], usPerQuarter);
			ctrlMidPos += 3;
		}
		else
		{
			/*Later version: the driver starts every song at $0199 ticks per frame (8.8
			  fixed point) until a TEMPO command. Quarter note = 48 driver ticks.*/
			usPerQuarter = (long)(48.0 * 256.0 * 1000000.0 / (59.7275 * 0x0199));
			valSize = CapWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
			ctrlMidPos += valSize;
			WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
			ctrlMidPos += 3;
			WriteBE24(&ctrlMidData[ctrlMidPos], usPerQuarter);
			ctrlMidPos += 3;
			lastTempo = 0x0199;
		}
		ctrlTime = 0;

		while (songEnd == 0)
		{
			if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1 && tracksEnd[3] == 1)
			{
				songEnd = 1;
			}

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				if (tracksEnd[curTrack] != 1)
				{
					while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] != 1)
					{
						/*Stop a track that leaves the sound bank or fills its MIDI buffer*/
						if (seqPosM[curTrack] < bankSize || seqPosM[curTrack] >= (bankSize * 2) - 4 || midPosM[curTrack] > midLength - 64)
						{
							tracksEnd[curTrack] = 1;
							break;
						}

						visited[curTrack][seqPosM[curTrack]] = 1;
						command[0] = romData[seqPosM[curTrack]];
						command[1] = romData[seqPosM[curTrack] + 1];
						command[2] = romData[seqPosM[curTrack] + 2];

						if (command[0] >= CAPCOM_STATUS_NOTE_MIN && command[0] <= CAPCOM_STATUS_NOTE_MAX)
						{
							if (drvVers == CAPCOM_VER_87)
							{
								/*Length in driver ticks, as the driver does it: table value, halved
								  (integer) if the song's half flag is set, notes of a TIE added up,
								  then +50% if DOTTED. A length of 0 still lasts one tick.*/
								if (triplet[curTrack] != 1)
								{
									curNoteLen = CapLengths87[command[0] >> 5];
								}
								else
								{
									curNoteLen = CapLengths87Triplet[command[0] >> 5];
									triplet[curTrack] = 0;
								}

								if (half != 0)
								{
									curNoteLen >>= 1;
								}

								/*TIE ($21-$2F): the driver reads all the tied notes in the same tick and
								  plays only the last one, with the lengths added up*/
								if (tieNum[curTrack] > 0)
								{
									tieLens[curTrack] += curNoteLen;
									tieNum[curTrack]--;
									if (tieNum[curTrack] > 0)
									{
										seqPosM[curTrack]++;
										continue;
									}
									curNoteLen = tieLens[curTrack];
									tieLens[curTrack] = 0;
								}

								if (dotted[curTrack] == 1)
								{
									curNoteLen += curNoteLen >> 1;
									dotted[curTrack] = 0;
								}

								if (curNoteLen < 1)
								{
									curNoteLen = 1;
								}

								/*5 MIDI ticks per driver tick*/
								curNoteLens[curTrack] = curNoteLen * 5;

								/*Rest*/
								if ((command[0] & 0x1F) == 0)
								{
									midPosM[curTrack] = FlushNote(multiMidData[curTrack], midPosM[curTrack], &holdNotes[curTrack], curNotes[curTrack], &curDelays[curTrack], noteGates[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									curDelays[curTrack] += curNoteLens[curTrack];
									masterDelays[curTrack] += curNoteLens[curTrack];
								}
								/*Play note*/
								else
								{
									if (curTrack == 3)
									{
										/*Noise: the driver only uses pitch & 7 (NR43 = $30 | (p & 7))*/
										tempNote = (command[0] & 0x07) + 36;
									}
									else
									{
										tempNote = baseNotes[curTrack] + (command[0] & 0x1F) + 24;
									}

									if (curTrack == 2)
									{
										tempNote -= 12;
									}

									midPosM[curTrack] = FlushNote(multiMidData[curTrack], midPosM[curTrack], &holdNotes[curTrack], curNotes[curTrack], &curDelays[curTrack], noteGates[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									curNotes[curTrack] = tempNote;
									curVol = curVols[curTrack];
									tempPos = CapWriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									firstNotes[curTrack] = 0;
									holdNotes[curTrack] = 1;
									midPosM[curTrack] = tempPos;
									/*DuckTales: the note sounds for its full length*/
									noteGates[curTrack] = curNoteLens[curTrack];
									curDelays[curTrack] = curNoteLens[curTrack];
									masterDelays[curTrack] += curNoteLens[curTrack];
								}
							}
							else
							{
								if (curTrack == 0)
								{
									curTrack = 0;
								}
								/*Later version: bits 7-5 = length (1-7), bits 4-0 = pitch (0 = rest)*/
								lenIdx = (command[0] >> 5) - 1;
								if (triplet[curTrack] == 1)
								{
									/*TRIPLET wins; a pending DOTTED waits for the next normal note*/
									curNoteLen = CapLengths90Triplet[lenIdx];
								}
								else if (dotted[curTrack] == 1)
								{
									curNoteLen = CapLengths90Dotted[lenIdx];
									dotted[curTrack] = 0;
								}
								else
								{
									curNoteLen = CapLengths90[lenIdx];
								}
								curNoteLens[curTrack] = curNoteLen * 5;
								pitch = command[0] & 0x1F;

								/*Rest: the connect flags are left alone*/
								if (pitch == 0)
								{
									midPosM[curTrack] = FlushNote(multiMidData[curTrack], midPosM[curTrack], &holdNotes[curTrack], curNotes[curTrack], &curDelays[curTrack], noteGates[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
									curDelays[curTrack] += curNoteLens[curTrack];
									masterDelays[curTrack] += curNoteLens[curTrack];
								}
								else
								{
									/*Note duration (HOLD): the driver keys off at length * hold / 256.
									  A hold of 0 still gives one tick.*/
									gate = holds[curTrack];
									if (gate < 1)
									{
										gate = 1;
									}

									if (gate > curNoteLen)
									{
										gate = curNoteLen;
									}
									gate *= 5;

									/*connect2 (flag bit 7) = CONNECT as it was at the previous note:
									  this note is not retriggered, the previous one goes on*/
									connected = connect2[curTrack];
									connect2[curTrack] = connect[curTrack];

									if (connected == 1)
									{
										if (holdNotes[curTrack] == 1)
										{
											noteGates[curTrack] = curDelays[curTrack] + gate;
										}
										curDelays[curTrack] += curNoteLens[curTrack];
										masterDelays[curTrack] += curNoteLens[curTrack];
									}
									else
									{
										if (curTrack == 3)
										{
											/*Noise: the driver only uses the low nibble (NR43 shift = 15 - (p & 15))*/
											tempNote = (pitch & 0x0F) + 36;
										}
										else
										{
											/*Driver pitch 1-84 (1 = C2): pitch + octave * 12 (+24 with OCTUP)
											  + global + channel transpose, at most 84*/
											tempNote = pitch + (octaves[curTrack] * 12) + (octaveUp[curTrack] * 24) + transpose + transposes[curTrack];
											if (tempNote > 0x54)
											{
												tempNote = 0x54;
											}
											tempNote += 35;
											/*Gargoyle's Quest is an octave lower*/
											if (octaveFix == 1)
											{
												tempNote -= 12;
											}
											if (curTrack == 2)
											{
												tempNote -= 12;
											}
										}
										if (tempNote < 0)
										{
											tempNote = 0;
										}
										if (tempNote > 127)
										{
											tempNote = 127;
										}
										midPosM[curTrack] = FlushNote(multiMidData[curTrack], midPosM[curTrack], &holdNotes[curTrack], curNotes[curTrack], &curDelays[curTrack], noteGates[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										curNotes[curTrack] = tempNote;
										curVol = curVols[curTrack];
										tempPos = CapWriteNoteEventOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
										firstNotes[curTrack] = 0;
										holdNotes[curTrack] = 1;
										midPosM[curTrack] = tempPos;
										if (pitchBend[curTrack] == 1 && firstNotes[curTrack] == 0)
										{
											tempPos = CapWriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
											midPosM[curTrack] += tempPos;
											Write8B(&multiMidData[curTrack][midPosM[curTrack]], (0xE0 | curTrack));
											Write8B(&multiMidData[curTrack][midPosM[curTrack] + 1], 0);
											Write8B(&multiMidData[curTrack][midPosM[curTrack] + 2], 0x40);
											Write8B(&multiMidData[curTrack][midPosM[curTrack] + 3], 0);
											midPosM[curTrack] += 3;
											pitchBend[curTrack] = 0;
											firstNotes[curTrack] = 1;
										}
										noteGates[curTrack] = gate;
										curDelays[curTrack] = curNoteLens[curTrack];
										masterDelays[curTrack] += curNoteLens[curTrack];
									}
								}
							}
							seqPosM[curTrack]++;
						}

						else if (command[0] == CAPCOM_STATUS_TRIPLET && drvVers == CAPCOM_VER_87)
						{
							triplet[curTrack] = 1;
							seqPosM[curTrack]++;
						}

						else if (command[0] >= CAPCOM_STATUS_END_TICK_MIN && command[0] <= CAPCOM_STATUS_END_TICK_MAX && drvVers == CAPCOM_VER_87)
						{
							/*Ends the tick without a note: one tick of silence (unused in DuckTales)*/
							curDelays[curTrack] += 5;
							masterDelays[curTrack] += 5;
							seqPosM[curTrack]++;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_TRIPLET && drvVers == CAPCOM_VER_90)
						{
							/*Toggle: on before the triplet notes, off after*/
							triplet[curTrack] ^= 1;
							seqPosM[curTrack]++;
						}

						else if (command[0] >= CAPCOM_STATUS_TIE_MIN && command[0] <= CAPCOM_STATUS_TIE_MAX && drvVers == CAPCOM_VER_87)
						{
							tieNum[curTrack] = command[0] - CAPCOM_STATUS_TIE_MIN + 2;
							seqPosM[curTrack]++;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_UNKNOWN0 || EventMap[command[0]] == CAPCOM_EVENT_NOP)
						{
							seqPosM[curTrack]++;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_UNKNOWN1 || EventMap[command[0]] == CAPCOM_EVENT_NOP1)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_DUTY)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_VOL)
						{
							if (curTrack != 2)
							{
								curVols[curTrack] = (command[1] * 0x10);

								if (curTrack != 3)
								{
									curVols[curTrack] /= 2;
								}
								if (curVols[curTrack] > 120)
								{
									curVols[curTrack] = 120;
								}
								if (curVols[curTrack] == 0)
								{
									curVols[curTrack] = 0;
								}
							}
							else
							{
								switch (command[0])
								{
								case 0x00:
									curVols[curTrack] = 1;
									break;
								case 0x01:
									curVols[curTrack] = 25;
									break;
								case 0x02:
									curVols[curTrack] = 50;
									break;
								case 0x03:
									curVols[curTrack] = 100;
									break;
								default:
									curVols[curTrack] = 100;
									break;
								}
							}
							seqPosM[curTrack] += 2;

						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_REPEAT1)
						{
							if (command[1] != 0x00)
							{
								if (repeats[curTrack][0][1] == -1)
								{
									if (drvVers == CAPCOM_VER_87)
									{
										repeats[curTrack][0][0] = ReadLE16(&romData[seqPosM[curTrack] + 2]);
									}
									else
									{
										repeats[curTrack][0][0] = ReadBE16(&romData[seqPosM[curTrack] + 2]);
									}
									repeats[curTrack][0][1] = command[1];
								}
								else if (repeats[curTrack][0][1] > 0)
								{
									seqPosM[curTrack] = repeats[curTrack][0][0];
									repeats[curTrack][0][1]--;
								}
								else
								{
									repeats[curTrack][0][1] = -1;
									seqPosM[curTrack] += 4;
								}
							}
							else
							{
								if (drvVers == CAPCOM_VER_87)
								{
									tempPos = ReadLE16(&romData[seqPosM[curTrack] + 2]);
								}
								else
								{
									tempPos = ReadBE16(&romData[seqPosM[curTrack] + 2]);
								}
								if (tempPos >= bankSize && tempPos < (bankSize * 2) && visited[curTrack][tempPos] == 0)
								{
									seqPosM[curTrack] = tempPos;
								}
								else
								{
									tracksEnd[curTrack] = 1;
								}

							}
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_REPEAT2)
						{
							if (command[1] != 0x00)
							{
								if (repeats[curTrack][1][1] == -1)
								{
									if (drvVers == CAPCOM_VER_87)
									{
										repeats[curTrack][1][0] = ReadLE16(&romData[seqPosM[curTrack] + 2]);
									}
									else
									{
										repeats[curTrack][1][0] = ReadBE16(&romData[seqPosM[curTrack] + 2]);
									}
									repeats[curTrack][1][1] = command[1];
								}
								else if (repeats[curTrack][1][1] > 0)
								{
									seqPosM[curTrack] = repeats[curTrack][1][0];
									repeats[curTrack][1][1]--;
								}
								else
								{
									repeats[curTrack][1][1] = -1;
									seqPosM[curTrack] += 4;
								}
							}
							else
							{
								if (drvVers == CAPCOM_VER_87)
								{
									tempPos = ReadLE16(&romData[seqPosM[curTrack] + 2]);
								}
								else
								{
									tempPos = ReadBE16(&romData[seqPosM[curTrack] + 2]);
								}
								if (tempPos >= bankSize && tempPos < (bankSize * 2) && visited[curTrack][tempPos] == 0)
								{
									seqPosM[curTrack] = tempPos;
								}
								else
								{
									tracksEnd[curTrack] = 1;
								}

							}

						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_REPEAT3)
						{
							if (command[1] != 0x00)
							{
								if (repeats[curTrack][2][1] == -1)
								{
									if (drvVers == CAPCOM_VER_87)
									{
										repeats[curTrack][2][0] = ReadLE16(&romData[seqPosM[curTrack] + 2]);
									}
									else
									{
										repeats[curTrack][2][0] = ReadBE16(&romData[seqPosM[curTrack] + 2]);
									}
									repeats[curTrack][2][1] = command[1];
								}
								else if (repeats[curTrack][2][1] > 0)
								{
									seqPosM[curTrack] = repeats[curTrack][2][0];
									repeats[curTrack][2][1]--;
								}
								else
								{
									repeats[curTrack][2][1] = -1;
									seqPosM[curTrack] += 4;
								}
							}
							else
							{
								if (drvVers == CAPCOM_VER_87)
								{
									tempPos = ReadLE16(&romData[seqPosM[curTrack] + 2]);
								}
								else
								{
									tempPos = ReadBE16(&romData[seqPosM[curTrack] + 2]);
								}
								if (tempPos >= bankSize && tempPos < (bankSize * 2) && visited[curTrack][tempPos] == 0)
								{
									seqPosM[curTrack] = tempPos;
								}
								else
								{
									tracksEnd[curTrack] = 1;
								}

							}

						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_REPEAT4)
						{
							if (command[1] != 0x00)
							{
								if (repeats[curTrack][3][1] == -1)
								{
									if (drvVers == CAPCOM_VER_87)
									{
										repeats[curTrack][3][0] = ReadLE16(&romData[seqPosM[curTrack] + 2]);
									}
									else
									{
										repeats[curTrack][3][0] = ReadBE16(&romData[seqPosM[curTrack] + 2]);
									}
									repeats[curTrack][3][1] = command[1];
								}
								else if (repeats[curTrack][3][1] > 0)
								{
									seqPosM[curTrack] = repeats[curTrack][3][0];
									repeats[curTrack][3][1]--;
								}
								else
								{
									repeats[curTrack][3][1] = -1;
									seqPosM[curTrack] += 4;
								}
							}
							else
							{
								if (drvVers == CAPCOM_VER_87)
								{
									tempPos = ReadLE16(&romData[seqPosM[curTrack] + 2]);
								}
								else
								{
									tempPos = ReadBE16(&romData[seqPosM[curTrack] + 2]);
								}
								if (tempPos >= bankSize && tempPos < (bankSize * 2) && visited[curTrack][tempPos] == 0)
								{
									seqPosM[curTrack] = tempPos;
								}
								else
								{
									tracksEnd[curTrack] = 1;
								}

							}

						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_BASE_NOTE && drvVers == CAPCOM_VER_87)
						{
							baseNotes[curTrack] = command[1];
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_DOTTED)
						{
							dotted[curTrack] = 1;
							seqPosM[curTrack]++;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_ENV)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_VIBRATO)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_STOP)
						{
							if (drvVers == CAPCOM_VER_87)
							{
								/*DuckTales: stops the whole song*/
								for (j = 0; j < trackCnt; j++)
								{
									tracksEnd[j] = 1;
								}
								songEnd = 1;
							}
							else
							{
								/*Later version: END only stops its own channel*/
								tracksEnd[curTrack] = 1;
							}
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_CONNECT && drvVers == CAPCOM_VER_90)
						{
							/*Toggle: before the first and before the last of the connected notes*/
							connect[curTrack] ^= 1;
							seqPosM[curTrack]++;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_OCTAVE_UP && drvVers == CAPCOM_VER_90)
						{
							/*Toggle: +2 octaves*/
							octaveUp[curTrack] ^= 1;
							seqPosM[curTrack]++;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_FLAGS && drvVers == CAPCOM_VER_90)
						{
							/*The driver does flags = (flags & $97) | value: CONNECT, TRIPLET and
							  OCTUP (bits 6, 5, 3) are replaced; bit 7, DOTTED (bit 4) and the
							  octave (bits 2-0) can only be set, not cleared*/
							connect2[curTrack] |= (command[1] & 0x80) >> 7;
							connect[curTrack] = (command[1] & 0x40) >> 6;
							triplet[curTrack] = (command[1] & 0x20) >> 5;
							dotted[curTrack] |= (command[1] & 0x10) >> 4;
							octaveUp[curTrack] = (command[1] & 0x08) >> 3;
							octaves[curTrack] |= command[1] & 0x07;
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_TEMPO && drvVers == CAPCOM_VER_90)
						{
							/*05 hi lo: driver ticks per frame, 8.8 fixed point (a 16-bit value:
							  the command is 3 bytes long). Applies to the whole song, so it goes
							  in the control track at the current time.*/
							songTempo = (command[1] << 8) | command[2];
							if (songTempo > 0 && songTempo != lastTempo)
							{
								eventTime = masterDelays[curTrack];
								if (eventTime < ctrlTime)
								{
									eventTime = ctrlTime;
								}
								usPerQuarter = (long)(48.0 * 256.0 * 1000000.0 / (59.7275 * songTempo));
								valSize = CapWriteDeltaTime(ctrlMidData, ctrlMidPos, eventTime - ctrlTime);
								ctrlMidPos += valSize;
								WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
								ctrlMidPos += 3;
								WriteBE24(&ctrlMidData[ctrlMidPos], usPerQuarter);
								ctrlMidPos += 3;
								ctrlTime = eventTime;
								lastTempo = songTempo;
							}
							seqPosM[curTrack] += 3;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_NOTE_DUR && drvVers == CAPCOM_VER_90)
						{
							/*Key-off point: x/256 of each note*/
							holds[curTrack] = command[1];
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_VOL && drvVers == CAPCOM_VER_90)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_PROG_CHANGE && drvVers == CAPCOM_VER_90)
						{
							/*As in the AudioArts converter: a new instrument is sent as a program
							  change with the next note*/
							if (curInsts[curTrack] != command[1])
							{
								curInsts[curTrack] = command[1];
								firstNotes[curTrack] = 1;
							}
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_OCTAVE && drvVers == CAPCOM_VER_90)
						{
							/*Octave 0-7 (bits 2-0 of the flags; OCTUP is separate)*/
							octaves[curTrack] = command[1] & 0x07;
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_GLOBAL_TRANSPOSE && drvVers == CAPCOM_VER_90)
						{
							transpose = (signed char)command[1];
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_LOCAL_TRANSPOSE && drvVers == CAPCOM_VER_90)
						{
							transposes[curTrack] = (signed char)command[1];
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_TUNING && drvVers == CAPCOM_VER_90)
						{
							seqPosM[curTrack] += 2;
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_PITCH_SLIDE && drvVers == CAPCOM_VER_90)
						{
							pitchBend[curTrack] = 1;
							seqPosM[curTrack] += 2;
						}

						else if ((EventMap[command[0]] == CAPCOM_EVENT_JUMP_FLAGS1 || EventMap[command[0]] == CAPCOM_EVENT_JUMP_FLAGS2 || EventMap[command[0]] == CAPCOM_EVENT_JUMP_FLAGS3 || EventMap[command[0]] == CAPCOM_EVENT_JUMP_FLAGS4) && drvVers == CAPCOM_VER_90)
						{
							/*BREAKn flags, hi, lo: on the LAST pass of LOOPn, set the flags (same
							  rule as FLAGS) and jump out of the loop. Otherwise skip it.
							  repeats[][n][1] is 0 during the last pass (no jumps left).*/
							repSlot = EventMap[command[0]] - CAPCOM_EVENT_JUMP_FLAGS1;
							if (repeats[curTrack][repSlot][1] == 0)
							{
								repeats[curTrack][repSlot][1] = -1;
								connect2[curTrack] |= (command[1] & 0x80) >> 7;
								connect[curTrack] = (command[1] & 0x40) >> 6;
								triplet[curTrack] = (command[1] & 0x20) >> 5;
								dotted[curTrack] |= (command[1] & 0x10) >> 4;
								octaveUp[curTrack] = (command[1] & 0x08) >> 3;
								octaves[curTrack] |= command[1] & 0x07;
								seqPosM[curTrack] = ReadBE16(&romData[seqPosM[curTrack] + 2]);
							}
							else
							{
								seqPosM[curTrack] += 4;
							}
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_JUMP && drvVers == CAPCOM_VER_90)
						{
							/*Follow the jump unless it goes back into data this track already
							  played (= the song's loop point)*/
							tempPos = ReadBE16(&romData[seqPosM[curTrack] + 1]);
							if (tempPos >= bankSize && tempPos < (bankSize * 2) && visited[curTrack][tempPos] == 0)
							{
								seqPosM[curTrack] = tempPos;
							}
							else
							{
								tracksEnd[curTrack] = 1;
							}
						}

						else if (EventMap[command[0]] == CAPCOM_EVENT_PAN && drvVers == CAPCOM_VER_90)
						{
							seqPosM[curTrack] += 2;
						}

						/*Unknown command*/
						else
						{
							seqPosM[curTrack]++;
						}
					}
				}
			}

			seqTime += 5;
			ctrlDelay += 5;

		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = FlushNote(multiMidData[curTrack], midPosM[curTrack], &holdNotes[curTrack], curNotes[curTrack], &curDelays[curTrack], noteGates[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);

			/*End of track; its delta keeps a final rest so all channels end together*/
			midPosM[curTrack] += CapWriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], curDelays[curTrack]);
			WriteBE24(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
			midPosM[curTrack] += 3;
			firstNotes[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		/*End of control track*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		ctrlTrackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], ctrlTrackSize);

		if (multiBanks == 0)
		{
			sprintf(outfile, "song%d.mid", songNum);
		}
		else
		{
			sprintf(outfile, "Bank %i/song%d.mid", (curBank + 1), songNum);
		}
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			fwrite(multiMidData[curTrack], midPosM[curTrack], 1, mid);
			free(multiMidData[curTrack]);
		}

		free(ctrlMidData);
		fclose(mid);

	}
}