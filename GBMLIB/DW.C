/*David Whittaker*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DW.H"

#define bankSize 16384

int multiBanks;
int curBank;

char folderName[100];

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
int drvVers;
char outfile[1000000];
const unsigned char DWMagicBytes[5] = { 0x22, 0x05, 0x20, 0xFC, 0x21 };

long seqPtrs[4];
long firstPtrs[4];
long noteLenPtr;
long nextPtr;
long endPtr;
int songNum;
long songPtrs[4];
long firstPtrs[4];
long songTempo;
long bankAmt;
int foundTable;
int curVols[8];
int volTrack;
int stopCvt;

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
void DWsong2mid(int songNum, long ptrs[4], int songTempo);

void DWProc(int bank)
{
	foundTable = 0;
	drvVers = DW_VER_STD;
	bankAmt = bankSize;
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
		if (!memcmp(&romData[i], DWMagicBytes, 5) && ReadLE16(&romData[i + 5]) < 0x8000)
		{
			tablePtrLoc = i + 5;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	if (foundTable == 1)
	{
		songNum = 1;

		i = tableOffset;

		firstPtrs[0] = ReadLE16(&romData[i + 1]);
		firstPtrs[1] = ReadLE16(&romData[i + 3]);
		firstPtrs[2] = ReadLE16(&romData[i + 5]);
		while ((i < firstPtrs[0]) && (i < firstPtrs[1]) && (i < firstPtrs[2]) && ReadLE16(&romData[i + 2]) != 0 && (ReadLE16(&romData[i + 1]) < (bankSize * 2)))
		{
			songTempo = romData[i];
			printf("Song %i tempo: %i\n", songNum, songTempo);
			songPtrs[0] = ReadLE16(&romData[i + 1]);
			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			songPtrs[1] = ReadLE16(&romData[i + 3]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			songPtrs[2] = ReadLE16(&romData[i + 5]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			songPtrs[3] = ReadLE16(&romData[i + 7]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			nextPtr = ReadLE16(&romData[i + 10]);
			DWsong2mid(songNum, songPtrs, songTempo);
			i += 9;
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
void DWsong2mid(int songNum, long ptrs[4], int songTempo)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
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

	int curPhrase;
	int phraseEnd[4] = { 0, 0, 0, 0 };

	int inMacro = 0;
	int inMacroM[5];
	int macros[5][4];

	int playTrack = 0;

	int seqPosM[5];

	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	int valSize = 0;

	long seqTime = 0;

	int hasLooped[4] = { 0, 0, 0, 0 };

	trackCnt = 4;

	tempo = songTempo * 3.5;

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

		switch (drvVers)
		{
		case DW_VER_STD:
			/*Fall-through*/
		default:
			DW_STATUS_NOTE_MIN = 0x00;
			DW_STATUS_NOTE_MAX = 0x5F;
			DW_STATUS_NOTE_LEN_MIN = 0x60;
			DW_STATUS_NOTE_LEN_MAX = 0x7F;
			EventMap[0xF4] = DW_EVENT_TEMPO;
			EventMap[0xF5] = DW_EVENT_LOOP;
			EventMap[0xF6] = DW_EVENT_ENV;
			EventMap[0xF7] = DW_EVENT_VIBRATO;
			EventMap[0xF8] = DW_EVENT_REST;
			EventMap[0xF9] = DW_EVENT_TIE;
			EventMap[0xFA] = DW_EVENT_DUTY;
			EventMap[0xFB] = DW_EVENT_GLOBAL_TRANSPOSE;
			EventMap[0xFC] = DW_EVENT_LOCAL_TRANSPOSE;
			EventMap[0xFD] = DW_EVENT_SWEEP;
			EventMap[0xFE] = DW_EVENT_STOP;
			EventMap[0xFF] = DW_EVENT_RETURN;
			break;
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
			patPosM[curTrack] = ptrs[curTrack];
			phraseEnd[curTrack] = 1;
			hasLooped[curTrack] = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(multiMidData[curTrack], midPosM[curTrack], 0);
			midPosM[curTrack] += valSize;
			WriteBE16(&multiMidData[curTrack][midPosM[curTrack]], 0xFF03);
			midPosM[curTrack] += 2;
			Write8B(&multiMidData[curTrack][midPosM[curTrack]], strlen(TRK_NAMES[curTrack]));
			midPosM[curTrack]++;
			sprintf((char*)&multiMidData[curTrack][midPosM[curTrack]], TRK_NAMES[curTrack]);
			midPosM[curTrack] += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSizes[curTrack] = midPosM[curTrack] - midTrackBase;
			WriteBE16(&multiMidData[curTrack][midTrackBase - 2], trackSizes[curTrack]);
		}

		seqTime = 0;
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			patPosM[curTrack] = ptrs[curTrack];
			if (patPosM[curTrack] == 0x0000)
			{
				tracksEnd[curTrack] = 1;
			}
			else
			{
				patPosM[curTrack] = songPtrs[curTrack];
				seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
			}
		}

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
					command[0] = romData[seqPosM[curTrack]];
					command[1] = romData[seqPosM[curTrack] + 1];
					command[2] = romData[seqPosM[curTrack] + 2];
					command[3] = romData[seqPosM[curTrack] + 3];

					if (command[0] >= DW_STATUS_NOTE_MIN && command[0] <= DW_STATUS_NOTE_MAX)
					{
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							curDelays[curTrack] = 0;
							midPosM[curTrack] = tempPos;
						}
						if (curTrack != 2)
						{
							curNotes[curTrack] = command[0] + 36 + transposes[curTrack] + transpose;
						}
						else if (curTrack == 2)
						{
							curNotes[curTrack] = command[0] + 24 + transposes[curTrack] + transpose;
						}
						volTrack = curTrack;
						curVol = curVols[curTrack];
						tempPos = WriteNoteEventAltOn(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
						firstNotes[curTrack] = 0;
						midPosM[curTrack] = tempPos;
						curDelays[curTrack] = curNoteLens[curTrack];
						holdNotes[curTrack] = 1;
						masterDelays[curTrack] += curNoteLens[curTrack];

						seqPosM[curTrack]++;
					}
					else if (command[0] >= DW_STATUS_NOTE_LEN_MIN && command[0] <= DW_STATUS_NOTE_LEN_MAX)
					{
						curNoteLens[curTrack] = 30 + (30 * (command[0] - DW_STATUS_NOTE_LEN_MIN));
						seqPosM[curTrack]++;
					}

					else if (EventMap[command[0]] == DW_EVENT_TEMPO)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						tempo = command[1] * 3.5;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == DW_EVENT_LOOP)
					{
						if (hasLooped[curTrack] == 0)
						{
							patPosM[curTrack] = (ReadLE16(&romData[seqPosM[curTrack] + 1]));
							if (ReadLE16(&romData[patPosM[curTrack]]) == 0x0000)
							{
								tracksEnd[curTrack] = 1;
							}
							else
							{
								seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
								hasLooped[curTrack] = 1;
							}
						}
						else
						{
							tracksEnd[curTrack] = 1;
						}

					}

					else if (EventMap[command[0]] == DW_EVENT_ENV)
					{
						if (curTrack != 2)
						{
							/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
							if ((command[1] & 0x08) == 0x00)
							{
								curVols[curTrack] = (command[1] & 0xF0) / 2;
							}
							else
							{
								if (((command[1] & 0xF0) / 2) == curVols[curTrack])
								{
									curVols[curTrack] = (command[1] & 0xF0) / 2;
								}
								else
								{
									curVols[curTrack] = 120;
								}

							}

							if (curVols[curTrack] == 0)
							{
								curVols[curTrack] = 1;
							}
							seqPosM[curTrack] += 2;
						}
						else
						{
							switch (command[1])
							{
							case 0x20:
								curVols[curTrack] = 120;
								break;
							case 0x40:
								curVols[curTrack] = 60;
								break;
							case 0x60:
								curVols[curTrack] = 30;
								break;
							case 0x00:
								curVols[curTrack] = 1;
								break;
							default:
								curVols[curTrack] = 120;
								break;
							}
							seqPosM[curTrack] += 3;
						}
					}

					else if (EventMap[command[0]] == DW_EVENT_VIBRATO)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == DW_EVENT_REST)
					{
						if (holdNotes[curTrack] == 1)
						{
							tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
							holdNotes[curTrack] = 0;
							curDelays[curTrack] = 0;
							midPosM[curTrack] = tempPos;
						}
						curDelays[curTrack] += curNoteLens[curTrack];
						masterDelays[curTrack] += curNoteLens[curTrack];
						seqPosM[curTrack]++;

					}

					else if (EventMap[command[0]] == DW_EVENT_TIE)
					{
						curDelays[curTrack] += curNoteLens[curTrack];
						masterDelays[curTrack] += curNoteLens[curTrack];
						seqPosM[curTrack]++;
					}

					else if (EventMap[command[0]] == DW_EVENT_DUTY)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == DW_EVENT_GLOBAL_TRANSPOSE)
					{
						transpose = (signed char)command[1];
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == DW_EVENT_LOCAL_TRANSPOSE)
					{
						transposes[curTrack] = (signed char)command[1];
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == DW_EVENT_SWEEP)
					{
						seqPosM[curTrack] += 2;
					}

					else if (EventMap[command[0]] == DW_EVENT_STOP)
					{
						trackEnd = 1;
						tracksEnd[curTrack] = 1;
					}

					else if (EventMap[command[0]] == DW_EVENT_RETURN)
					{
						patPosM[curTrack] += 2;

						if (ReadLE16(&romData[patPosM[curTrack]]) == 0x0000)
						{
							tracksEnd[curTrack] = 1;
						}
						else
						{
							seqPosM[curTrack] = ReadLE16(&romData[patPosM[curTrack]]);
						}
					}

					/*Unknown command*/
					else
					{
						seqPosM[curTrack]++;
					}

				}
				seqTime += 5;
				ctrlDelay += 5;
			}
		}
		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			if (holdNotes[curTrack] == 1)
			{
				tempPos = WriteNoteEventAltOff(multiMidData[curTrack], midPosM[curTrack], curNotes[curTrack], curNoteLens[curTrack], curDelays[curTrack], firstNotes[curTrack], curTrack, curInsts[curTrack]);
				holdNotes[curTrack] = 0;
				curDelays[curTrack] = 0;
				midPosM[curTrack] = tempPos;
			}
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