/*Natsume*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "NATSUME.H"

#define bankSize 16384

FILE* rom, * mid;
long bank1;
long masterBank;
long offset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int chanMask;
long bankAmt;
long firstPtr;
int format;
int curInst;
long stopPtr;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;
int multiBanks;
int curBank;

char folderName[100];

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Natsong2mid(int songNum, long ptr);

void NatProc(int bank, char parameters[4][100])
{
	firstPtr = 0;
	format = 1;
	curInst = 0;
	stopPtr = 0;
	bank1 = bank;
	offset = strtoul(parameters[0], NULL, 16);

	if (parameters[1][0] != 0)
	{
		format = strtoul(parameters[1], NULL, 16);
		masterBank = bank;
	}

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	if (format != 2)
	{
		fseek(rom, ((bank1 - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}
	else if (format == 2)
	{
		fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);

	}

	/*Convert the songs*/
	i = offset - bankAmt;
	firstPtr = ReadLE16(&romData[i]);

	/*Workaround for Ninja Gaiden Shadow*/
	if (bank == 4 && offset == 0x4A5F)
	{
		firstPtr = 0x4A81;
	}
	songNum = 1;

	while (i < (firstPtr - bankAmt))
	{
		songPtr = ReadLE16(&romData[i]);
		printf("Song %i: 0x%04X\n", songNum, songPtr);
		if (songPtr > bankAmt && songPtr < (bankAmt * 2))
		{
			Natsong2mid(songNum, songPtr);
		}
		i += 2;
		songNum++;
	}
	free(romData);

}

/*Convert the song data to MIDI*/
void Natsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char mask[3];
	long chanPtrs[4];
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int chanSpeed = 0;
	int octave = 0;
	int transpose = 0;
	long macroPos = 0;
	long macroRet = 0;
	int repeat1 = 0;
	int repeat2 = 0;
	long repeat1Pos = 0;
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
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;


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

		romPos = ptr - bankAmt;

		if (format != 4)
		{
			mask[0] = romData[romPos];
			mask[1] = romData[romPos + 1];
		}


		/*If later format, also get the current song's bank*/
		if (format == 2)
		{
			bank1 = romData[romPos + 2];
			fseek(rom, (bank1 * bankSize), SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize);
			fread(exRomData, 1, bankSize, rom);
			romPos++;
		}

		romPos += 2;

		if (format == 4)
		{
			romPos -= 2;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			chanPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			if (chanPtrs[curTrack] < bankAmt || chanPtrs[curTrack] > (bankSize * 2))
			{
				seqEnd = 1;
			}
			seqPos = chanPtrs[curTrack] - bankAmt;

			octave = 0;
			transpose = 0;
			seqEnd = 0;
			firstNote = 1;
			chanSpeed = 1;

			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat1 = -1;
			repeat2 = -1;
			macroPos = 0;

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

			while (seqEnd == 0 && seqPos < bankSize && midPos < 48000)
			{
				if (format != 2)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
					command[4] = romData[seqPos + 4];
					command[5] = romData[seqPos + 5];
					command[6] = romData[seqPos + 6];
					command[7] = romData[seqPos + 7];

					/*Play note*/
					if (command[0] < 0xC0)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);
						if (curTrack != 3)
						{
							curNote = lowNibble + (12 * octave) + transpose;
						}
						else if (curTrack == 3)
						{
							curNote = lowNibble + 48;
						}

						curNoteLen = (highNibble + 1) * chanSpeed * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*Rest*/
					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						highNibble = (command[0] & 15);
						curDelay += (highNibble + 1) * chanSpeed * 5;
						seqPos++;
					}

					/*Set octave*/
					else if (command[0] >= 0xD0 && command[0] < 0xD8)
					{
						transpose = 0;
						highNibble = (command[0] & 15);
						switch (highNibble)
						{
						case 0x00:
							octave = 6;
							break;
						case 0x01:
							octave = 5;
							break;
						case 0x02:
							octave = 4;
							break;
						case 0x03:
							octave = 3;
							break;
						case 0x04:
							octave = 2;
							break;
						case 0x05:
							octave = 1;
							break;
						case 0x06:
							octave = 0;
							break;
						case 0x07:
							octave = 0;
							break;
						}
						seqPos++;
					}

					/*Set octave + transpose +1*/
					else if (command[0] >= 0xD8 && command[0] < 0xE0)
					{
						transpose = 1;
						highNibble = (command[0] & 15);
						switch (highNibble)
						{
						case 0x08:
							octave = 6;
							break;
						case 0x09:
							octave = 5;
							break;
						case 0x0A:
							octave = 4;
							break;
						case 0x0B:
							octave = 3;
							break;
						case 0x0C:
							octave = 2;
							break;
						case 0x0D:
							octave = 1;
							break;
						case 0x0E:
							octave = 0;
							break;
						case 0x0F:
							octave = 0;
							break;
						}
						seqPos++;
					}

					/*Noise effects?*/
					else if (command[0] >= 0xE0 && command[0] < 0xE6)
					{
						if (format != 4)
						{
							seqPos += 2;
						}
						else if (format == 4)
						{
							seqPos++;
						}

					}

					/*Set volume*/
					else if (command[0] >= 0xE6 && command[0] < 0xE9)
					{
						if (format == 3)
						{
							seqPos += 2;
						}
						else
						{
							if (command[0] == 0xE8 && format == 4)
							{
								seqPos++;
							}
							seqPos++;
						}
					}

					/*Set up waveform*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Set echo*/
					else if (command[0] == 0xEA)
					{
						seqPos += 2;
					}

					/*Set envelope/duty*/
					else if (command[0] == 0xEB)
					{
						seqPos += 2;
					}

					/*Turn off "glide"*/
					else if (command[0] == 0xEC)
					{
						seqPos++;
					}

					/*Set volume (v2)*/
					else if (command[0] >= 0xED && command[0] < 0xF0)
					{
						seqPos++;
					}

					/*Repeat command (v1)*/
					else if (command[0] == 0xF0)
					{
						repeat1Pos = seqPos + 2;
						repeat1 = command[1];
						seqPos += 2;
					}

					/*End of repeat (v1)*/
					else if (command[0] == 0xF1)
					{
						if (repeat1 > 1)
						{
							seqPos = repeat1Pos;
							repeat1--;
						}
						else
						{
							seqPos++;
						}
					}

					/*Go to loop point*/
					else if (command[0] == 0xF2)
					{
						seqEnd = 1;
					}

					/*Repeat command (v2)*/
					else if (command[0] == 0xF3)
					{
						repeat2Pos = seqPos + 2;
						repeat2 = command[1];
						seqPos += 2;
					}

					/*End of repeat (v2)*/
					else if (command[0] == 0xF4)
					{
						if (repeat2 > 1)
						{
							seqPos = repeat2Pos;
							repeat2--;
						}
						else
						{
							seqPos++;
						}
					}

					/*Go to macro*/
					else if (command[0] == 0xF5)
					{
						macroPos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
						macroRet = seqPos + 3;
						seqPos = macroPos;
					}

					/*Return from macro*/
					else if (command[0] == 0xF6)
					{
						seqPos = macroRet;
					}

					/*Sweep up*/
					else if (command[0] == 0xF7)
					{
						seqPos += 2;
					}

					/*Sweep down*/
					else if (command[0] == 0xF8)
					{
						seqPos += 2;
					}

					/*Set attack rate values?*/
					else if (command[0] == 0xF9)
					{
						seqPos += 3;
					}

					/*Set duty/envelope*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Set envelope parameters*/
					else if (command[0] == 0xFB)
					{
						seqPos += 5;
					}

					/*Turn on "glide"*/
					else if (command[0] == 0xFC)
					{
						seqPos++;
					}

					/*Set channel speed*/
					else if (command[0] == 0xFD)
					{
						chanSpeed = command[1];
						seqPos += 2;
					}

					/*Set noise effect?*/
					else if (command[0] == 0xFE)
					{
						seqPos += 2;
					}

					/*End of channel*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}
				else if (format == 2)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];
					command[5] = exRomData[seqPos + 5];
					command[6] = exRomData[seqPos + 6];
					command[7] = exRomData[seqPos + 7];

					/*Play note*/
					if (command[0] < 0x80)
					{
						if (curTrack != 3)
						{
							curNote = command[0] + 12;
						}
						else if (curTrack == 3)
						{
							curNote = command[0] + 48;
						}

						curNoteLen = (command[1] + 1) * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos += 2;
					}

					/*Rest*/
					else if (command[0] == 0x80)
					{
						curDelay += (command[1] + 1) * 5;
						seqPos += 2;
					}

					/*Play note (again?)*/
					else if (command[0] > 0x80 && command[0] < 0xD0)
					{
						curNote = command[0] - 0x81 + 12;
						curNoteLen = (command[1] + 1) * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos += 2;
					}

					/*Set volume/attack rate*/
					else if (command[0] >= 0xD0 && command[0] < 0xE0)
					{
						seqPos++;
					}

					/*Stereo flag 1?*/
					else if (command[0] == 0xE0)
					{
						seqPos += 2;
					}

					/*Stereo flag 2?*/
					else if (command[0] == 0xE1)
					{
						seqPos += 2;
					}

					/*Set waveform*/
					else if (command[0] == 0xE3)
					{
						seqPos += 2;
					}

					/*Set duty*/
					else if (command[0] >= 0xE4 && command[0] < 0xE8)
					{
						seqPos++;
					}

					/*Transpose*/
					else if (command[0] == 0xE8)
					{
						transpose = (signed char)command[1];
						seqPos += 2;
					}

					/*Set echo*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Set volume?*/
					else if (command[0] >= 0xEA && command[0] < 0xF0)
					{
						seqPos += 2;
					}

					/*Repeat command (v1)*/
					else if (command[0] == 0xF0)
					{
						repeat1Pos = seqPos + 2;
						repeat1 = command[1];
						seqPos += 2;
					}

					/*End of repeat (v1)*/
					else if (command[0] == 0xF1)
					{
						if (repeat1 > 1)
						{
							seqPos = repeat1Pos;
							repeat1--;
						}
						else
						{
							seqPos++;
						}
					}

					/*Repeat command (v2)*/
					else if (command[0] == 0xF2)
					{
						repeat2Pos = seqPos + 2;
						repeat2 = command[1];
						seqPos += 2;
					}

					/*End of repeat (v2)*/
					else if (command[0] == 0xF3)
					{
						if (repeat2 > 1)
						{
							seqPos = repeat2Pos;
							repeat2--;
						}
						else
						{
							seqPos++;
						}
					}

					/*Go to loop point*/
					else if (command[0] == 0xF4)
					{
						seqEnd = 1;
					}

					/*Go to macro*/
					else if (command[0] == 0xF5)
					{
						macroPos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
						macroRet = seqPos + 3;
						seqPos = macroPos;
					}

					/*Return from macro*/
					else if (command[0] == 0xF6)
					{
						seqPos = macroRet;
					}

					/*Sweep?*/
					else if (command[0] == 0xF7)
					{
						seqPos += 2;
					}

					/*Set tuning*/
					else if (command[0] == 0xF8)
					{
						seqPos += 2;
					}

					/*Set attack rate values?*/
					else if (command[0] == 0xF9)
					{
						seqPos += 3;
					}

					/*Set envelope?*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Set envelope parameters*/
					else if (command[0] == 0xFB)
					{
						seqPos += 5;
					}

					/*Turn on "glide"*/
					else if (command[0] == 0xFC)
					{
						seqPos++;
					}

					/*Turn off "glide"*/
					else if (command[0] == 0xFD)
					{
						seqPos++;
					}

					/*Set noise effect?*/
					else if (command[0] == 0xFE)
					{
						seqPos += 2;
					}

					/*End of channel*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
					}

					/*Unknown command*/
					else
					{
						seqPos++;
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
		if (exRomData != NULL)
		{
			free(exRomData);
		}
		fclose(mid);
	}
}