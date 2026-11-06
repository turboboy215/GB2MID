/*Biox #2*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "BIOX2.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;

int i, j;
char outfile[1000000];
int songNum;
long songPtr;
long seqPtrs[4];
long bankAmt;
int curVol;
int curInst;
int drvVers;
int foundTable;
int progBank;
int verOverride;
int songBank;
long sfxTablePtrLoc;
long sfxTableOffset;

int foundTimes;

int multiBanks;
int curBank;

char folderName[100];

const char Biox2MagicBytes[4] = { 0xE7, 0x2A, 0x4F, 0x11 };
const char Biox2MagicBytesBank[7] = { 0xE7, 0x4C, 0x7D, 0xE0, 0xC8, 0xE0, 0xAA };

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
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Biox2song2mid(int songNum, long songPtr);

void Biox2Proc(int bank, char parameters[4][100])
{
	drvVers = BIOX2_VER_BBP;
	curVol = 120;
	curInst = 0;
	foundTable = 0;
	verOverride = 0;
	foundTimes = 0;

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers < BIOX2_VER_BBP && drvVers > BIOX2_VER_GS)
		{
			fclose(rom);
			printf("ERROR: Invalid version number!\n");
			exit(-1);
		}
	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], Biox2MagicBytes, 4) && foundTable != 1)
		{
			foundTimes++;
			if (foundTimes == 1)
			{
				sfxTablePtrLoc = i - 2;
				printf("Found pointer to sound effects table at address 0x%04X!\n", sfxTablePtrLoc);
				sfxTableOffset = ReadLE16(&romData[sfxTablePtrLoc]);
				printf("Sound effects table starts at 0x%04X...\n", sfxTableOffset);
			}
			if (foundTimes >= 2)
			{
				tablePtrLoc = i - 2;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
				break;
			}

		}
	}

	if (foundTable == 1)
	{

		i = tableOffset;
		songNum = 1;

		while (i != sfxTableOffset && (ReadLE16(&romData[i])) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			if (songPtr != 0x0000)
			{
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				Biox2song2mid(songNum, songPtr);
			}
			else
			{
				printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
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
		exit(1);
	}

}

/*Convert the song data to MIDI*/
void Biox2song2mid(int songNum, long songPtr)
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
	int macros[8][2];
	int repeats[8][2];
	int inMacro;
	int curSpeed;
	int maskVal[4] = { 0, 0, 0, 0 };

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 0;

	int curInst = 0;

	unsigned long seqPos = 0;

	unsigned char command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;
	int octave = 0;
	int noteLenAdd = 0;
	int volVal = 0;
	int tempLen;
	int tie;

	midPos = 0;
	ctrlMidPos = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;

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
		/*Get pointers*/
		romPos = songPtr;

		if ((romData[romPos] & 0x01) != 0x00)
		{
			maskVal[0] = 1;
		}
		if ((romData[romPos] & 0x02) != 0x00)
		{
			maskVal[1] = 1;
		}
		if ((romData[romPos] & 0x04) != 0x00)
		{
			maskVal[2] = 1;
		}
		if ((romData[romPos] & 0x08) != 0x00)
		{
			maskVal[3] = 1;
		}
		romPos++;
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			if (maskVal[curTrack] == 1)
			{
				seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}
			else
			{
				seqPtrs[curTrack] = 0x0000;
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
		case BIOX2_VER_BBP:
			/*Fall-through*/
		default:
			BIOX2_STATUS_NOTE_LEN_MIN = 0x00;
			BIOX2_STATUS_NOTE_LEN_MAX = 0x31;
			BIOX2_STATUS_NOTE_MIN = 0x32;
			BIOX2_STATUS_NOTE_MAX = 0x8F;
			BIOX2_STATUS_DRUM_MIN = 0x90;
			BIOX2_STATUS_DRUM_MAX = 0x95;
			EventMap[0xED] = BIOX2_EVENT_NOISE;
			EventMap[0xEE] = BIOX2_EVENT_TIE;
			EventMap[0xEF] = BIOX2_EVENT_REST;
			EventMap[0xF0] = BIOX2_EVENT_STOP;
			EventMap[0xF1] = BIOX2_EVENT_JUMP;
			EventMap[0xF2] = BIOX2_EVENT_CALL;
			EventMap[0xF3] = BIOX2_EVENT_RETURN;
			EventMap[0xF4] = BIOX2_EVENT_REPEAT_START;
			EventMap[0xF5] = BIOX2_EVENT_REPEAT_END;
			EventMap[0xF6] = BIOX2_EVENT_TEMPO;
			EventMap[0xF7] = BIOX2_EVENT_ENV;
			EventMap[0xF8] = BIOX2_EVENT_PITCH_ENV;
			EventMap[0xF9] = BIOX2_EVENT_NOP;
			EventMap[0xFA] = BIOX2_EVENT_DUTY;
			EventMap[0xFB] = BIOX2_EVENT_PAN;
			EventMap[0xFC] = BIOX2_EVENT_TRANSPOSE;
			break;
		case BIOX2_VER_GS:
			BIOX2_STATUS_NOTE_LEN_MIN = 0x00;
			BIOX2_STATUS_NOTE_LEN_MAX = 0x7F;
			BIOX2_STATUS_NOTE_MIN = 0x80;
			BIOX2_STATUS_NOTE_MAX = 0xD3;
			BIOX2_STATUS_DRUM_MIN = 0xD4;
			BIOX2_STATUS_DRUM_MAX = 0xE3;
			EventMap[0xE4] = BIOX2_EVENT_NOISE;
			EventMap[0xE5] = BIOX2_EVENT_TIE;
			EventMap[0xE6] = BIOX2_EVENT_REST;
			EventMap[0xE7] = BIOX2_EVENT_STOP;
			EventMap[0xE8] = BIOX2_EVENT_JUMP;
			EventMap[0xE9] = BIOX2_EVENT_CALL;
			EventMap[0xEA] = BIOX2_EVENT_RETURN;
			EventMap[0xEB] = BIOX2_EVENT_REPEAT_START;
			EventMap[0xEC] = BIOX2_EVENT_REPEAT_END;
			EventMap[0xED] = BIOX2_EVENT_TEMPO;
			EventMap[0xEE] = BIOX2_EVENT_ENV;
			EventMap[0xEF] = BIOX2_EVENT_PITCH_ENV;
			EventMap[0xF0] = BIOX2_EVENT_NOP;
			EventMap[0xF1] = BIOX2_EVENT_DUTY;
			EventMap[0xF2] = BIOX2_EVENT_PAN;
			EventMap[0xF3] = BIOX2_EVENT_TRANSPOSE;
			EventMap[0xF4] = BIOX2_EVENT_NRX2_HIGH;
			EventMap[0xF5] = BIOX2_EVENT_NRX2_LOW;
			EventMap[0xF6] = BIOX2_EVENT_OCTAVE_ENV;
			EventMap[0xF7] = BIOX2_EVENT_WAVEFORM;
			EventMap[0xF8] = BIOX2_EVENT_WAVEFORM_RESTORE;
			EventMap[0xF9] = BIOX2_EVENT_ENV;
			EventMap[0xFA] = BIOX2_EVENT_CH3_LEN;
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
			ctrlDelay = 0;
			masterDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			holdNote = 0;
			curVol = 120;
			noteLenAdd = 0;
			inMacro = 0;
			volVal = 0xF0;
			transpose = 0;
			curSpeed = 1;
			curVol = 120;
			octave = 0;
			tempLen = 0;
			tie = 0;
			for (j = 0; j < 8; j++)
			{
				macros[j][0] = 0;
				macros[j][1] = 0;
				repeats[j][0] = 0;
				repeats[j][1] = 0;
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

			seqPos = seqPtrs[curTrack];
			seqEnd = 0;

			if (seqPos == 0x0000)
			{
				seqEnd = 1;
			}
			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				if (command[0] >= BIOX2_STATUS_NOTE_LEN_MIN && command[0] <= BIOX2_STATUS_NOTE_LEN_MAX)
				{
					curNoteLen = (command[0] - BIOX2_STATUS_NOTE_LEN_MIN) * 10;
					seqPos++;
				}

				else if (command[0] >= BIOX2_STATUS_NOTE_MIN && command[0] <= BIOX2_STATUS_NOTE_MAX)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					curNote = command[0] - BIOX2_STATUS_NOTE_MIN + transpose + 24;
					if (curTrack != 2)
					{
						curNote += 12;
					}
					midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					curDelay = 0;
					holdNote = 1;
					firstNote = 0;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (command[0] >= BIOX2_STATUS_DRUM_MIN && command[0] <= BIOX2_STATUS_DRUM_MAX)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					curNote = command[0] - BIOX2_STATUS_DRUM_MIN + 36;
					midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					curDelay = 0;
					holdNote = 1;
					firstNote = 0;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_UNKNOWN0 || EventMap[command[0]] == BIOX2_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_NOISE)
				{
					curNote = ReadLE16(&romData[seqPos + 1]);
					midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					curDelay = 0;
					holdNote = 1;
					firstNote = 0;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					curDelay += (command[1] * 5);
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_TIE)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_REST)
				{
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_STOP)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_JUMP)
				{
					if ((ReadLE16(&romData[seqPos + 1])) > seqPos)
					{
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_CALL)
				{
					if (inMacro >= 1)
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

				else if (EventMap[command[0]] == BIOX2_EVENT_RETURN)
				{
					if (inMacro >= 1)
					{
						seqPos = macros[inMacro][1];
						inMacro--;
					}
					else
					{
						seqEnd = 1;
					}
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_REPEAT_START)
				{
					repeats[0][0] = seqPos + 2;
					repeats[0][1] = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_REPEAT_END)
				{
					if (repeats[0][1] > 1)
					{
						seqPos = repeats[0][0];
						repeats[0][1]--;
					}
					else
					{
						seqPos++;
					}
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_TEMPO)
				{
					tempo = command[1];
					if (drvVers == BIOX2_VER_GS)
					{
						tempo *= 2;
					}
					ctrlMidPos++;
					valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
					ctrlDelay = 0;
					ctrlMidPos += valSize;
					WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
					ctrlMidPos += 3;
					WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);

					if (tempo < 2)
					{
						tempo = 2;
					}
					ctrlMidPos += 2;
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_ENV)
				{
					if (curTrack != 2)
					{
						/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
						if ((command[1] & 0x08) == 0x00)
						{
							curVol = (command[1] & 0xF0) / 2;
						}
						else
						{
							if (((command[1] & 0xF0) / 2) == curVol)
							{
								curVol = (command[1] & 0xF0) / 2;
							}
							else
							{
								curVol = 120;
							}

						}

						if (curVol == 0)
						{
							curVol = 1;
						}
					}
					else
					{
						switch (command[1])
						{
						case 0x20:
							curVol = 120;
							break;
						case 0x40:
							curVol = 60;
							break;
						case 0x60:
							curVol = 30;
							break;
						case 0x00:
							curVol = 1;
							break;
						default:
							curVol = 120;
							break;
						}
					}
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_PITCH_ENV)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_NRX2_HIGH)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_NRX2_LOW)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_OCTAVE_ENV)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_WAVEFORM)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_WAVEFORM_RESTORE)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == BIOX2_EVENT_CH3_LEN)
				{
					seqPos += 2;
				}

				/*Unknown command*/
				else
				{
					seqPos++;
				}

			}

			if (holdNote == 1)
			{
				tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
				curDelay = 0;
				holdNote = 0;
				midPos = tempPos;
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