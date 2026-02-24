/*Konami (KCE Kobe)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "KCEK.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long firstPtr;
long bankAmt;
int foundTable;
int curInst;
int drvVers;
int songBank;
int tableBank;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* exRomData2;
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
void KCEKsong2mid(int songNum, long songPtr);

void KCEKProc(int bank, char parameters[4][100])
{
	curInst = 0;
	drvVers = KCEKOBE_VER_TOKIMEKI;

	if (bank <= 0x02)
	{
		bank = 0x02;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	if (parameters[1][0] != 0)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
		drvVers = strtol(parameters[1], NULL, 16);
	}

	else if (parameters[0][0] != 0x00)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
	}

	if (drvVers != KCEKOBE_VER_TOKIMEKI && drvVers != KCEKOBE_VER_GOEMON && drvVers != KCEKOBE_VER_POPNMUSIC)
	{
		printf("ERROR: Invalid version number!\n");
		exit(2);
	}

	printf("Table offset: 0x%04X\n", tableOffset);

	i = tableOffset;
	songNum = 1;

	if (drvVers == KCEKOBE_VER_TOKIMEKI || drvVers == KCEKOBE_VER_GOEMON)
	{
		firstPtr = ReadLE16(&romData[i]);
		while (i < firstPtr && ReadLE16(&romData[i]) >= bankSize)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			KCEKsong2mid(songNum, songPtr);
			i += 2;
			songNum++;
		}
	}
	else
	{
		while (ReadLE16(&romData[i]) >= bankSize && ReadLE16(&romData[i]) < (bankSize * 2) && romData[i + 2] < 0x10)
		{
			songPtr = ReadLE16(&romData[i]);
			tableBank = romData[i + 2] + 1;
			printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, tableBank);

			fseek(rom, 0, SEEK_SET);
			exRomData2 = (unsigned char*)malloc(bankSize * 2);
			fread(exRomData2, 1, bankSize, rom);
			fseek(rom, ((tableBank - 1) * bankSize), SEEK_SET);
			fread(exRomData2 + bankSize, 1, bankSize, rom);
			KCEKsong2mid(songNum, songPtr);
			free(exRomData2);

			i += 3;
			songNum++;
		}
	}


	free(romData);
}

/*Convert the song data to MIDI*/
void KCEKsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int maskArray[8];
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int tempoVal = 0;
	int chanSpeed;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int autoLen = 0;
	int transpose = 0;
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
	int isSFX = 0;
	int isSFXs[4] = { 0, 0, 0, 0 };
	int songLoop = 0;
	int inMacro = 0;
	long macroPos = 0;
	long macroRet = 0;
	int repeat1 = 0;
	int repeat1Times = 0;
	long repeat1Pos = 0;
	int repeat2 = 0;
	int repeat2Times = 0;
	long repeat2Pos = 0;
	int repeatNote = 0;
	int repeatNoteAmt = 0;


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
		/*Get channel pointer information*/
		romPos = songPtr;

		romPos++;

		for (k = 0; k < 8; k++)
		{
			maskArray[k] = 0;
		}
		if (drvVers == KCEKOBE_VER_TOKIMEKI)
		{
			mask = romData[romPos];
		}
		else if (drvVers == KCEKOBE_VER_GOEMON)
		{
			mask = romData[romPos + 1];
		}
		else
		{
			mask = exRomData2[romPos + 1];
		}

		/*Try to get active channels for sound effects*/

		j = 7;
		for (k = 0; k < 8; k++)
		{
			maskArray[j] = mask >> k & 1;
			j--;
		}

		for (k = 0; k < 4; k++)
		{
			isSFXs[k] = 0;
		}

		for (k = 0; k < 4; k++)
		{
			seqPtrs[k] = 0x0000;
		}

		romPos++;

		if (drvVers == KCEKOBE_VER_TOKIMEKI)
		{
			songBank = romData[romPos];
		}
		else if (drvVers == KCEKOBE_VER_GOEMON)
		{
			songBank = romData[romPos - 1];
		}
		else
		{
			songBank = exRomData2[romPos - 1];
		}

		if (songBank < 1)
		{
			songBank = 1;
		}
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, (songBank * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);

		romPos++;

		if (drvVers == KCEKOBE_VER_TOKIMEKI || drvVers == KCEKOBE_VER_GOEMON)
		{
			for (k = 7; k >= 5; k--)
			{
				if (maskArray[k] == 1)
				{
					switch (k)
					{
					case 0:
						break;
					case 1:
						seqPtrs[3] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 2:
						seqPtrs[2] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 3:
						seqPtrs[1] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 4:
						seqPtrs[0] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 5:
						seqPtrs[3] = ReadLE16(&romData[romPos]);
						romPos += 2;
						isSFXs[3] = 1;
						break;
					case 6:
						seqPtrs[1] = ReadLE16(&romData[romPos]);
						romPos += 2;
						isSFXs[1] = 1;
						break;
					case 7:
						seqPtrs[0] = ReadLE16(&romData[romPos]);
						romPos += 2;
						isSFXs[0] = 1;
						break;
					default:
						break;
					}
				}
			}


			for (k = 4; k >= 0; k--)
			{
				if (maskArray[k] == 1)
				{
					switch (k)
					{
					case 0:
						break;
					case 1:
						seqPtrs[3] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 2:
						seqPtrs[2] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 3:
						seqPtrs[1] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 4:
						seqPtrs[0] = ReadLE16(&romData[romPos]);
						romPos += 2;
						break;
					case 5:
						seqPtrs[3] = ReadLE16(&romData[romPos]);
						romPos += 2;
						isSFXs[3] = 1;
						break;
					case 6:
						seqPtrs[1] = ReadLE16(&romData[romPos]);
						romPos += 2;
						isSFXs[1] = 1;
						break;
					case 7:
						seqPtrs[0] = ReadLE16(&romData[romPos]);
						romPos += 2;
						isSFXs[0] = 1;
						break;
					default:
						break;
					}
				}
			}
		}

		else
		{
			for (k = 7; k >= 5; k--)
			{
				if (maskArray[k] == 1)
				{
					switch (k)
					{
					case 0:
						break;
					case 1:
						seqPtrs[3] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 2:
						seqPtrs[2] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 3:
						seqPtrs[1] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 4:
						seqPtrs[0] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 5:
						seqPtrs[3] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						isSFXs[3] = 1;
						break;
					case 6:
						seqPtrs[1] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						isSFXs[1] = 1;
						break;
					case 7:
						seqPtrs[0] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						isSFXs[0] = 1;
						break;
					default:
						break;
					}
				}
			}


			for (k = 4; k >= 0; k--)
			{
				if (maskArray[k] == 1)
				{
					switch (k)
					{
					case 0:
						break;
					case 1:
						seqPtrs[3] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 2:
						seqPtrs[2] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 3:
						seqPtrs[1] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 4:
						seqPtrs[0] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						break;
					case 5:
						seqPtrs[3] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						isSFXs[3] = 1;
						break;
					case 6:
						seqPtrs[1] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						isSFXs[1] = 1;
						break;
					case 7:
						seqPtrs[0] = ReadLE16(&exRomData2[romPos]);
						romPos += 2;
						isSFXs[0] = 1;
						break;
					default:
						break;
					}
				}
			}
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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			autoLen = 0;

			songLoop = 0;
			autoLen = 0;
			repeat1 = 0;
			repeat2 = 0;
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
			if (isSFXs[curTrack] == 1)
			{
				autoLen = 1;
				curNoteLen = 30;
			}
			transpose = 0;
			repeat1 = 0;
			repeat1Times = -1;
			repeat2 = 0;
			repeat2Times = -1;

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
			}
			else
			{
				seqEnd = 1;
			}

			switch (drvVers)
			{
			case KCEKOBE_VER_TOKIMEKI:
				/*Fall-through*/
			case KCEKOBE_VER_GOEMON:
			case KCEKOBE_VER_POPNMUSIC:
			default:
				KCEKOBE_STATUS_REST = 0x00;
				KCEKOBE_STATUS_NOTE_MIN = 0x01;
				KCEKOBE_STATUS_NOTE_MAX = 0x60;
				KCEKOBE_STATUS_TRANSPOSE_UP_MIN = 0x61;
				EventMap[0x61] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x62] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x63] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x64] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x65] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x66] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x67] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x68] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x69] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x6A] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x6B] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x6C] = KCEKOBE_EVENT_TRANSPOSE_UP;
				EventMap[0x6D] = KCEKOBE_EVENT_NOP;
				EventMap[0x6E] = KCEKOBE_EVENT_NOP;
				EventMap[0x6F] = KCEKOBE_EVENT_NOP;
				KCEKOBE_STATUS_TRANSPOSE_DOWN_MIN = 0x70;
				EventMap[0x70] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x71] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x72] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x73] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x74] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x75] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x76] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x77] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x78] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x79] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x7A] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x7B] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x7C] = KCEKOBE_EVENT_TRANSPOSE_DOWN;
				EventMap[0x7D] = KCEKOBE_EVENT_SLUR;
				EventMap[0x7E] = KCEKOBE_EVENT_PITCH_BEND;
				EventMap[0x7F] = KCEKOBE_EVENT_DUTY_ABS;
				EventMap[0x80] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x81] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x82] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x83] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x84] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x85] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x86] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x87] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x88] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x89] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x8A] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x8B] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x8C] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x8D] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x8E] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x8F] = KCEKOBE_EVENT_WAVEFORM;
				EventMap[0x90] = KCEKOBE_EVENT_DUTY;
				EventMap[0x91] = KCEKOBE_EVENT_DUTY;
				EventMap[0x92] = KCEKOBE_EVENT_DUTY;
				EventMap[0x93] = KCEKOBE_EVENT_DUTY;
				EventMap[0x94] = KCEKOBE_EVENT_SWEEP;
				EventMap[0x95] = KCEKOBE_EVENT_NOP;
				EventMap[0x96] = KCEKOBE_EVENT_FADE1;
				EventMap[0x97] = KCEKOBE_EVENT_FADE2;
				EventMap[0x98] = KCEKOBE_EVENT_NOP;
				EventMap[0x99] = KCEKOBE_EVENT_NOP;
				EventMap[0x9A] = KCEKOBE_EVENT_NOP;
				EventMap[0x9B] = KCEKOBE_EVENT_NOP;
				EventMap[0x9C] = KCEKOBE_EVENT_NOP;
				EventMap[0x9D] = KCEKOBE_EVENT_NOP;
				EventMap[0x9E] = KCEKOBE_EVENT_NOP;
				EventMap[0x9F] = KCEKOBE_EVENT_NOP;
				EventMap[0xA0] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA1] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA2] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA3] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA4] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA5] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA6] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA7] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xA8] = KCEKOBE_EVENT_NOP;
				EventMap[0xA9] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xAA] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xAB] = KCEKOBE_EVENT_PITCH_SPEED;
				EventMap[0xAC] = KCEKOBE_EVENT_NOP;
				EventMap[0xAD] = KCEKOBE_EVENT_NOP;
				EventMap[0xAE] = KCEKOBE_EVENT_NOP;
				EventMap[0xAF] = KCEKOBE_EVENT_NOP;
				EventMap[0xB0] = KCEKOBE_EVENT_COMBO_B0;
				EventMap[0xB1] = KCEKOBE_EVENT_COMBO_B1;
				EventMap[0xB2] = KCEKOBE_EVENT_COMBO_B2;
				EventMap[0xB3] = KCEKOBE_EVENT_COMBO_B3;
				EventMap[0xB4] = KCEKOBE_EVENT_COMBO_B4;
				EventMap[0xB5] = KCEKOBE_EVENT_COMBO_B5;
				EventMap[0xB6] = KCEKOBE_EVENT_COMBO_B6;
				EventMap[0xB7] = KCEKOBE_EVENT_NOP;
				EventMap[0xB8] = KCEKOBE_EVENT_NOP;
				EventMap[0xB9] = KCEKOBE_EVENT_NOP;
				EventMap[0xBA] = KCEKOBE_EVENT_NOP;
				EventMap[0xBB] = KCEKOBE_EVENT_NOP;
				EventMap[0xBC] = KCEKOBE_EVENT_NOP;
				EventMap[0xBD] = KCEKOBE_EVENT_REPEAT_NOTE;
				EventMap[0xBE] = KCEKOBE_EVENT_AUTO_NOTE_LEN;
				EventMap[0xBF] = KCEKOBE_EVENT_LAST_NOTE;
				EventMap[0xC0] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC1] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC2] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC3] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC4] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC5] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC6] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC7] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC8] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xC9] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xCA] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xCB] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xCC] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xCD] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xCE] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xCF] = KCEKOBE_EVENT_VOLUME;
				EventMap[0xD0] = KCEKOBE_EVENT_CHANNEL_SPEED;
				EventMap[0xD1] = KCEKOBE_EVENT_PAN;
				EventMap[0xD2] = KCEKOBE_EVENT_PAN;
				EventMap[0xD3] = KCEKOBE_EVENT_PAN;
				EventMap[0xD4] = KCEKOBE_EVENT_UNKNOWN1;
				EventMap[0xD5] = KCEKOBE_EVENT_UNKNOWN1;
				EventMap[0xD6] = KCEKOBE_EVENT_NOP;
				EventMap[0xD7] = KCEKOBE_EVENT_NOP;
				EventMap[0xD8] = KCEKOBE_EVENT_NOP;
				EventMap[0xD9] = KCEKOBE_EVENT_NOP;
				EventMap[0xDA] = KCEKOBE_EVENT_NOP;
				EventMap[0xDB] = KCEKOBE_EVENT_NOP;
				EventMap[0xDC] = KCEKOBE_EVENT_NOP;
				EventMap[0xDD] = KCEKOBE_EVENT_NOP;
				EventMap[0xDE] = KCEKOBE_EVENT_NOP;
				EventMap[0xDF] = KCEKOBE_EVENT_UNKNOWN0;
				EventMap[0xE0] = KCEKOBE_EVENT_COMBO_E0;
				EventMap[0xE1] = KCEKOBE_EVENT_COMBO_E1;
				EventMap[0xE2] = KCEKOBE_EVENT_COMBO_E2;
				EventMap[0xE3] = KCEKOBE_EVENT_COMBO_E3;
				EventMap[0xE4] = KCEKOBE_EVENT_NOP;
				EventMap[0xE5] = KCEKOBE_EVENT_NOP;
				EventMap[0xE6] = KCEKOBE_EVENT_NOP;
				EventMap[0xE7] = KCEKOBE_EVENT_NOP;
				EventMap[0xE8] = KCEKOBE_EVENT_NOP;
				EventMap[0xE9] = KCEKOBE_EVENT_NOP;
				EventMap[0xEA] = KCEKOBE_EVENT_NOP;
				EventMap[0xEB] = KCEKOBE_EVENT_NOP;
				EventMap[0xEC] = KCEKOBE_EVENT_NOP;
				EventMap[0xED] = KCEKOBE_EVENT_NOP;
				EventMap[0xEE] = KCEKOBE_EVENT_NOP;
				EventMap[0xEF] = KCEKOBE_EVENT_NOP;
				EventMap[0xF0] = KCEKOBE_EVENT_NOP;
				EventMap[0xF1] = KCEKOBE_EVENT_NOP;
				EventMap[0xF2] = KCEKOBE_EVENT_NOP;
				EventMap[0xF3] = KCEKOBE_EVENT_NOP;
				EventMap[0xF4] = KCEKOBE_EVENT_NOP;
				EventMap[0xF5] = KCEKOBE_EVENT_NOP;
				EventMap[0xF6] = KCEKOBE_EVENT_NOP;
				EventMap[0xF7] = KCEKOBE_EVENT_NOP;
				EventMap[0xF8] = KCEKOBE_EVENT_NOP;
				EventMap[0xF9] = KCEKOBE_EVENT_CONDITIONAL_JUMP;
				EventMap[0xFA] = KCEKOBE_EVENT_JUMP;
				EventMap[0xFB] = KCEKOBE_EVENT_REPEAT1;
				EventMap[0xFC] = KCEKOBE_EVENT_REPEAT2;
				EventMap[0xFD] = KCEKOBE_EVENT_CALL;
				EventMap[0xFE] = KCEKOBE_EVENT_LOOP;
				EventMap[0xFF] = KCEKOBE_EVENT_END;
				break;
			}

			while (seqEnd == 0 && midPos < 48000 && masterDelay < 110000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];

				if (command[0] == KCEKOBE_STATUS_REST)
				{
					if (autoLen == 0)
					{
						curNote = 0;
						curNoteLen = command[1] * 5;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos += 2;
					}
					else
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;
					}

				}

				else if (command[0] >= KCEKOBE_STATUS_NOTE_MIN && command[0] <= KCEKOBE_STATUS_NOTE_MAX)
				{
					if (autoLen == 0)
					{
						if (repeatNote == 1)
						{
							curNote = command[0] + transpose + 23;
							if (curTrack == 2)
							{
								curNote -= 12;
							}
							seqPos++;

							while (repeatNoteAmt > 0)
							{
								curNoteLen = exRomData[seqPos] * 5;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
								seqPos++;
								repeatNoteAmt--;
							}
							repeatNote = 0;

						}
						else
						{
							curNote = command[0] + transpose + 23;
							if (curTrack == 2)
							{
								curNote -= 12;
							}
							curNoteLen = command[1] * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							seqPos += 2;
						}
					}
					else
					{
						if (repeatNote == 1)
						{
							curNote = command[0] + transpose + 23;
							if (curTrack == 2)
							{
								curNote -= 12;
							}
							seqPos++;

							while (repeatNoteAmt > 0)
							{
								curNoteLen = exRomData[seqPos] * 5;
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
								seqPos++;
								repeatNoteAmt--;
							}
							repeatNote = 0;

						}
						else
						{
							curNote = command[0] + transpose + 23;
							if (curTrack == 2)
							{
								curNote -= 12;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							seqPos++;
						}
					}


				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_NOP || EventMap[command[0]] == KCEKOBE_EVENT_UNKNOWN0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_UNKNOWN1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_TRANSPOSE_UP)
				{
					transpose = (command[0] - KCEKOBE_STATUS_TRANSPOSE_UP_MIN + 1);
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_TRANSPOSE_DOWN)
				{
					transpose = (command[0] - KCEKOBE_STATUS_TRANSPOSE_DOWN_MIN + 1) * -1;
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_SLUR)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_PITCH_BEND)
				{
					tempPos = WriteDeltaTime(midData, midPos, curDelay);
					midPos += tempPos;
					Write8B(&midData[midPos], (0xE0 | curTrack));
					Write8B(&midData[midPos + 1], 0);
					Write8B(&midData[midPos + 2], 0x40);
					Write8B(&midData[midPos + 3], 0);
					curDelay = 0;
					firstNote = 1;
					midPos += 3;
					if (command[1] == 0x00)
					{
						seqPos += 2;
					}
					else
					{
						seqPos += 3;
					}
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_DUTY_ABS)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_WAVEFORM)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_DUTY)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_FADE1)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_FADE2)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_PITCH_SPEED)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_B0)
				{
					if (tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 1.15;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 4;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_B1)
				{
					seqPos += 4;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_B2)
				{
					if (tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 1.15;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 5;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_B3)
				{
					seqPos += 5;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_B4)
				{
					if (tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 1.15;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 4;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_B5)
				{
					seqPos += 5;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_B6)
				{
					if (tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 1.15;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 6;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_REPEAT_NOTE)
				{
					repeatNote = 1;
					repeatNoteAmt = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_AUTO_NOTE_LEN)
				{
					if (autoLen == 0)
					{
						autoLen = 1;
						curNoteLen = command[1] * 5;
						seqPos += 2;
					}
					else
					{
						autoLen = 0;
						seqPos++;
					}
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_LAST_NOTE)
				{
					if (curNote == 0)
					{
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;
					}
					else
					{
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_VOLUME)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_CHANNEL_SPEED)
				{
					if (tempoVal < 2)
					{
						if (tempoVal == 0)
						{
							tempoVal = 1;
						}
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 1.15;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
					}
					seqPos += 2;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_PAN)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_E0)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_E1)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_E2)
				{
					seqPos += 4;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_COMBO_E3)
				{
					seqPos += 5;
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_CONDITIONAL_JUMP)
				{
					seqPos = ReadLE16(&exRomData[seqPos + 1]);
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_JUMP)
				{
					seqEnd = ReadLE16(&exRomData[seqPos + 1]);
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_REPEAT1)
				{
					if (repeat1 == 0)
					{
						repeat1 = 1;
						repeat1Pos = seqPos + 1;
						seqPos++;
					}
					else
					{
						if (repeat1Times == -1)
						{
							repeat1Times = command[1];
						}
						else if (repeat1Times > 1)
						{
							repeat1Times--;
							seqPos = repeat1Pos;
						}
						else
						{
							repeat1Times = -1;
							repeat1 = 0;
							seqPos += 2;
						}
					}
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_REPEAT2)
				{
					if (repeat2 == 0)
					{
						repeat2 = 1;
						repeat2Pos = seqPos + 1;
						seqPos++;
					}
					else
					{
						if (repeat2Times == -1)
						{
							repeat2Times = command[1];
						}
						else if (repeat2Times > 1)
						{
							repeat2Times--;
							seqPos = repeat2Pos;
						}
						else
						{
							repeat2Times = -1;
							repeat2 = 0;
							seqPos += 2;
						}
					}
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_CALL)
				{
					if (inMacro == 0)
					{
						inMacro = 1;
						macroPos = ReadLE16(&exRomData[seqPos + 1]);
						macroRet = seqPos + 3;
						seqPos = macroPos;
					}
					else
					{
						inMacro = 0;
						seqPos = macroRet;
					}
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_LOOP)
				{
					if (songLoop == 0)
					{
						songLoop = 1;
						seqPos++;
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == KCEKOBE_EVENT_END)
				{
					seqEnd = 1;
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

			if (tempoVal == 1)
			{
				tempoVal = 2;
			}


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
		free(exRomData);
		fclose(mid);
	}
}