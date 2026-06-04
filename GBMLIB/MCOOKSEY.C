/*Mark Cooksey*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MCOOKSEY.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long macroPtrLoc;
long macroOffset;
long sfxPtrLoc;
long sfxOffset;
int i, j;
int drvVers;
char outfile[1000000];
const char MCMagicBytesGB[10] = { 0x6F, 0x26, 0x00, 0x29, 0x54, 0x5D, 0x29, 0x29, 0x19, 0x11 };
const char MCMagicBytesGG[5] = { 0x85, 0x6F, 0x30, 0x01, 0x24 };
const char MCMacroFindOldGB[13] = { 0xE1, 0x11, 0xFE, 0xFF, 0x19, 0x3E, 0x01, 0x22, 0x03, 0x0A, 0xCB, 0x27, 0x11 };
const char MCMacroFindNewGB[5] = { 0xCB, 0x27, 0x30, 0x06, 0x11 };
const char MCMacroFindGG[8] = { 0x23, 0x7E, 0x87, 0x83, 0x5F, 0x30, 0x01, 0x14 };
const char MCSFXFindGB[5] = { 0xCB, 0x27, 0x85, 0x6F, 0x30 };
const char MCSFXFindGG[6] = { 0x87, 0x85, 0x6F, 0x30, 0x01, 0x24 };
long seqPtrs[4];
long noteLenPtr;
long nextPtr;
long endPtr;
int songNum;
long bankAmt;
int foundTable;
int curVols[8];
int volTrack;
int stopCvt;
int sysMode;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* midData;
unsigned char* multiMidData[8];
unsigned char* ctrlMidData;

long midLength;

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

void MCsong2mid(int songNum, long ptrs[], long nextPtr);

void MCProc(int bank)
{
	foundTable = 0;
	drvVers = MC_VER_STD;
	bankAmt = bankSize;
	stopCvt = 0;
	sysMode = SYSTEM_GB;

	if (bank < 0x02)
	{
		bankAmt = 0x0000;
		bank = 0x02;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], MCMagicBytesGB, 10))
		{
			tablePtrLoc = i + 10;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Search for sound effects table*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], MCSFXFindGB, 5))
		{
			sfxPtrLoc = i - 2;
			printf("Found pointer to sound effects table at address 0x%04X!\n", sfxPtrLoc);
			sfxOffset = ReadLE16(&romData[sfxPtrLoc]);
			printf("Sound effects table starts at 0x%04X...\n", sfxOffset);
			break;
		}
	}

	/*Now try to search the bank for macro table loader*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		/*First, try old method (games before 1999)*/
		if (!memcmp(&romData[i], MCMacroFindOldGB, 13))
		{
			macroPtrLoc = i + 13;
			printf("Found pointer to macro table at address 0x%04X!\n", macroPtrLoc);
			macroOffset = ReadLE16(&romData[macroPtrLoc]);
			printf("Macro table starts at 0x%04X...\n", macroOffset);
			break;
		}

		/*Now try new method (games from 1999-)*/
		else if (!memcmp(&romData[i], MCMacroFindNewGB, 5))
		{
			macroPtrLoc = i + 5;
			printf("Found pointer to macro table at address 0x%04X!\n", macroPtrLoc);
			macroOffset = ReadLE16(&romData[macroPtrLoc]);
			printf("Macro table starts at 0x%04X...\n", macroOffset);
			break;
		}
	}

	if (foundTable == 1)
	{
		songNum = 1;

		i = tableOffset;
		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) != 9839)
		{
			seqPtrs[0] = ReadLE16(&romData[i]);
			printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
			seqPtrs[1] = ReadLE16(&romData[i + 2]);
			printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
			seqPtrs[2] = ReadLE16(&romData[i + 4]);
			printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
			seqPtrs[3] = ReadLE16(&romData[i + 6]);
			printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
			noteLenPtr = ReadLE16(&romData[i + 8]);
			printf("Song %i note length table: 0x%04X\n", songNum, noteLenPtr);
			endPtr = nextPtr;

			MCsong2mid(songNum, seqPtrs, endPtr);
			i += 10;
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
void MCsong2mid(int songNum, long ptrs[], long nextPtr)
{
	static const char* TRK_NAMES_GB[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	static const char* TRK_NAMES_GG[4] = { "Square 1", "Square 2", "Square 3", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	unsigned int midPosM[5];
	int trackCnt = 4;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int curDelays[5];
	int prevDelays[5];
	int midChan = 0;
	int seqEnd = 0;
	int seqsEnd[5];
	int tracksEnd[5];
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;
	long startPos = 0;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	long trackSize = 0;
	long trackSizes[5];
	long ctrlTrackSize = 0;

	unsigned int curNote = 0;
	unsigned int curNotes[5];
	unsigned int prevNotes[5];
	int curNoteLen = 0;
	int curNoteLens[5];
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 140;
	float fadeTempo;

	int curInst = 0;
	int curInsts[5];

	int macRepeat = 0;
	long macStart = 0;
	long macEnd = 0;

	int playTimes = 1;

	unsigned long patPos = 0;
	unsigned long seqPos = 0;
	unsigned long patPosM[5];

	unsigned char command[4];
	unsigned char patCommand[4];

	signed int transpose = 0;
	signed int transposes[5];

	int firstNote = 1;
	int firstNotes[5];

	int timeVal = 0;

	int holdNote = 0;
	int holdNotes[5];

	long ctrlDelay = 0;
	long masterDelay = 0;
	long masterDelays[5];

	int patRepeat = 0;
	long patJump = 0;

	int inMacro = 0;
	int inMacroM[5];
	int macros[5][4];

	int playTrack = 0;

	int seqPosM[5];

	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	int valSize = 0;

	long seqTime = 0;

	int tempoFix = 0;

	trackCnt = 4;

	if (sysMode == SYSTEM_GG && tempoFix != 1)
	{
		tempo = 120;
	}
	else
	{
		tempo = 140;
	}

	for (curTrack = 0; curTrack < trackCnt; curTrack++)
	{
		midPosM[curTrack] = 0;
	}
	ctrlMidPos = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < trackCnt; j++)
	{
		multiMidData[j] = (unsigned char*)malloc(midLength);
	}

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		for (k = 0; k < trackCnt; k++)
		{
			multiMidData[k][j] = 0;
		}

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

		/*Now retrieve version information...*/

		if (sysMode == SYSTEM_GG)
		{
			switch (drvVers)
			{
			case MC_VER_STD:
				/*Fall-through*/
			default:
				MC_STATUS_NOTE_MIN = 0x00;
				MC_STATUS_NOTE_MAX = 0x5F;
				MC_STATUS_ALT_NOTE_MIN = 0x80;
				MC_STATUS_ALT_NOTE_MAX = 0xFF;
				EventMap[0x60] = MC_EVENT_TIE;
				EventMap[0x61] = MC_EVENT_STOP;
				EventMap[0x62] = MC_EVENT_JUMP;
				EventMap[0x63] = MC_EVENT_NOISE;
				EventMap[0x64] = MC_EVENT_CALL;
				EventMap[0x65] = MC_EVENT_RETURN;
				EventMap[0x66] = MC_EVENT_SET_LOOP_FLAG;
				EventMap[0x67] = MC_EVENT_SET_NOTE_LENS;
				EventMap[0x68] = MC_EVENT_TEMPO;
				break;

			}
		}
		else
		{
			switch (drvVers)
			{
			case MC_VER_STD:
				/*Fall-through*/
			default:
				MC_STATUS_NOTE_MIN = 0x00;
				MC_STATUS_NOTE_MAX = 0x5F;
				MC_STATUS_ALT_NOTE_MIN = 0x80;
				MC_STATUS_ALT_NOTE_MAX = 0xFF;
				EventMap[0x60] = MC_EVENT_TIE;
				EventMap[0x61] = MC_EVENT_STOP;
				EventMap[0x62] = MC_EVENT_JUMP;
				EventMap[0x63] = MC_EVENT_NOISE;
				EventMap[0x64] = MC_EVENT_CALL;
				EventMap[0x65] = MC_EVENT_RETURN;
				EventMap[0x66] = MC_EVENT_SET_LOOP_FLAG;
				EventMap[0x67] = MC_EVENT_GLOBAL_PAN;
				EventMap[0x68] = MC_EVENT_SET_NOTE_LENS;
				EventMap[0x69] = MC_EVENT_TEMPO;
				EventMap[0x6A] = MC_EVENT_PAN1;
				EventMap[0x6B] = MC_EVENT_PAN2;
				EventMap[0x6C] = MC_EVENT_PAN3;
				EventMap[0x6D] = MC_EVENT_PAN4;
				break;

			}
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			midPosM[curTrack] = 0;
			transposes[curTrack] = 0;
			curDelays[curTrack] = 0;
			ctrlDelay = 0;
			masterDelays[curTrack] = 0;
			firstNotes[curTrack] = 1;
			holdNotes[curTrack] = 0;
			curVols[curTrack] = 120;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0x4D54726B);
			midPosM[curTrack] += 8;
			midTrackBase = midPosM[curTrack];

			curNotes[curTrack] = 0;
			prevNotes[curTrack] = 0;
			curNoteLens[curTrack] = 0;
			curInsts[curTrack] = 0;
			transposes[curTrack] = 0;
			tracksEnd[curTrack] = 0;
			inMacroM[curTrack] = 0;
			macros[curTrack][1] = 0;
			macros[curTrack][3] = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
			midPosM[curTrack] += valSize;
			WriteBE16(&multiMidData[curTrack][midPosM[curTrack]], 0xFF03);
			midPosM[curTrack] += 2;
			Write8B(&multiMidData[curTrack][midPosM[curTrack]], strlen(TRK_NAMES_GB[curTrack]));
			midPosM[curTrack]++;
			sprintf((char*)&multiMidData[curTrack][midPosM[curTrack]], TRK_NAMES_GB[curTrack]);
			midPosM[curTrack] += strlen(TRK_NAMES_GB[curTrack]);

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);

			seqPosM[curTrack] = ptrs[curTrack];
		}

		seqTime = 0;

		while (trackEnd == 0)
		{
			if (tracksEnd[0] == 1 && tracksEnd[1] == 1 && tracksEnd[2] == 1 && tracksEnd[3] == 1)
			{
				trackEnd = 1;
			}

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				while (seqTime >= masterDelays[curTrack] && tracksEnd[curTrack] == 0)
				{
					if ((seqPosM[curTrack] < bankAmt) || (seqPosM[curTrack] >= (bankSize * 2)))
					{
						if (curTrack < trackCnt)
						{
							tracksEnd[curTrack] = 1;
							trackEnd = 1;
							break;
						}
					}
					command[0] = romData[seqPosM[curTrack]];
					command[1] = romData[seqPosM[curTrack] + 1];
					command[2] = romData[seqPosM[curTrack] + 2];
					command[3] = romData[seqPosM[curTrack] + 3];

					command[0] = command[0] & 0x7F;

					if ((command[0] >= MC_STATUS_NOTE_MIN && command[0] <= MC_STATUS_NOTE_MAX))
					{
						command[0] = romData[seqPosM[curTrack]];
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							curDelays[curTrack] = 0;
							midPosM[curTrack] = tempPos;
						}

						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);

						if (command[0] >= MC_STATUS_ALT_NOTE_MIN)
						{
							lowNibble += 16;
						}

						curNotes[curTrack] = (command[0] & 0x7F) + transposes[curTrack];
						if (curTrack == 0 || curTrack == 1)
						{
							curNotes[curTrack] += 36;
						}
						else if (curTrack == 2)
						{
							curNotes[curTrack] += 24;
						}
						else
						{
							curNotes[curTrack] += 12;
						}

						if (curNotes[curTrack] > 127)
						{
							curNotes[curTrack] = 127;
						}
						curNoteLens[curTrack] = romData[noteLenPtr + highNibble] * 5;
						if (lowNibble != curInsts[curTrack])
						{
							if (lowNibble == 0x00 && (command[0] == 0x00 || command[0] == 0x24))
							{
								;
							}
							else
							{
								curInsts[curTrack] = lowNibble;
								firstNotes[curTrack] = 1;
							}
						}

						volTrack = curTrack;

						if (lowNibble == 0 && (command[0] == 0x00 || command[0] == 0x24))
						{
							curDelays[curTrack] += curNoteLens[curTrack];
							masterDelays[curTrack] += curNoteLens[curTrack];
						}
						else
						{
							tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							firstNotes[curTrack] = 0;
							midPosM[curTrack] = tempPos;
							curDelays[curTrack] = curNoteLens[curTrack];
							holdNotes[curTrack] = 1;
							masterDelays[curTrack] += curNoteLens[curTrack];
						}

						seqPosM[curTrack] += 2;

					}

					else if (EventMap[command[0]] == MC_EVENT_TIE)
					{
						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);

						curNoteLens[curTrack] = romData[noteLenPtr + highNibble] * 5;

						curDelays[curTrack] += curNoteLens[curTrack];
						masterDelays[curTrack] += curNoteLens[curTrack];
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_STOP)
					{
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							curDelays[curTrack] = 0;
							midPosM[curTrack] = tempPos;
						}
						tracksEnd[curTrack] = 1;
					}

					else if (EventMap[command[0]] == MC_EVENT_JUMP)
					{
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							curDelays[curTrack] = 0;
							midPosM[curTrack] = tempPos;
						}
						tracksEnd[curTrack] = 1;
					}

					else if (EventMap[command[0]] == MC_EVENT_NOISE)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_CALL)
					{
						if (inMacroM[curTrack] == 0)
						{
							/*Macro position*/
							macros[curTrack][0] = ReadLE16(&romData[macroOffset + (command[1] * 2)]);
							/*Macro transpose*/
							transposes[curTrack] = (signed char)command[2];
							/*Macro times*/
							if (macros[curTrack][3] == 0)
							{
								macros[curTrack][1] = command[3] - 1;
								macros[curTrack][3] = 1;
							}

							/*Macro return*/
							macros[curTrack][2] = seqPosM[curTrack] + 4;

							inMacroM[curTrack]++;
							seqPosM[curTrack] = macros[curTrack][0];
						}
						else
						{
							/*Don't allow nested macros*/
							if (holdNotes[curTrack] == 1)
							{
								tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
								holdNotes[curTrack] = 0;
								curDelays[curTrack] = 0;
								midPosM[curTrack] = tempPos;
							}
							tracksEnd[curTrack] = 1;
						}
					}

					else if (EventMap[command[0]] == MC_EVENT_RETURN)
					{
						if (macros[curTrack][1] <= 0)
						{
							inMacroM[curTrack] = 0;
							seqPosM[curTrack] = macros[curTrack][2];
							transposes[curTrack] = 0;
							macros[curTrack][0] = 0;
							macros[curTrack][3] = 0;
						}
						else
						{
							inMacroM[curTrack] = 0;
							macros[curTrack][1]--;
							seqPosM[curTrack] = macros[curTrack][2] - 4;
						}

					}

					else if (EventMap[command[0]] == MC_EVENT_SET_LOOP_FLAG)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_GLOBAL_PAN)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_SET_NOTE_LENS)
					{
						noteLenPtr = ReadLE16(&romData[seqPosM[curTrack] + 1]);
						seqPosM[curTrack] += 3;
					}

					else if (EventMap[command[0]] == MC_EVENT_TEMPO)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 0.6;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;

						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_PAN1)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_PAN2)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_PAN3)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == MC_EVENT_PAN4)
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
			seqTime += 5;
			ctrlDelay += 5;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			/*End of track*/
			WriteBE32(&multiMidData[curTrack][midPosM[curTrack]], 0xFF2F00);
			midPosM[curTrack] += 4;
			firstNotes[curTrack] = 0;

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		/*End of control track*/
		ctrlMidPos++;
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
		}

		free(multiMidData[0]);
		free(ctrlMidData);
		fclose(mid);
	}
}