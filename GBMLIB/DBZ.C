/*Unknown (Dragon Ball Z: Legendary Super Warriors)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "DBZ.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j, k;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
int songBank;
long bankAmt;
long bankMap;
int foundTable;
int curInst;
int drvVers;
int curVol;

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
unsigned int WriteNoteEventAltOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventAltOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void DBZsong2mid(int songNum, long songPtr);

void DBZProc(int bank)
{
	curInst = 0;
	drvVers = DBZ_VER_STD;
	bankAmt = bankSize;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	songNum = 1;
	tableOffset = 0x42CC;
	bankMap = 0x42B8;
	i = tableOffset;
	j = bankMap;

	printf("Song table: 0x%04X\n", tableOffset);
	printf("Song bank mapping: 0x%04X\n", bankMap);

	while (songNum <= 20)
	{
		songPtr = ReadLE16(&romData[i]);
		songBank = romData[j] + 1;
		printf("Song %i: 0x%04X (bank %02X)\n", songNum, songPtr, songBank);
		fseek(rom, 0, SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 2);
		fread(exRomData, 1, bankSize, rom);
		fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
		fread(exRomData + bankSize, 1, bankSize, rom);
		DBZsong2mid(songNum, songPtr);
		free(exRomData);
		i += 2;
		j++;
		songNum++;
	}


	free(romData);


}

/*Convert the song data to MIDI*/
void DBZsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 150;
	int tempoVal = 0;
	int k = 0;
	int trackEnd = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int curNoteVal = 0;
	unsigned char command[3];
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
	long songLoopPt = 0;
	int songLoopAmt = 0;
	long tempPos = 0;
	int holdNote = 0;
	int lenBase = 0;
	long startPos = 0;
	int inMacro = 0;
	long macroPos = 0;
	int macroTimes = 0;
	long macroRet = 0;
	int tie = 0;
	int transpose = 0;
	int repeats[256][3];
	int defaultLen = 0;

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (k = 0; k < midLength; k++)
	{
		midData[k] = 0;
		ctrlMidData[k] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
		long romPos = 0;
		long seqPos = 0;
		int curTrack = 0;
		int trackCnt = 4;
		int ticks = 120;
		int tempo = 150;
		int k = 0;
		int trackEnd = 0;
		int seqEnd = 0;
		int curNote = 0;
		int curNoteLen = 0;
		int curNoteVal = 0;
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
		long songLoopPt = 0;
		int songLoopAmt = 0;
		long tempPos = 0;
		int holdNote = 0;
		int lenBase = 0;
		long startPos = 0;
		int inMacro = 0;
		long macroPos = 0;
		int macroTimes = 0;
		long macroRet = 0;
		int tie = 0;
		int chanSpeed = 0;

		midPos = 0;
		ctrlMidPos = 0;

		midLength = 0x10000;
		midData = (unsigned char*)malloc(midLength);

		ctrlMidData = (unsigned char*)malloc(midLength);

		for (k = 0; k < midLength; k++)
		{
			midData[k] = 0;
			ctrlMidData[k] = 0;
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
			tempo = exRomData[songPtr + 2];
			tempo = tempo ^ 0xFF;
			lowNibble = tempo >> 4;
			highNibble = tempo & 0x0F;
			highNibble = highNibble << 4;
			tempo = lowNibble | highNibble;
			tempo = tempo & 0x0F;

			if (tempo == 0)
			{
				tempo = 1;
			}

			tempo = 600 / tempo;

			if (tempo < 2)
			{
				tempo = 2;
			}

			tempoVal = 0;

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


			switch (drvVers)
			{
			case DBZ_VER_STD:
				/*Fall-through*/
			default:
				EventMap[0x80] = DBZ_EVENT_OCTAVE;
				EventMap[0x81] = DBZ_EVENT_OCTAVE;
				EventMap[0x82] = DBZ_EVENT_OCTAVE;
				EventMap[0x83] = DBZ_EVENT_OCTAVE;
				EventMap[0x84] = DBZ_EVENT_OCTAVE;
				EventMap[0x85] = DBZ_EVENT_OCTAVE;
				EventMap[0x86] = DBZ_EVENT_OCTAVE;
				EventMap[0x87] = DBZ_EVENT_OCTAVE;
				EventMap[0x88] = DBZ_EVENT_OCTAVE_UP;
				EventMap[0x89] = DBZ_EVENT_OCTAVE_DOWN;
				EventMap[0x90] = DBZ_EVENT_PROG_CHANGE;
				EventMap[0xA0] = DBZ_EVENT_MASTER_VOLUME;
				EventMap[0xA1] = DBZ_EVENT_VOLUME;
				EventMap[0xA3] = DBZ_EVENT_PAN;
				EventMap[0xBA] = DBZ_EVENT_TUNING;
				EventMap[0xC0] = DBZ_EVENT_STOP;
				EventMap[0xC2] = DBZ_EVENT_CLEAR_RAM;
				EventMap[0xC3] = DBZ_EVENT_TEMPO;
				EventMap[0xD0] = DBZ_EVENT_UNKNOWN0;
				EventMap[0xD1] = DBZ_EVENT_UNKNOWN0;
				EventMap[0xD2] = DBZ_EVENT_INS_PARAM;
				EventMap[0xD3] = DBZ_EVENT_UNKNOWN1;
				EventMap[0xF0] = DBZ_EVENT_REPEAT_START;
				EventMap[0xF1] = DBZ_EVENT_REPEAT_END;
				EventMap[0xF3] = DBZ_EVENT_LOOP;
				EventMap[0xF4] = DBZ_EVENT_GOTO_LOOP;
				EventMap[0xF5] = DBZ_EVENT_SFX_F5;
				EventMap[0xF6] = DBZ_EVENT_SFX_F6;
				EventMap[0xF7] = DBZ_EVENT_SFX_F7;
				EventMap[0xF8] = DBZ_EVENT_SFX_F8;
				EventMap[0xFF] = DBZ_EVENT_UNKNOWN0;

				DBZ_STATUS_NOTE_MIN = 0x00;
				DBZ_STATUS_NOTE_MAX = 0x7F;
			}

			for (curTrack = 0; curTrack < trackCnt; curTrack++)
			{
				trackEnd = 0;
				firstNote = 1;
				holdNote = 0;

				for (k = 0; k < 256; k++)
				{
					repeats[k][0] = -1;
					repeats[k][1] = 0;
					repeats[k][2] = 0;
				}

				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;

				curDelay = 0;
				ctrlDelay = 0;
				seqEnd = 0;

				curNote = 0;
				curNoteLen = 0;
				curVol = 120;
				tie = 0;
				transpose = 0;
				curInst = 0;

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

				/*Get the default parameters of each channel*/

				defaultLen = exRomData[songPtr + 4 + curTrack] * 5;

				/*For volume...*/
				tempByte = exRomData[songPtr + 8 + curTrack];
				if (curTrack != 2)
				{
					/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
					if ((tempByte & 0x08) == 0x00)
					{
						curVol = (tempByte & 0xF0) / 2;
					}
					else
					{
						if (((tempByte & 0xF0) / 2) == curVol)
						{
							curVol = (tempByte & 0xF0) / 2;
						}
						else
						{
							curVol = 120;
						}

					}

					if (curVol == 0)
					{
						curVol = 1;
					}
				}
				else
				{
					switch (tempByte)
					{
					case 0x20:
						curVol = 120;
						break;
					case 0x40:
						curVol = 60;
						break;
					case 0x60:
						curVol = 30;
						break;
					case 0x00:
						curVol = 1;
						break;
					default:
						curVol = 120;
						break;
					}
				}
				seqPtrs[curTrack] = ReadLE16(&exRomData[songPtr + 16 + (curTrack * 2)]) + songPtr;
				seqPos = seqPtrs[curTrack];

				while (seqEnd == 0)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
					command[4] = exRomData[seqPos + 4];
					command[5] = exRomData[seqPos + 5];


					if (seqPos < bankAmt || seqPos >= 0x8000)
					{
						seqEnd = 1;
					}

					if (command[0] >= DBZ_STATUS_NOTE_MIN && command[0] <= DBZ_STATUS_NOTE_MAX)
					{
						highNibble = (command[0] & 0x0F);

						if ((command[0] & 0x10) != 0x00)
						{
							curNoteLen = command[1] * 5;
							seqPos++;
						}
						else
						{
							curNoteLen = defaultLen;
						}
						/*Rest*/
						if (highNibble == 0x0C)
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						/*Tie*/
						else if (highNibble == 0x0D)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
						}
						else
						{
							if (holdNote == 1)
							{
								tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
								curDelay = 0;
								holdNote = 0;
								midPos = tempPos;
							}
							curNote = highNibble + transpose + 24;
							if (curTrack < 2)
							{
								curNote += 12;
							}
							tempPos = WriteNoteEventAltOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							holdNote = 1;
							midPos = tempPos;
							curDelay = curNoteLen;
							ctrlDelay += curNoteLen;

						}
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_UNKNOWN0)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_UNKNOWN1)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_OCTAVE)
					{
						transpose = (command[0] & 0x0F) * 12;
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_OCTAVE_UP)
					{
						transpose += 12;
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_OCTAVE_DOWN)
					{
						transpose -= 12;
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_PROG_CHANGE)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						curInst = command[1];
						firstNote = 1;
						seqPos += 2;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_MASTER_VOLUME)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_VOLUME)
					{
						if (curTrack != 2)
						{
							/*If bit 3 (increase/decrease flag) is not set, use the value to determine the volume. If it is set, use max volume*/
							if ((command[1] & 0x08) == 0x00)
							{
								curVol = (command[1] & 0xF0) / 2;
							}
							else
							{
								if (((command[1] & 0xF0) / 2) == curVol)
								{
									curVol = (command[1] & 0xF0) / 2;
								}
								else
								{
									curVol = 120;
								}

							}

							if (curVol == 0)
							{
								curVol = 60;
							}
						}
						else
						{
							switch (command[1])
							{
							case 0x20:
								curVol = 120;
								break;
							case 0x40:
								curVol = 60;
								break;
							case 0x60:
								curVol = 30;
								break;
							case 0x00:
								curVol = 1;
								break;
							default:
								curVol = 120;
								break;
							}
						}
						seqPos += 2;

					}

					else if (EventMap[command[0]] == DBZ_EVENT_PAN)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_TUNING)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_STOP)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						seqEnd = 1;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_CLEAR_RAM)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_TEMPO)
					{
						if (tempoVal < 2)
						{
							if (tempoVal == 0)
							{
								tempoVal = 1;
							}
							tempo = command[1] ^ 0xFF;
							lowNibble = tempo >> 4;
							highNibble = tempo & 0x0F;
							highNibble = highNibble << 4;
							tempo = lowNibble | highNibble;
							tempo = tempo & 0x0F;

							if (tempo == 0)
							{
								tempo = 1;
							}

							tempo = 600 / tempo;

							if (tempo < 2)
							{
								tempo = 2;
							}
						}


						ctrlMidPos++;
						valSize = WriteDeltaTime(ctrlMidData, ctrlMidPos, ctrlDelay);
						ctrlDelay = 0;
						ctrlMidPos += valSize;
						WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
						ctrlMidPos += 3;
						WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
						ctrlMidPos += 2;

						seqPos += 2;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_INS_PARAM)
					{
						seqPos += 6;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_REPEAT_START)
					{
						repeats[command[1]][1] = seqPos + 2;
						seqPos += 2;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_REPEAT_END)
					{
						if (repeats[command[1]][0] == -1)
						{
							repeats[command[1]][0] = command[2];
						}
						else if (repeats[command[1]][0] > 1)
						{
							repeats[command[1]][0]--;
							seqPos = repeats[command[1]][1];
						}

						else
						{
							repeats[command[1]][0] = -1;
							seqPos += 3;
						}
					}

					else if (EventMap[command[0]] == DBZ_EVENT_REG_WRITE)
					{
						seqPos += 3;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_LOOP)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_GOTO_LOOP)
					{
						if (holdNote == 1)
						{
							tempPos = WriteNoteEventAltOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							curDelay = 0;
							holdNote = 0;
							midPos = tempPos;
						}
						seqEnd = 1;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_SFX_F5)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_SFX_F6)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_SFX_F7)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == DBZ_EVENT_SFX_F8)
					{
						seqPos++;
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

				if (tempoVal == 1)
				{
					tempoVal = 2;
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
			free(midData);
			free(ctrlMidData);
			fclose(mid);
		}
	}
}