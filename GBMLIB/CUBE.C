/*Cube (Hitoshi Sakimoto)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <direct.h>
#include "SHARED.H"
#include "CUBE.H"
#include "BGDEC.H"

#define bankSize 16384

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
long firstPtr;
int chanMask;
long bankAmt;
int foundTable;
int version;
int curInst;
int multiBanks;
int curBank;

char folderName[100];

const char MagicBytes[4] = { 0x19, 0x2A, 0x66, 0x6F };

unsigned char* romData;
unsigned char* compData;
unsigned char* songData;
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
void Cubesong2mid(int songNum, long ptr);
void copyDataCube(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd);

void CubeProc(int bank, char parameters[4][50])
{
	version = strtol(parameters[0], NULL, 16);

	if (version == 1 || version == 2)
	{
		foundTable = 0;
		bankAmt = bankSize;
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);

		/*Special case for Koguru Guruguru*/
		if (bank == 0x14 && ReadLE16(&romData[0x0114]) == 0x469F)
		{
			tablePtrLoc = 0x4114;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
		else
		{
			/*Try to search the bank for song table loader*/
			for (i = 0; i < bankSize; i++)
			{
				if ((!memcmp(&romData[i], MagicBytes, 4)) && foundTable != 1)
				{
					tablePtrLoc = bankAmt + i - 2;
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
			if (ReadLE16(&romData[i]) == 0x0000)
			{
				i += 2;
			}
			if (ReadLE16(&romData[i]) == 0x0000 && bank == 0x3D && version == 2)
			{
				i += 24;
			}
			if (ReadLE16(&romData[i]) == 0x0000 && version == 2)
			{
				while (ReadLE16(&romData[i]) == 0x0000)
				{
					i += 2;
				}
			}
			firstPtr = ReadLE16(&romData[i]);

			/*Exceptions for Pocket Pro Yakyuu*/
			if (bank == 0x40 && version == 2 && tableOffset == 0x703C)
			{
				while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
				{
					if (ReadLE16(&romData[i]) < firstPtr)
					{
						firstPtr = ReadLE16(&romData[i]);
					}
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Cubesong2mid(songNum, songPtr);
					i += 2;
					songNum++;
				}
			}
			else if (bank == 0x11 && version == 2 && tableOffset == 0x6535)
			{
				while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
				{
					if (ReadLE16(&romData[i]) < firstPtr)
					{
						firstPtr = ReadLE16(&romData[i]);
					}
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Cubesong2mid(songNum, songPtr);
					i += 2;
					songNum++;
				}
			}
			else if (bank == 0x12 && version == 2 && tableOffset == 0x673E)
			{
				while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
				{
					if (ReadLE16(&romData[i]) < firstPtr)
					{
						firstPtr = ReadLE16(&romData[i]);
					}
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Cubesong2mid(songNum, songPtr);
					i += 2;
					songNum++;
				}
			}
			/*Otherwise...*/
			else
			{
				while ((ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000) && (i + bankAmt) < firstPtr)
				{
					if (ReadLE16(&romData[i]) < firstPtr)
					{
						firstPtr = ReadLE16(&romData[i]);
					}
					songPtr = ReadLE16(&romData[i]);
					printf("Song %i: 0x%04X\n", songNum, songPtr);
					Cubesong2mid(songNum, songPtr);
					i += 2;
					songNum++;
				}
			}

			free(romData);
		}
		else
		{
			printf("ERROR: Magic bytes not found!\n");
			exit(-1);
		}
	}

	/*Special case for Bubble Ghost - compressed data*/
	else if (version == 0)
	{
		bankAmt = 0xD600;

		romData = (unsigned char*)malloc(bankSize);
		compData = (unsigned char*)malloc(bankSize);
		fseek(rom, 0, SEEK_SET);
		fread(romData, 1, bankSize, rom);

		songData = (unsigned char*)malloc(bankSize);

		/*Copy the data and then decompress it*/
		copyDataCube(romData, compData, 0x12FB, 0x18E7);
		BGDecomp(compData, songData);
		free(compData);

		i = 2;
		songNum = 1;


		while (songNum <= 4)
		{
			songPtr = ReadLE16(&songData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0x0000)
			{
				Cubesong2mid(songNum, songPtr);
			}
			i += 2;
			songNum++;
		}
		free(songData);
		free(romData);

	}
}

/*Convert the song data to MIDI*/
void Cubesong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int songPtrs[4];
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
	int octave = 0;
	int repeat1 = 0;
	int repeat1Pos = 0;
	int repeat2 = 0;
	int repeat2Pos = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	int macro1Times = 0;
	long macro2Pos = 1;
	long macro2Ret = 1;
	int macro2Times = 1;
	int inMacro = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	unsigned char command[6];
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
	int holdNote = 0;
	long startPos = 0;
	int defaultLen = 0;

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

		if (version == 1)
		{
			romPos = songPtr - bankAmt;
			/*Get the channel pointers*/
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				songPtrs[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}

			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				octave = 3;
				midPos += 8;
				midTrackBase = midPos;

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

				if (songPtrs[curTrack] >= bankAmt && songPtrs[curTrack] < 0x8000)
				{
					seqPos = songPtrs[curTrack] - bankAmt;
					seqEnd = 0;
					inMacro = 0;
					curDelay = 0;
					ctrlDelay = 0;
					defaultLen = 0;
					repeat1 = -1;
					repeat2 = -1;
					firstNote = 1;
					holdNote = 0;

					while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];
						command[3] = romData[seqPos + 3];
						command[4] = romData[seqPos + 4];
						command[5] = romData[seqPos + 5];

						if (command[0] >= 0xC0 && command[0] <= 0xCF)
						{
							highNibble = (command[0] & 15);

							/*Rest*/
							if (highNibble == 0x0D)
							{
								if (command[1] < 0xC0)
								{
									curNoteLen = command[1] * 5;
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									seqPos += 2;
								}
								else
								{
									curNoteLen = defaultLen * 5;
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									seqPos++;
								}

							}

							/*Play note*/
							else
							{
								curNote = highNibble + (octave * 12) + 23;
								if (curTrack == 2)
								{
									curNote -= 12;
								}

								if (command[1] < 0xC0)
								{
									curNoteLen = command[1] * 5;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									seqPos += 2;
								}
								else
								{
									curNoteLen = defaultLen * 5;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									seqPos++;
								}

							}

						}

						/*Command D0 (instrument/SFX related?)*/
						else if (command[0] == 0xD0)
						{
							seqPos += 2;
						}

						/*Set octave*/
						else if (command[0] == 0xD1)
						{
							octave = command[1];
							seqPos += 2;
						}

						/*Set instrument*/
						else if (command[0] == 0xD2)
						{
							curInst = command[1];
							firstNote = 1;
							seqPos += 2;
						}

						/*(No function)*/
						else if (command[0] == 0xD3)
						{
							seqPos++;
						}

						/*Hold note?*/
						else if (command[0] == 0xD4)
						{
							seqPos++;
						}

						/*Set note size*/
						else if (command[0] == 0xD5)
						{
							seqPos += 2;
						}

						/*Octave up*/
						else if (command[0] == 0xD6)
						{
							octave++;
							seqPos++;
						}

						/*Octave down*/
						else if (command[0] == 0xD7)
						{
							octave--;
							seqPos++;
						}

						/*(No function)*/
						else if (command[0] == 0xD8)
						{
							seqPos++;
						}

						/*Set default note length*/
						else if (command[0] == 0xD9)
						{
							defaultLen = command[1];
							seqPos += 2;
						}

						/*Set volume?*/
						else if (command[0] == 0xDA)
						{
							seqPos += 2;
						}

						/*Repeat the following section*/
						else if (command[0] == 0xDB)
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

						/*End of repeat section*/
						else if (command[0] == 0xDC)
						{
							if (repeat2 == -1)
							{
								if (repeat1 > 1)
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

						/*Go to loop point*/
						else if (command[0] == 0xDD)
						{
							seqEnd = 1;
						}

						/*(No function)*/
						else if (command[0] >= 0xDE && command[0] <= 0xE3)
						{
							seqPos++;
						}

						/*Set waveform*/
						else if (command[0] == 0xE4)
						{
							seqPos += 2;
						}

						/*Set instrument parameters?*/
						else if (command[0] == 0xE5)
						{
							seqPos += 3;
						}

						/*Go to macro*/
						else if (command[0] == 0xE6)
						{
							if (inMacro == 0)
							{
								macro1Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
								macro1Ret = seqPos + 3;
								macro1Times = 0;
								seqPos = macro1Pos;
								inMacro = 1;
							}
							else
							{
								macro2Pos = ReadLE16(&romData[seqPos + 1]) - bankAmt;
								macro2Ret = seqPos + 3;
								macro2Times = 0;
								seqPos = macro2Pos;
								inMacro = 2;
							}

						}

						/*Return from macro*/
						else if (command[0] == 0xE7)
						{
							if (inMacro == 1)
							{
								if (macro1Times > 0)
								{
									seqPos = macro1Pos;
									macro1Times--;
								}
								else
								{
									seqPos = macro1Ret;
									macro1Times = -1;
									inMacro = 0;
								}
							}
							else if (inMacro == 2)
							{
								if (macro2Times > 0)
								{
									seqPos = macro2Pos;
									macro2Times--;
								}
								else
								{
									seqPos = macro2Ret;
									macro2Times = -1;
									inMacro = 1;
								}
							}

						}

						/*Go to macro position multiple times*/
						else if (command[0] == 0xE8)
						{
							if (inMacro == 0)
							{
								macro1Pos = ReadLE16(&romData[seqPos + 2]) - bankAmt;
								macro1Ret = seqPos + 4;
								macro1Times = command[1];
								seqPos = macro1Pos;
								inMacro = 1;
							}
							else
							{
								macro2Pos = ReadLE16(&romData[seqPos + 2]) - bankAmt;
								macro2Ret = seqPos + 4;
								macro2Times = command[1];
								seqPos = macro2Pos;
								inMacro = 2;
							}

						}

						/*(No function)*/
						else if (command[0] == 0xE9)
						{
							seqPos++;
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
			fclose(mid);
		}

		else if (version == 0)
		{
			romPos = songPtr - bankAmt;
			/*Get the channel pointers*/
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				songPtrs[curTrack] = ReadLE16(&songData[romPos]);
				romPos += 2;
			}

			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				octave = 3;
				midPos += 8;
				midTrackBase = midPos;

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

				if (songPtrs[curTrack] >= bankAmt)
				{
					seqPos = songPtrs[curTrack] - bankAmt;
					seqEnd = 0;
					inMacro = 0;
					curDelay = 0;
					ctrlDelay = 0;
					defaultLen = 0;
					repeat1 = -1;
					repeat2 = -1;
					firstNote = 1;
					holdNote = 0;

					while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
					{
						command[0] = songData[seqPos];
						command[1] = songData[seqPos + 1];
						command[2] = songData[seqPos + 2];
						command[3] = songData[seqPos + 3];
						command[4] = songData[seqPos + 4];
						command[5] = songData[seqPos + 5];

						if (command[0] >= 0xC0 && command[0] <= 0xCF)
						{
							highNibble = (command[0] & 15);

							/*Rest*/
							if (highNibble == 0x0D)
							{
								if (command[1] < 0xC0)
								{
									curNoteLen = command[1] * 5;
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									seqPos += 2;
								}
								else
								{
									curNoteLen = defaultLen * 5;
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									seqPos++;
								}

							}

							/*Play note*/
							else
							{
								curNote = highNibble + (octave * 12) + 23;
								if (curTrack == 2)
								{
									curNote -= 12;
								}

								if (command[1] < 0xC0)
								{
									curNoteLen = command[1] * 5;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									seqPos += 2;
								}
								else
								{
									curNoteLen = defaultLen * 5;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									seqPos++;
								}

							}

						}

						/*Command D0 (instrument/SFX related?)*/
						else if (command[0] == 0xD0)
						{
							seqPos += 2;
						}

						/*Set octave*/
						else if (command[0] == 0xD1)
						{
							octave = command[1];
							seqPos += 2;
						}

						/*Set instrument*/
						else if (command[0] == 0xD2)
						{
							curInst = command[1];
							firstNote = 1;
							seqPos += 2;
						}

						/*(No function)*/
						else if (command[0] == 0xD3)
						{
							seqPos++;
						}

						/*Hold note?*/
						else if (command[0] == 0xD4)
						{
							seqPos++;
						}

						/*Set note size*/
						else if (command[0] == 0xD5)
						{
							seqPos += 2;
						}

						/*Octave up*/
						else if (command[0] == 0xD6)
						{
							octave++;
							seqPos++;
						}

						/*Octave down*/
						else if (command[0] == 0xD7)
						{
							octave--;
							seqPos++;
						}

						/*(No function)*/
						else if (command[0] == 0xD8)
						{
							seqPos++;
						}

						/*Set default note length*/
						else if (command[0] == 0xD9)
						{
							defaultLen = command[1];
							seqPos += 2;
						}

						/*Set volume?*/
						else if (command[0] == 0xDA)
						{
							seqPos += 2;
						}

						/*Repeat the following section*/
						else if (command[0] == 0xDB)
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

						/*End of repeat section*/
						else if (command[0] == 0xDC)
						{
							if (repeat2 == -1)
							{
								if (repeat1 > 1)
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

						/*Go to loop point*/
						else if (command[0] == 0xDD)
						{
							seqEnd = 1;
						}

						/*(No function)*/
						else if (command[0] >= 0xDE && command[0] <= 0xE3)
						{
							seqPos++;
						}

						/*Set waveform*/
						else if (command[0] == 0xE4)
						{
							seqPos += 2;
						}

						/*Set instrument parameters?*/
						else if (command[0] == 0xE5)
						{
							seqPos += 3;
						}

						/*Go to macro*/
						else if (command[0] == 0xE6)
						{
							if (inMacro == 0)
							{
								macro1Pos = ReadLE16(&songData[seqPos + 1]) - bankAmt;
								macro1Ret = seqPos + 3;
								macro1Times = 0;
								seqPos = macro1Pos;
								inMacro = 1;
							}
							else
							{
								macro2Pos = ReadLE16(&songData[seqPos + 1]) - bankAmt;
								macro2Ret = seqPos + 3;
								macro2Times = 0;
								seqPos = macro2Pos;
								inMacro = 2;
							}

						}

						/*Return from macro*/
						else if (command[0] == 0xE7)
						{
							if (inMacro == 1)
							{
								if (macro1Times > 0)
								{
									seqPos = macro1Pos;
									macro1Times--;
								}
								else
								{
									seqPos = macro1Ret;
									macro1Times = -1;
									inMacro = 0;
								}
							}
							else if (inMacro == 2)
							{
								if (macro2Times > 0)
								{
									seqPos = macro2Pos;
									macro2Times--;
								}
								else
								{
									seqPos = macro2Ret;
									macro2Times = -1;
									inMacro = 1;
								}
							}

						}

						/*Go to macro position multiple times*/
						else if (command[0] == 0xE8)
						{
							if (inMacro == 0)
							{
								macro1Pos = ReadLE16(&songData[seqPos + 2]) - bankAmt;
								macro1Ret = seqPos + 4;
								macro1Times = command[1];
								seqPos = macro1Pos;
								inMacro = 1;
							}
							else
							{
								macro2Pos = ReadLE16(&songData[seqPos + 2]) - bankAmt;
								macro2Ret = seqPos + 4;
								macro2Times = command[1];
								seqPos = macro2Pos;
								inMacro = 2;
							}

						}

						/*(No function)*/
						else if (command[0] == 0xE9)
						{
							seqPos++;
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
			fclose(mid);
		}

		else if (version == 2)
		{
			romPos = songPtr - bankAmt;
			/*Get the channel pointers*/
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				songPtrs[curTrack] = ReadLE16(&romData[romPos]);
				romPos += 2;
			}

			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				octave = 3;
				midPos += 8;
				midTrackBase = midPos;

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

				if (songPtrs[curTrack] >= bankAmt && songPtrs[curTrack] < 0x8000)
				{
					seqPos = songPtrs[curTrack] - bankAmt;
					seqEnd = 0;
					inMacro = 0;
					curDelay = 0;
					ctrlDelay = 0;
					defaultLen = 0;
					repeat1 = -1;
					repeat2 = -1;
					firstNote = 1;
					holdNote = 0;

					while (seqEnd == 0 && midPos < 48000 && seqPos < 0x4000 && ctrlDelay < 110000)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];
						command[2] = romData[seqPos + 2];
						command[3] = romData[seqPos + 3];
						command[4] = romData[seqPos + 4];
						command[5] = romData[seqPos + 5];

						if (command[0] >= 0xC0 && command[0] <= 0xCF)
						{
							highNibble = (command[0] & 15);

							/*Rest*/
							if (highNibble == 0x0D)
							{
								if (command[1] < 0xC0)
								{
									curNoteLen = command[1] * 5;
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									seqPos += 2;
								}
								else
								{
									curNoteLen = defaultLen * 5;
									curDelay += curNoteLen;
									ctrlDelay += curNoteLen;
									seqPos++;
								}

							}

							/*Play note*/
							else
							{
								curNote = highNibble + (octave * 12) + 14;
								if (curTrack == 2 && octave > 1)
								{
									curNote -= 12;
								}

								if (command[1] < 0xC0)
								{
									curNoteLen = command[1] * 5;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									seqPos += 2;
								}
								else
								{
									curNoteLen = defaultLen * 5;
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									curDelay = 0;
									ctrlDelay += curNoteLen;
									seqPos++;
								}

							}

						}

						/*Command D0 (instrument/SFX related?)*/
						else if (command[0] == 0xD0)
						{
							seqPos += 2;
						}

						/*Set octave*/
						else if (command[0] == 0xD1)
						{
							octave = command[1];
							seqPos += 2;
						}

						/*Set instrument*/
						else if (command[0] == 0xD2)
						{
							curInst = command[1];
							firstNote = 1;
							seqPos += 2;
						}

						/*(No function)*/
						else if (command[0] == 0xD3)
						{
							seqPos++;
						}

						/*Set note size*/
						else if (command[0] == 0xD4)
						{
							seqPos += 2;
						}

						/*Octave up*/
						else if (command[0] == 0xD5)
						{
							octave++;
							seqPos++;
						}

						/*Octave down*/
						else if (command[0] == 0xD6)
						{
							octave--;
							seqPos++;
						}

						/*Set default note length*/
						else if (command[0] == 0xD7)
						{
							defaultLen = command[1];
							seqPos += 2;
						}

						/*Set pan?*/
						else if (command[0] == 0xD8)
						{
							seqPos += 2;
						}

						/*Set volume?*/
						else if (command[0] == 0xD9)
						{
							seqPos += 2;
						}

						/*Unknown command DA*/
						else if (command[0] == 0xDA)
						{
							seqPos++;
						}

						/*Go to loop point*/
						else if (command[0] == 0xDB)
						{
							seqEnd = 1;
						}

						/*Set waveform*/
						else if (command[0] == 0xDC)
						{
							seqPos += 2;
						}

						/*Set instrument parameters?*/
						else if (command[0] == 0xDD)
						{
							seqPos += 3;
						}

						/*Unknown commands*/
						else if (command[0] == 0xDE || command[0] == 0xDF || command[0] == 0xE0)
						{
							seqPos++;
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
			fclose(mid);
		}
	}
}

/*Copy data for Bubble Ghost*/
void copyDataCube(unsigned char* source, unsigned char* dest, long dataStart, long dataEnd)
{
	int k = dataStart;
	int l = 0;

	while (k <= dataEnd)
	{
		dest[l] = source[k];
		k++;
		l++;
	}
}


