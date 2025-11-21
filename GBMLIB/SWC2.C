/*Software Creations 2 (by Paul Tonge?)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "SWC2.H"

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
long firstPtr;
int stopCvt;
long bankAmt;
int curInst;
int drvVers;
int songTempo;
int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
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
void SWC2song2mid(int songNum, long ptrs[4]);

void SWC2Proc(int bank)
{
	curInst = 0;
	stopCvt = 0;

	if (bank < 2)
	{
		bank = 2;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	tableOffset = ReadLE16(&romData[0x5A4C]);
	if (tableOffset == 0xF973)
	{
		tableOffset = 0x5A5F;
	}
	else if (tableOffset == 0xF807)
	{
		tableOffset = 0x5A8B;
	}
	printf("Song table: 0x%04X\n", tableOffset);
	i = tableOffset;
	songNum = 1;
	firstPtr = ReadLE16(&romData[i + 1]);

	while (stopCvt == 0)
	{
		songTempo = romData[i];
		for (j = 0; j < 4; j++)
		{
			seqPtrs[j] = ReadLE16(&romData[i + (j * 2) + 1]);
			if (seqPtrs[j] >= 0x4000 && seqPtrs[j] < 0x8000)
			{
				stopCvt = 0;
			}
			else
			{
				stopCvt = 1;
				break;
			}

		}
		if (stopCvt == 0)
		{
			printf("Song %i tempo: %02X\n", songNum, songTempo);
			printf("Song %i channel 1: 0x%04X\n", songNum, seqPtrs[0]);
			printf("Song %i channel 2: 0x%04X\n", songNum, seqPtrs[1]);
			printf("Song %i channel 3: 0x%04X\n", songNum, seqPtrs[2]);
			printf("Song %i channel 4: 0x%04X\n", songNum, seqPtrs[3]);
			SWC2song2mid(songNum, seqPtrs);
			i += 9;
			songNum++;
		}
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void SWC2song2mid(int songNum, long ptrs[4])
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

	for (repeatNum = 0; repeatNum < 16; repeatNum++)
	{
		repeats[repeatNum][0] = -1;
		repeats[repeatNum][1] = 0;
	}

	tempo = songTempo * 2;

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

			if (seqPtrs[curTrack] < 0x8000 && seqPtrs[curTrack] >= 0x4000)
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

				/*Play note*/
				if (command[0] <= 0x7F)
				{
					if (command[0] == 0x60)
					{
						if (autoLen == 0)
						{
							if (command[1] >= 0x80)
							{
								curDelay += ((ReadBE16(&romData[seqPos + 1]) - 0x8000) * 5);
								ctrlDelay += ((ReadBE16(&romData[seqPos + 1]) - 0x8000) * 5);
								seqPos += 3;
							}
							else
							{
								curDelay += romData[seqPos + 1] * 5;
								ctrlDelay += romData[seqPos + 1] * 5;
								seqPos += 2;
							}
						}
						else
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							seqPos++;
						}

					}
					else
					{
						curNote = command[0] + 24 + transpose;

						if (curTrack == 2)
						{
							curNote -= 12;
						}
						else if (curTrack == 3)
						{
							curNote += 12;
						}

						if (curNote > 127)
						{
							curNote = 0;
						}
						seqPos++;

						if (autoSize == 0)
						{
							seqPos++;
						}

						if (autoLen == 0)
						{
							if (romData[seqPos] >= 0x80)
							{
								curNoteLen = (ReadBE16(&romData[seqPos]) - 0x8000) * 5;
								seqPos += 2;
							}
							else
							{
								curNoteLen = romData[seqPos] * 5;
								seqPos++;
							}

						}

						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						curDelay = 0;

						firstNote = 0;
						midPos = tempPos;

						ctrlDelay += curNoteLen;
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
					autoLen = 1;
					if (command[1] >= 0x80)
					{
						curNoteLen = ((ReadBE16(&romData[seqPos + 1]) - 0x8000) * 5);
						seqPos += 3;
					}
					else
					{
						curNoteLen = command[1] * 5;
						seqPos += 2;
					}
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

				/*Jump to position*/
				else if (command[0] == 0x86)
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

				/*"Wobble" effect*/
				else if (command[0] == 0x87)
				{
					seqPos += 2;
				}

				/*"Distort" effect*/
				else if (command[0] == 0x88)
				{
					seqPos += 2;
				}

				/*Set vibrato*/
				else if (command[0] == 0x89)
				{
					seqPos += 2;
				}

				/*Unknown command 8A*/
				else if (command[0] == 0x8A)
				{
					seqPos += 2;
				}

				/*Set sweep*/
				else if (command[0] == 0x8B)
				{
					seqPos += 2;
				}

				/*Set panning*/
				else if (command[0] == 0x8C)
				{
					seqPos += 2;
				}

				/*Set panning - L*/
				else if (command[0] == 0x8D)
				{
					seqPos++;
				}

				/*Set panning - R*/
				else if (command[0] == 0x8E)
				{
					seqPos++;
				}

				/*Set panning - L/R*/
				else if (command[0] == 0x8F)
				{
					seqPos++;
				}

				/*Set panning - none*/
				else if (command[0] == 0x90)
				{
					seqPos++;
				}

				/*Set instrument?*/
				else if (command[0] == 0x91)
				{
					curInst = command[1];
					firstNote = 1;
					seqPos += 2;
				}

				/*Set envelope?*/
				else if (command[0] == 0x92)
				{
					autoSize = 1;
					if (command[1] >= 0x80)
					{
						seqPos++;
					}
					else
					{
						seqPos += 2;
					}
				}

				/*Song loop point*/
				else if (command[0] == 0x93)
				{
					seqPos++;
				}

				/*Go to song loop point*/
				else if (command[0] == 0x94)
				{
					seqEnd = 1;
				}

				/*Disable automatic length*/
				else if (command[0] == 0x95)
				{
					autoLen = 0;
					seqPos++;
				}

				/*Reset envelope?*/
				else if (command[0] == 0x96)
				{
					autoSize = 0;
					seqPos++;
				}

				/*Set ? to FF*/
				else if (command[0] == 0x97)
				{
					seqPos++;
				}

				/*Set ? to 0*/
				else if (command[0] == 0x98)
				{
					seqPos++;
				}

				/*Unknown command 99*/
				else if (command[0] == 0x99)
				{
					seqPos += 2;
				}

				/*Unknown command 9A*/
				else if (command[0] == 0x9A)
				{
					seqPos += 2;
				}

				/*Rest*/
				else
				{
					curDelay += ((command[0] - 0xBF) * 5);
					ctrlDelay += ((command[0] - 0xBF) * 5);
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