/*Software Creations 1 (by Stephen Ruddy)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "SWC1.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tableOffset;
int numSongs;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int curInst;
int drvVers;
int songTempo;

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
void SWC1song2mid(int songNum, long ptrs[4]);

void SWC1Proc(int bank, char parameters[4][100])
{
	curInst = 0;
	drvVers = 1;

	if (bank < 2)
	{
		bank = 2;
	}

	if (parameters[2][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		tableOffset = strtol(parameters[1], NULL, 16);
		numSongs = strtol(parameters[2], NULL, 16);
	}
	else if (parameters[1][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		tableOffset = strtol(parameters[1], NULL, 16);
	}
	else if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
	}

	if (drvVers != 1 && drvVers != 2)
	{
		printf("ERROR: Invalid version number!\n");
		exit(2);
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	songNum = 1;

	if (drvVers == 1)
	{
		i = 0;
		while (songNum <= numSongs)
		{
			for (j = 0; j < 4; j++)
			{
				seqPtrs[j] = romData[tableOffset + (numSongs * j * 2) + i];
				seqPtrs[j] += (romData[tableOffset + (numSongs * j * 2) + numSongs + i]) * 0x100;
				printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
			}
			SWC1song2mid(songNum, seqPtrs);
			i++;
			songNum++;
		}
	}
	else
	{
		i = tableOffset;
		while (songNum <= numSongs)
		{
			songTempo = romData[i];
			printf("Song %i tempo: %02X\n", songNum, songTempo);
			for (j = 0; j < 4; j++)
			{
				seqPtrs[j] = ReadLE16(&romData[i + (j * 2) + 1]);
				printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
			}
			SWC1song2mid(songNum, seqPtrs);
			i += 9;
			songNum++;
		}
	}

	free(romData);

}

/*Convert the song data to MIDI*/
void SWC1song2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
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
	int curNoteSize = 0;
	int autoLen = 0;
	int autoSize = 0;
	int transpose = 0;
	int repeats[500][2];
	int repeatNum;
	unsigned char command[5];
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
	long macro1Pos = 0;
	long macro1End = 0;
	long macro2Pos = 0;
	long macro2End = 0;
	long macro3Pos = 0;
	long macro3End = 0;
	long macro4Pos = 0;
	long macro4End = 0;
	int inMacro = 0;
	long jumpAmt = 0;

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

	for (repeatNum = 0; repeatNum < 16; repeatNum++)
	{
		repeats[repeatNum][0] = -1;
		repeats[repeatNum][1] = 0;
	}

	if (drvVers == 2)
	{
		tempo = songTempo * 2;
	}
	else
	{
		tempo = 150;
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
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			autoLen = 0;
			autoSize = 0;
			transpose = 0;
			inMacro = 0;
			repeatNum = 0;

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

			if (seqPtrs[curTrack] < 0x8000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];
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
				command[3] = romData[seqPos + 3];

				/*Play note/rest*/
				if (command[0] <= 0x7F)
				{
					if (drvVers == 1)
					{
						if (autoLen != 1)
						{
							curNoteLen = command[1] * 5;
							seqPos++;
						}
						/*Rest*/
						if (command[0] == 0x5F || command[0] == 0x32)
						{
							curDelay += curNoteLen;
						}
						else
						{
							curNote = command[0] + 24 + transpose;

							if (curNote > 127)
							{
								curNote = 0;
							}
							/*
							if (curTrack == 2)
							{
								curNote -= 12;
							}
							*/
							else if (curTrack == 3)
							{
								curNote += 12;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos++;
					}
					else
					{
						curNote = command[0] + 24 + transpose;
						seqPos++;

						if (curNote > 127)
						{
							curNote = 0;
						}
						else if (curNote == 121)
						{
							curNote = 121;
						}
						/*
						if (curTrack == 2)
						{
							curNote -= 12;
						}
						*/
						else if (curTrack == 3)
						{
							curNote += 12;
						}

						if (command[0] != 0x60 && command[0] != 0x61 && command[0] != 0x62)
						{
							if (autoSize == 0)
							{
								curNoteSize = romData[seqPos] * 10;
								seqPos++;
							}
							if (autoLen == 0)
							{
								curNoteLen = romData[seqPos] * 5;
								seqPos++;
							}

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;

							firstNote = 0;
							midPos = tempPos;

							ctrlDelay += curNoteLen;
						}
						/*Rest*/
						else if (command[0] == 0x60)
						{
							if (autoLen == 0)
							{
								curNoteLen = romData[seqPos] * 5;
								seqPos++;
							}
							curDelay += curNoteLen;
						}

						else if (command[0] == 0x61)
						{
							curDelay += curNoteLen;
						}

						else if (command[0] == 0x62)
						{
							curDelay += (curNoteLen + 5);
						}

						else
						{
							if (autoLen == 0)
							{
								curNoteLen = command[1] * 5;
								seqPos++;
							}

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							firstNote = 0;
							midPos = tempPos;
							ctrlDelay += curNoteLen;
						}

					}

				}

				/*Repeat section*/
				else if (command[0] == 0x80)
				{
					repeatNum++;
					repeats[repeatNum][0] = command[1];
					repeats[repeatNum][1] = seqPos + 2;
					seqPos += 2;
				}

				/*End of repeat section*/
				else if (command[0] == 0x81)
				{
					if (repeats[repeatNum][0] > 1)
					{
						repeats[repeatNum][0]--;
						seqPos = repeats[repeatNum][1];
					}
					else
					{
						repeats[repeatNum][0] = -1;
						if (repeatNum > 0)
						{
							repeatNum--;
						}
						seqPos++;
					}
				}

				/*Set automatic note length*/
				else if (command[0] == 0x82)
				{
					if (seqPos == 0x7159)
					{
						seqPos = 0x7159;
					}
					if (drvVers == 1 && command[1] == 0x00)
					{
						autoLen = 0;
					}
					else
					{
						autoLen = 1;
						curNoteLen = command[1] * 5;
					}
					seqPos += 2;
				}

				/*End of channel*/
				else if (command[0] == 0x83)
				{
					seqEnd = 1;
				}

				/*Go to macro*/
				else if (command[0] == 0x84)
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&romData[seqPos + 1]);
						macro1End = seqPos + 3;
						inMacro = 1;
						seqPos = macro1Pos;
					}
					else if (inMacro == 1)
					{
						macro2Pos = ReadLE16(&romData[seqPos + 1]);
						macro2End = seqPos + 3;
						inMacro = 2;
						seqPos = macro2Pos;
					}
					else if (inMacro == 2)
					{
						macro3Pos = ReadLE16(&romData[seqPos + 1]);
						macro3End = seqPos + 3;
						inMacro = 3;
						seqPos = macro3Pos;
					}
					else if (inMacro == 3)
					{
						macro4Pos = ReadLE16(&romData[seqPos + 1]);
						macro4End = seqPos + 3;
						inMacro = 4;
						seqPos = macro4Pos;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Return from macro*/
				else if (command[0] == 0x85)
				{
					if (inMacro == 1)
					{
						seqPos = macro1End;
						inMacro = 0;
					}
					else if (inMacro == 2)
					{
						seqPos = macro2End;
						inMacro = 1;
					}
					else if (inMacro == 3)
					{
						seqPos = macro3End;
						inMacro = 2;
					}
					else if (inMacro == 4)
					{
						seqPos = macro4End;
						inMacro = 3;
					}
					else
					{
						seqPos++;
					}
				}

				/*Transpose*/
				else if (command[0] == 0x86)
				{
					if (drvVers == 1 && curTrack != 3)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*"Distort" effect*/
				else if (command[0] == 0x87)
				{
					if (drvVers == 1 && curTrack != 3)
					{
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*Jump to position*/
				else if (command[0] == 0x88)
				{
					jumpAmt = ReadLE16(&romData[seqPos + 1]);

					if (jumpAmt > seqPos)
					{
						seqPos = jumpAmt;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Set envelope*/
				else if (command[0] == 0x89)
				{
					if (drvVers == 1)
					{
						seqPos += 4;
					}
					else
					{
						seqPos++;
					}
				}

				/*Set channel/envelope parameters*/
				else if (command[0] == 0x8A)
				{
					if (seqPos == 0x719E)
					{
						seqPos = 0x719E;
					}
					if (drvVers == 1)
					{
						if (curTrack == 2)
						{
							seqPos += 3;
						}
						else
						{
							seqPos += 4;
						}

					}
					else
					{
						seqPos++;
					}
				}

				/*Toggle "wobble"*/
				else if (command[0] == 0x8B)
				{
					if (drvVers == 1)
					{
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*Portamento*/
				else if (command[0] == 0x8C)
				{
					if (drvVers == 1)
					{
						seqPos += 4;
					}
					else
					{
						seqPos++;
					}
				}

				/*Set vibrato*/
				else if (command[0] == 0x8D)
				{
					if (drvVers == 1)
					{
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*Set duty*/
				else if (command[0] == 0x8E)
				{
					if (drvVers == 1)
					{
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*Set effect*/
				else if (command[0] == 0x8F)
				{
					if (drvVers == 1)
					{
						seqPos += 5;
					}
					else
					{
						seqPos++;
					}
				}

				/*Unknown command 90*/
				else if (command[0] == 0x90)
				{
					seqPos++;
				}

				/*Unknown command 91*/
				else if (command[0] == 0x91)
				{
					if (drvVers == 1)
					{
						seqPos += 5;
					}
					else
					{
						seqPos++;
					}
				}

				/*Unknown command 92*/
				else if (command[0] == 0x92)
				{
					if (drvVers == 1)
					{
						seqPos += 4;
					}
					else
					{
						seqPos++;
					}
				}

				/*Unknown command 93*/
				else if (command[0] == 0x93)
				{
					if (drvVers == 1)
					{
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*Set note size*/
				else if (command[0] == 0x94)
				{
					if (drvVers == 1)
					{
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*Set panning*/
				else if (command[0] == 0x95)
				{
					if (drvVers == 1)
					{
						seqPos += 2;
					}
					else
					{
						seqPos++;
					}
				}

				/*Conditional panning?*/
				else if (command[0] == 0x96)
				{
					if (drvVers == 1)
					{
						if (command[1] == 0x00)
						{
							seqPos += 2;
						}
						else
						{
							seqPos += 4;
						}
					}
					else
					{
						seqPos++;
					}
				}

				/*Unknown command 97*/
				else if (command[0] == 0x97)
				{
					seqPos++;
				}

				/*Set duty (later)*/
				else if (command[0] == 0x98)
				{
					seqPos += 2;
				}

				/*Set envelope (later)*/
				else if (command[0] == 0x99)
				{
					if (drvVers == 1)
					{
						seqEnd = 1;
					}
					else
					{
						autoSize = 1;
						seqPos += 2;
					}

				}

				/*Song loop point*/
				else if (command[0] == 0x9A)
				{
					seqPos++;
				}

				/*Go to song loop point*/
				else if (command[0] == 0x9B)
				{
					seqEnd = 1;
				}

				/*Disable automatic note length*/
				else if (command[0] == 0x9C)
				{
					autoLen = 0;
					seqPos++;
				}

				/*Disable automatic note length*/
				else if (command[0] == 0x9D)
				{
					autoSize = 0;
					seqPos++;
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
		fclose(mid);
	}
}