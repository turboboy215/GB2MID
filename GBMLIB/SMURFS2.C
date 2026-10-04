/*Unknown (Climax?) (The Smurfs Travel the World)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "SMURFS2.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long patTab;
int i, j;
char outfile[1000000];
int foundTable;
int curInst;

long songPtrs[4];
long bankAmt;
int songNum;
int numSongs;

int curVol;
int drvVers;

unsigned char* romData;
unsigned char* midData;

unsigned char* ctrlMidData;

long midLength;

unsigned const char Smurfs2tempos[13] = { 0xF3, 0xF2, 0xF3, 0xF3, 0xF4, 0xF3, 0xF2, 0xF6, 0xF1, 0xEF, 0xF3, 0xF3, 0xF3 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Smurfs2song2mid(int songNum, long songPtrs[4]);

void Smurfs2Proc(int bank)
{
	drvVers = SMURFS2_VER_STD;
	curVol = 120;
	curInst = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	tableOffset = 0x5E68;
	patTab = 0x5F80;
	songNum = 1;
	numSongs = 13;
	i = tableOffset;

	while (songNum <= numSongs)
	{
		songPtrs[0] = ReadLE16(&romData[i]);
		printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
		songPtrs[1] = ReadLE16(&romData[i + 4]);
		printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
		songPtrs[2] = ReadLE16(&romData[i + 8]);
		printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
		songPtrs[3] = ReadLE16(&romData[i + 12]);
		printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
		Smurfs2song2mid(songNum, songPtrs);
		i += 16;
		songNum++;
	}
	free(romData);

}

/*Convert the song data to MIDI*/
void Smurfs2song2mid(int songNum, long songPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	long seqPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int noteTrans = 0;
	int ticks = 120;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curVol = 0;
	int volVal = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	long tempPos = 0;

	long tempo = 150;

	int hasPlayedNote = 0;

	int baseNote = 0;
	int transpose = 0;

	unsigned char command[4];
	unsigned char lowNibble;
	unsigned char highNibble;

	int firstNote = 1;

	long patPos = 0;
	int curPat = 0;
	long patPtr = 0;

	int tempVal1 = 0;
	int tempVal2 = 0;

	midPos = 0;
	ctrlMidPos = 0;

	int isNote = 0;

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
		tempo = ((256 - Smurfs2tempos[songNum - 1]) * 59.7275) / 2;

		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		if (tempo < 2)
		{
			tempo = 2;
		}

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

		switch (drvVers)
		{
		case SMURFS2_VER_STD:
			/*Fall-through*/
		default:
			EventMap[0x00] = SMURFS2_EVENT_STOP;
			EventMap[0x01] = SMURFS2_EVENT_PROG_CHANGE;
			EventMap[0x02] = SMURFS2_EVENT_NOP1;
			EventMap[0x03] = SMURFS2_EVENT_UNKNOWN1;
			EventMap[0x04] = SMURFS2_EVENT_UNKNOWN1;
			EventMap[0x05] = SMURFS2_EVENT_NOP1;
			EventMap[0x06] = SMURFS2_EVENT_NOP1;
			EventMap[0x07] = SMURFS2_EVENT_TRANSPOSE;
			break;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			seqEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			curInst = 0;
			baseNote = 0;
			transpose = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);

			patPos = songPtrs[curTrack];

			while (romData[patPos] != 0x00 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				/*Rest*/
				if ((romData[patPos] & 0x80) != 0x00)
				{
					tempVal1 = romData[patPos] & 0x7F;
					tempVal2 = romData[patPos + 1];

					curDelay += ((tempVal1 << 9) | (tempVal2 << 1));
					patPos += 2;
				}

				/*Play pattern*/
				else
				{
					baseNote = (romData[patPos] + 0x52) >> 1;
					curNote = baseNote;
					curPat = romData[patPos + 1];
					seqEnd = 0;
					seqPos = ReadLE16(&romData[patTab + (curPat * 2)]);

					/*Start delta time*/
					if ((romData[seqPos] & 0x80) == 0x00)
					{
						curDelay += ((2 * romData[seqPos]));
						ctrlDelay += ((2 * romData[seqPos]));
						masterDelay += ((2 * romData[seqPos]));
						seqPos++;
					}
					else
					{
						curDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
						ctrlDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
						masterDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
						seqPos += 2;
					}

					if (curTrack != 3)
					{
						SMURFS2_STATUS_NOTE_MIN = 0x08;
					}
					else
					{
						SMURFS2_STATUS_NOTE_MIN = 0x07;
					}
					SMURFS2_STATUS_NOTE_MAX = 0xFF;

					while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
					{
						command[0] = romData[seqPos];
						command[1] = romData[seqPos + 1];

						if (command[0] >= SMURFS2_STATUS_NOTE_MIN && command[0] <= SMURFS2_STATUS_NOTE_MAX)
						{
							curNote = curNote + ((command[0] >> 3) & 0x1F) - 16;
							volVal = command[0] & 0x07;
							curVol = volVal * 0x10;

							if (curVol > 120)
							{
								curVol = 120;
							}

							if (curVol == 0)
							{
								curVol = 1;
							}

							if (curTrack == 3)
							{
								curNote += 16;
							}

							if (curNote > 127)
							{
								curNote = 0;
							}
							seqPos++;

							/*Get note delta/length*/
							if ((romData[seqPos] & 0x80) == 0x00)
							{
								curNoteLen = ((2 * romData[seqPos]));
								ctrlDelay += ((2 * romData[seqPos]));
								masterDelay += ((2 * romData[seqPos]));
								seqPos++;
							}
							else
							{
								curNoteLen = (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
								ctrlDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
								masterDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
								seqPos += 2;
							}
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
						}
						else
						{
							if (EventMap[command[0]] == SMURFS2_EVENT_UNKNOWN0 || EventMap[command[0]] == SMURFS2_EVENT_NOP)
							{
								seqPos++;
							}

							else if (EventMap[command[0]] == SMURFS2_EVENT_UNKNOWN1 || EventMap[command[0]] == SMURFS2_EVENT_NOP1)
							{
								seqPos += 2;
							}

							else if (EventMap[command[0]] == SMURFS2_EVENT_STOP)
							{
								seqEnd = 1;
								patPos += 2;
							}

							else if (EventMap[command[0]] == SMURFS2_EVENT_PROG_CHANGE)
							{
								curInst = command[1];
								firstNote = 1;
								seqPos += 2;
							}

							else if (EventMap[command[0]] == SMURFS2_EVENT_TRANSPOSE && curTrack != 3)
							{
								transpose = (signed char)command[1];
								seqPos += 2;
								if (curTrack == 2)
								{
									curTrack = 2;
								}
								curNote = curNote + ((romData[seqPos] >> 3) & 0x1F) - 16 + transpose;
								volVal = command[0] & 0x07;
								curVol = volVal * 0x10;

								if (curVol > 120)
								{
									curVol = 120;
								}

								if (curVol == 0)
								{
									curVol = 1;
								}

								if (curNote > 127)
								{
									curNote = 0;
								}
								seqPos++;

								/*Get note delta/length*/
								if ((romData[seqPos] & 0x80) == 0x00)
								{
									curNoteLen = ((2 * romData[seqPos]));
									ctrlDelay += ((2 * romData[seqPos]));
									masterDelay += ((2 * romData[seqPos]));
									seqPos++;
								}
								else
								{
									curNoteLen = (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
									ctrlDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
									masterDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
									seqPos += 2;
								}
								tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								curDelay = 0;
							}


							/*End delta if not stop*/
							if (seqEnd != 1 && command[0] != 0x07)
							{
								if ((romData[seqPos] & 0x80) == 0x00)
								{
									curDelay += ((2 * romData[seqPos]));
									ctrlDelay += ((2 * romData[seqPos]));
									masterDelay += ((2 * romData[seqPos]));
									seqPos++;
								}
								else
								{
									curDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
									ctrlDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
									masterDelay += (romData[seqPos + 1] * 256 + (2 * (romData[seqPos] & 0x7F)));
									seqPos += 2;
								}
							}
						}
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