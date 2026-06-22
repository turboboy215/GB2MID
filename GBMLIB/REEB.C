/*Reeb*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "REEB.H"

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
long songPtrs[8];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
int drvVers;
int verOverride;
int ptrOverride;
int stopCvt;
int curVol;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

int multiBanks;
int curBank;

char folderName[100];

const char ReebMagicBytes[7] = { 0x29, 0x29, 0x29, 0xE5, 0x29, 0xD1, 0x19 };
const char ReebMagicBytesQIX[7] = { 0xE5, 0x29, 0x29, 0x29, 0x29, 0xD1, 0x19 };

long midLength;

char* argv3;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Reebsong2mid(int songNum, long seqPtrs[4]);


void ReebProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	curInst = 0;
	drvVers = REEB_VER_KPSM;
	verOverride = 0;
	ptrOverride = 0;
	bankAmt = bankSize;
	stopCvt = 0;

	if (parameters[1][0] != 0x00)
	{
		verOverride = 1;
		ptrOverride = 1;
		drvVers = strtol(parameters[0], NULL, 16);
		tableOffset = strtol(parameters[1], NULL, 16);
		foundTable = 1;
	}

	else if (parameters[0][0] != 0x00)
	{
		verOverride = 1;
		drvVers = strtol(parameters[0], NULL, 16);
	}

	if (drvVers != REEB_VER_KPSM && drvVers != REEB_VER_SPPS && drvVers != REEB_VER_QIX)
	{
		printf("ERROR: Invalid version number!\n");
		exit(2);
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	if (ptrOverride != 1)
	{

		/*Try to search the bank for song table loader - Common method*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], ReebMagicBytes, 7)) && foundTable != 1)
			{
				if (ReadLE16(&romData[i + 7]) == 0x19D1)
				{
					if (verOverride != 1)
					{
						drvVers = REEB_VER_KPSM;
					}

					tablePtrLoc = i + 10;
				}
				else
				{
					if (verOverride != 1)
					{
						drvVers = REEB_VER_SPPS;
					}

					tablePtrLoc = i + 8;
				}
				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);

				/*Fix for F-1 World Grand Prix 1*/
				if (tableOffset == 0xDE80)
				{
					tableOffset = 0x4000;
				}
				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
			}
		}

		/*Alternate method for QIX Adventure*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], ReebMagicBytesQIX, 7)) && foundTable != 1)
			{
				if (verOverride != 1)
				{
					drvVers = REEB_VER_QIX;
				}
				tablePtrLoc = i + 8;

				printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);

				printf("Song table starts at 0x%04X...\n", tableOffset);
				foundTable = 1;
			}
		}

	}

	if (foundTable == 1)
	{
		songNum = 1;
		i = tableOffset;

		if (drvVers == REEB_VER_KPSM)
		{
			while ((romData[i] == 0x00 || romData[i] == 0x01) && stopCvt != 1)
			{
				if (romData[i + 1] == 0x00 || romData[i + 4] == 0x00 || romData[i + 7] == 0x00 || romData[i + 10] == 0x00)
				{
					songPtrs[0] = ReadLE16(&romData[i + 14]);
					songPtrs[1] = ReadLE16(&romData[i + 17]);
					songPtrs[2] = ReadLE16(&romData[i + 20]);
					songPtrs[3] = ReadLE16(&romData[i + 23]);
				}
				else
				{
					songPtrs[0] = ReadLE16(&romData[i + 2]);
					songPtrs[1] = ReadLE16(&romData[i + 5]);
					songPtrs[2] = ReadLE16(&romData[i + 8]);
					songPtrs[3] = ReadLE16(&romData[i + 11]);
				}
				for (j = 0; j < 4; j++)
				{
					if (songPtrs[j] >= 0x8000)
					{
						stopCvt = 1;
						break;
					}
				}

				if (stopCvt != 1)
				{
					printf("Song %i, channel 1: 0x%04X\n", songNum, songPtrs[0]);
					printf("Song %i, channel 2: 0x%04X\n", songNum, songPtrs[1]);
					printf("Song %i, channel 3: 0x%04X\n", songNum, songPtrs[2]);
					printf("Song %i, channel 4: 0x%04X\n", songNum, songPtrs[3]);
					Reebsong2mid(songNum, songPtrs);
					i += 25;
					songNum++;
				}

			}

		}
		else if (drvVers == REEB_VER_QIX)
		{
			while ((romData[i] == 0x00 || romData[i] == 0x05 || romData[i] == 0x01) && stopCvt != 1)
			{
				if (ReadLE16(&romData[i + 1]) == 0x0000 && ReadLE16(&romData[i + 3]) == 0x0000 && ReadLE16(&romData[i + 5]) == 0x0000 && ReadLE16(&romData[i + 7]) == 0x0000)
				{
					songPtrs[0] = ReadLE16(&romData[i + 9]);
					songPtrs[1] = ReadLE16(&romData[i + 11]);
					songPtrs[2] = ReadLE16(&romData[i + 13]);
					songPtrs[3] = ReadLE16(&romData[i + 15]);
				}
				else
				{
					songPtrs[0] = ReadLE16(&romData[i + 1]);
					songPtrs[1] = ReadLE16(&romData[i + 3]);
					songPtrs[2] = ReadLE16(&romData[i + 5]);
					songPtrs[3] = ReadLE16(&romData[i + 7]);
				}
				for (j = 0; j < 4; j++)
				{
					if (songPtrs[j] >= 0x8000)
					{
						stopCvt = 1;
						break;
					}
				}

				if (stopCvt != 1)
				{
					printf("Song %i, channel 1: 0x%04X\n", songNum, songPtrs[0]);
					printf("Song %i, channel 2: 0x%04X\n", songNum, songPtrs[1]);
					printf("Song %i, channel 3: 0x%04X\n", songNum, songPtrs[2]);
					printf("Song %i, channel 4: 0x%04X\n", songNum, songPtrs[3]);
					Reebsong2mid(songNum, songPtrs);
					i += 17;
					songNum++;
				}
			}
		}
		else
		{
			while (stopCvt != 1)
			{
				if (romData[i] == 0x00 || romData[i + 3] == 0x00 || romData[i + 6] == 0x00 || romData[i + 9] == 0x00)
				{
					songPtrs[0] = ReadLE16(&romData[i + 13]);
					songPtrs[1] = ReadLE16(&romData[i + 16]);
					songPtrs[2] = ReadLE16(&romData[i + 19]);
					songPtrs[3] = ReadLE16(&romData[i + 22]);
				}
				else
				{
					songPtrs[0] = ReadLE16(&romData[i + 1]);
					songPtrs[1] = ReadLE16(&romData[i + 4]);
					songPtrs[2] = ReadLE16(&romData[i + 7]);
					songPtrs[3] = ReadLE16(&romData[i + 10]);
				}
				for (j = 0; j < 4; j++)
				{
					if (songPtrs[j] >= 0x8000)
					{
						stopCvt = 1;
						break;
					}
				}

				if (stopCvt != 1)
				{
					printf("Song %i, channel 1: 0x%04X\n", songNum, songPtrs[0]);
					printf("Song %i, channel 2: 0x%04X\n", songNum, songPtrs[1]);
					printf("Song %i, channel 3: 0x%04X\n", songNum, songPtrs[2]);
					printf("Song %i, channel 4: 0x%04X\n", songNum, songPtrs[3]);
					Reebsong2mid(songNum, seqPtrs);
					i += 24;
					songNum++;
				}

			}
		}
		free(romData);
	}
	else
	{
		free(romData);
		fclose(rom);
		printf("ERROR: Magic bytes not found!\n");
		exit(3);
	}

}

/*Convert the song data to MIDI*/
void Reebsong2mid(int songNum, long seqPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int trackEnd = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curNoteVal = 0;
	unsigned char command[3];
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
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	int lenBase = 0;
	long startPos = 0;
	int inMacro = 0;
	long macroPos = 0;
	int macroTimes = 0;
	long macroRet = 0;
	int tie = 0;
	int chanSpeed = 0;

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


		switch (drvVers)
		{
		case REEB_VER_KPSM:
			/*Fall-through*/
		case REEB_VER_SPPS:
		case REEB_VER_QIX:
		default:
			EventMap[0xE9] = REEB_EVENT_SET_SPEED;
			EventMap[0xEA] = REEB_EVENT_UNKNOWN1;
			EventMap[0xEB] = REEB_EVENT_DUTY;
			EventMap[0xEC] = REEB_EVENT_LENGTH_CH3;
			EventMap[0xED] = REEB_EVENT_ENV_DIR;
			EventMap[0xEE] = REEB_EVENT_WAVEFORM;
			EventMap[0xEF] = REEB_EVENT_TIE_ON;
			EventMap[0xF0] = REEB_EVENT_TIE_OFF;
			EventMap[0xF1] = REEB_EVENT_VOLUME;
			EventMap[0xF2] = REEB_EVENT_UNKNOWN1;
			EventMap[0xF3] = REEB_EVENT_VIBRATO;
			EventMap[0xF4] = REEB_EVENT_GLOBAL_VOLUME;
			EventMap[0xF5] = REEB_EVENT_SWEEP;
			EventMap[0xF6] = REEB_EVENT_PAN;
			EventMap[0xF7] = REEB_EVENT_JUMP;
			EventMap[0xF8] = REEB_EVENT_MACRO_F8;
			EventMap[0xFF] = REEB_EVENT_RETURN;
			REEB_STATUS_NOTE_MIN = 0x00;
			REEB_STATUS_NOTE_MAX = 0xE8;
			REEB_STATUS_MACRO_MIN = 0x00;
			REEB_STATUS_MACRO_MAX = 0xE8;

		}
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			trackEnd = 0;
			firstNote = 1;
			holdNote = 0;
			inMacro = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			inMacro = 0;
			curVol = 120;
			tie = 0;

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

			if (songPtrs[curTrack] != 0x0000)
			{
				if (songPtrs[curTrack] < bankAmt || songPtrs[curTrack] >= 0x8000)
				{
					seqEnd = 1;
				}
				else
				{
					seqEnd = 0;
					seqPos = songPtrs[curTrack];
				}
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];

				if (seqPos < bankAmt || seqPos >= 0x8000)
				{
					seqEnd = 1;
				}

				if (command[0] >= REEB_STATUS_MACRO_MIN && command[0] <= REEB_STATUS_MACRO_MAX && inMacro == 0)
				{
					/*Rest*/
					if (command[0] >= 0xE6)
					{
						if (command[1] < 0x80)
						{
							curNoteLen = (command[1] & 0x7F) * 5 * chanSpeed;
							seqPos++;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos++;
					}
					else
					{
						macroTimes = command[0];
						macroPos = ReadLE16(&romData[seqPos + 1]);

						if (macroTimes == 0)
						{
							seqPos += 3;
						}
						else
						{
							inMacro = 1;
							macroRet = seqPos + 3;
							seqPos = macroPos;
						}
					}
				}

				else if (command[0] >= REEB_STATUS_NOTE_MIN && command[0] <= REEB_STATUS_NOTE_MAX && inMacro == 1)
				{

					if (tie == 1 && command[0] == curNoteVal)
					{
						if (command[1] < 0x80)
						{
							curNoteLen = (command[1] & 0x7F) * 5 * chanSpeed;
							seqPos++;
						}
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
					}
					else
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}

						/*Rest*/
						if (command[0] >= 0xE6)
						{
							if (command[1] < 0x80)
							{
								curNoteLen = (command[1] & 0x7F) * 5 * chanSpeed;
								seqPos++;
							}
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							if (tie == 1 && curNoteVal == command[0])
							{
								if (command[1] < 0x80)
								{
									curNoteLen = (command[1] & 0x7F) * 5 * chanSpeed;
									seqPos++;
								}
								curDelay += curNoteLen;
								ctrlDelay += curNoteLen;
							}
							curNote = (command[0] & 0x7F) + 24;

							if (curTrack < 2)
							{
								curNote += 12;
							}
							if (command[1] < 0x80)
							{
								curNoteLen = (command[1] & 0x7F) * 5 * chanSpeed;
								seqPos++;
							}

							tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 1;
							midPos = tempPos;
							curDelay = curNoteLen;
							ctrlDelay += curNoteLen;
						}
					}

					curNoteVal = command[0];
					seqPos++;
				}

				else if (EventMap[command[0]] == REEB_EVENT_SET_SPEED)
				{
					chanSpeed = command[1];
					seqPos += 2;
				}

				else if (EventMap[command[0]] == REEB_EVENT_DUTY)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == REEB_EVENT_LENGTH_CH3)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == REEB_EVENT_ENV_DIR)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == REEB_EVENT_WAVEFORM)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == REEB_EVENT_TIE_ON)
				{
					tie = 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == REEB_EVENT_TIE_OFF)
				{
					tie = 0;
					seqPos++;
				}

				else if (EventMap[command[0]] == REEB_EVENT_VOLUME)
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

				else if (EventMap[command[0]] == REEB_EVENT_VIBRATO)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == REEB_EVENT_GLOBAL_VOLUME)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == REEB_EVENT_SWEEP)
				{
					seqPos += 3;
				}

				else if (EventMap[command[0]] == REEB_EVENT_PAN)
				{
					seqPos += 2;
				}

				else if (EventMap[command[0]] == REEB_EVENT_JUMP && inMacro == 0)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						holdNote = 0;
						curDelay = 0;
						midPos = tempPos;
					}
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == REEB_EVENT_MACRO_F8 && inMacro == 0)
				{
					macroTimes = command[0];
					macroPos = ReadLE16(&romData[seqPos + 1]);

					if (macroTimes == 0)
					{
						seqPos += 3;
					}
					else
					{
						inMacro = 1;
						seqPos = macroPos;
					}
				}

				else if (EventMap[command[0]] == REEB_EVENT_RETURN && inMacro == 0)
				{
					if (holdNote == 1)
					{
						tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						holdNote = 0;
						curDelay = 0;
						midPos = tempPos;
					}
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == REEB_EVENT_RETURN && inMacro == 1)
				{
					if (macroTimes > 1)
					{
						macroTimes--;
						seqPos = macroPos;
					}
					else
					{
						inMacro = 0;
						seqPos = macroRet;
					}
				}

				else if (EventMap[command[0]] == REEB_EVENT_UNKNOWN0)
				{
					seqPos++;
				}

				else if (EventMap[command[0]] == REEB_EVENT_UNKNOWN1)
				{
					seqPos += 2;
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