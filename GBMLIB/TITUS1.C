/*Titus (early driver)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "TITUS1.H"

#define bankSize 16384

FILE* rom, * mid;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long bankAmt;
int numSongs;
int foundTable;
int curInst;
long firstPtr;
long sfxPtr;
int songBank;
int isSFX;
int tempo;

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
void Tit1song2mid(int songNum, long ptrs[4], int tempo);

void Tit1Proc(int bank)
{
	foundTable = 0;
	curInst = 0;
	firstPtr = 0;
	sfxPtr = 0;
	if (bank != 1)
	{
		bankAmt = bankSize;
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}

	else
	{
		bankAmt = 0;
		fseek(rom, ((bank - 1) * bankSize * 2), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize * 2, rom);
	}

	/*Check for The Blues Brothers values*/
	if (ReadLE16(&romData[0x0000]) == 0x44C5)
	{
		tableOffset = 0x4000;
		foundTable = 1;
		numSongs = 6;
	}

	/*Otherwise, check for Titus the Fox values*/
	else if (ReadLE16(&romData[0x1040]) == 0x5656)
	{
		tableOffset = 0x5040;
		foundTable = 1;
		numSongs = 13;
	}

	/*Otherwise, check for Nick Faldo Golf values*/
	else if (ReadLE16(&romData[0x3A9B]) == 0x3488)
	{
		tableOffset = 0x3A99;
		foundTable = 1;
		numSongs = 1;
	}

	if (foundTable == 1)
	{
		printf("Song table: 0x%04X\n", tableOffset);
		songNum = 1;
		i = tableOffset - bankAmt;

		while (songNum <= numSongs)
		{
			for (j = 0; j < 4; j++)
			{
				songPtrs[j] = ReadLE16(&romData[i + (j * 2)]);
				printf("Song %i channel %i: 0x%04X\n", songNum, (j + 1), songPtrs[j]);
			}
			tempo = ReadLE16(&romData[i + 8]);
			if (tempo == 0xFF90)
			{
				tempo = 0x0E0;
			}
			printf("Tempo: %04X\n", tempo);
			Tit1song2mid(songNum, songPtrs, tempo);
			i += 10;
			songNum++;
		}
	}
	else
	{
		printf("ERROR: Song table not found!\n");
	}
	free(romData);
}

void Tit1song2mid(int songNum, long ptrs[4], int tempo)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	unsigned char mask = 0;
	int altVal = 0;
	long patPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	long macroPos = 0;
	long macroRet = 0;
	int repeatNum = 0;
	int repeatPos = 0;
	long jumpPos = 0;
	long jumpPosRet = 0;
	int chanSpeed = 0;
	int k = 0;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int octave = 0;
	int seqEnd = 0;
	int patEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int inMacro = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	unsigned char command[4];

	tempo = tempo * 0.6;

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

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNote = 1;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			octave = 3;
			midPos += 8;
			midTrackBase = midPos;
			if (ptrs[curTrack] != 0)
			{
				patPos = ptrs[curTrack] - bankAmt;
				patEnd = 0;
				seqEnd = 0;
				curDelay = 0;
				chanSpeed = 1;
				repeatNum = 0;
				inMacro = 0;

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

				while (patEnd == 0 && midPos < 48000)
				{
					command[0] = romData[patPos];
					command[1] = romData[patPos + 1];
					command[2] = romData[patPos + 2];
					command[3] = romData[patPos + 3];

					if (command[0] < 0x80)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);
						octave = lowNibble;

						/*Rest*/
						if (highNibble == 0x0C)
						{
							curDelay += chanSpeed * 5;
						}
						else
						{
							if (curTrack != 3)
							{
								curNote = (octave * 12) + 24 + highNibble;
							}
							else if (curTrack == 3)
							{
								if (octave == 0)
								{
									curNote = 36;
								}
								else if (octave == 1)
								{
									curNote = 38;
								}
								else if (octave == 2)
								{
									curNote = 36;
								}
								else
								{
									curNote = (octave * 12) + 24 + highNibble;
								}
							}

							curNoteLen = chanSpeed * 5;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						ctrlDelay += chanSpeed * 5;
						patPos++;
					}

					/*Set speed*/
					else if (command[0] >= 0x80 && command[0] <= 0x8F)
					{
						mask = command[0] & 7;

						if (mask != 7)
						{
							chanSpeed = (0x60 >> mask) | ((0x60 & mask) << 7);
						}
						else if (mask == 7)
						{
							chanSpeed = altVal;
						}


						patPos++;
					}

					/*Channel mask?*/
					else if (command[0] > 0x8F && command[0] < 0xB0)
					{
						patPos++;
					}

					/*Set "alternate length"?*/
					else if (command[0] >= 0xB0 && command[0] < 0xC0)
					{
						mask = command[0] & 7;

						if (songNum == 3 && curTrack == 1)
						{
							mask = command[0] & 7;
						}
						altVal = (0x40 >> mask) | ((0x40 & mask) << 7);
						patPos++;
					}

					/*Change instrument?*/
					else if (command[0] >= 0xC0 && command[0] < 0xF0)
					{
						curInst = command[0] - 0xC0;
						firstNote = 1;
						patPos++;
					}

					/*Go to macro*/
					else if (command[0] == 0xF0)
					{
						macroPos = ReadLE16(&romData[patPos + 1]) - bankAmt;
						macroRet = patPos + 3;
						inMacro = 1;
						patPos = macroPos;
					}

					/*Repeat macro*/
					else if (command[0] == 0xF1)
					{
						repeatNum = command[1];
						patPos += 2;
					}

					/*End of repeat/jump to position*/
					else if (command[0] == 0xF2)
					{
						repeatPos = ReadLE16(&romData[patPos + 1]) - bankAmt;

						if (repeatNum > 1)
						{
							patPos = repeatPos;
							repeatNum--;
						}
						else
						{
							patPos += 3;
							repeatNum = 0;
						}
					}

					/*Exit macro*/
					else if (command[0] == 0xF3)
					{
						if (inMacro == 0)
						{
							patPos++;
						}
						else
						{
							patPos = macroRet;
							inMacro = 0;
						}
					}

					/*Go to loop point*/
					else if (command[0] == 0xF4)
					{
						patEnd = 1;
					}

					/*Change instrument?*/
					else if (command[0] >= 0xF5 && command[0] <= 0xFE)
					{
						patPos++;
					}

					/*End of sequence*/
					else if (command[0] == 0xFF)
					{
						patEnd = 1;
					}
				}

				/*End of track*/
				WriteBE32(&midData[midPos], 0xFF2F00);
				midPos += 4;

				/*Calculate MIDI channel size*/
				trackSize = midPos - midTrackBase;
				WriteBE16(&midData[midTrackBase - 2], trackSize);

			}
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
		free(ctrlMidData);
		free(midData);
		fclose(mid);
	}
}