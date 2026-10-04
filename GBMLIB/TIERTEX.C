/*Tiertex*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "TIERTEX.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tablePtrLoc;
long tableOffset;

char outfile[1000000];

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*End of frequency table - sequence pointers start immediately after*/
const char TTMusicFind[10] = { 0xE5, 0x07, 0xE7, 0x07, 0xE8, 0x07, 0xE9, 0x07, 0xEB, 0x07 };

/*Alternate table end + initial pointer for Triple Play 2001*/
const char TTMusicFindTP[10] = { 0xD9, 0x07, 0xDB, 0x07, 0xDD, 0x07, 0xDF, 0x07, 0x39, 0x50 };
long seqPtrs[4];
long nextPtr;
long endPtr;
long bankAmt;
int songNum;
int foundTable;

long c1Pos = 0x4675;
long c2Pos = 0x4687;
long c3Pos = 0x4699;
long c4Pos = 0x46AB;

int curInst;
int curVol;
int drvVers;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void TTsong2mid(int songNum, long ptrs[]);

void TTProc(int bank, char parameters[4][100])
{
	drvVers = TT_VER_STD;
	curVol = 120;
	curInst = 0;
	foundTable = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	bankAmt = bankSize;

	if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);

		if (drvVers < TT_VER_WC94 && drvVers > TT_VER_STD)
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

	/*Try to search the bank for song table after frequency table*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], TTMusicFind, 10)) && foundTable != 1)
		{
			tableOffset = i + 10;
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Alternate method for Triple Play 2001*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], TTMusicFindTP, 10)) && foundTable != 1)
		{
			tableOffset = i + 8;
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		if (drvVers != TT_VER_WC94)
		{
			songNum = 1;
			i = tableOffset;
			while (ReadLE16(&romData[i]) != 0xFFFF)
			{
				seqPtrs[0] = ReadLE16(&romData[i]);
				printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[i + 2]);
				printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[i + 4]);
				printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[i + 6]);
				printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
				TTsong2mid(songNum, seqPtrs);

				i += 8;
				songNum++;
			}
		}
		else
		{
			songNum = 1;
			i = tableOffset;
			c1Pos = 0x4675;
			c2Pos = 0x4687;
			c3Pos = 0x4699;
			c4Pos = 0x46AB;
			for (i = 0; i < 9; i++)
			{
				seqPtrs[0] = ReadLE16(&romData[c1Pos + (i * 2)]);
				printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
				seqPtrs[1] = ReadLE16(&romData[c2Pos + (i * 2)]);
				printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
				seqPtrs[2] = ReadLE16(&romData[c3Pos + (i * 2)]);
				printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
				seqPtrs[3] = ReadLE16(&romData[c4Pos + (i * 2)]);
				printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
				TTsong2mid(songNum, seqPtrs);
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
		exit(-1);
	}
}

/*Convert the song data to MIDI*/
void TTsong2mid(int songNum, long seqPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	long seqPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int noteTrans = 0;
	int ticks = 120;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curVol = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	long tempPos = 0;

	long tempo = 0;

	int hasPlayedNote = 0;

	unsigned int macReturn = 0;
	unsigned long macroBase = 0;
	signed int macTranspose = 0;
	int transposeVal = 0;
	int initTranspose = 0;
	unsigned short macCount = 0;

	unsigned char command[4];
	unsigned char lowNibble;
	unsigned char highNibble;

	int firstNote = 1;
	int repeat = 0;

	unsigned long repeatBase = 0;

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
		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], trackCnt + 1);
		WriteBE16(&ctrlMidData[ctrlMidPos + 4], ticks);
		ctrlMidPos += 6;

		/*Get the initial tempo*/

		if (drvVers != TT_VER_WC94)
		{
			tempo = 150;
		}
		else
		{
			tempo = 120;
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

		switch (drvVers)
		{
		case TT_VER_WC94:
			/*Fall-through*/
		case TT_VER_STD:
		default:
			TT_STATUS_REST = 0x00;
			TT_STATUS_NOTE_MIN = 0x01;
			TT_STATUS_NOTE_MAX = 0x7F;
			TT_STATUS_PROG_CHANGE_MIN = 0x80;
			TT_STATUS_PROG_CHANGE_MAX = 0xBF;
			EventMap[0xFF] = TT_EVENT_RESTART;
			EventMap[0xFE] = TT_EVENT_STOP;
			EventMap[0xFD] = TT_EVENT_REPEAT_END;
			EventMap[0xFC] = TT_EVENT_REPEAT_START;
			EventMap[0xF9] = TT_EVENT_CALL;
			EventMap[0xF8] = TT_EVENT_RETURN;
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
			seqEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			curInst = 0;
			macTranspose = 0;
			transposeVal = 0;
			hasPlayedNote = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);

			seqPos = seqPtrs[curTrack];

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = romData[seqPos];
				command[1] = romData[seqPos + 1];
				command[2] = romData[seqPos + 2];
				command[3] = romData[seqPos + 3];

				if (command[0] == TT_STATUS_REST)
				{
					curNoteLen = command[1] * 5;
					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				else if (command[0] >= TT_STATUS_NOTE_MIN && command[0] <= TT_STATUS_NOTE_MAX)
				{
					if (hasPlayedNote == 0)
					{
						hasPlayedNote = 1;
					}
					curNote = command[0] + macTranspose;

					curNoteLen = command[1] * 5;

					if (curTrack == 3)
					{
						/*Re-map percussion to general MIDI*/
						if (curTrack == 3 && drvVers != TT_VER_WC94)
						{
							if (command[0] == 1)
							{
								curNote = 35;
							}
							else if (command[0] == 2)
							{
								curNote = 38;
							}
							else if (command[0] == 3)
							{
								curNote = 42;
							}
							else if (command[0] == 4)
							{
								curNote = 46;
							}
							else if (command[0] == 5)
							{
								curNote = 49;
							}
							else if (command[0] == 6)
							{
								curNote = 40;
							}
							else if (command[0] == 7)
							{
								curNote = 41;
							}
							else if (command[0] == 8)
							{
								curNote = 43;
							}
							else if (command[0] == 9)
							{
								curNote = 45;
							}
							else if (command[0] == 10)
							{
								curNote = 47;
							}
							else if (command[0] == 11)
							{
								curNote = 48;
							}
							else if (command[0] == 12)
							{
								curNote = 50;
							}
						}
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					masterDelay += curNoteLen;
					seqPos += 2;
				}

				else if (command[0] >= TT_STATUS_PROG_CHANGE_MIN && command[0] <= TT_STATUS_PROG_CHANGE_MAX)
				{
					curInst = command[0] - TT_STATUS_PROG_CHANGE_MIN;
					firstNote = 1;
					seqPos++;
				}

				else if (EventMap[command[0]] == TT_EVENT_RESTART)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == TT_EVENT_STOP)
				{
					seqEnd = 1;
				}

				else if (EventMap[command[0]] == TT_EVENT_REPEAT_END)
				{
					/*Workaround for A Bug's Life*/
					if (repeat == 8 && seqPtrs[0] == 0x4F0D && seqPtrs[1] == 0x4DA9 && seqPtrs[2] == 0x4E89 && seqPtrs[3] == 0x4F30)
					{
						repeat = 1;
					}

					/*Workaround for Olympic Summer Games*/
					if (repeat == 200 || repeat == 100)
					{
						repeat = 1;
					}

					/*Workaround for Olympic Summer Games*/
					if (repeat == 20 && seqPtrs[0] == 0x6EBE && seqPtrs[1] == 0x6ECD && seqPtrs[2] == 0x43B4 && seqPtrs[3] == 0x6EEB)
					{
						repeat = 1;
					}
					if (repeat > 1)
					{
						seqPos = repeatBase;
						repeat--;
					}
					else
					{
						seqPos++;
					}
				}

				else if (EventMap[command[0]] == TT_EVENT_REPEAT_START)
				{
					/*Fix for Hercules empty channel 4 (fanfare)*/
					if (command[1] == 0xFF && command[2] == 0xF9)
					{
						seqEnd = 1;
					}
					repeatBase = seqPos + 2;
					repeat = command[1];
					seqPos = repeatBase;
				}

				else if (EventMap[command[0]] == TT_EVENT_CALL)
				{
					macCount = command[1];

					if (transposeVal == 0)
					{
						transposeVal = 1;
						initTranspose = (signed char)command[2];
					}
					macTranspose = (signed char)command[2] - initTranspose;
					macroBase = ReadLE16(&romData[seqPos + 3]);
					if (macroBase == seqPtrs[curTrack])
					{
						seqEnd = 1;
					}
					macReturn = seqPos + 5;
					seqPos = macroBase;
				}

				else if (EventMap[command[0]] == TT_EVENT_RETURN)
				{
					if (macCount > 1)
					{
						seqPos = macroBase;
						macCount--;
					}
					else
					{
						macTranspose = 0;
						if (hasPlayedNote == 0)
						{
							transposeVal = 0;
						}
						/*Workaround for Men in Black*/
						if (seqPos == 0x5A1D && seqPtrs[0] == 0x59FA && seqPtrs[1] == 0x5A0C && seqPtrs[2] == 0x5A1E && seqPtrs[3] == 0x5A2F)
						{
							seqEnd = 1;
						}
						if (seqPos == 0x5A2E && seqPtrs[0] == 0x59FA && seqPtrs[1] == 0x5A0C && seqPtrs[2] == 0x5A1E && seqPtrs[3] == 0x5A2F)
						{
							seqEnd = 1;
						}
						if (seqPos == 0x5AD7 && seqPtrs[0] == 0x59FA && seqPtrs[1] == 0x5A0C && seqPtrs[2] == 0x5A1E && seqPtrs[3] == 0x5A2F)
						{
							seqEnd = 1;
						}
						seqPos = macReturn;
					}
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		free(exRomData);
		fclose(mid);

	}

}