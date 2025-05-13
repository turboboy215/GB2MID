/*Neil Baldwin (Eurocom)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "NBALDWIN.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long seqTablePtrLoc;
long seqTableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[5];
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int jbTempo;

const char NBMagicBytesA[10] = { 0xCB, 0x21, 0xCB, 0x00, 0xCB, 0x21, 0xCB, 0x00, 0x09, 0x01 };
const char NBMagicBytesB[5] = { 0x6F, 0x26, 0x00, 0x29, 0x01 };

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent45(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void NBsong2mid(int songNum, long ptrs[5]);

void NBProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	firstPtr = 0;
	jbTempo = 0;

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

	if (bank == 8)
	{
		jbTempo = 1;
	}

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], NBMagicBytesA, 10)) && foundTable != 1)
		{
			tablePtrLoc = bankAmt + i + 10;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Now search for the sequence table*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], NBMagicBytesB, 5)) && foundTable != 2)
		{
			seqTablePtrLoc = bankAmt + i + 5;
			printf("Found pointer to sequence table at address 0x%04x!\n", seqTablePtrLoc);
			seqTableOffset = ReadLE16(&romData[seqTablePtrLoc - bankAmt]);
			printf("Sequence table starts at 0x%04x...\n", seqTableOffset);
			foundTable = 2;
		}
	}

	if (foundTable == 2)
	{
		i = tableOffset - bankAmt;
		songNum = 1;

		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtrs[0] = ReadLE16(&romData[i]);
			songPtrs[1] = ReadLE16(&romData[i + 2]);
			songPtrs[2] = ReadLE16(&romData[i + 4]);
			songPtrs[3] = ReadLE16(&romData[i + 6]);
			songPtrs[4] = ReadLE16(&romData[i + 8]);
			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			printf("Song %i channel 5: 0x%04X\n", songNum, songPtrs[4]);
			i += 10;
			NBsong2mid(songNum, songPtrs);
			songNum++;
			;
		}

		free(romData);
	}
	else
	{
		free(romData);
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}
}

/*Convert the song data to MIDI*/
void NBsong2mid(int songNum, long ptrs[5])
{
	static const char* TRK_NAMES_NB[5] = { "Square 1", "Square 2", "Wave", "Noise 1", "Noise 2" };
	unsigned char mask = 0;
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 5;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int songEnd = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int chanSpeed = 0;
	int transpose = 0;
	long macroPos = 0;
	long macroRet = 0;
	int repeat = -1;
	long repeatStart = 0;
	unsigned char command[4];
	unsigned char patCommand[4];
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
	long startPos = 0;
	int inMacro = 0;
	int curPat = 0;
	long patPos = 0;
	int portamento = 0;

	if (jbTempo == 1)
	{
		tempo = 120;
	}


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
			songEnd = 0;
			firstNote = 1;
			chanSpeed = 1;

			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			songEnd = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat = -1;
			transpose = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES_NB[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES_NB[curTrack]);
			midPos += strlen(TRK_NAMES_NB[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (songPtrs[curTrack] >= bankAmt && songPtrs[curTrack] < 0x8000)
			{
				romPos = songPtrs[curTrack] - bankAmt;
				while (songEnd == 0)
				{
					patCommand[0] = romData[romPos];
					patCommand[1] = romData[romPos + 1];
					patCommand[2] = romData[romPos + 2];
					patCommand[3] = romData[romPos + 3];

					/*Play pattern*/
					if (patCommand[0] < 0x80)
					{
						curPat = patCommand[0];

						seqPos = ReadLE16(&romData[seqTableOffset - bankAmt] + (2 * curPat)) - bankAmt;
						seqEnd = 0;
						inMacro = 0;

						if (curPat == 0)
						{
							romPos++;
						}

						while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
						{
							command[0] = romData[seqPos];
							command[1] = romData[seqPos + 1];
							command[2] = romData[seqPos + 2];
							command[3] = romData[seqPos + 3];

							/*Play note*/
							if (command[0] < 0x80)
							{
								curNote = command[0] + 24 + transpose;
								curNoteLen = chanSpeed * 5;

								tempPos = WriteNoteEvent45(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								holdNote = 0;
								midPos = tempPos;
								curDelay = 0;
								ctrlDelay += curNoteLen;
								seqPos++;
							}

							/*Set note length (custom)*/
							else if (command[0] == 0x80)
							{
								chanSpeed = command[1];
								seqPos += 2;
							}

							/*Set note length*/
							else if (command[0] >= 0x81 && command[0] <= 0x9F)
							{
								chanSpeed = command[0] - 0x80;
								seqPos++;
							}

							/*Initialize channel envelopes?*/
							else if (command[0] == 0xA0)
							{
								seqPos++;
							}

							/*Set channel envelope*/
							else if (command[0] == 0xA1)
							{
								seqPos += 2;
								if (curTrack == 2)
								{
									seqPos += 2;
								}
							}

							/*Set pitch modulation*/
							else if (command[0] == 0xA2)
							{
								seqPos += 4;
								if (curTrack == 3)
								{
									seqPos -= 2;
								}
							}

							/*Set duty parameters*/
							else if (command[0] == 0xA3)
							{
								seqPos += 4;
							}

							/*Set arpeggio*/
							else if (command[0] == 0xA4)
							{
								seqPos += 4;
							}

							/*Set sweep*/
							else if (command[0] == 0xA5)
							{
								seqPos += 2;
							}

							/*Set vibrato*/
							else if (command[0] == 0xA6)
							{
								seqPos += 4;
							}

							/*Set waveform*/
							else if (command[0] == 0xA7)
							{
								seqPos += 2;
							}

							/*Delay*/
							else if (command[0] == 0xB0)
							{
								curDelay += (command[1] * 5);
								ctrlDelay += (command[1] * 5);
								seqPos += 2;
							}

							/*Enable portamento*/
							else if (command[0] == 0xC0)
							{
								portamento = 1;
								seqPos++;
							}

							/*Disable portamento*/
							else if (command[0] == 0xC1)
							{
								portamento = 0;
								seqPos++;
							}

							/*Go to macro*/
							else if (command[0] == 0xD0)
							{
								macroPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
								macroRet = seqPos + 3;
								seqPos = macroPos;
							}

							/*Exit macro*/
							else if (command[0] == 0xD1)
							{
								seqPos = macroRet;
							}

							/*End of sequence*/
							else if (command[0] == 0xFF)
							{
								seqEnd = 1;
								romPos++;
							}

							/*Unknown command*/
							else
							{
								seqPos++;
							}
						}

					}

					/*Transpose*/
					else if (patCommand[0] == 0x80)
					{
						transpose = (signed char)patCommand[1];
						romPos += 2;
					}

					/*End of repeat*/
					else if (patCommand[0] == 0xA0)
					{
						if (repeat > 1)
						{
							romPos = repeatStart;
							repeat--;
						}
						else
						{
							romPos++;
							repeat = -1;
						}
					}

					/*Repeat the following section*/
					else if (patCommand[0] == 0xA1)
					{
						repeat = patCommand[1];
						repeatStart = romPos + 2;
						romPos += 2;
					}

					/*Loop back to pattern position*/
					else if (patCommand[0] == 0xFD)
					{
						songEnd = 1;
					}

					/*Loop back to start of pattern*/
					else if (patCommand[0] == 0xFE)
					{
						songEnd = 1;
					}

					/*End of pattern (no loop)*/
					else if (patCommand[0] == 0xFF)
					{
						songEnd = 1;
					}

					/*Unknown command*/
					else
					{
						romPos++;
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