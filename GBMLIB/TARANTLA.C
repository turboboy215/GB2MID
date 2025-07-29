/*Tarantula Studios*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "TARANTLA.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
int i, j;
char outfile[1000000];
int songNum;
int numSongs;
long seqPtrs[4];
int songBanks[4];
long bankAmt;
int curInst;
int format;
int curVol;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

char* argv3;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Tarantulasong2mid(int songNum, long ptrs[4], int banks[4]);

void TarantulaProc(int bank, char parameters[4][100])
{
	curInst = 0;
	format = 2;
	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	if (parameters[0][0] != 0)
	{
		format = strtoul(parameters[0], NULL, 16);

		if (format != 1 && format != 2)
		{
			printf("ERROR: Invalid format specified!\n");
			exit(2);
		}
	}

	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	i = 0;

	if (bank == 0x10 && romData[i] == 0x62 && romData[i + 1] == 0xC9 && format == 1)
	{
		i = 0x20A;
	}
	else if (bank == 0x05 && romData[0x2400] == 0x03 && format == 1)
	{
		i = 0x2400;
	}
	else if (bank == 0x05 && romData[0x2E00] == 0x03 && format == 1)
	{
		i = 0x2E00;
	}
	else if (bank == 0x06 && romData[0x2E34] == 0x3B && romData[0x0000] == 0x02 && format == 2)
	{
		i = 0x2E34;
	}
	numSongs = romData[i];
	printf("Number of songs: %i\n", numSongs);

	songNum = 1;
	i++;

	if (format == 1)
	{
		for (j = 0; j < 4; j++)
		{
			songBanks[j] = bank - 1;
		}
	}

	while (songNum <= numSongs)
	{
		if (format == 1)
		{
			for (j = 0; j < 4; j++)
			{
				seqPtrs[j] = ReadLE16(&romData[i + (j * 2)]);
				printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
			}
			Tarantulasong2mid(songNum, seqPtrs, songBanks);
			i += 8;
			songNum++;
		}
		else
		{
			for (j = 0; j < 4; j++)
			{
				seqPtrs[j] = ReadLE16(&romData[i + (j * 3)]);
				printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), seqPtrs[j]);
				songBanks[j] = romData[i + (j * 3) + 2];
				printf("Song %i channel %i bank: %01X\n", songNum, (j + 1), songBanks[j]);
			}
			Tarantulasong2mid(songNum, seqPtrs, songBanks);
			i += 12;
			songNum++;
		}
	}

	free(romData);
}

void Tarantulasong2mid(int songNum, long ptrs[4], int banks[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 160;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int repeat = 0;
	long repeatStart;
	unsigned char command[5];
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
	long tempPos = 0;
	int skipTime = 0;
	int tempDelay = 0;
	int playingNote = 0;

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
			repeat = -1;
			skipTime = 0;
			playingNote = 0;
			curVol = 100;

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

			/*Copy the current channel's bank*/
			fseek(rom, (songBanks[curTrack] * bankSize), SEEK_SET);
			exRomData = (unsigned char*)malloc(bankSize);
			fread(exRomData, 1, bankSize, rom);

			if (seqPtrs[curTrack] != 0)
			{
				seqPos = seqPtrs[curTrack] - bankAmt;
				seqEnd = 0;
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && seqPos < 0x4000)
			{

				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];
				command[4] = exRomData[seqPos + 4];

				if (songNum == 6 && curTrack == 0)
				{
					songNum = 6;
				}

				if (skipTime == 0)
				{
					tempDelay = command[0] * 5;

					/*Event*/
					if (command[1] >= 0x80)
					{
						/*Panning?*/
						if (command[1] <= 0x8F)
						{
							seqPos += 2;
							curDelay += tempDelay;
						}

						/*Continue pitch bend?*/
						else if (command[1] == 0xE0)
						{
							curDelay += 5;
							seqPos += 4;
						}

						/*Volume*/
						else if ((command[1] >= 0x90 && command[1] <= 0x9F) || command[1] >= 0xA3 && command[1] <= 0xFE)
						{
							seqPos += 2;
							curDelay += tempDelay;

							if (command[1] >= 0x90 && command[1] <= 0x9F || (command[1] >= 0xD0 && command[1] < 0xDF))
							{
								highNibble = (command[1] & 15);

								switch (highNibble)
								{
								case 0x00:
									curVol = 5;
									break;
								case 0x01:
									curVol = 10;
									break;
								case 0x02:
									curVol = 20;
									break;
								case 0x03:
									curVol = 25;
									break;
								case 0x04:
									curVol = 30;
									break;
								case 0x05:
									curVol = 35;
									break;
								case 0x06:
									curVol = 40;
									break;
								case 0x07:
									curVol = 45;
									break;
								case 0x08:
									curVol = 50;
									break;
								case 0x09:
									curVol = 55;
									break;
								case 0x0A:
									curVol = 60;
									break;
								case 0x0B:
									curVol = 70;
									break;
								case 0x0C:
									curVol = 75;
									break;
								case 0x0D:
									curVol = 80;
									break;
								case 0x0E:
									curVol = 90;
									break;
								case 0x0F:
									curVol = 100;
									break;
								default:
									curVol = 100;
									break;
								}
							}

							if (command[1] >= 0xA3 && command[1] < 0xE0)
							{
								skipTime = 1;
							}
						}

						/*Pitch bend*/
						else if (command[1] == 0xA0)
						{
							tempDelay += command[4] * 5;
							curDelay += tempDelay;
							tempDelay = 0;
							skipTime = 1;
							seqPos += 5;
						}

						/*Repeat*/
						else if (command[1] == 0xA1)
						{
							repeatStart = seqPos + 3;
							repeat = command[2];
							seqPos += 3;
							curDelay += tempDelay;
						}

						/*End of repeat*/
						else if (command[1] == 0xA2)
						{
							if (repeat > 1)
							{
								seqPos = repeatStart;
								repeat--;
							}
							else
							{
								seqPos += 2;
								repeat = -1;
							}
							curDelay += tempDelay;
						}

						/*End of track*/
						else if (command[1] == 0xFF)
						{
							if (playingNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								midPos = tempPos;
								curDelay = 0;
								playingNote = 0;
							}
							seqEnd = 1;
						}
					}

					/*Play note, but use same time as previous*/
					else if (command[1] >= 0x40)
					{
						curDelay += tempDelay;
						if (command[1] != 0x40)
						{

							if (playingNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								midPos = tempPos;
								curDelay = 0;
								playingNote = 0;
							}
							curNote = (command[1] & 0x3F) + 23;
							tempPos = WriteNoteEventOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							playingNote = 1;
						}
						/*Note off*/
						else if (command[1] == 0x40)
						{
							tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							midPos = tempPos;
							curDelay = 0;
							playingNote = 0;
						}
						skipTime = 1;
						seqPos += 2;
					}

					/*Play note*/
					else if (command[1] < 0x40)
					{
						curDelay += tempDelay;
						if (command[1] != 0x00)
						{
							if (playingNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								midPos = tempPos;
								curDelay = 0;
								playingNote = 0;
							}
							curNote = command[1] + 23;
							tempPos = WriteNoteEventOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							playingNote = 1;
						}
						/*Note off*/
						else if (command[1] == 0x00)
						{
							tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							midPos = tempPos;
							curDelay = 0;
							playingNote = 0;
						}
						seqPos += 2;
					}
				}

				else if (skipTime == 1)
				{
					/*Event*/
					if (command[0] >= 0x80)
					{
						/*Panning?*/
						if (command[0] <= 0x8F)
						{
							seqPos++;
							curDelay += tempDelay;
							skipTime = 0;
						}

						/*Continue pitch bend?*/
						else if (command[0] == 0xE0)
						{
							curDelay += 5;
							seqPos += 3;
						}

						/*Volume*/
						else if ((command[0] >= 0x90 && command[0] <= 0x9F) || (command[0] >= 0xA3 && command[0] <= 0xFE))
						{
							seqPos++;
							curDelay += tempDelay;

							if ((command[0] >= 0x90 && command[0] <= 0x9F) || (command[0] >= 0xD0 && command[0] <= 0xDF))
							{
								highNibble = (command[0] & 15);

								switch (highNibble)
								{
								case 0x00:
									curVol = 5;
									break;
								case 0x01:
									curVol = 10;
									break;
								case 0x02:
									curVol = 20;
									break;
								case 0x03:
									curVol = 25;
									break;
								case 0x04:
									curVol = 30;
									break;
								case 0x05:
									curVol = 35;
									break;
								case 0x06:
									curVol = 40;
									break;
								case 0x07:
									curVol = 45;
									break;
								case 0x08:
									curVol = 50;
									break;
								case 0x09:
									curVol = 55;
									break;
								case 0x0A:
									curVol = 60;
									break;
								case 0x0B:
									curVol = 70;
									break;
								case 0x0C:
									curVol = 75;
									break;
								case 0x0D:
									curVol = 80;
									break;
								case 0x0E:
									curVol = 90;
									break;
								case 0x0F:
									curVol = 100;
									break;
								default:
									curVol = 100;
									break;
								}
							}

							if (command[0] < 0xA3)
							{
								skipTime = 0;
							}
						}

						/*Pitch bend*/
						else if (command[0] == 0xA0)
						{
							tempDelay += command[3] * 5;
							curDelay += tempDelay;
							tempDelay = 0;
							skipTime = 1;
							seqPos += 4;
						}

						/*Repeat*/
						else if (command[0] == 0xA1)
						{
							repeatStart = seqPos + 2;
							repeat = command[1];
							seqPos += 2;
							skipTime = 0;
							curDelay += tempDelay;
						}

						/*End of repeat*/
						else if (command[0] == 0xA2)
						{
							if (repeat > 1)
							{
								seqPos = repeatStart;
								repeat--;
							}
							else
							{
								seqPos++;
								repeat = -1;
							}
							skipTime = 0;
							curDelay += tempDelay;
						}

						/*End of track*/
						else if (command[0] == 0xFF)
						{
							if (playingNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								midPos = tempPos;
								curDelay = 0;
								playingNote = 0;
							}
							seqEnd = 1;
						}
					}

					/*Play note, but use same time as previous*/
					else if (command[0] >= 0x40)
					{
						curDelay += tempDelay;
						if (command[0] != 0x40)
						{
							if (playingNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								midPos = tempPos;
								curDelay = 0;
								playingNote = 0;
							}
							curNote = (command[0] & 0x3F) + 23;
							tempPos = WriteNoteEventOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							playingNote = 1;
						}
						/*Note off*/
						else if (command[0] == 0x40)
						{
							tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							midPos = tempPos;
							curDelay = 0;
							playingNote = 0;
						}
						seqPos++;
					}

					/*Play note*/
					else if (command[0] < 0x40)
					{
						curDelay += tempDelay;
						if (command[0] != 0x00)
						{

							if (playingNote == 1)
							{
								tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								midPos = tempPos;
								curDelay = 0;
								playingNote = 0;
							}
							curNote = command[0] + 23;
							tempPos = WriteNoteEventOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							playingNote = 1;
						}
						/*Note off*/
						else if (command[0] == 0x00)
						{
							tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							midPos = tempPos;
							curDelay = 0;
							playingNote = 0;
						}
						skipTime = 0;
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

			free(exRomData);
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