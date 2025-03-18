/*Neverland (Lufia: The Legend Returns)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "LUFIA.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtr;
int songBank;
int chanMask;
long bankAmt;
int curInst;
unsigned char LufiaNoteLens[4][7];

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
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Lufiasong2mid(int songNum, long songPtr, int songBank);

void LufiaProc(int bank)
{
	bank = 0x21;
	bankAmt = bankSize;

	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	tableOffset = 0x6500;
	printf("Song table: 0x%04X\n", tableOffset);

	songNum = 1;
	i = tableOffset - bankAmt;

	while (songNum <= 68)
	{
		songPtr = ReadLE16(&romData[i]);
		songBank = romData[i + 2] + 1;
		printf("Song %i: offset: %04X, bank %01X\n", songNum, songPtr, songBank);
		Lufiasong2mid(songNum, songPtr, songBank);
		i += 3;
		songNum++;
	}

	free(romData);
}

void Lufiasong2mid(int songNum, long songPtr, int songBank)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long songPtrs[4];
	unsigned char mask = 0;
	unsigned char mask1 = 0;
	unsigned char mask2 = 0;
	long romPos = 0;
	long seqPos = 0;
	int numChan = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	long jumpPos1 = 0;
	long jumpPos1Ret = 0;
	long jumpPos2 = 0;
	long jumpPos2Ret = 0;
	int getBytes1 = -1;
	int getBytes2 = -1;
	int noteCheck = 0;
	unsigned char command[6];
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
	midPos = 0;
	ctrlMidPos = 0;

	int notePlay = 0;

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
		curInst = 0;
		/*Copy data from the song's bank*/
		fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize);
		fread(exRomData, 1, bankSize, rom);

		/*Get information from the song header*/
		romPos = songPtr + 4 - bankAmt;
		mask1 = exRomData[romPos];
		mask2 = exRomData[romPos + 1];
		romPos += 2;
		numChan = ReadLE16(&exRomData[romPos]);
		romPos += 2;

		/*Get the note lengths*/
		for (curTrack == 0; curTrack < 4; curTrack++)
		{
			if (curTrack == 0)
			{
				romPos = songPtr + 0x10 + 0x11 - bankAmt;
			}
			else if (curTrack == 1)
			{
				romPos = songPtr + 0x10 + 0x22 - bankAmt;
			}
			else if (curTrack == 2)
			{
				romPos = songPtr + 0x10 - bankAmt;
			}
			else
			{
				romPos = songPtr + 0x10 + (0x11 * curTrack) - bankAmt;
			}

			for (k = 0; k < 7; k++)
			{
				LufiaNoteLens[curTrack][k] = exRomData[romPos + k];
			}
		}

		romPos = songPtr + 8 - bankAmt;

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
			if (curTrack == 0)
			{
				songPtrs[curTrack] = ReadLE16(&exRomData[romPos] + 2);
			}
			else if (curTrack == 1)
			{
				songPtrs[curTrack] = ReadLE16(&exRomData[romPos] + 4);
			}
			else if (curTrack == 2)
			{
				songPtrs[curTrack] = ReadLE16(&exRomData[romPos]);
			}
			else
			{
				songPtrs[curTrack] = ReadLE16(&exRomData[romPos] + (2 * curTrack));
			}

		}
		romPos += 8;

		for (curTrack = 0; curTrack < 4; curTrack++)
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
			getBytes1 = -1;
			getBytes2 = -1;
			jumpPos1 = -1;
			jumpPos1Ret = -1;
			jumpPos2 = -1;
			jumpPos2Ret = -1;
			notePlay = 0;
			curDelay = 0;
			ctrlDelay = 0;

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

			if (songNum == 30 && curTrack == 1)
			{
				songNum = 30;
			}
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (songPtrs[curTrack] != 0xFFFF)
			{
				seqPos = songPtrs[curTrack] + songPtr - bankAmt;
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{

					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];
					command[5] = exRomData[seqPos + 5];

					if (getBytes1 == 0)
					{
						seqPos = jumpPos1Ret;
						jumpPos1 = -1;
						jumpPos1Ret = -1;
						getBytes1 = -1;
						command[0] = exRomData[seqPos];
						command[1] = exRomData[seqPos + 1];
						command[2] = exRomData[seqPos + 2];
						command[3] = exRomData[seqPos + 3];
						command[4] = exRomData[seqPos + 4];
						command[5] = exRomData[seqPos + 5];
					}

					if (getBytes2 == 0)
					{
						seqPos = jumpPos2Ret;
						jumpPos2 = -1;
						jumpPos2Ret = -1;
						getBytes2 = -1;
						command[0] = exRomData[seqPos];
						command[1] = exRomData[seqPos + 1];
						command[2] = exRomData[seqPos + 2];
						command[3] = exRomData[seqPos + 3];
						command[4] = exRomData[seqPos + 4];
						command[5] = exRomData[seqPos + 5];
					}

					/*Play note*/
					if (command[0] < 0x80)
					{
						curNote = command[0];
						if (curNote >= 12 && curTrack != 3)
						{
							curNote -= 12;
						}

						if (seqPos == 0x169B)
						{
							seqPos = 0x169B;
						}

						seqPos++;
						noteCheck = 0;
						notePlay = 1;

						/*Test 1*/
						mask = command[1] & 0xE0;
						/*Invert the two nibbles (SWAP instruction)*/
						mask = ((mask & 0x0F) << 4 | (mask & 0xF0) >> 4);
						mask = mask / 2;

						if (mask >= 7)
						{
							noteCheck = 1;
						}

						if (command[1] == 0xFF)
						{
							seqPos++;
						}

						/*Test 2*/
						mask = command[1] & 0x1C;
						mask = mask / 2;
						mask = mask / 2;

						if (mask >= 7 && noteCheck == 0)
						{
							noteCheck = 2;
						}

						else if (mask >= 7 && noteCheck == 1)
						{
							noteCheck = 3;
						}

						if (noteCheck == 0)
						{
							/*Test 1*/
							mask = command[1] & 0xE0;
							/*Invert the two nibbles (SWAP instruction)*/
							mask = ((mask & 0x0F) << 4 | (mask & 0xF0) >> 4);
							mask = mask / 2;

							curNoteLen = LufiaNoteLens[curTrack][mask] * 10;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							notePlay = 0;
							ctrlDelay += curNoteLen;

							/*Additional test*/
							mask = command[1];

							if (mask == 0x27 && songNum == 30)
							{
								songNum = 30;
							}
							mask = command[1] & 0x03;
							if (mask < 3)
							{
								seqPos++;
							}
							else
							{
								seqPos += 2;
							}

						}

						else if (noteCheck == 1)
						{
							curNoteLen = command[2] * 10;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							notePlay = 0;
							ctrlDelay += curNoteLen;
							seqPos += 2;

							/*Additional test*/
							mask = command[1] & 0x03;
							if (mask < 3)
							{
								seqPos += 0;
							}
							else
							{
								seqPos++;
							}
						}

						else if (noteCheck == 2)
						{
							/*Test 1*/
							mask = command[1] & 0xE0;
							/*Invert the two nibbles (SWAP instruction)*/
							mask = ((mask & 0x0F) << 4 | (mask & 0xF0) >> 4);
							mask = mask / 2;

							curNoteLen = LufiaNoteLens[curTrack][mask] * 10;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							notePlay = 0;
							ctrlDelay += curNoteLen;

							/*Additional test*/
							mask = command[1] & 0x03;
							if (mask < 3)
							{
								seqPos += 2;
							}
							else
							{
								seqPos += 3;
							}

						}

						else if (noteCheck == 3)
						{
							curNoteLen = command[2] * 10;

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							notePlay = 0;
							ctrlDelay += curNoteLen;
							seqPos += 3;
						}
						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}

					}

					/*Delay*/
					else if (command[0] >= 0x80 && command[0] <= 0xEF)
					{
						curDelay += ((command[0] - 0x80) * 10);
						ctrlDelay += ((command[0] - 0x80) * 10);

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
						seqPos++;
					}

					/*Set volume?*/
					else if (command[0] == 0xF0)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Set envelope?*/
					else if (command[0] == 0xF1)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Set vibrato*/
					else if (command[0] == 0xF2)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Set instrument parameters?*/
					else if (command[0] == 0xF3)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Set waveform*/
					else if (command[0] == 0xF4)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Delay and change frequency*/
					else if (command[0] == 0xF5)
					{
						curDelay += (command[1] * 10);
						ctrlDelay += (command[1] * 10);
						seqPos += 3;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Set sweep*/
					else if (command[0] == 0xF6)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Set noise type*/
					else if (command[0] == 0xF7)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Delay*/
					else if (command[0] == 0xF8)
					{
						curDelay += (command[1] * 10);
						ctrlDelay += (command[1] * 10);
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Go back to position*/
					else if (command[0] == 0xF9)
					{
						mask = command[1] & 0xE0;
						/*Invert the two nibbles (SWAP instruction)*/
						mask = ((mask & 0x0F) << 4 | (mask & 0xF0) >> 4);
						mask = mask / 2;
						mask += 1;

						if (jumpPos1 == -1)
						{
							getBytes1 = mask;

							mask = command[1] & 0x1F;
							jumpPos1 = seqPos - mask;
							jumpPos1Ret = seqPos += 2;
							seqPos = jumpPos1;
						}
						else
						{
							getBytes2 = mask;

							mask = command[1] & 0x1F;
							jumpPos2 = seqPos - mask;
							jumpPos2Ret = seqPos += 2;
							seqPos = jumpPos2;
						}

					}

					/*Read bytes from relative position*/
					else if (command[0] == 0xFA)
					{
						mask = command[2] & 0xFC;
						mask = mask / 2;
						mask = mask / 2;
						mask += 1;

						if (jumpPos1 == -1)
						{
							jumpPos1 = seqPos - (command[1]);
							jumpPos1Ret = seqPos + 3;
							getBytes1 = mask;
							seqPos = jumpPos1;
						}
						else
						{
							jumpPos2 = seqPos - (command[1]);
							jumpPos2Ret = seqPos + 3;
							getBytes2 = mask;
							seqPos = jumpPos2;
						}


					}

					/*Song loop point*/
					else if (command[0] == 0xFB)
					{
						seqPos++;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Manual note length/size*/
					else if (command[0] == 0xFC)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Set duty*/
					else if (command[0] == 0xFD)
					{
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*Manual note length and volume/envelope(?)*/
					else if (command[0] == 0xFE)
					{
						curDelay += (command[1] * 10);
						ctrlDelay += (command[1] * 10);
						seqPos += 2;

						if (getBytes2 > 0)
						{
							getBytes2--;
						}
						else if (getBytes1 > 0)
						{
							getBytes1--;
						}
					}

					/*End of channel*/
					else if (command[0] == 0xFF)
					{
						seqEnd = 1;
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
		free(exRomData);
		fclose(mid);
	}
}