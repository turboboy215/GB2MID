/*Kouji Murata (Mega Man 3-5/Bionic Commando)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MEGAMAN3.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int foundTable;
int curInst;
long firstPtr;
int songBank;
int isSFX;


long bankAmt;

int curVol;
int drvVers;

int multiBanks;
int curBank;

char folderName[100];

const char MM3MagicBytesA[6] = { 0xCE, 0x00, 0x67, 0x2A, 0x66, 0x6F };
const char MM3MagicBytesB[7] = { 0xCE, 0x00, 0x67, 0xF1, 0x2A, 0x66, 0x6F };

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
void MM3song2mid(int songNum, long songPtr);

void MM3Proc(int bank, char parameters[4][100])
{
	drvVers = MM3_VER_STD;
	foundTable = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	bankAmt = bankSize;

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers < MM3_VER_BC && drvVers > MM3_VER_MT)
		{
			printf("ERROR: Invalid version number!\n");
			exit(1);
		}
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	if (romData[0x4099] == 0x44 && romData[0x409A] == 0x07)
	{
		drvVers = MM3_VER_BC;
	}

	/*Try to search the bank for song table loader - Method 1: Mega Man 3/Bionic Commando*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MM3MagicBytesA, 6)) && foundTable != 1)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc] + (romData[tablePtrLoc + 3] * 0x100);
			if (drvVers == MM3_VER_MT)
			{
				tableOffset = 0x48F8;
			}
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Method 2: Mega Man 4/5*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], MM3MagicBytesB, 7)) && foundTable != 1)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = romData[tablePtrLoc] + (romData[tablePtrLoc + 3] * 0x100);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		firstPtr = ReadLE16(&romData[i]);
		songNum = 1;
		if (drvVers != MM3_VER_PK && drvVers != MM3_VER_MT)
		{
			songBank = bank - 1;
		}
		while (i != firstPtr)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0)
			{
				MM3song2mid(songNum, songPtr);
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
		exit(-1);
	}
}

/*Convert the song data to MIDI*/
void MM3song2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
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
	int chanSpeed = 0;
	int octave = 0;
	int transpose = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	int inMacro1 = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	int inMacro2 = 0;
	int repeat1 = 0;
	long repeat1Pos = 0;
	int repeat2 = 0;
	long repeat2Pos = 0;
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
	int repeat = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int noteDur = 0;

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

		romPos = songPtr;
		mask = romData[romPos];

		switch (drvVers)
		{
		case MM3_VER_PK:
			MM3_STATUS_NOTE_MIN = 0x00;
			MM3_STATUS_NOTE_MAX = 0xCF;
			MM3_STATUS_PARAM_MIN = 0xD0;
			MM3_STATUS_PARAM_MAX = 0xDF;
			EventMap[0xE0] = MM3_EVENT_OCTAVE;
			EventMap[0xE1] = MM3_EVENT_OCTAVE;
			EventMap[0xE2] = MM3_EVENT_OCTAVE;
			EventMap[0xE3] = MM3_EVENT_OCTAVE;
			EventMap[0xE4] = MM3_EVENT_OCTAVE;
			EventMap[0xE5] = MM3_EVENT_OCTAVE;
			EventMap[0xE6] = MM3_EVENT_OCTAVE;
			EventMap[0xE7] = MM3_EVENT_OCTAVE;
			EventMap[0xE8] = MM3_EVENT_DUTY_VOL;
			EventMap[0xE9] = MM3_EVENT_DECAY;
			EventMap[0xEA] = MM3_EVENT_SWEEP;
			EventMap[0xEB] = MM3_EVENT_VIBRATO;
			EventMap[0xEC] = MM3_EVENT_TRANSPOSE;
			EventMap[0xED] = MM3_EVENT_WAVEFORM;
			EventMap[0xEE] = MM3_EVENT_DUTY_VOL_DECAY23;
			EventMap[0xEF] = MM3_EVENT_TUNING;
			EventMap[0xF0] = MM3_EVENT_ENV;
			EventMap[0xF1] = MM3_EVENT_DECAY2;
			EventMap[0xF2] = MM3_EVENT_DECAY3;
			EventMap[0xF3] = MM3_EVENT_FINAL_VOL;
			EventMap[0xF4] = MM3_EVENT_UNKNOWN1;
			EventMap[0xF5] = MM3_EVENT_NOP1;
			EventMap[0xF6] = MM3_EVENT_SPEED;
			EventMap[0xF7] = MM3_EVENT_CALL1;
			EventMap[0xF8] = MM3_EVENT_CALL2;
			EventMap[0xF9] = MM3_EVENT_RETURN1;
			EventMap[0xFA] = MM3_EVENT_RETURN2;
			EventMap[0xFB] = MM3_EVENT_REPEAT1_START;
			EventMap[0xFC] = MM3_EVENT_REPEAT2_START;
			EventMap[0xFD] = MM3_EVENT_REPEAT1_END;
			EventMap[0xFE] = MM3_EVENT_REPEAT2_END;
			EventMap[0xFF] = MM3_EVENT_STOP;
			break;
		case MM3_VER_MT:
			MM3_STATUS_NOTE_MIN = 0x00;
			MM3_STATUS_NOTE_MAX = 0x7F;
			EventMap[0x80] = MM3_EVENT_ENV;
			EventMap[0x81] = MM3_EVENT_TRANSPOSE;
			EventMap[0x82] = MM3_EVENT_WAVEFORM;
			EventMap[0x83] = MM3_EVENT_VIBRATO;
			EventMap[0x84] = MM3_EVENT_PAN;
			EventMap[0x85] = MM3_EVENT_DUTY;
			EventMap[0x86] = MM3_EVENT_NOP;
			EventMap[0x87] = MM3_EVENT_ENV_VEL;
			EventMap[0x88] = MM3_EVENT_PITCH_MASK_ON;
			EventMap[0x89] = MM3_EVENT_PITCH_MASK_OFF;
			EventMap[0x8A] = MM3_EVENT_NOP;
			EventMap[0x8B] = MM3_EVENT_NOP;
			EventMap[0x8C] = MM3_EVENT_DECAY_SEQ;
			EventMap[0x8D] = MM3_EVENT_NOP;
			EventMap[0x8E] = MM3_EVENT_DECAY_SEQ_LOOP;
			EventMap[0x8F] = MM3_EVENT_NOP;
			EventMap[0x90] = MM3_EVENT_NOP;
			EventMap[0x91] = MM3_EVENT_NOP;
			EventMap[0x92] = MM3_EVENT_CALL1;
			EventMap[0x93] = MM3_EVENT_RETURN1;
			EventMap[0x94] = MM3_EVENT_NOP;
			EventMap[0x95] = MM3_EVENT_REPEAT1_START;
			EventMap[0x96] = MM3_EVENT_REPEAT1_END;
			EventMap[0x97] = MM3_EVENT_NOP;
			EventMap[0x98] = MM3_EVENT_NOP;
			EventMap[0x99] = MM3_EVENT_NOP;
			EventMap[0x9A] = MM3_EVENT_STOP;
			EventMap[0x9B] = MM3_EVENT_CALL2;
			EventMap[0x9C] = MM3_EVENT_RETURN2;
			EventMap[0x9D] = MM3_EVENT_REPEAT2_START;
			EventMap[0x9E] = MM3_EVENT_REPEAT2_END;
			EventMap[0x9F] = MM3_EVENT_STOP;
			EventMap[0xFF] = MM3_EVENT_STOP;
			break;
		case MM3_VER_BC:
			/*Fall-through*/
		case MM3_VER_STD:
		default:
			MM3_STATUS_NOTE_MIN = 0x00;
			MM3_STATUS_NOTE_MAX = 0xCF;
			MM3_STATUS_PARAM_MIN = 0xD0;
			MM3_STATUS_PARAM_MAX = 0xDF;
			EventMap[0xE0] = MM3_EVENT_OCTAVE;
			EventMap[0xE1] = MM3_EVENT_OCTAVE;
			EventMap[0xE2] = MM3_EVENT_OCTAVE;
			EventMap[0xE3] = MM3_EVENT_OCTAVE;
			EventMap[0xE4] = MM3_EVENT_OCTAVE;
			EventMap[0xE5] = MM3_EVENT_OCTAVE;
			EventMap[0xE6] = MM3_EVENT_OCTAVE;
			EventMap[0xE7] = MM3_EVENT_OCTAVE;
			EventMap[0xE8] = MM3_EVENT_DUTY_VOL;
			EventMap[0xE9] = MM3_EVENT_DECAY;
			EventMap[0xEA] = MM3_EVENT_SWEEP;
			EventMap[0xEB] = MM3_EVENT_VIBRATO;
			EventMap[0xEC] = MM3_EVENT_TRANSPOSE;
			EventMap[0xED] = MM3_EVENT_WAVEFORM;
			EventMap[0xEE] = MM3_EVENT_DUTY_VOL_DECAY23;
			EventMap[0xEF] = MM3_EVENT_TUNING;
			EventMap[0xF0] = MM3_EVENT_ENV;
			EventMap[0xF1] = MM3_EVENT_DECAY2;
			EventMap[0xF2] = MM3_EVENT_DECAY3;
			EventMap[0xF3] = MM3_EVENT_FINAL_VOL;
			EventMap[0xF4] = MM3_EVENT_UNKNOWN2;
			EventMap[0xF5] = MM3_EVENT_NOP1;
			EventMap[0xF6] = MM3_EVENT_SPEED;
			EventMap[0xF7] = MM3_EVENT_CALL1;
			EventMap[0xF8] = MM3_EVENT_CALL2;
			EventMap[0xF9] = MM3_EVENT_RETURN1;
			EventMap[0xFA] = MM3_EVENT_RETURN2;
			EventMap[0xFB] = MM3_EVENT_REPEAT1_START;
			EventMap[0xFC] = MM3_EVENT_REPEAT2_START;
			EventMap[0xFD] = MM3_EVENT_REPEAT1_END;
			EventMap[0xFE] = MM3_EVENT_REPEAT2_END;
			EventMap[0xFF] = MM3_EVENT_STOP;
			break;

		}

		if (drvVers != MM3_VER_PK && drvVers != MM3_VER_MT)
		{
			/*Try to get active channels for sound effects*/
			maskArray[3] = mask >> 7 & 1;
			maskArray[2] = mask >> 6 & 1;
			maskArray[1] = mask >> 5 & 1;
			maskArray[0] = mask >> 4 & 1;

			/*Otherwise, it is music*/
			if (maskArray[0] == 0 && maskArray[1] == 0 && maskArray[2] == 0 && maskArray[3] == 0)
			{
				maskArray[3] = mask >> 3 & 1;
				maskArray[2] = mask >> 2 & 1;
				maskArray[1] = mask >> 1 & 1;
				maskArray[0] = mask & 1;
				isSFX = 0;
			}
			else
			{
				isSFX = 1;
			}

			romPos += 2;
		}
		else
		{
			/*Try to get active channels for sound effects*/
			maskArray[3] = mask >> 7 & 1;
			maskArray[2] = mask >> 6 & 1;
			maskArray[1] = mask >> 5 & 1;
			maskArray[0] = mask >> 4 & 1;

			/*Otherwise, it is music*/
			if (maskArray[0] == 0 && maskArray[1] == 0 && maskArray[2] == 0 && maskArray[3] == 0)
			{
				maskArray[3] = mask >> 3 & 1;
				maskArray[2] = mask >> 2 & 1;
				maskArray[1] = mask >> 1 & 1;
				maskArray[0] = mask & 1;
				isSFX = 0;
			}
			else
			{
				isSFX = 1;
			}

			romPos += 2;

			songBank = romData[romPos];
			romPos++;
		}

		/*Copy the current bank data from the ROM*/
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, (songBank * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			macro1Ret = -1;
			macro2Ret = -1;
			transpose = 0;

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

			if (maskArray[curTrack] != 0)
			{
				seqEnd = 0;
				seqPos = ReadLE16(&romData[romPos]);
				repeatStart = seqPos;
				startPos = seqPos;
				romPos += 2;
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];
				command[4] = exRomData[seqPos + 4];
				command[5] = exRomData[seqPos + 5];
				command[6] = exRomData[seqPos + 6];
				command[7] = exRomData[seqPos + 7];

				lowNibble = (command[0] >> 4);
				highNibble = (command[0] & 15);

				if (command[0] >= MM3_STATUS_NOTE_MIN && command[0] <= MM3_STATUS_NOTE_MAX)
				{
					if (drvVers != MM3_VER_MT)
					{

						if (highNibble == 0x00)
						{
							if (drvVers != MM3_VER_PK)
							{
								highNibble = 16;
							}
							else
							{
								highNibble = command[1];
								seqPos++;
							}

						}

						curNoteLen = highNibble * chanSpeed * 5;

						/*Rest*/
						if (lowNibble == 0x0C)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						/*Play note*/
						else
						{
							curNote = lowNibble + (octave * 12) + transpose;
							if (curTrack < 2)
							{
								curNote += 36;
							}
							else if (curTrack == 2)
							{
								curNote += 24;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						seqPos++;
					}
					else
					{
						/*Note*/
						if (command[0] == 0x00)
						{
							rest = 1;
						}
						else
						{
							rest = 0;
						}
						curNote = command[0] + 23 + transpose;
						seqPos++;

						if (curTrack < 2)
						{
							curNote += 12;
						}

						/*Length*/
						if ((exRomData[seqPos] & 0x80) == 0x00)
						{
							curNoteLen = exRomData[seqPos] * 5;
							seqPos++;
						}
						else
						{
							curNoteLen = (((unsigned int)(exRomData[seqPos] & 0x7F) << 7) | (exRomData[seqPos + 1] & 0x7F)) * 5;
							seqPos += 2;
						}

						/*Duration*/
						if ((exRomData[seqPos] & 0x80) == 0x00)
						{
							noteDur = exRomData[seqPos] * 5;
							seqPos++;
						}
						else
						{
							noteDur = (((unsigned int)(exRomData[seqPos] & 0x7F) << 7) | (exRomData[seqPos + 1] & 0x7F)) * 5;
							seqPos += 2;
						}

						/*Rest*/
						if (rest == 1)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;

							curDelay += noteDur;
							ctrlDelay += noteDur;
							masterDelay += noteDur;
						}
						/*Play note*/
						else
						{
							curNote = command[0] + transpose + 24;
							if (curTrack < 2)
							{
								curNote += 12;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;

							curDelay += noteDur;
							ctrlDelay += noteDur;
							masterDelay += noteDur;
						}
					}
				}

				else if (command[0] >= MM3_STATUS_PARAM_MIN && command[0] <= MM3_STATUS_PARAM_MAX && drvVers != MM3_VER_MT)
				{
					chanSpeed = highNibble;

					if (curTrack == 2 && drvVers != MM3_VER_BC)
					{
						seqPos += 2;
						if (exRomData[seqPos] >= 0x80)
						{
							seqPos += 2;
						}
						else
						{
							seqPos++;
						}
					}
					else if (curTrack == 2 && drvVers == MM3_VER_BC)
					{
						seqPos += 4;
					}
					else if (curTrack == 3 && isSFX == 1)
					{
						seqPos += 3;
						if (exRomData[seqPos] >= 0x80)
						{
							seqPos += 3;
						}
						else
						{
							seqPos++;
						}
					}
					else if (curTrack == 3 && drvVers != MM3_VER_BC)
					{
						seqPos++;
					}
					else
					{
						seqPos += 4;
						if (exRomData[seqPos] >= 0x80)
						{
							seqPos += 3;
						}
						else
						{
							seqPos++;
						}
					}
				}

				else if (EventMap[command[0]] == MM3_EVENT_UNKNOWN0 || EventMap[command[0]] == MM3_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == MM3_EVENT_UNKNOWN1 || EventMap[command[0]] == MM3_EVENT_NOP1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_UNKNOWN2)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == MM3_EVENT_OCTAVE)
				{
					octave = highNibble;
					seqPos++;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DUTY_VOL)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DECAY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_VIBRATO)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_WAVEFORM)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DUTY_VOL_DECAY23)
				{
					if ((command[1] & 0x80) != 0x00)
					{
						seqPos += 4;
					}
					else
					{
						seqPos += 2;
					}
				}

				else if (EventMap[command[0]] == MM3_EVENT_TUNING)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_ENV)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DECAY2)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DECAY3)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_FINAL_VOL)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_SPEED)
				{
					chanSpeed = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_CALL1)
				{
					/*Workaround for "endless loop" issue in MM3 Dr. Wily Stage 1*/
					if ((ReadLE16(&exRomData[seqPos + 1]) == macro1Pos) && macro1Pos == 0x6C29 && songNum == 78)
					{
						seqEnd = 1;
					}

					else if (ReadLE16(&exRomData[seqPos + 1]) == startPos)
					{
						seqEnd = 1;
					}
					macro1Pos = ReadLE16(&exRomData[seqPos + 1]);
					macro1Ret = seqPos + 3;
					seqPos = macro1Pos;
					holdNote = 0;
				}

				else if (EventMap[command[0]] == MM3_EVENT_CALL2)
				{
					/*Workaround for "endless loop" issue in MM3 Dr. Wily Stage 1*/
					if ((ReadLE16(&exRomData[seqPos + 1]) == macro2Pos) && macro2Pos == 0x6C29 && songNum == 78)
					{
						seqEnd = 1;
					}

					else if (ReadLE16(&exRomData[seqPos + 1]) == startPos)
					{
						seqEnd = 1;
					}
					macro2Pos = ReadLE16(&exRomData[seqPos + 1]);
					macro2Ret = seqPos + 3;
					seqPos = macro2Pos;
					holdNote = 0;
				}

				else if (EventMap[command[0]] == MM3_EVENT_RETURN1)
				{
					if (macro1Ret != -1)
					{
						seqPos = macro1Ret;
						macro1Ret = -1;
					}
					else
					{
						seqPos++;
					}

				}

				else if (EventMap[command[0]] == MM3_EVENT_RETURN2)
				{
					if (macro2Ret != -1)
					{
						seqPos = macro2Ret;
						macro2Ret = -1;
					}
					else
					{
						seqPos++;
					}

				}

				else if (EventMap[command[0]] == MM3_EVENT_REPEAT1_START)
				{
					repeat1Pos = seqPos + 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == MM3_EVENT_REPEAT2_START)
				{
					repeat2Pos = seqPos + 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == MM3_EVENT_REPEAT1_END)
				{
					if (command[1] == 0x00)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat1 > 0)
						{
							seqPos = repeat1Pos;
							repeat1--;
						}

						else if (repeat1 == 0)
						{
							seqPos += 2;
							repeat1 = -1;
							repeat1Pos = seqPos;
						}

						else if (repeat1 == -1)
						{
							repeat1 = command[1];
							if (repeat1 == 255)
							{
								repeat1 = 1;
							}
							else if (repeat1 == 0)
							{
								seqEnd = 1;
							}
						}
					}
				}

				else if (EventMap[command[0]] == MM3_EVENT_REPEAT2_END)
				{
					if (command[1] == 0x00)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat2 > 0)
						{
							seqPos = repeat2Pos;
							repeat2--;
						}

						else if (repeat2 == 0)
						{
							seqPos += 2;
							repeat2 = -1;
							repeat2Pos = seqPos;
						}

						else if (repeat2 == -1)
						{
							repeat2 = command[1];
							if (repeat2 == 255)
							{
								repeat2 = 1;
							}
							else if (repeat2 == 0)
							{
								seqEnd = 1;
							}
						}
					}
				}

				else if (EventMap[command[0]] == MM3_EVENT_STOP)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == MM3_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_ENV_VEL)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == MM3_EVENT_PITCH_MASK_ON)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == MM3_EVENT_PITCH_MASK_OFF)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DECAY_SEQ)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == MM3_EVENT_DECAY_SEQ_LOOP)
				{
					seqPos += 3;
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
		free(exRomData);
		fclose(mid);
	}
}