/*Atelier Double*/
/*For Marie/Elie no Atelier GB, see TOSE.*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "ATELIER.H"

#define bankSize 16384
#define ramSize 65536
#define sramStart 0xA000

FILE* rom, * mid, * cfg;
long bank;
long tableOffset;
int i, j;
int drvVers;
char outfile[1000000];
long songPtr;
long seqPtrs[4];
int songNum;
int songBank;
long bankAmt;
int curVol;
int curInst;
int numSongs;
long macroTab1;
long macroTab2;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

char string1[100];
char string2[100];
char AteliercheckStrings[5][100] = { "version=", "bank=", "numSongs=", "songTable=", "macroTable=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventAlt(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Ateliersong2mid(int songNum, long ptr);

void AtelierProc(int bank, char parameters[4][100])
{
	drvVers = ATELIER_VER_STD;
	bankAmt = bankSize;
	curVol = 127;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the driver version value*/
		fgets(string1, 9, cfg);
		if (memcmp(string1, AteliercheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		drvVers = strtod(string1, NULL);

		if (drvVers != ATELIER_VER_BOX && drvVers != ATELIER_VER_STD && drvVers != ATELIER_VER_JW && drvVers != ATELIER_VER_BO && drvVers != ATELIER_VER_QST && drvVers != ATELIER_VER_MP)
		{
			printf("ERROR: Invalid version number!\n");
			exit(2);
		}
		printf("Version: %i\n", drvVers);

		fgets(string1, 3, cfg);
		/*Get the song bank*/
		fgets(string1, 6, cfg);
		if (memcmp(string1, AteliercheckStrings[1], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}
		fgets(string1, 5, cfg);
		bank = strtol(string1, NULL, 16);

		printf("Bank: %04X\n", bank);

		if (bank < 0x02)
		{
			bankAmt = 0x0000;
			bank = 0x02;
		}

		fgets(string1, 3, cfg);
		/*Get the total number of songs*/
		fgets(string1, 10, cfg);
		if (memcmp(string1, AteliercheckStrings[2], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 3, cfg);

		numSongs = strtod(string1, NULL);

		printf("Number of songs: %i\n", numSongs);

		fgets(string1, 3, cfg);

		/*Get the song table offset*/
		fgets(string1, 11, cfg);
		if (memcmp(string1, AteliercheckStrings[3], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);
		}
		fgets(string1, 5, cfg);
		tableOffset = strtol(string1, NULL, 16);

		printf("Song table: 0x%04X\n", tableOffset);

		fgets(string1, 3, cfg);

		if (drvVers != ATELIER_VER_BO && drvVers != ATELIER_VER_MP)
		{
			/*Get the macro table offset*/
			fgets(string1, 12, cfg);
			if (memcmp(string1, AteliercheckStrings[4], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);
			macroTab1 = strtol(string1, NULL, 16);

			printf("Macro table: 0x%04X\n", macroTab1);

			fgets(string1, 3, cfg);
		}

		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);

		songNum = 1;
		i = tableOffset;

		if (drvVers != ATELIER_VER_MP)
		{

			while (songNum <= numSongs)
			{
				songBank = bank;
				if (drvVers == ATELIER_VER_QST && songNum > 48)
				{
					songBank++;
				}
				songPtr = ReadLE16(&romData[i]);

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(ramSize);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);

				printf("Song %i: 0x%04X\n", songNum, songPtr);
				if (drvVers == ATELIER_VER_JW)
				{
					/*Decompress compressed song data to SRAM*/
					JWDecomp(exRomData, songPtr);
					songPtr = sramStart;
				}
				Ateliersong2mid(songNum, songPtr);
				free(exRomData);
				i += 2;
				songNum++;
			}
		}
		else
		{
			while (songNum <= numSongs)
			{
				songBank = romData[i] + 1;
				songPtr = ReadLE16(&romData[i + 1]);

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);

				printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, songBank);
				Ateliersong2mid(songNum, songPtr);
				free(exRomData);
				i += 3;
				songNum++;
			}
		}
		free(romData);
	}

}

/*Convert the song data to MIDI*/
void Ateliersong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int numTracks = 0;
	int ticks = 120;
	int tempo = 160;
	int chanSpeed;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int autoLen = 0;
	int holdNote = 0;
	int transpose = 0;
	int transposeVal = 0;
	int initTranspose = 0;
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
	int masterDelay = 0;
	long tempPos = 0;
	int songLoop = 0;
	int repeatNum = 0;
	int repeats[5];
	int patEnd = 0;
	unsigned char patCommand[4];
	long patPos;
	int macroNum;
	long macroPos;

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

	for (j = 0; j < 4; j++)
	{
		seqPtrs[j] = 0x0000;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{

		/*Get channel pointer information*/
		romPos = ptr;

		if (drvVers != ATELIER_VER_BO && drvVers != ATELIER_VER_MP)
		{
			seqPtrs[0] = ReadLE16(&exRomData[romPos]);
			seqPtrs[1] = ReadLE16(&exRomData[romPos + 2]);
			seqPtrs[2] = ReadLE16(&exRomData[romPos + 4]);
			seqPtrs[3] = ReadLE16(&exRomData[romPos + 6]);
		}
		else
		{
			macroTab1 = ReadLE16(&exRomData[romPos + 2]);
			seqPtrs[0] = ReadLE16(&exRomData[romPos + 4]);
			seqPtrs[1] = ReadLE16(&exRomData[romPos + 6]);
			seqPtrs[2] = ReadLE16(&exRomData[romPos + 8]);
			seqPtrs[3] = ReadLE16(&exRomData[romPos + 10]);
		}

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
		case ATELIER_VER_BOX:
			ATELIER_STATUS_NOTE_MIN = 0x00;
			ATELIER_STATUS_NOTE_MAX = 0x5F;
			ATELIER_STATUS_PROG_CHANGE_MIN = 0x80;
			ATELIER_STATUS_PROG_CHANGE_MAX = 0x8F;
			ATELIER_STATUS_RETURN = 0xFF;
			EventMap[0x60] = ATELIER_EVENT_ENV;
			EventMap[0x61] = ATELIER_EVENT_ENV;
			EventMap[0x62] = ATELIER_EVENT_ENV;
			EventMap[0x63] = ATELIER_EVENT_ENV;
			EventMap[0x64] = ATELIER_EVENT_ENV;
			EventMap[0x65] = ATELIER_EVENT_ENV;
			EventMap[0x66] = ATELIER_EVENT_ENV;
			EventMap[0x67] = ATELIER_EVENT_ENV;
			EventMap[0x68] = ATELIER_EVENT_ENV;
			EventMap[0x69] = ATELIER_EVENT_ENV;
			EventMap[0x6A] = ATELIER_EVENT_ENV;
			EventMap[0x6B] = ATELIER_EVENT_ENV;
			EventMap[0x6C] = ATELIER_EVENT_ENV;
			EventMap[0x6D] = ATELIER_EVENT_ENV;
			EventMap[0x6E] = ATELIER_EVENT_ENV;
			EventMap[0x6F] = ATELIER_EVENT_ENV;
			EventMap[0x70] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x71] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x72] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x73] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x74] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x75] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x76] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x77] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x78] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x79] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x7A] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x7B] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x7C] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x7D] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x7E] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x7F] = ATELIER_EVENT_ENV_FINE;
			EventMap[0x80] = ATELIER_EVENT_PERIOD;
			EventMap[0x81] = ATELIER_EVENT_PERIOD;
			EventMap[0x82] = ATELIER_EVENT_PERIOD;
			EventMap[0x83] = ATELIER_EVENT_PERIOD;
			EventMap[0x84] = ATELIER_EVENT_PERIOD;
			EventMap[0x85] = ATELIER_EVENT_PERIOD;
			EventMap[0x86] = ATELIER_EVENT_PERIOD;
			EventMap[0x87] = ATELIER_EVENT_PERIOD;
			EventMap[0x88] = ATELIER_EVENT_PERIOD;
			EventMap[0x89] = ATELIER_EVENT_PERIOD;
			EventMap[0x8A] = ATELIER_EVENT_PERIOD;
			EventMap[0x8B] = ATELIER_EVENT_PERIOD;
			EventMap[0x8C] = ATELIER_EVENT_PERIOD;
			EventMap[0x8D] = ATELIER_EVENT_PERIOD;
			EventMap[0x8E] = ATELIER_EVENT_PERIOD;
			EventMap[0x8F] = ATELIER_EVENT_PERIOD;
			EventMap[0x90] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x91] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x92] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x93] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x94] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x95] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x96] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x97] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x98] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x99] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x9A] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x9B] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x9C] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x9D] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x9E] = ATELIER_EVENT_WAVEFORM;
			EventMap[0x9F] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA0] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA1] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA2] = ATELIER_EVENT_LOOP;
			break;
		case ATELIER_VER_STD:
			/*Fall-through*/
		case ATELIER_VER_BO:
		case ATELIER_VER_JW:
		case ATELIER_VER_MP:
		default:
			ATELIER_STATUS_NOTE_MIN = 0x00;
			ATELIER_STATUS_NOTE_MAX = 0x4F;
			ATELIER_STATUS_PROG_CHANGE_MIN = 0x80;
			ATELIER_STATUS_PROG_CHANGE_MAX = 0x9F;
			ATELIER_STATUS_RETURN = 0xFF;
			EventMap[0x50] = ATELIER_EVENT_ENV;
			EventMap[0x51] = ATELIER_EVENT_ENV;
			EventMap[0x52] = ATELIER_EVENT_ENV;
			EventMap[0x53] = ATELIER_EVENT_ENV;
			EventMap[0x54] = ATELIER_EVENT_ENV;
			EventMap[0x55] = ATELIER_EVENT_ENV;
			EventMap[0x56] = ATELIER_EVENT_ENV;
			EventMap[0x57] = ATELIER_EVENT_ENV;
			EventMap[0x58] = ATELIER_EVENT_ENV;
			EventMap[0x59] = ATELIER_EVENT_ENV;
			EventMap[0x5A] = ATELIER_EVENT_ENV;
			EventMap[0x5B] = ATELIER_EVENT_ENV;
			EventMap[0x5C] = ATELIER_EVENT_ENV;
			EventMap[0x5D] = ATELIER_EVENT_ENV;
			EventMap[0x5E] = ATELIER_EVENT_ENV;
			EventMap[0x5F] = ATELIER_EVENT_ENV;
			EventMap[0x60] = ATELIER_EVENT_SWEEP;
			EventMap[0x61] = ATELIER_EVENT_SWEEP;
			EventMap[0x62] = ATELIER_EVENT_SWEEP;
			EventMap[0x63] = ATELIER_EVENT_SWEEP;
			EventMap[0x64] = ATELIER_EVENT_SWEEP;
			EventMap[0x65] = ATELIER_EVENT_SWEEP;
			EventMap[0x66] = ATELIER_EVENT_SWEEP;
			EventMap[0x67] = ATELIER_EVENT_SWEEP;
			EventMap[0x68] = ATELIER_EVENT_SWEEP;
			EventMap[0x69] = ATELIER_EVENT_SWEEP;
			EventMap[0x6A] = ATELIER_EVENT_SWEEP;
			EventMap[0x6B] = ATELIER_EVENT_SWEEP;
			EventMap[0x6C] = ATELIER_EVENT_SWEEP;
			EventMap[0x6D] = ATELIER_EVENT_SWEEP;
			EventMap[0x6E] = ATELIER_EVENT_SWEEP;
			EventMap[0x6F] = ATELIER_EVENT_SWEEP;
			EventMap[0x70] = ATELIER_EVENT_PERIOD;
			EventMap[0x71] = ATELIER_EVENT_PERIOD;
			EventMap[0x72] = ATELIER_EVENT_PERIOD;
			EventMap[0x73] = ATELIER_EVENT_PERIOD;
			EventMap[0x74] = ATELIER_EVENT_PERIOD;
			EventMap[0x75] = ATELIER_EVENT_PERIOD;
			EventMap[0x76] = ATELIER_EVENT_PERIOD;
			EventMap[0x77] = ATELIER_EVENT_PERIOD;
			EventMap[0x78] = ATELIER_EVENT_PERIOD;
			EventMap[0x79] = ATELIER_EVENT_PERIOD;
			EventMap[0x7A] = ATELIER_EVENT_PERIOD;
			EventMap[0x7B] = ATELIER_EVENT_PERIOD;
			EventMap[0x7C] = ATELIER_EVENT_PERIOD;
			EventMap[0x7D] = ATELIER_EVENT_PERIOD;
			EventMap[0x7E] = ATELIER_EVENT_PERIOD;
			EventMap[0x7F] = ATELIER_EVENT_PERIOD;
			EventMap[0x81] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x82] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x83] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x84] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x85] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x86] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x87] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x88] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x89] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x8A] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x8B] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x8C] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x8D] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x8E] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x8F] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x90] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x91] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x92] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x93] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x94] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x95] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x96] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x97] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x98] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x99] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x9A] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x9B] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x9C] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x9D] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x9E] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0x9F] = ATELIER_EVENT_PROG_CHANGE;
			EventMap[0xA0] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA1] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA2] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA3] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA4] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA5] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA6] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA7] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA8] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xA9] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xAA] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xAB] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xAC] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xAD] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xAE] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xAF] = ATELIER_EVENT_WAVEFORM;
			EventMap[0xB0] = ATELIER_EVENT_LOOP;
			EventMap[0xB1] = ATELIER_EVENT_REPEAT_START;
			EventMap[0xB2] = ATELIER_EVENT_REPEAT_END;
			EventMap[0xB3] = ATELIER_EVENT_REPEAT_COND_JUMP;
			EventMap[0xB4] = ATELIER_EVENT_SLUR_ON;
			EventMap[0xB5] = ATELIER_EVENT_SLUR_OFF;
			EventMap[0xB6] = ATELIER_EVENT_TIE;
			EventMap[0xB7] = ATELIER_EVENT_JUMP;
			break;
		}
	}

	switch (drvVers)
	{
	case ATELIER_VER_JW:
		EventMap[0xBE] = ATELIER_EVENT_STOP;
		EventMap[0xBF] = ATELIER_EVENT_RESTART;
	case ATELIER_VER_BO:
	case ATELIER_VER_MP:
		EventMap[0xB8] = ATELIER_EVENT_UNKNOWN0;
		EventMap[0xB9] = ATELIER_EVENT_UNKNOWN3;
		/*Fall-through again...*/
	case ATELIER_VER_BOX:
	case ATELIER_VER_STD:
	default:
		EventMap[0xF0] = ATELIER_EVENT_STOP;
		EventMap[0xFF] = ATELIER_EVENT_RESTART;
	}

	switch (drvVers)
	{
	case ATELIER_VER_BOX:
		ATELIER_STATUS_MACRO_MIN = 0x00;
		ATELIER_STATUS_MACRO_MAX = 0x5F;
		break;
	case ATELIER_VER_STD:
		/*Fall-through yet again...*/
	case ATELIER_VER_JW:
	case ATELIER_VER_BO:
	case ATELIER_VER_MP:
	default:
		ATELIER_STATUS_MACRO_MIN = 0x00;
		ATELIER_STATUS_MACRO_MAX = 0xAF;
		EventMapList[0xB0] = ATELIER_EVENT_LOOP;
		EventMapList[0xB1] = ATELIER_EVENT_REPEAT_START;
		EventMapList[0xB2] = ATELIER_EVENT_REPEAT_END;
		EventMapList[0xB3] = ATELIER_EVENT_REPEAT_START;
		EventMapList[0xB4] = ATELIER_EVENT_REPEAT_COND_JUMP;
		EventMapList[0xB5] = ATELIER_EVENT_SLUR_ON;
		EventMapList[0xB6] = ATELIER_EVENT_SLUR_OFF;
		EventMapList[0xB7] = ATELIER_EVENT_JUMP;
		break;
	}

	for (curTrack = 0; curTrack < trackCnt; curTrack++)
	{
		for (j = 0; j < 5; j++)
		{
			repeats[j] = 0;
		}
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

		if (seqPtrs[curTrack] != 0x0000)
		{
			seqEnd = 0;
			seqPos = seqPtrs[curTrack] + 4;
		}
		else
		{
			seqEnd = 1;
		}

		firstNote = 1;
		holdNote = 0;
		curDelay = 0;
		ctrlDelay = 0;
		masterDelay = 0;
		repeatNum = 0;
		curInst = 0;
		patEnd = 0;

		if (curTrack < 3)
		{
			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];

				if (command[0] >= ATELIER_STATUS_NOTE_MIN && command[0] <= ATELIER_STATUS_NOTE_MAX)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}

					curNoteLen = command[1] * 5;
					if (command[0] == 0x00 || command[0] == 0x01)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
					}
					else
					{
						if (curTrack != 2)
						{
							curNote = command[0] + 34;
						}
						else
						{
							curNote = command[0] + 22;
						}

						tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						holdNote = 1;
						midPos = tempPos;
						curDelay = curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
					}

					seqPos += 2;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN0 || EventMap[command[0]] == ATELIER_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN2)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN3)
				{
					seqPos += 4;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_ENV)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_SWEEP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_PERIOD)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_PROG_CHANGE)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					curInst = command[0] - ATELIER_STATUS_PROG_CHANGE_MIN;
					firstNote = 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_WAVEFORM)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_LOOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_REPEAT_START)
				{
					repeats[0] = seqPos + 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_REPEAT_END)
				{
					if (repeatNum != 1)
					{
						repeats[1] = seqPos + 1;
						seqPos++;
					}
					else
					{
						seqPos = repeats[2];
						repeatNum = 0;
					}

				}

				else if (EventMap[command[0]] == ATELIER_EVENT_REPEAT_COND_JUMP)
				{
					if (songNum == 15 && curTrack == 1)
					{
						curTrack = 1;
					}
					repeats[2] = seqPos + 1;
					repeatNum = 1;
					seqPos = repeats[0];
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_SLUR_ON)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_SLUR_OFF)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_TIE)
				{
					curDelay += command[1] * 5;
					ctrlDelay += command[1] * 5;
					masterDelay += command[1] * 5;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_JUMP)
				{
					seqPos = ReadLE16(&exRomData[seqPos + 1]);
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_STOP)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_RESTART)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == ATELIER_EVENT_ENV_FINE && drvVers == ATELIER_VER_BOX)
				{
					seqPos += 2;
				}

				/*Unknown command*/
				else
				{
					seqPos++;
				}
			}
		}
		else
		{
			if (seqEnd == 0)
			{
				patPos = seqPos;
				while (patEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
				{
					if (songNum == 49)
					{
						songNum = 49;
					}
					patCommand[0] = exRomData[patPos];
					patCommand[1] = exRomData[patPos + 1];
					patCommand[2] = exRomData[patPos + 2];
					patCommand[3] = exRomData[patPos + 3];

					if (patCommand[0] >= ATELIER_STATUS_MACRO_MIN && patCommand[0] <= ATELIER_STATUS_MACRO_MAX)
					{
						seqEnd = 0;
						macroNum = patCommand[0];

						if (drvVers != ATELIER_VER_QST)
						{
							macroPos = ReadLE16(&exRomData[macroTab1 + (macroNum * 2)]);
						}
						else
						{
							macroPos = ReadLE16(&romData[macroTab1 + (macroNum * 2)]);
						}


						seqPos = macroPos;

						while (seqEnd == 0)
						{
							if (drvVers != ATELIER_VER_QST)
							{
								command[0] = exRomData[seqPos];
								command[1] = exRomData[seqPos + 1];
								command[2] = exRomData[seqPos + 2];
								command[3] = exRomData[seqPos + 3];
							}
							else
							{
								command[0] = romData[seqPos];
								command[1] = romData[seqPos + 1];
								command[2] = romData[seqPos + 2];
								command[3] = romData[seqPos + 3];
							}


							if (command[0] >= ATELIER_STATUS_NOTE_MIN && command[0] <= ATELIER_STATUS_NOTE_MAX)
							{
								if (holdNote == 1)
								{
									tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									curDelay = 0;
									holdNote = 0;
									midPos = tempPos;
								}

								curNoteLen = command[1] * 5;
								if (command[0] == 0x00 || command[0] == 0x01)
								{
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									masterDelay += curNoteLen;
								}
								else
								{
									if (curTrack != 2)
									{
										curNote = command[0] + 34;
									}
									else
									{
										curNote = command[0] + 22;
									}

									tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									holdNote = 1;
									midPos = tempPos;
									curDelay = curNoteLen;
									ctrlDelay += curNoteLen;
									masterDelay += curNoteLen;
								}

								seqPos += 2;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN0 || EventMap[command[0]] == ATELIER_EVENT_NOP)
							{
								seqPos++;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN1)
							{
								seqPos += 2;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN2)
							{
								seqPos += 3;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_UNKNOWN3)
							{
								seqPos += 4;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_ENV)
							{
								seqPos++;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_SWEEP)
							{
								seqPos++;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_PERIOD)
							{
								seqPos += 2;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_PROG_CHANGE)
							{
								if (holdNote == 1)
								{
									tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									curDelay = 0;
									holdNote = 0;
									midPos = tempPos;
								}
								curInst = command[0] - ATELIER_STATUS_PROG_CHANGE_MIN;
								firstNote = 1;
								seqPos++;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_WAVEFORM)
							{
								seqPos++;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_TIE)
							{
								curDelay += command[1] * 5;
								ctrlDelay += command[1] * 5;
								masterDelay += command[1] * 5;
								seqPos += 2;
							}

							else if (command[0] == ATELIER_STATUS_RETURN)
							{
								if (holdNote == 1)
								{
									tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									curDelay = 0;
									holdNote = 0;
									midPos = tempPos;
								}
								seqEnd = 1;
								patPos++;
							}

							else if (EventMap[command[0]] == ATELIER_EVENT_ENV_FINE && drvVers == ATELIER_VER_BOX)
							{
								seqPos += 2;
							}

							/*Unknown command*/
							else
							{
								seqPos++;
							}
						}
					}

					else if (EventMap[command[0]] == ATELIER_EVENT_ENV)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == ATELIER_EVENT_SWEEP)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == ATELIER_EVENT_PERIOD)
					{
						seqPos += 2;
					}

					else if (EventMap[patCommand[0]] == ATELIER_EVENT_LOOP)
					{
						patPos++;
					}

					else if (EventMap[patCommand[0]] == ATELIER_EVENT_REPEAT_START)
					{
						repeats[0] = patPos + 1;
						patPos++;
					}

					else if (EventMap[patCommand[0]] == ATELIER_EVENT_REPEAT_END)
					{
						if (repeatNum != 1)
						{
							repeats[1] = patPos + 1;
							patPos++;
						}
						else
						{
							patPos = repeats[2];
							repeatNum = 0;
						}

					}

					else if (EventMap[patCommand[0]] == ATELIER_EVENT_REPEAT_COND_JUMP)
					{
						repeats[2] = patPos + 1;
						repeatNum = 1;
						patPos = repeats[0];
					}

					else if (EventMap[patCommand[0]] == ATELIER_EVENT_JUMP)
					{
						patPos = ReadLE16(&exRomData[patPos + 1]);
					}

					else if (EventMap[patCommand[0]] == ATELIER_EVENT_STOP)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						patEnd = 1;
					}

					else if (EventMap[patCommand[0]] == ATELIER_EVENT_RESTART)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						patEnd = 1;
					}

					else if (EventMap[command[0]] == ATELIER_EVENT_ENV_FINE && drvVers == ATELIER_VER_BOX)
					{
						seqPos += 2;
					}

					/*Unknown command*/
					else
					{
						patPos++;
					}
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

	fwrite(ctrlMidData, ctrlMidPos, 1, mid);
	fwrite(midData, midPos, 1, mid);

	free(midData);
	free(ctrlMidData);
	fclose(mid);
}