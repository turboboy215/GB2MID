/*Sculptured Software*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "SCULPT.H"

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
long seqPtrs[4];
long bankAmt;
int foundTable;
int curInst;
int numSongs;
int ptrOverride;
int ultimaMode;

char* argv4;

const unsigned char SculptMagicBytes[5] = { 0x29, 0x29, 0x29, 0x09, 0x01 };
const unsigned char SculptUltimaNotes[] = { 0x15, 0x17, 0x0C, 0x11, 0x13, 0x21, 0x23, 0x18, 0x1A, 0x1C,
0x1D, 0x1F, 0x2D, 0x2F, 0x24, 0x26, 0x28, 0x29, 0x2B, 0x30, 0x32, 0x34, 0x35, 0x2C, 0x22, 0x1E,
0x20, 0x27, 0xFF, 0x80, 0x81, 0xFF };

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Sculptsong2mid(int songNum, long ptrs[4]);

void SculptProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	ptrOverride = 0;
	ultimaMode = 0;
	curInst = 0;

	if (parameters[1][0] != 0)
	{
		numSongs = strtol(parameters[0], NULL, 16);
		if (strcmp(parameters[1], "u") == 0 || strcmp(parameters[1], "U") == 0)
		{
			ultimaMode = 1;
		}
		else
		{
			tableOffset = strtol(parameters[1], NULL, 16);
			ptrOverride = 1;
			foundTable = 1;
		}
	}
	else if (parameters[0][0] != 0)
	{
		numSongs = strtol(parameters[0], NULL, 16);
	}

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	if (ptrOverride != 1)
	{
		for (i = 0; i < bankSize; i++)
		{
			if ((!memcmp(&romData[i], SculptMagicBytes, 5)) && foundTable != 1)
			{
				tablePtrLoc = bankAmt + i + 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;
			}
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt;
		songNum = 1;
		while (songNum <= numSongs)
		{
			songPtrs[0] = ReadLE16(&romData[i]);
			songPtrs[1] = ReadLE16(&romData[i + 2]);
			songPtrs[2] = ReadLE16(&romData[i + 4]);
			songPtrs[3] = ReadLE16(&romData[i + 6]);
			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			Sculptsong2mid(songNum, songPtrs);
			i += 9;
			songNum++;
		}

		free(romData);
	}
	else
	{
		free(romData);
		printf("ERROR: Magic bytes not found!\n");
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void Sculptsong2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
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
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	int inMacro = 0;
	int repeat1 = -1;
	long repeat1Pos = 0;
	int repeat2 = -1;
	long repeat2Pos = 0;
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
	long tempPos = 0;
	long startPos = 0;
	int chanSpeed = 0;
	int noteMode = 0;
	int remainder = 0;
	int event = 0;

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

	for (j = 0; j < trackCnt; j++)
	{
		seqPtrs[j] = 0;
	}

	for (j = 0; j < trackCnt; j++)
	{
		activeChan[j] = 0;
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
			if (ptrs[curTrack] != 0xFFFF)
			{
				romPos = ptrs[curTrack] - bankAmt;
				activeChan[curTrack] = ReadLE16(&romData[romPos]);
			}
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			inMacro = 0;
			repeat1 = -1;
			repeat2 = -1;
			noteMode = 0;
			remainder = 0;
			event = 0;
			chanSpeed = 0x90;

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

			if (activeChan[curTrack] >= bankAmt && activeChan[curTrack] < 0x8000)
			{
				seqEnd = 0;
				seqPos = activeChan[curTrack] - bankAmt;

				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					if (songNum == 39 && curTrack == 1)
					{
						songNum = 39;
					}

					if (ultimaMode == 1 && noteMode == 1 && command[0] != 0xFF)
					{
						event = command[0] / 8;

						remainder = command[0] % 8;

						switch (remainder)
						{
						case 0x00:
							curNoteLen = chanSpeed;
							break;
						case 0x01:
							curNoteLen = chanSpeed / 2;
							break;
						case 0x02:
							curNoteLen = chanSpeed / 4;
							break;
						case 0x03:
							curNoteLen = chanSpeed / 8;
							break;
						case 0x04:
							curNoteLen = chanSpeed / 16;
							break;
						case 0x05:
							curNoteLen = chanSpeed * 0.75;
							break;
						case 0x06:
							curNoteLen = (chanSpeed * 0.75) / 2;
							break;
						case 0x07:
							curNoteLen = (chanSpeed * 0.75) / 4;
							break;
						default:
							curNoteLen = chanSpeed;
							break;
						}
						curNoteLen *= 5;

						if (event < 0x1C)
						{
							curNote = SculptUltimaNotes[command[0] / 8] + 12;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							ctrlDelay += curNoteLen;
							curDelay = 0;
						}

						else if (event == 0x1C)
						{
							curNote = command[1] + 12;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							ctrlDelay += curNoteLen;
							curDelay = 0;
							seqPos++;
						}

						else if (event == 0x1D || event == 0x1E)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}

						else if (event >= 0x1F)
						{
							noteMode = 0;
						}


						seqPos++;
					}

					/*Play note*/
					else if (command[0] <= 0x7F)
					{
						curNote = command[0] + 12;
						curNoteLen = command[1] * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						ctrlDelay += curNoteLen;
						curDelay = 0;
						seqPos += 2;
					}

					/*Rest*/
					else if (command[0] >= 0x80 && command[0] <= 0x8F)
					{
						if (command[0] == 0x80)
						{
							curDelay += (command[1] * 5);
							ctrlDelay += (command[1] * 5);
							seqPos += 2;
						}
						else
						{
							seqPos++;
							if (ultimaMode == 1)
							{

								if (command[0] == 0x82)
								{
									noteMode = 1;
								}
								else if (command[0] == 0x83)
								{
									chanSpeed = command[1];
									seqPos++;
								}
							}
						}
					}

					/*Set channel/noise type*/
					else if (command[0] >= 0x90 && command[0] <= 0x9F)
					{
						seqPos++;
					}

					/*Set channel/instrument parameters*/
					else if (command[0] >= 0xA0 && command[0] <= 0xAF)
					{
						seqPos += 12;
						if (ultimaMode == 1)
						{
							seqPos--;
						}
					}

					/*Set frequency?*/
					else if (command[0] >= 0xB0 && command[0] <= 0xBF)
					{
						seqPos += 2;
					}

					/*Set channel volumes*/
					else if (command[0] == 0xC0)
					{
						seqPos += 2;
					}

					/*Set panning?*/
					else if (command[0] == 0xC1)
					{
						seqPos += 2;
					}

					/*Repeat the following section X times*/
					else if (command[0] == 0xC2)
					{
						if (repeat1 == -1)
						{
							repeat1 = command[1];
							repeat1Pos = seqPos + 2;
							seqPos += 2;
						}
						else
						{
							repeat2 = command[1];
							repeat2Pos = seqPos + 2;
							seqPos += 2;
						}
					}

					/*Song loop point*/
					else if (command[0] == 0xC3)
					{
						repeat1 = 0;
						repeat1Pos = seqPos + 1;
						seqPos++;
					}

					/*Go to repeat point/song loop point*/
					else if (command[0] == 0xC4)
					{
						if (repeat2 == -1)
						{
							if (repeat1 == 0)
							{
								seqEnd = 1;
							}
							else if (repeat1 > 1)
							{
								seqPos = repeat1Pos;
								repeat1--;
							}
							else if (repeat1 <= 1)
							{
								repeat1 = -1;
								seqPos++;
							}
						}
						else if (repeat2 > -1)
						{
							if (repeat2 > 1)
							{
								seqPos = repeat2Pos;
								repeat2--;
							}
							else if (repeat2 <= 1)
							{
								repeat2 = -1;
								seqPos++;
							}
						}
					}


					/*Go to macro*/
					else if (command[0] == 0xC5)
					{
						if (inMacro == 0)
						{
							macro1Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro1Ret = seqPos + 3;
							seqPos = macro1Pos;
							inMacro = 1;
						}
						else if (inMacro == 1)
						{
							macro2Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
							macro2Ret = seqPos + 3;
							seqPos = macro2Pos;
							inMacro = 2;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Unknown command C6*/
					else if (command[0] == 0xC6)
					{
						seqPos++;
					}

					/*Unknown command C7*/
					else if (command[0] == 0xC7)
					{
						seqPos++;
					}

					/*Portamento?*/
					else if (command[0] >= 0xD0 && command[0] <= 0xDF)
					{
						seqPos += 2;
					}

					/*Set tuning*/
					else if (command[0] >= 0xE0 && command[0] <= 0xFE)
					{
						seqPos += 2;
					}

					/*End of channel or return from macro*/
					else if (command[0] == 0xFF)
					{
						if (inMacro == 0)
						{
							seqEnd = 1;
						}
						else if (inMacro == 1)
						{
							seqPos = macro1Ret;
							inMacro = 0;
						}
						else if (inMacro == 2)
						{
							seqPos = macro2Ret;
							inMacro = 1;
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

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}