/*Junichi Saito (Pack-in Video)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "JSAITO.H"

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
int drvVers;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char JSaitoMagicBytes[5] = { 0x6F, 0x26, 0x00, 0x29, 0x11 };
const unsigned int JSaitoNoteLens[14] = { 0x01, 0x03, 0x06, 0x08, 0x09, 0x0C, 0x10, 0x12, 0x18, 0x20, 0x24, 0x30, 0x48, 0x60 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void JSaitosong2mid(int songNum, long ptr);

void JSaitoProc(int bank)
{
	curInst = 0;
	foundTable = 0;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	if (bank != 1)
	{
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
		fclose(rom);
	}

	else
	{
		fseek(rom, ((bank - 1) * bankSize * 2), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize * 2, rom);
		fclose(rom);
	}

	if (bank == 0x20)
	{
		drvVers = 2;
	}
	else if (bank > 0x20)
	{
		drvVers = 3;
	}
	else
	{
		drvVers = 1;
	}

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], JSaitoMagicBytes, 5)) && romData[i + 9] == 0x66 && foundTable != 1)
		{
			tablePtrLoc = i + 5 + bankAmt;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{
		i = tableOffset - bankAmt + 2;
		songNum = 1;
		while (ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000)
		{
			songPtr = ReadLE16(&romData[i]);
			printf("Song %i: 0x%04X\n", songNum, songPtr);
			if (songPtr != 0 && ReadLE16(&romData[songPtr + 2 - bankAmt]) != 0x0000)
			{
				JSaitosong2mid(songNum, songPtr);
			}
			i += 2;
			songNum++;
		}
	}
}

/*Convert the song data to MIDI*/
void JSaitosong2mid(int songNum, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long songPtrs[4];
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 120;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int chanSpeed = 0;
	int octave = 0;
	int transpose = 0;
	int transposeNote = 0;
	int transposeOct = 0;
	int inMacro = 0;
	long macro1Pos = 0;
	long macro1Ret = 0;
	long macro2Pos = 0;
	long macro2Ret = 0;
	long macro3Pos = 0;
	long macro3Ret = 0;
	long jumpPos = 0;
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
	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	int repNote = 0;
	int repNoteLen = 0;

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

		romPos = ptr - bankAmt;
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			songPtrs[curTrack] = ReadLE16(&romData[romPos]);
			romPos += 2;
		}

		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			seqEnd = 0;
			octave = 3;
			transpose = 0;
			transposeNote = 0;
			transposeOct = 0;
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;

			curNote = 0;
			curNoteLen = 0;
			curDelay = 0;
			ctrlDelay = 0;
			inMacro = 0;
			macro1Pos = 0;
			macro1Ret = 0;
			macro2Pos = 0;
			macro2Ret = 0;
			macro3Pos = 0;
			macro3Ret = 0;

			seqPos = songPtrs[curTrack] - bankAmt;

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
				while (seqEnd == 0 && midPos < 48000 && seqPos < 0x8000 && ctrlDelay < 110000)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];

					if (command[0] < 0xD0)
					{
						lowNibble = (command[0] >> 4);
						highNibble = (command[0] & 15);

						/*Rest*/
						if (lowNibble == 0)
						{
							if (highNibble == 0x0F)
							{
								curDelay += (command[1] * 5);
								ctrlDelay += (command[1] * 5);
								seqPos += 2;
							}
							else if (highNibble == 0x0E)
							{
								lowNibble = (command[1] >> 4);
								highNibble = (command[1] & 15);
								repNote = lowNibble;

								if (highNibble == 0x0F)
								{
									curNoteLen = command[2] * 5;
									seqPos++;
								}
								else
								{
									curNoteLen = JSaitoNoteLens[highNibble] * 5;
								}

								if (drvVers == 1 || drvVers == 3)
								{
									for (k = 0; k <= repNote; k++)
									{
										curDelay += curNoteLen;
										ctrlDelay += curNoteLen;
									}
								}
								else
								{
									for (k = 0; k <= (repNote + 1); k++)
									{
										curDelay += curNoteLen;
										ctrlDelay += curNoteLen;
									}
								}

								seqPos += 2;
							}
							else
							{
								curDelay += (JSaitoNoteLens[highNibble] * 5);
								ctrlDelay += (JSaitoNoteLens[highNibble] * 5);
								seqPos++;
							}
						}

						/*Play note*/
						else
						{
							curNote = (lowNibble - 1) + (octave * 12) + transpose;

							if (highNibble == 0x0F)
							{
								curNoteLen = command[1] * 5;
								seqPos += 2;
							}

							else if (highNibble == 0x0E)
							{
								lowNibble = (command[1] >> 4);
								highNibble = (command[1] & 15);
								repNote = lowNibble;

								if (highNibble == 0x0F)
								{
									curNoteLen = command[2] * 5;
									seqPos++;
								}
								else
								{
									curNoteLen = JSaitoNoteLens[highNibble] * 5;
								}

								if (drvVers == 1 || drvVers == 3)
								{
									for (k = 0; k <= repNote; k++)
									{
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
										ctrlDelay += curNoteLen;
									}
								}
								else
								{
									for (k = 0; k <= (repNote + 1); k++)
									{
										tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
										firstNote = 0;
										holdNote = 0;
										midPos = tempPos;
										curDelay = 0;
										ctrlDelay += curNoteLen;
									}
								}

								seqPos += 2;
							}

							else
							{
								curNoteLen = JSaitoNoteLens[highNibble] * 5;
								seqPos++;
							}

							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;

						}
					}

					/*Set octave*/
					else if (command[0] >= 0xD0 && command[0] <= 0xD7)
					{
						highNibble = (command[0] & 15);

						switch (highNibble)
						{
						case 0x00:
							octave = 1;
							break;
						case 0x01:
							octave = 2;
							break;
						case 0x02:
							octave = 3;
							break;
						case 0x03:
							octave = 4;
							break;
						case 0x04:
							octave = 5;
							break;
						case 0x05:
							octave = 6;
							break;
						case 0x06:
							octave = 5;
							break;
						case 0x07:
							octave = 3;
							break;
						default:
							octave = 1;
							break;
						}
						seqPos++;
					}

					/*Manually set volume*/
					else if (command[0] == 0xD8)
					{
						seqPos += 2;
					}

					/*Set volume*/
					else if (command[0] >= 0xD9 && command[0] <= 0xDF)
					{
						seqPos++;
					}

					/*Set note size*/
					else if (command[0] >= 0xE0 && command[0] <= 0xE7)
					{
						seqPos++;
					}

					/*Manually set note size*/
					else if (command[0] == 0xE8)
					{
						seqPos += 2;
					}

					/*Set vibrato*/
					else if (command[0] == 0xE9)
					{
						seqPos += 2;
					}

					/*Set transpose*/
					else if (command[0] == 0xEA)
					{
						if (seqPos == 0x1D91)
						{
							seqPos = 0x1D91;
						}
						lowNibble = (command[1] >> 4);
						highNibble = (command[1] & 15);

						transposeNote = highNibble;

						switch (lowNibble)
						{
						case 0x00:
							transposeOct = 1;
							break;
						case 0x01:
							transposeOct = 2;
							break;
						case 0x02:
							transposeOct = 3;
							break;
						case 0x03:
							transposeOct = 4;
							break;
						case 0x04:
							transposeOct = -2;
							break;
						case 0x05:
							transposeOct = -2;
							break;
						case 0x06:
							transposeOct = -1;
							break;
						case 0x07:
							transposeOct = 0;
							break;
						case 0x08:
							transposeOct = 1;
							break;
						case 0x09:
							transposeOct = 2;
							break;
						case 0x0A:
							transposeOct = 3;
							break;
						case 0x0B:
							transposeOct = 4;
							break;
						case 0x0C:
							transposeOct = -2;
							break;
						case 0x0D:
							transposeOct = -2;
							break;
						case 0x0E:
							transposeOct = -1;
							break;
						case 0x0F:
							transposeOct = 0;
							break;
						default:
							transposeOct = 0;
							break;
						}

						transpose = (transposeOct * 12) + transposeNote;
						seqPos += 2;
					}

					/*Set noise type*/
					else if (command[0] == 0xEB)
					{
						seqPos += 2;
					}

					/*Set noise frequency*/
					else if (command[0] == 0xEC)
					{
						seqPos += 2;
					}

					/*Set duty/waveform*/
					else if (command[0] == 0xED)
					{
						seqPos += 2;
					}

					/*Set global volume*/
					else if (command[0] == 0xEE)
					{
						seqPos += 2;
					}

					/*Set individual channel volume?*/
					else if (command[0] == 0xEF)
					{
						seqPos += 2;
					}

					/*Set frequency/tuning*/
					else if (command[0] == 0xF0)
					{
						seqPos += 2;
					}

					/*Set panning: R(ight)*/
					else if (command[0] == 0xF1)
					{
						seqPos++;
					}

					/*Set panning: L(eft)*/
					else if (command[0] == 0xF2)
					{
						seqPos++;
					}

					/*Set panning to L(eft)/R(ight)*/
					else if (command[0] == 0xF3)
					{
						seqPos++;
					}

					/*Set all envelopes?*/
					else if (command[0] == 0xF4)
					{
						seqPos += 2;
					}

					/*Set sweep/distortion effect*/
					else if (command[0] == 0xF5)
					{
						seqPos += 2;
					}

					/*Command F6 (No function)*/
					else if (command[0] == 0xF6)
					{
						seqPos++;
					}

					/*Unknown command F7*/
					else if (command[0] == 0xF7)
					{
						seqPos += 3;
					}

					/*Unknown command F8*/
					else if (command[0] == 0xF8)
					{
						seqPos += 3;
					}

					/*Exit macro*/
					else if (command[0] == 0xF9)
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
						else if (inMacro == 3)
						{
							seqPos = macro3Ret;
							inMacro = 2;
						}
						else
						{
							seqEnd = 1;
						}
					}

					/*Go to macro*/
					else if (command[0] == 0xFA)
					{
						if (inMacro == 0)
						{
							macro1Ret = ReadLE16(&romData[seqPos + 1]);
							macro1Pos = (((seqPos + bankAmt + 3) + macro1Ret) & 0xFFFF) - bankAmt;

							macro1Ret = seqPos + 3;
							inMacro = 1;
							seqPos = macro1Pos;
						}

						else if (inMacro == 1)
						{
							macro2Ret = ReadLE16(&romData[seqPos + 1]);
							macro2Pos = (((seqPos + bankAmt + 3) + macro2Ret) & 0xFFFF) - bankAmt;
							macro2Ret = seqPos + 3;
							inMacro = 2;
							seqPos = macro2Pos;
						}

						else if (inMacro == 2)
						{
							macro3Ret = ReadLE16(&romData[seqPos + 1]);
							macro3Pos = (((seqPos + bankAmt + 3) + macro3Ret) & 0xFFFF) - bankAmt;
							macro3Ret = seqPos + 3;
							inMacro = 3;
							seqPos = macro3Pos;
						}

						else
						{
							seqEnd = 1;
						}

					}

					/*Noise sweep effect*/
					else if (command[0] == 0xFB)
					{
						seqPos += 2;
					}

					/*Go to song loop point*/
					else if (command[0] == 0xFC)
					{
						if (ReadLE16(&romData[seqPos + 1]) == 0x0000)
						{
							seqPos += 3;
						}
						else
						{
							seqEnd = 1;
						}

					}

					/*Set tempo*/
					else if (command[0] == 0xFD)
					{
						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						if (drvVers == 1)
						{
							tempo = command[1] * 0.8;
						}
						else if (drvVers == 2)
						{
							tempo = command[1] * 1.5;
						}
						else
						{
							tempo = command[1];
						}


						if (tempo > 1)
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						}
						else
						{
							WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / 150);
						}

						ctrlMidPos += 2;
						seqPos += 2;
					}

					/*Reset envelopes?*/
					else if (command[0] == 0xFE)
					{
						seqPos++;
					}

					/*End of channel (no loop)*/
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
		free(ctrlMidData);
		fclose(mid);
	}
}