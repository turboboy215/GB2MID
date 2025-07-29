/*Aicom*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "AICOM.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
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
int curInst;
long firstPtr;
int drvVers;
int fmtOverride;
int ptrOverride;
int songBank;
int multiBanks;
int curBank;

char folderName[100];

const char AicomMagicBytes[6] = { 0xFE, 0xFF, 0x28, 0xF2, 0xF1, 0x01 };
const char AicomMagicBytesEarly[6] = { 0x1E, 0xD6, 0x80, 0x28, 0x19, 0x21 };
const char AicomMagicBytesEarly2[9] = { 0xFE, 0x94, 0x30, 0x1E, 0xD6, 0x80, 0x28, 0x19, 0x21 };
const char AicomMagicBytesSK2[5] = { 0xEA, 0x22, 0xD2, 0xF1, 0x01 };

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Aicomsong2mid(int songNum, long ptr);

void AicomProc(int bank, char parameters[4][100])
{
	foundTable = 0;
	curInst = 0;
	firstPtr = 0;
	drvVers = 2;
	fmtOverride = 0;
	ptrOverride = 0;

	if (parameters[1][0] != 0)
	{
		tableOffset = strtol(parameters[1], NULL, 16);
		ptrOverride = 1;

		drvVers = strtol(parameters[0], NULL, 16);
		fmtOverride = 1;
		foundTable = 1;
	}

	else if (parameters[0][0] != 0)
	{
		drvVers = strtol(parameters[0], NULL, 16);
		fmtOverride = 1;
	}
	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	if (ptrOverride != 1)
	{
		/*Try to search the bank for song table loader - Common method*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], AicomMagicBytes, 6)) && foundTable != 1)
			{
				tablePtrLoc = i + 6;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (fmtOverride != 1)
				{
					drvVers = 2;
				}
			}
		}

		/*Early version*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], AicomMagicBytesEarly, 6)) && foundTable != 1)
			{
				tablePtrLoc = i + 6;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (fmtOverride != 1)
				{
					drvVers = 1;
				}
			}
		}

		/*Early version (alt - Painter Momo Pie)*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], AicomMagicBytesEarly2, 9)) && foundTable != 1)
			{
				tablePtrLoc = i + 9;
				;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (fmtOverride != 1)
				{
					drvVers = 4;
				}
			}
		}

		/*Late (banked) method - Survival Kids 2*/
		for (i = 0; i < (bankSize * 2); i++)
		{
			if ((!memcmp(&romData[i], AicomMagicBytesSK2, 5)) && foundTable != 1)
			{
				tablePtrLoc = i + 5;
				printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
				tableOffset = ReadLE16(&romData[tablePtrLoc]);
				printf("Song table starts at 0x%04x...\n", tableOffset);
				foundTable = 1;

				if (fmtOverride != 1)
				{
					drvVers = 3;
				}
			}
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;
		if (drvVers == 1 || drvVers == 4)
		{
			i += 2;
			if (bank == 1)
			{
				while (ReadLE16(&romData[i]) > 0x0800)
				{
					songPtr = ReadLE16(&romData[i]);
					songBank = bank;
					printf("Song %i: 0x%04X\n", songNum, songPtr);

					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
					Aicomsong2mid(songNum, songPtr);

					i += 2;
					songNum++;
				}
			}
			else
			{
				while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
				{
					songPtr = ReadLE16(&romData[i]);
					songBank = bank;
					printf("Song %i: 0x%04X\n", songNum, songPtr);

					fseek(rom, 0, SEEK_SET);
					exRomData = (unsigned char*)malloc(bankSize * 2);
					fread(exRomData, 1, bankSize, rom);
					fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
					fread(exRomData + bankSize, 1, bankSize, rom);
					Aicomsong2mid(songNum, songPtr);

					i += 2;
					songNum++;
				}
			}
		}
		else if (drvVers == 2)
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
			{
				songPtr = ReadLE16(&romData[i]);
				songBank = bank;
				printf("Song %i: 0x%04X\n", songNum, songPtr);

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				Aicomsong2mid(songNum, songPtr);

				i += 2;
				songNum++;
			}
		}
		else
		{
			while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
			{
				songPtr = ReadLE16(&romData[i]);
				songBank = romData[i + 2] + 1;
				printf("Song %i: 0x%04X (bank %01X)\n", songNum, songPtr, songBank);

				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				Aicomsong2mid(songNum, songPtr);

				i += 3;
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
		exit(1);
	}
}

/*Convert the song data to MIDI*/
void Aicomsong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
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
	int chanSpeed = 0;
	int repeat1 = 0;
	long repeat1Pos = 0;
	int repeat2 = 0;
	long repeat2Pos = 0;
	unsigned char command[4];
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
	int repeat = 0;
	long repeatStart;
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	int numTracks = 0;
	unsigned int noteVal = 0;
	int firstTrack = 0;
	int repPts[8][3];
	int repLevel = 0;
	int transposes[4];
	int pitchMode = 0;

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

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
		free(exRomData);
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

		romPos = ptr;

		/*Process the initial header data*/
		if (drvVers == 1 || drvVers == 4)
		{
			numTracks = exRomData[romPos];
			romPos++;
		}
		else
		{
			numTracks = exRomData[romPos];
			tempByte = exRomData[romPos + 2];

			switch (tempByte)
			{
			case 0x00:
				tempo = 150;
				break;
			case 0x01:
				tempo = 75;
				break;
			case 0x02:
				tempo = 100;
				break;
			case 0x03:
				tempo = 115;
				break;
			case 0x04:
				tempo = 120;
				break;
			case 0x05:
				tempo = 125;
				break;
			case 0x06:
				tempo = 130;
				break;
			case 0x07:
				tempo = 135;
				break;
			case 0x08:
				tempo = 140;
				break;
			case 0x09:
				tempo = 150;
				break;
			default:
				tempo = 150;
				break;
			}

			tempByte = exRomData[romPos + 1];

			if (tempByte < 1)
			{
				tempByte = 1;
			}

			tempo = tempo / tempByte;
			if (tempo < 2)
			{
				tempo = 150;
			}
			romPos += 3;
		}

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

		/*Fill in initial information*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			seqPtrs[curTrack] = 0x0000;
			transposes[curTrack] = 0;
		}

		/*Process the header information for each channel*/
		if (drvVers == 1)
		{
			for (k = 0; k < numTracks; k++)
			{
				curTrack = exRomData[romPos + 1];

				if (curTrack < 0)
				{
					curTrack = 0;
				}
				else if (curTrack > 3)
				{
					curTrack = 0;
				}
				seqPtrs[curTrack] = ReadLE16(&exRomData[romPos + 3]);
				transposes[curTrack] = (signed char)exRomData[romPos + 5];
				romPos += 8;
			}
		}
		else if (drvVers == 4)
		{
			for (k = 0; k < numTracks; k++)
			{
				curTrack = exRomData[romPos + 1];

				if (curTrack < 0)
				{
					curTrack = 0;
				}
				else if (curTrack > 3)
				{
					curTrack = 0;
				}
				seqPtrs[curTrack] = ReadLE16(&exRomData[romPos + 3]);
				transposes[curTrack] = (signed char)exRomData[romPos + 5];
				romPos += 10;
			}
		}
		else
		{
			for (k = 0; k < numTracks; k++)
			{
				curTrack = exRomData[romPos + 5] - 1;

				if (curTrack < 0)
				{
					curTrack = 0;
				}
				else if (curTrack > 3)
				{
					curTrack = 0;
				}

				seqPtrs[curTrack] = ReadLE16(&exRomData[romPos]);
				transposes[curTrack] = (signed char)exRomData[romPos + 2];
				romPos += 6;
			}
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			holdNote = 0;
			chanSpeed = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;

			for (k = 0; k < 8; k++)
			{
				repPts[k][0] = -1;
				repPts[k][1] = 0;
			}
			inMacro = 0;
			pitchMode = 0;

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

			if (seqPtrs[curTrack] != 0x0000)
			{
				seqEnd = 0;
				seqPos = seqPtrs[curTrack];
			}

			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{

				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];
				command[3] = exRomData[seqPos + 3];

				/*Set note length*/
				if (command[0] <= 0x7F)
				{
					if (songNum == 10 && curTrack == 0)
					{
						curTrack = 0;
					}
					chanSpeed = command[0];
					curDelay += (chanSpeed * 5);
					seqPos++;
				}

				/*Play note*/
				else if (command[0] >= 0x80 && command[0] <= 0xE0)
				{
					if (pitchMode != 1)
					{
						if (command[1] <= 0x7F)
						{
							chanSpeed = command[1];
							seqPos++;
						}

						if (drvVers == 4)
						{
							curNoteLen = chanSpeed * 10;
						}
						else
						{
							curNoteLen = chanSpeed * 5;
						}

						/*Rest*/
						if (command[0] == 0x80)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = command[0] - 0x81 + transposes[curTrack] + 24;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos++;
					}
					else
					{
						chanSpeed = command[2];
						if (drvVers == 4)
						{
							curNoteLen = chanSpeed * 10;
						}
						else
						{
							curNoteLen = chanSpeed * 5;
						}
						seqPos += 2;

						/*Rest*/
						if (command[0] == 0x80)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							curNote = command[0] - 0x81 + transposes[curTrack] + 24;
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
						}
						seqPos++;
					}


				}

				/*Go to macro*/
				else if (command[0] == 0xE1)
				{
					if (inMacro == 0)
					{
						macro1Pos = ReadLE16(&exRomData[seqPos + 1]);
						macro1Ret = seqPos + 3;
						seqPos = macro1Pos;
						inMacro = 1;
					}
					else if (inMacro == 1)
					{
						macro2Pos = ReadLE16(&exRomData[seqPos + 1]);
						macro2Ret = seqPos + 3;
						seqPos = macro2Pos;
						inMacro = 2;
					}
					else if (inMacro == 2)
					{
						macro3Pos = ReadLE16(&exRomData[seqPos + 1]);
						macro3Ret = seqPos + 3;
						seqPos = macro3Pos;
						inMacro = 1;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*Exit macro*/
				else if (command[0] == 0xE2)
				{
					if (inMacro == 1)
					{
						seqPos = macro1Ret;
						inMacro = 0;
					}
					else if (inMacro == 2)
					{
						seqPos = macro2Ret;
						inMacro = 1;
					}
					else
					{
						seqEnd = 1;
					}
				}

				/*End of channel*/
				else if (command[0] == 0xE3)
				{
					seqEnd = 1;
				}

				/*Repeat section*/
				else if (command[0] == 0xE4)
				{
					repLevel = command[1];
					if (repPts[repLevel][0] == -1)
					{
						repPts[repLevel][0] = command[2];
						repPts[repLevel][1] = ReadLE16(&exRomData[seqPos + 3]);
					}
					else if (repPts[repLevel][0] > 1)
					{
						repPts[repLevel][0]--;
						seqPos = repPts[repLevel][1];
					}
					else
					{
						repPts[repLevel][0] = -1;
						seqPos += 5;
					}
				}

				/*Jump to position*/
				else if (command[0] == 0xE5)
				{
					if (ReadLE16(&exRomData[seqPos + 1]) > seqPos)
					{
						seqPos = ReadLE16(&exRomData[seqPos + 1]);
					}
					else
					{
						if (ReadLE16(&exRomData[seqPos + 1]) < seqPtrs[curTrack])
						{
							seqPos = ReadLE16(&exRomData[seqPos + 1]);
						}
						else
						{
							seqEnd = 1;
						}

					}

				}

				/*Set envelope (v2)*/
				else if (command[0] == 0xE6)
				{
					seqPos += 2;
				}

				/*Set transpose*/
				else if (command[0] == 0xE7)
				{
					transposes[curTrack] += (signed char)command[1];
					seqPos += 2;
				}

				/*Set duty*/
				else if (command[0] == 0xE8)
				{
					seqPos += 2;
				}

				/*Unknown command E9*/
				else if (command[0] == 0xE9)
				{
					seqPos += 2;
				}

				/*Set waveform*/
				else if (command[0] == 0xEA)
				{
					seqPos += 2;
				}

				/*End of channel (v2)?*/
				else if (command[0] == 0xEB)
				{
					seqEnd = 1;
				}

				/*Turn on pitch bend mode*/
				else if (command[0] == 0xEC)
				{
					pitchMode = 1;
					seqPos++;

				}

				/*Turn off pitch bend mode*/
				else if (command[0] == 0xED)
				{
					pitchMode = 0;
					seqPos++;
				}

				/*Set delay*/
				else if (command[0] == 0xEE)
				{
					seqPos++;
				}

				/*Reset delay?*/
				else if (command[0] == 0xEF)
				{
					seqPos++;
				}

				/*Set vibrato*/
				else if (command[0] == 0xF0)
				{
					seqPos += 2;
				}

				/*Unknown command F1*/
				else if (command[0] == 0xF1)
				{
					seqPos += 2;
				}

				/*Unknown command F2*/
				else if (command[0] == 0xF2)
				{
					seqPos += 2;
				}

				/*Set envelope*/
				else if (command[0] == 0xF3)
				{
					seqPos += 2;
				}

				/*Unknown command F4*/
				else if (command[0] == 0xF4)
				{
					seqPos += 2;
				}

				/*Unknown command*/
				else
				{
					seqPos += 2;
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

		fclose(mid);
		free(midData);
		free(ctrlMidData);
		free(exRomData);
	}
}