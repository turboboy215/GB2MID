/*Game Freak*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "GAMEFRK.H"

#define bankSize 16384

FILE* rom, * mid;
int format;
long bank1;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int foundTable;
long firstPtr;
int lowNibble1;
int highNibble1;
int lowNibble2;
int highNibble2;
int totalChan;
int chanNum;
long startMus;
int activeChan[4];
int curTrack;
long masterBank;
long songLoc;
int curInst;
int curVol;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char GFMagicBytesA[3] = { 0xFF, 0xFF, 0xFF };
const char GFMagicBytesB[4] = { 0x73, 0x23, 0x72, 0x21 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void GFsong2mid(int songNum, long ptrs[4]);

void GFProc(int bank)
{
	bank1 = bank;
	format = 1;
	foundTable = 0;
	firstPtr = 0;
	lowNibble1 = 0;
	highNibble1 = 0;
	lowNibble2 = 0;
	highNibble2 = 0;
	totalChan = 0;
	chanNum = 0;
	startMus = 0;
	curTrack = 0;
	masterBank = 0;
	songLoc = 0;
	curInst = 0;
	curVol = 100;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((bank1 - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	/*Earlier version (Mario & Yoshi, Pokémon: Red/Blue/Yellow Version)*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], GFMagicBytesA, 3)) && foundTable != 1)
		{
			tableOffset = i + bankAmt;
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			format = 1;
			printf("Detected version: 1\n");
		}
	}

	/*Later version*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], GFMagicBytesB, 4)) && foundTable != 1 && romData[i + 6] == 0x19)
		{
			tablePtrLoc = bankAmt + i + 4;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			/*Multi-banked variant (Pokémon: Gold/Silver/Crystal Version)*/
			if (romData[i + 8] == 0x19)
			{
				printf("Detected version: 2 (multi-banked)\n");
				format = 3;
			}
			/*Single-banked variant (Pokémon Pinball)*/
			else
			{
				printf("Detected version: 2 (single-banked)\n");
				format = 2;
			}
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		if (format == 1)
		{
			i = tableOffset - bankAmt + 3;
			/*Skip the sound effects*/

			masterBank = bank;
			startMus = 0;
			chanNum = 5;
			firstPtr = ReadLE16(&romData[i + 1]);
			while (startMus == 0 && i < 0x4000)
			{
				lowNibble1 = (romData[i] >> 4);
				highNibble1 = (romData[i] & 15);
				switch (lowNibble1)
				{
				case 0x00:
					totalChan = 1;
					break;
				case 0x04:
					totalChan = 2;
					break;
				case 0x08:
					totalChan = 3;
					break;
				case 0x0C:
					totalChan = 4;
					break;
				}
				chanNum = highNibble1;

				if (chanNum > 3)
				{
					i += (3 * totalChan);
				}
				else
				{
					if (totalChan > 2)
					{
						startMus = i + bankAmt;
						printf("Music pointers start at offset: 0x%01X\n", startMus);
						break;
					}
					else
					{
						i += (3 * totalChan);
					}
				}
			}

			/*Get the pointers*/
			if (startMus > 0)
			{
				songNum = 1;

				lowNibble2 = (romData[i] >> 4);
				highNibble2 = (romData[i] & 15);
				while (i != (firstPtr - bankAmt) && i < bankSize && highNibble2 < 0x05)
				{
					lowNibble2 = (romData[i] >> 4);
					highNibble2 = (romData[i] & 15);
					switch (lowNibble2)
					{
					case 0x00:
						totalChan = 1;
						break;
					case 0x04:
						totalChan = 2;
						break;
					case 0x08:
						totalChan = 3;
						break;
					case 0x0C:
						totalChan = 4;
						break;
					}
					for (curTrack = 0; curTrack < 4; curTrack++)
					{
						seqPtrs[curTrack] = -1;
					}
					printf("Song %i number of channels: %i\n", songNum, totalChan);

					for (curTrack = 0; curTrack < totalChan; curTrack++)
					{
						lowNibble1 = (romData[i] >> 4);
						highNibble1 = (romData[i] & 15);
						if (highNibble1 < 4)
						{
							seqPtrs[curTrack] = ReadLE16(&romData[i + 1]);
							printf("Song %i channel %i: 0x%04X\n", songNum, (highNibble1 + 1), seqPtrs[curTrack]);
						}
						i += 3;
					}
					fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize);
					fread(exRomData, 1, bankSize, rom);

					GFsong2mid(songNum, seqPtrs);
					songNum++;
				}

			}

		}

		else if (format == 2)
		{
			/*Get the pointers*/
			songNum = 1;
			i = tableOffset - bankAmt;
			masterBank = bank1;
			while (ReadLE16(&romData[i]) > bankAmt && ReadLE16(&romData[i]) < bankSize * 2)
			{
				songLoc = ReadLE16(&romData[i]);
				printf("Song %i: 0x%04X\n", songNum, songLoc);
				j = songLoc - bankAmt;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					seqPtrs[curTrack] = -1;
				}
				lowNibble1 = (romData[j] >> 4);
				highNibble1 = (romData[j] & 15);
				switch (lowNibble1)
				{
				case 0x00:
					totalChan = 1;
					break;
				case 0x04:
					totalChan = 2;
					break;
				case 0x08:
					totalChan = 3;
					break;
				case 0x0C:
					totalChan = 4;
					break;
				}
				printf("Song %i number of channels: %i\n", songNum, totalChan);
				for (curTrack = 0; curTrack < totalChan; curTrack++)
				{
					lowNibble1 = (romData[j] >> 4);
					highNibble1 = (romData[j] & 15);
					if (highNibble1 < 4)
					{
						seqPtrs[curTrack] = ReadLE16(&romData[j + 1]);
						printf("Song %i channel %i: 0x%04X\n", songNum, (highNibble1 + 1), seqPtrs[curTrack]);
					}
					j += 3;
				}
				fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize);
				fread(exRomData, 1, bankSize, rom);
				GFsong2mid(songNum, seqPtrs);

				songNum++;
				i += 2;
			}
		}

		else if (format == 3)
		{
			/*Get the pointers*/
			songNum = 1;
			i = tableOffset - bankAmt;
			while (ReadLE16(&romData[i + 1]) >= bankAmt && ReadLE16(&romData[i + 1]) < bankSize * 2 && songNum < 104)
			{
				bank1 = romData[i] + 1;
				songLoc = ReadLE16(&romData[i + 1]);
				printf("Song %i: 0x%04X\n", songNum, songLoc);
				j = songLoc - bankAmt;

				for (curTrack = 0; curTrack < 4; curTrack++)
				{
					seqPtrs[curTrack] = -1;
				}

				fseek(rom, ((bank1 - 1) * bankSize), SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize);
				fread(exRomData, 1, bankSize, rom);
				lowNibble1 = (exRomData[j] >> 4);
				highNibble1 = (exRomData[j] & 15);
				switch (lowNibble1)
				{
				case 0x00:
					totalChan = 1;
					break;
				case 0x04:
					totalChan = 2;
					break;
				case 0x08:
					totalChan = 3;
					break;
				case 0x0C:
					totalChan = 4;
					break;
				}
				printf("Song %i number of channels: %i\n", songNum, totalChan);
				for (curTrack = 0; curTrack < totalChan; curTrack++)
				{
					lowNibble1 = (exRomData[j] >> 4);
					highNibble1 = (exRomData[j] & 15);
					if (highNibble1 < 4)
					{
						seqPtrs[curTrack] = ReadLE16(&exRomData[j + 1]);
						printf("Song %i channel %i: 0x%04X\n", songNum, (highNibble1 + 1), seqPtrs[curTrack]);
					}
					j += 3;
				}
				GFsong2mid(songNum, seqPtrs);

				songNum++;
				i += 3;
			}
		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(2);
	}
}

void GFsong2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int seqPos = 0;
	int curTrack = 0;
	int seqEnd = 0;
	int octave = 0;
	int tempo = 150;
	int curNote = 0;
	int curNoteLen = 0;
	long macroPos = 0;
	int macroTimes = 0;
	long macroRet = 0;
	long loopPos = 0;
	int loopTimes = 0;
	long otherAddr = 0;
	int perfPitch = 0;
	int transpose = 0;
	int transposeKey = 0;
	int transposeOct = 0;
	int lowNibble = 0;
	int highNibble = 0;
	long pitchOffset = 0;
	int tempoRel = 0;
	unsigned int command[4];
	int trackCnt = 4;
	int ticks = 120;
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
	int curSpeed = 0;
	int inMacro = 0;
	int inLoop = 0;
	long tempAddr = 0;

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
			octave = 8;
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
			transpose = 0;
			perfPitch = 0;
			inMacro = 0;
			curSpeed = 0;
			inLoop = 0;
			transposeKey = 0;
			transposeOct = 0;
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
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (seqPtrs[curTrack] >= bankAmt)
			{
				seqPos = ptrs[curTrack] - bankAmt;
			}

			else
			{
				seqEnd = 1;
			}


			if (format == 1)
			{
				while (seqEnd == 0 && ctrlDelay < 110000)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];

					/*Play note*/
					if (command[0] < 0xC0)
					{
						if (curTrack != 3)
						{

							lowNibble = (command[0] >> 4);
							highNibble = (command[0] & 15) + 1;
							curNote = lowNibble + (12 * octave);
							curNoteLen = highNibble * curSpeed * 2;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							seqPos++;
						}

						else if (curTrack == 3)
						{
							highNibble = (command[0] & 15) + 1;
							curNote = command[1] + 24;
							curNoteLen = highNibble * curSpeed * 2;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen * curSpeed;
							seqPos += 2;
						}

					}

					/*Rest*/
					else if (command[0] >= 0xC0 && command[0] < 0xD0)
					{
						highNibble = (command[0] & 15) + 1;
						curNoteLen = highNibble * 2 * curSpeed;
						curDelay += curNoteLen;
						seqPos++;
					}

					/*Note type/Drum speed*/
					else if (command[0] >= 0xD0 && command[0] < 0xE0)
					{
						if (curTrack != 3)
						{
							highNibble = (command[0] & 15);
							curSpeed = highNibble;
							seqPos += 2;
						}
						else if (curTrack == 3)
						{
							highNibble = (command[0] & 15);
							curSpeed = highNibble;
							seqPos++;
						}

					}

					/*Set octave*/
					else if (command[0] >= 0xE0 && command[0] < 0xE8)
					{
						highNibble = (command[0] & 15);
						switch (highNibble)
						{
						case 0:
							octave = 8;
							break;
						case 1:
							octave = 7;
							break;
						case 2:
							octave = 6;
							break;
						case 3:
							octave = 5;
							break;
						case 4:
							octave = 4;
							break;
						case 5:
							octave = 3;
							break;
						case 6:
							octave = 2;
							break;
						case 7:
							octave = 1;
							break;
						}
						seqPos++;
					}

					/*Toggle "perfect pitch"*/
					else if (command[0] == 0xE8)
					{
						if (perfPitch == 0)
						{
							perfPitch = 1;
						}
						else if (perfPitch == 1)
						{
							perfPitch = 0;
						}
						seqPos++;
					}

					/*(Invalid command) - E9*/
					else if (command[0] == 0xE9)
					{
						seqPos++;
					}

					/*Vibrato*/
					else if (command[0] == 0xEA)
					{
						seqPos += 3;
					}

					/*Pitch slide*/
					else if (command[0] == 0xEB)
					{
						seqPos += 3;
					}

					/*Set duty*/
					else if (command[0] == 0xEC)
					{
						seqPos += 2;
					}

					/*Set tempo*/
					else if (command[0] == 0xED)
					{
						tempo = ReadBE16(&exRomData[seqPos + 1]);
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						if (tempo >= 100)
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / ((362 - tempo) / 2));
						}
						else if (tempo < 100)
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / ((392 - tempo) / 2));
						}

						ctrlMidPos += 2;
						seqPos += 3;
					}

					/*Set pan*/
					else if (command[0] == 0xEE)
					{
						seqPos += 2;
					}

					/*Unknown command EF*/
					else if (command[0] == 0xEF)
					{
						seqPos++;
					}

					/*Set master volume*/
					else if (command[0] == 0xF0)
					{
						seqPos += 2;
					}

					/*Silent drum note?*/
					else if (command[0] == 0xF1)
					{
						seqPos++;
					}

					/*Interpret SFX data as music*/
					else if (command[0] == 0xF8)
					{
						seqPos++;
					}

					/*Duty cycle pattern*/
					else if (command[0] == 0xFC)
					{
						seqPos += 2;
					}

					/*Call macro*/
					else if (command[0] == 0xFD)
					{
						macroPos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
						macroRet = seqPos + 3;
						macroTimes = 1;
						inMacro = 1;
						seqPos = macroPos;
					}

					/*Call macro amount of times*/
					else if (command[0] == 0xFE)
					{
						if (inLoop == 0)
						{
							loopTimes = command[1];
							loopPos = ReadLE16(&exRomData[seqPos + 2]) - bankAmt;

							if (loopPos == 0x3DEF)
							{
								seqEnd = 1;
							}
							seqPos = loopPos;
							inLoop = 1;

							if (loopTimes == 0)
							{
								seqEnd = 1;
							}

						}
						else if (inLoop == 1)
						{
							if (loopTimes > 2)
							{
								seqPos = loopPos;
								loopTimes--;
							}
							else
							{
								seqPos += 4;
								inLoop = 0;
							}
						}

					}

					/*End of song or return macro*/
					else if (command[0] == 0xFF)
					{
						if (inMacro == 1)
						{
							seqPos = macroRet;
							inMacro = 0;
						}
						else
						{
							seqEnd = 1;
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
			else if (format == 2 || format == 3)
			{
				while (seqEnd == 0 && ctrlDelay < 110000 && seqPos < bankAmt)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];

					/*Rest*/
					if (command[0] < 0x10)
					{
						highNibble = (command[0] & 15) + 1;
						curNoteLen = highNibble * 2 * curSpeed;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x10 && command[0] < 0xD0)
					{

						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15) + 1;
						if (curTrack != 3)
						{
							curNote = lowNibble + (12 * octave) + (12 * transposeOct) + transposeKey + 11;
						}
						else if (curTrack == 3)
						{
							curNote = lowNibble + 24;
						}
						curNoteLen = highNibble * curSpeed * 2;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						ctrlDelay += curNoteLen;
						seqPos++;
					}

					/*Drum speed*/
					else if (command[0] >= 0xD0 && command[0] < 0xDF && curTrack == 3)
					{
						highNibble = (command[0] & 15);
						curSpeed = command[1];
						seqPos += 2;
					}

					/*Set octave*/
					else if (command[0] >= 0xD0 && command[0] < 0xD8)
					{
						highNibble = (command[0] & 15);
						switch (highNibble)
						{
						case 0:
							octave = 8;
							break;
						case 1:
							octave = 7;
							break;
						case 2:
							octave = 6;
							break;
						case 3:
							octave = 5;
							break;
						case 4:
							octave = 4;
							break;
						case 5:
							octave = 3;
							break;
						case 6:
							octave = 2;
							break;
						case 7:
							octave = 1;
							break;
						}
						seqPos++;
					}

					/*Note type*/
					else if (command[0] == 0xD8)
					{
						curSpeed = command[1];
						seqPos += 3;
					}

					/*Transpose*/
					else if (command[0] == 0xD9)
					{
						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);
						transposeOct = 0;
						transposeKey = highNibble;
						seqPos += 2;
					}

					/*Set tempo*/
					else if (command[0] == 0xDA)
					{
						tempo = ReadBE16(&exRomData[seqPos + 1]);
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						if (tempo >= 100 && tempo <= 250)
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / ((362 - tempo) / 2));
						}
						else if (tempo < 100)
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / ((392 - tempo) / 2));
						}
						else if (tempo > 250)
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / ((382 - tempo) / 2));
						}

						ctrlMidPos += 2;
						seqPos += 3;
					}

					/*Set duty*/
					else if (command[0] == 0xDB)
					{
						seqPos += 2;
					}

					/*Set volume envelope*/
					else if (command[0] == 0xDC)
					{
						seqPos += 2;
					}

					/*Pitch sweep*/
					else if (command[0] == 0xDD)
					{
						seqPos += 2;
					}

					/*Duty cycle pattern*/
					else if (command[0] == 0xDE)
					{
						seqPos += 2;
					}

					/*Interpret SFX data as music*/
					else if (command[0] == 0xDF)
					{
						seqPos++;
					}

					/*Pitch slide*/
					else if (command[0] == 0xE0)
					{
						seqPos += 3;
					}

					/*Vibrato*/
					else if (command[0] == 0xE1)
					{
						seqPos += 3;
					}

					/*Command E2*/
					else if (command[0] == 0xE2)
					{
						seqPos += 2;
					}

					/*Toggle noise command*/
					else if (command[0] == 0xE3)
					{
						seqPos += 2;
					}

					/*Force stereo panning*/
					else if (command[0] == 0xE4)
					{
						seqPos += 2;
					}

					/*Set master volume*/
					else if (command[0] == 0xE5)
					{
						seqPos += 2;
					}

					/*Set pitch offset*/
					else if (command[0] == 0xE6)
					{
						seqPos += 3;
					}

					/*Command E7*/
					else if (command[0] == 0xE7)
					{
						seqPos += 2;
					}

					/*Command E8*/
					else if (command[0] == 0xE8)
					{
						seqPos += 2;
					}

					/*Change tempo (relative)*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Restart channel at address*/
					else if (command[0] == 0xEA)
					{
						seqPos += 3;
					}

					/*New song address*/
					else if (command[0] == 0xEB)
					{
						seqPos += 3;
					}

					/*Turn SFX priority on*/
					else if (command[0] == 0xEC)
					{
						seqPos++;
					}

					/*Turn SFX priority off*/
					else if (command[0] == 0xED)
					{
						seqPos++;
					}

					/*Command EE*/
					else if (command[0] == 0xEE)
					{
						seqPos += 3;
					}

					/*Set pan*/
					else if (command[0] == 0xEF)
					{
						seqPos += 2;
					}

					/*Toggle noise*/
					else if (command[0] == 0xF0)
					{
						seqPos++;
					}

					/*Commands F1-F9*/
					else if (command[0] >= 0xF1 && command[0] <= 0xF9)
					{
						seqPos++;
					}

					/*Set condition command*/
					else if (command[0] == 0xFA)
					{
						seqPos += 2;
					}

					/*Jump to position on condition*/
					else if (command[0] == 0xFB)
					{
						seqPos += 4;
					}

					/*Jump to address*/
					else if (command[0] == 0xFC)
					{
						tempAddr = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
						seqPos = tempAddr;
					}

					/*Call macro amount of times*/
					else if (command[0] == 0xFD)
					{
						if (inLoop == 0)
						{
							loopTimes = command[1];
							loopPos = ReadLE16(&exRomData[seqPos + 2]) - bankAmt;

							/*Workaround for problematic track in Gold and Silver Versions*/
							if (loopPos == 0x3DEF && seqPos == 0x3DFE && songNum == 59)
							{
								seqEnd = 1;
							}

							/*The same track in Crystal Version*/
							if (loopPos == 0x3DFB && seqPos == 0x3E0A && songNum == 59)
							{
								seqEnd = 1;
							}

							seqPos = loopPos;
							inLoop = 1;

							if (loopTimes == 0)
							{
								seqEnd = 1;
							}

						}
						else if (inLoop == 1)
						{
							if (loopTimes > 2)
							{
								seqPos = loopPos;
								loopTimes--;
							}
							else
							{
								seqPos += 4;
								inLoop = 0;
							}
						}

					}

					/*Call macro*/
					else if (command[0] == 0xFE)
					{
						macroPos = ReadLE16(&exRomData[seqPos + 1]) - bankAmt;
						macroRet = seqPos + 3;
						macroTimes = 1;
						inMacro = 1;
						seqPos = macroPos;
					}

					/*End of song or return macro*/
					else if (command[0] == 0xFF)
					{
						if (inMacro == 1)
						{
							seqPos = macroRet;
							inMacro = 0;
						}
						else
						{
							seqEnd = 1;
						}
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