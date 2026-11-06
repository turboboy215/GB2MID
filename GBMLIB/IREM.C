/*Irem/Tamtex/Lenar*/

#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "IREM.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;

int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long bankAmt;
int curVol;
int curInst;
int drvVers;
int foundTable;
int progBank;
int verOverride;

const char IremMagicBytesA[8] = { 0xCB, 0x27, 0xCB, 0x27, 0xCB, 0x27, 0x6F, 0x26 };
const char IremMagicBytesB[6] = { 0x26, 0x00, 0x29, 0x29, 0x29, 0x11 };

unsigned char* romData;
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
void Iremsong2mid(int songNum, long songPtrs[4]);

void IremProc(int bank, char parameters[4][100])
{
	drvVers = IREM_VER_HH;
	curVol = 120;
	curInst = 0;
	foundTable = 0;
	verOverride = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	if (drvVers == IREM_VER_KN || drvVers == IREM_VER_HH)
	{
		/*Try to search the bank for song pattern table loader - later version*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if (!memcmp(&romData[i], IremMagicBytesB, 6) && foundTable != 1)
			{
				tablePtrLoc = i + 6;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
				drvVers = IREM_VER_HH;
				break;
			}
		}

		/*Try to search the bank for song pattern table loader - earlier version*/
		for (i = bankSize; i < (bankSize * 2); i++)
		{
			if (!memcmp(&romData[i], IremMagicBytesA, 8) && foundTable != 1)
			{
				tablePtrLoc = i + 8;
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = romData[tablePtrLoc] * 0x100;
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
				drvVers = IREM_VER_KN;
				break;
			}
		}
	}
	else
	{
		tableOffset = 0x7D00;
		printf("Song table starts at 0x%04X...\n", tableOffset);
		foundTable = 1;
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;
		while (ReadLE16(&romData[i + 2]) < 0x8000)
		{
			songPtrs[0] = ReadLE16(&romData[i]);
			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			songPtrs[1] = ReadLE16(&romData[i + 2]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			songPtrs[2] = ReadLE16(&romData[i + 4]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			songPtrs[3] = ReadLE16(&romData[i + 6]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			Iremsong2mid(songNum, songPtrs);
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
void Iremsong2mid(int songNum, long songPtrs[4])
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
	int inMacro;
	int curSpeed;

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

	midPos = 0;
	ctrlMidPos = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;

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
		case IREM_VER_SS:
			/*Fall-through*/
		case IREM_VER_KQ:
			IREM_STATUS_NOTE_MIN = 0x00;
			IREM_STATUS_NOTE_MAX = 0x7F;
			IREM_STATUS_SPEED_MIN = 0x90;
			IREM_STATUS_SPEED_MAX = 0xFF;
			EventMap[0x80] = IREM_EVENT_DUTY;
			EventMap[0x81] = IREM_EVENT_SWEEP;
			EventMap[0x82] = IREM_EVENT_TRANSPOSE;
			EventMap[0x83] = IREM_EVENT_ENV;
			EventMap[0x84] = IREM_EVENT_GLOBAL_VOL;
			EventMap[0x85] = IREM_EVENT_CALL;
			EventMap[0x86] = IREM_EVENT_RETURN;
			EventMap[0x87] = IREM_EVENT_STOP;
			EventMap[0x88] = IREM_EVENT_NRX4;
			EventMap[0x89] = IREM_EVENT_TIE;
			EventMap[0x8A] = IREM_EVENT_MUS_PAN;
			EventMap[0x8B] = IREM_EVENT_JUMP;
			EventMap[0x8C] = IREM_EVENT_WAVEFORM;
			EventMap[0x8D] = IREM_EVENT_NOP1;
			EventMap[0x8E] = IREM_EVENT_VIBRATO;
			EventMap[0x8F] = IREM_EVENT_SFX_PAN;
			break;
		case IREM_VER_KN:
			IREM_STATUS_NOTE_MIN = 0x00;
			IREM_STATUS_NOTE_MAX = 0x7F;
			IREM_STATUS_SPEED_MIN = 0x90;
			IREM_STATUS_SPEED_MAX = 0xFF;
			EventMap[0x80] = IREM_EVENT_DUTY;
			EventMap[0x81] = IREM_EVENT_SWEEP;
			EventMap[0x82] = IREM_EVENT_TRANSPOSE;
			EventMap[0x83] = IREM_EVENT_ENV;
			EventMap[0x84] = IREM_EVENT_GLOBAL_VOL;
			EventMap[0x85] = IREM_EVENT_CALL;
			EventMap[0x86] = IREM_EVENT_RETURN;
			EventMap[0x87] = IREM_EVENT_STOP;
			EventMap[0x88] = IREM_EVENT_NRX4;
			EventMap[0x89] = IREM_EVENT_TIE;
			EventMap[0x8A] = IREM_EVENT_MUS_PAN;
			EventMap[0x8B] = IREM_EVENT_JUMP;
			EventMap[0x8C] = IREM_EVENT_WAVEFORM;
			EventMap[0x8D] = IREM_EVENT_NOP1;
			EventMap[0x8E] = IREM_EVENT_VIBRATO;
			EventMap[0x8F] = IREM_EVENT_SFX_PAN;
			break;
		case IREM_VER_HH:
		default:
			IREM_STATUS_NOTE_MIN = 0x00;
			IREM_STATUS_NOTE_MAX = 0x7F;
			IREM_STATUS_SPEED_MIN = 0x91;
			IREM_STATUS_SPEED_MAX = 0xFF;
			EventMap[0x80] = IREM_EVENT_DUTY;
			EventMap[0x81] = IREM_EVENT_SWEEP;
			EventMap[0x82] = IREM_EVENT_TRANSPOSE;
			EventMap[0x83] = IREM_EVENT_ENV;
			EventMap[0x84] = IREM_EVENT_GLOBAL_VOL;
			EventMap[0x85] = IREM_EVENT_CALL;
			EventMap[0x86] = IREM_EVENT_RETURN;
			EventMap[0x87] = IREM_EVENT_STOP;
			EventMap[0x88] = IREM_EVENT_NRX4;
			EventMap[0x89] = IREM_EVENT_TIE;
			EventMap[0x8A] = IREM_EVENT_MUS_PAN;
			EventMap[0x8B] = IREM_EVENT_JUMP;
			EventMap[0x8C] = IREM_EVENT_WAVEFORM;
			EventMap[0x8D] = IREM_EVENT_ENV_SEQ;
			EventMap[0x8E] = IREM_EVENT_VIBRATO;
			EventMap[0x8F] = IREM_EVENT_SFX_PAN;
			EventMap[0x90] = IREM_EVENT_PAN_MASK;
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
			for (j = 0; j < 8; j++)
			{
				macros[j][0] = 0;
				macros[j][1] = 0;
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

			seqPos = songPtrs[curTrack];
			seqEnd = 0;
			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				lowNibble = (command[0] >> 4);
				highNibble = (command[0] & 15);

				if (command[0] >= IREM_STATUS_NOTE_MIN && command[0] <= IREM_STATUS_NOTE_MAX)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;
						holdNote = 0;
						midPos = tempPos;
					}
					if (drvVers != IREM_VER_KQ)
					{
						curNote = command[0] - IREM_STATUS_NOTE_MIN + transpose + 24;
						if (drvVers == IREM_VER_SS)
						{
							curNote += 12;
						}
						if (curTrack < 2)
						{
							curNote += 12;
						}
					}
					else
					{
						curNote = command[0] - IREM_STATUS_NOTE_MIN - transpose + 24;
						if (curTrack != 2)
						{
							curNote += 12;
						}
					}

					midPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					curDelay = curSpeed;
					ctrlDelay += curSpeed;
					masterDelay += curSpeed;
					firstNote = 0;
					holdNote = 1;
					seqPos++;
				}

				else if (command[0] >= IREM_STATUS_SPEED_MIN && command[0] <= IREM_STATUS_SPEED_MAX)
				{
					curSpeed = (command[0] - IREM_STATUS_SPEED_MIN) * 5;
					seqPos++;
				}

				else if (EventMap[command[0]] == IREM_EVENT_UNKNOWN0 || EventMap[command[0]] == IREM_EVENT_NOP)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == IREM_EVENT_UNKNOWN1 || EventMap[command[0]] == IREM_EVENT_NOP1)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_SWEEP)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_TRANSPOSE)
				{
					transpose = (signed char)command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_ENV)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_GLOBAL_VOL)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_CALL)
				{
					/*Macro is too high*/
					if (inMacro >= 7)
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

				else if (EventMap[command[0]] == IREM_EVENT_RETURN)
				{
					if (inMacro <= 0)
					{
						seqEnd = 1;
					}
					else
					{
						seqPos = macros[inMacro][1];
						inMacro--;
					}
				}

				else if (EventMap[command[0]] == IREM_EVENT_STOP)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == IREM_EVENT_NRX4)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_TIE)
				{
					curDelay += curSpeed;
					ctrlDelay += curSpeed;
					masterDelay += curSpeed;
					seqPos++;
				}

				else if (EventMap[command[0]] == IREM_EVENT_MUS_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_JUMP)
				{
					if ((ReadLE16(&romData[seqPos + 1])) > seqPos)
					{
						seqPos = ReadLE16(&romData[seqPos + 1]);
					}
					else
					{
						if (inMacro > 0)
						{
							seqPos = ReadLE16(&romData[seqPos + 1]);
						}
						else
						{
							seqEnd = 1;
						}

					}
				}

				else if (EventMap[command[0]] == IREM_EVENT_WAVEFORM)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_ENV_SEQ)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_VIBRATO)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_SFX_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == IREM_EVENT_PAN_MASK)
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}

}