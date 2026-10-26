/*Unknown (Toki Tori)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "math.h"
#include "SHARED.H"
#include "TOKITORI.H"

#define bankSize 16384

/*Bank + Game Boy address -> offset in the ROM file*/
#define ROMOFS(b, p) ((long)(b - 1) * bankSize + ((long)(p) - 0x4000))

FILE* rom, * mod;
long offset;
long headerOffset;
int i, j;
char outfile[1000000];
long bank;
long songBank;
long offset;
long tableOffset;
int songNum;
long songPtr;
long songLoopPtr;
long bankAmt;
int curVol;
int drvVers;
int songTempo;
int curInst;
int preferVol;
long romSize;
int bnk;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* exRomData2;
unsigned char* modData;

long modLength;

/*Our sample*/
const unsigned char TokiTorisineWave[64] = { 0x00, 0x00, 0x00, 0x03, 0x06, 0x0A, 0x0E, 0x11, 0x15, 0x17, 0x1A,
0x1D, 0x20, 0x23, 0x27, 0x2A, 0x2C, 0x30, 0x33, 0x35, 0x38, 0x3B, 0x3E, 0x41, 0x43, 0x46, 0x4A, 0x4C,
0x4F, 0x52, 0x55, 0x58, 0x59, 0x59, 0x54, 0x4F, 0x4B, 0x48, 0x44, 0x41, 0x3E, 0x3B, 0x39, 0x37, 0x35,
0x33, 0x30, 0x2E, 0x2C, 0x29, 0x26, 0x22, 0x1F, 0x1B, 0x18, 0x12, 0x0B, 0x05, 0xFE, 0xF8, 0xF2, 0xEC,
0xE7, 0xE4 };

/*ProTracker periods, C-1 to B-3, finetune 0*/
const unsigned short TokiTorimodPeriods[36] = { 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 };

/*One decoded row of one channel. -1 = field not present*/
typedef struct
{
	int note;
	int ins;
	int vol;
	int fx;
	int param;
	int len;
} TTRow;

/*One order entry*/
typedef struct
{
	long chanPos[TT_CHANNELS];	/*ROM file offset of each channel's stream*/
	int rows;
} TTOrder;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void TokiToriReadRow(long pos, TTRow* row);
int TokiToriGetSlot(int chan, int insVal, int* noiseVals, int* numNoise);
int TokiToriSlideParam(int period, int gbParam, int maxVal);
void TokiTorisong2mod(int songNum, long songPtr);
unsigned char gb_read_byte(int bank, int cpu);

void TokiToriProc(int bank)
{
	drvVers = TOKITORI_VER_STD;
	curVol = 120;
	curInst = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	bnk = bank;

	/*Load the whole ROM: every channel of a pattern can be in a different bank*/
	fseek(rom, 0, SEEK_END);
	romSize = ftell(rom);
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(romSize);
	if (fread(romData, 1, romSize, rom) != (size_t)romSize)
	{
		printf("ERROR: Unable to read file!\n");
		exit(1);
	}

	/*Song table: 8 bytes per song, $FF after the last one*/
	tableOffset = 0x6000;
	i = ROMOFS(bank, tableOffset);
	songNum = 1;

	while (romData[i] != 0xFF)
	{
		songBank = bank + romData[i];
		songPtr = ReadLE16(&romData[i + 1]);
		songTempo = romData[i + 3];
		songLoopPtr = ReadLE16(&romData[i + 4]);

		printf("Song %i: 0x%04lX, bank %02lX, speed %02X, loop: 0x%04lX\n", songNum, songPtr, songBank, songTempo, songLoopPtr);
		TokiTorisong2mod(songNum, songPtr);
		i += 8;
		songNum++;
	}

	free(romData);
}

/*Decode the row at pos.
  Byte < $80: note, instrument, volume, effect, param (5 bytes).
  Byte $80-$9F: mask; bit 0 note, 1 instrument, 2 volume, 3 effect, 4 param, one byte each
  (the driver copies at most 4). Effect without param = param 0; param without effect = effect 0.*/
void TokiToriReadRow(long pos, TTRow* row)
{
	unsigned char mask = romData[pos];
	long k = pos + 1;
	int left = 4;

	row->note = -1;
	row->ins = -1;
	row->vol = -1;
	row->fx = -1;
	row->param = -1;

	if (mask < 0x80)
	{
		row->note = mask;
		row->ins = romData[pos + 1];
		row->vol = romData[pos + 2];
		row->fx = romData[pos + 3];
		row->param = romData[pos + 4];
		row->len = 5;
		return;
	}

	if ((mask & 0x01) && left > 0)
	{
		row->note = romData[k++];
		left--;
	}
	if ((mask & 0x02) && left > 0)
	{
		row->ins = romData[k++];
		left--;
	}
	if ((mask & 0x04) && left > 0)
	{
		row->vol = romData[k++];
		left--;
	}
	if ((mask & 0x08) && left > 0)
	{
		row->fx = romData[k++];
		left--;
	}
	if ((mask & 0x10) && left > 0)
	{
		row->param = romData[k++];
		left--;
	}
	if (row->fx >= 0 && row->param < 0)
	{
		row->param = 0;
	}
	if (row->fx < 0 && row->param >= 0)
	{
		row->fx = 0;
	}
	row->len = (int)(k - pos);
}

/*The instrument byte is a register value, not a number: give each (channel, value) its own
  MOD sample. ch1/ch2: NRx1 duty (bits 6-7). ch3: wave number (values >= $10 are read
  misaligned by the driver; $80, the only one used, sounds like wave 0). ch4: NR43 value.*/
int TokiToriGetSlot(int chan, int insVal, int* noiseVals, int* numNoise)
{
	int k;

	if (insVal < 0)
	{
		insVal = 0;
	}
	switch (chan)
	{
	case 0:
		return SMP_CH1 + (insVal >> 6);
	case 1:
		return SMP_CH2 + (insVal >> 6);
	case 2:
		return SMP_CH3 + ((insVal < 0x10) ? insVal : 0);
	default:
		for (k = 0; k < *numNoise; k++)
		{
			if (noiseVals[k] == insVal)
			{
				return SMP_CH4 + k;
			}
		}
		if (*numNoise < SMP_CH4_NUM)
		{
			noiseVals[*numNoise] = insVal;
			(*numNoise)++;
			return SMP_CH4 + *numNoise - 1;
		}
		printf("WARNING: more than %i noise values, NR43 $%02X shares the last sample\n", SMP_CH4_NUM, insVal);
		return SMP_CH4 + SMP_CH4_NUM - 1;
	}
}

/*The driver's slides move (param / 16) semitones per tick; MOD slides move period units.
  Convert at the channel's current period.*/
int TokiToriSlideParam(int period, int gbParam, int maxVal)
{
	int v;

	if (gbParam == 0)
	{
		return 0;
	}
	if (period <= 0)
	{
		period = 428;
	}
	v = (int)(period * (pow(2.0, gbParam / 192.0) - 1.0) + 0.5);
	if (v < 1)
	{
		v = 1;
	}
	if (v > maxVal)
	{
		v = maxVal;
	}
	return v;
}

/*Convert the song data to MOD*/
void TokiTorisong2mod(int songNum, long songPtr)
{
	TTOrder orders[MOD_MAX_PATS];
	TTRow row[TT_CHANNELS];
	int numOrders = 0;
	long romPos = 0;
	long modPos = 0;
	long patPos;
	long cell;
	long chanPos[TT_CHANNELS];
	int curPat = 0;
	int curRow = 0;
	int curChan = 0;
	int i = 0;
	int j = 0;
	int k = 0;

	/*Octave handling: one sample length per channel type (ch1, ch2, ch3)*/
	long noteHist[3][128];
	int noteOfs[4];
	int sampLen[32];
	int sampVol[32];
	long volHist[32][65];
	int insUsed[32];
	int noiseVals[SMP_CH4_NUM];
	int numNoise = 0;
	int folded = 0;
	int best, bestCount, count, ofs, n;

	/*Per-channel state*/
	int curIns[TT_CHANNELS];		/*last instrument byte*/
	int modVol[TT_CHANNELS];		/*volume the MOD channel has now (-1 = unknown)*/
	int curPeriod[TT_CHANNELS];

	/*Per-row output*/
	int cellNote[TT_CHANNELS], cellIns[TT_CHANNELS], cellFx[TT_CHANNELS], cellParam[TT_CHANNELS];
	int gFx[8], gParam[8], gChan[8];
	int numGlobal;
	int gbVol, needVol, mfx, mpar, isGlobal, haveBreak;
	int lostVol = 0;
	int lostFx = 0;
	int period;
	char name[32];

	/*Read the order list*/
	romPos = ROMOFS(songBank, songPtr);
	while (romData[romPos] != 0xFF)
	{
		if (numOrders >= MOD_MAX_PATS)
		{
			/*
			printf("WARNING: more than %i order entries, the rest are dropped\n", MOD_MAX_PATS);
			*/
			break;
		}
		for (curChan = 0; curChan < TT_CHANNELS; curChan++)
		{
			orders[numOrders].chanPos[curChan] = ROMOFS(bnk + romData[romPos + curChan * 3],
				ReadLE16(&romData[romPos + curChan * 3 + 1]));
		}
		orders[numOrders].rows = romData[romPos + 12];
		if (orders[numOrders].rows > TT_MAX_ROWS)
		{
			/*
			printf("WARNING: order entry %i has %i rows, only %i are converted\n", numOrders, orders[numOrders].rows, TT_MAX_ROWS);
			*/
		}
		numOrders++;
		romPos += 16;
	}

	/*Pass 1: notes per channel type and the samples used. The instrument carries over
	  from row to row (and entry to entry) until a row sets a new one.*/
	memset(noteHist, 0, sizeof(noteHist));
	memset(volHist, 0, sizeof(volHist));
	for (curChan = 0; curChan < TT_CHANNELS; curChan++)
	{
		curIns[curChan] = 0;
	}
	for (i = 0; i < 32; i++)
	{
		insUsed[i] = 0;
		sampLen[i] = 64;
	}
	for (curPat = 0; curPat < numOrders; curPat++)
	{
		for (curChan = 0; curChan < TT_CHANNELS; curChan++)
		{
			chanPos[curChan] = orders[curPat].chanPos[curChan];
		}
		for (curRow = 0; curRow < orders[curPat].rows && curRow < TT_MAX_ROWS; curRow++)
		{
			for (curChan = 0; curChan < TT_CHANNELS; curChan++)
			{
				TokiToriReadRow(chanPos[curChan], &row[curChan]);
				chanPos[curChan] += row[curChan].len;
				if (row[curChan].ins >= 0)
				{
					curIns[curChan] = row[curChan].ins;
				}
				if (row[curChan].ins >= 0 || row[curChan].note >= 0)
				{
					k = TokiToriGetSlot(curChan, curIns[curChan], noiseVals, &numNoise);
					insUsed[k] = 1;
					/*The volume this note starts at ($3F if the row has none)*/
					n = (row[curChan].vol >= 0) ? row[curChan].vol : 0x3F;
					volHist[k][(n > 64) ? 64 : n]++;
				}
				if (row[curChan].note >= 0 && curChan < 3)
				{
					noteHist[curChan][row[curChan].note & 0x7F]++;
				}
			}
		}
	}

	/*Pick the octave offset (0/12/24/36 = sample length 64/32/16/8) that needs the
	  fewest notes folded into C-1..B-3. A 64-byte cycle at period 856 plays GB note 0 (C2).*/
	for (curChan = 0; curChan < 3; curChan++)
	{
		best = 0;
		bestCount = -1;
		for (ofs = 0; ofs <= 36; ofs += 12)
		{
			count = 0;
			for (n = 0; n < 128; n++)
			{
				if (n - ofs < 0 || n - ofs > 35)
				{
					count += noteHist[curChan][n];
				}
			}
			if (bestCount < 0 || count < bestCount)
			{
				best = ofs;
				bestCount = count;
			}
		}
		noteOfs[curChan] = best;
	}
	noteOfs[3] = 0;

	/*Each sample's default volume = the volume its notes most often start at. Writing the
	  sample number on a note then sets that volume with no Cxx, which keeps the effect
	  column free for the row's own effect.*/
	for (i = 0; i < 32; i++)
	{
		sampVol[i] = 63;
		for (n = 0; n <= 64; n++)
		{
			if (volHist[i][n] > volHist[i][sampVol[i]])
			{
				sampVol[i] = n;
			}
		}
	}
	for (i = 0; i < 4; i++)
	{
		sampLen[SMP_CH1 + i] = 64 >> (noteOfs[0] / 12);
		sampLen[SMP_CH2 + i] = 64 >> (noteOfs[1] / 12);
	}
	for (i = 0; i < 16; i++)
	{
		sampLen[SMP_CH3 + i] = 64 >> (noteOfs[2] / 12);
	}
	printf("  %i order entries; octave offsets ch1 %i, ch2 %i, ch3 %i\n", numOrders, noteOfs[0], noteOfs[1], noteOfs[2]);

	modLength = 0x100000;
	modData = ((unsigned char*)malloc(modLength));

	for (i = 0; i < modLength; i++)
	{
		modData[i] = 0;
	}

	sprintf(outfile, "song%i.mod", songNum);
	if ((mod = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mod!\n", songNum);
		free(modData);
		return;
	}

	/*Write MOD header*/
	modPos = 0;
	sprintf((char*)&modData[modPos], "GB2MOD conversion");
	/*The song title is 20 bytes*/
	modPos += 20;

	/*Write sample/instrument parameters*/
	for (i = 1; i <= 31; i++)
	{
		if (insUsed[i])
		{
			if (i < SMP_CH2)
			{
				sprintf(name, "ch1 duty %i", i - SMP_CH1);
			}
			else if (i < SMP_CH3)
			{
				sprintf(name, "ch2 duty %i", i - SMP_CH2);
			}
			else if (i < SMP_CH4)
			{
				sprintf(name, "ch3 wave %i", i - SMP_CH3);
			}
			else
			{
				sprintf(name, "ch4 NR43 $%02X", noiseVals[i - SMP_CH4]);
			}
			memcpy(&modData[modPos], name, strlen(name));
			modPos += 22;
			/*Sample length (in words)*/
			WriteBE16(&modData[modPos], sampLen[i] / 2);
			modPos += 2;
			/*Fine tune: +1 (1/8 semitone), the PAL period table is about 18 cents flat here*/
			Write8B(&modData[modPos], 1);
			modPos++;
			/*Sample volume: the most common starting volume of this sample's notes*/
			Write8B(&modData[modPos], sampVol[i]);
			modPos++;
			/*Loop start*/
			WriteBE16(&modData[modPos], 0);
			modPos += 2;
			/*Loop length (in words)*/
			WriteBE16(&modData[modPos], sampLen[i] / 2);
			modPos += 2;
		}
		else
		{
			/*Unused slot: empty sample, loop length 1 as ProTracker writes it*/
			modPos += 22;
			WriteBE16(&modData[modPos], 0);
			modPos += 2;
			modPos += 2;
			WriteBE16(&modData[modPos], 0);
			modPos += 2;
			WriteBE16(&modData[modPos], 1);
			modPos += 2;
		}
	}

	/*Number of song positions: one pattern per order entry, so Bxx parameters (order
	  entry numbers) stay valid*/
	Write8B(&modData[modPos], numOrders);
	modPos++;
	/*Restart byte: 127 as ProTracker writes it*/
	Write8B(&modData[modPos], 127);
	modPos++;
	/*Fill in the pattern table*/
	for (j = 0; j < numOrders; j++)
	{
		Write8B(&modData[modPos + j], j);
	}
	modPos += 128;
	/*Put in the initials M.K.*/
	memcpy(&modData[modPos], "M.K.", 4);
	modPos += 4;

	/*Pass 2: the pattern data. Each channel's stream is read independently, row by row*/
	for (curChan = 0; curChan < TT_CHANNELS; curChan++)
	{
		curIns[curChan] = 0;
		modVol[curChan] = -1;
		curPeriod[curChan] = 428;
	}
	for (curPat = 0; curPat < numOrders; curPat++)
	{
		patPos = modPos + (curPat * MOD_PAT_SIZE);
		for (curChan = 0; curChan < TT_CHANNELS; curChan++)
		{
			chanPos[curChan] = orders[curPat].chanPos[curChan];
		}

		for (curRow = 0; curRow < orders[curPat].rows && curRow < TT_MAX_ROWS; curRow++)
		{
			numGlobal = 0;
			haveBreak = 0;

			/*The song's starting speed goes on the first row*/
			if (curPat == 0 && curRow == 0)
			{
				gFx[numGlobal] = 0x0F;
				gParam[numGlobal] = songTempo;
				gChan[numGlobal] = 0;
				numGlobal++;
			}

			for (curChan = 0; curChan < TT_CHANNELS; curChan++)
			{
				TokiToriReadRow(chanPos[curChan], &row[curChan]);
				chanPos[curChan] += row[curChan].len;

				cellNote[curChan] = -1;
				cellIns[curChan] = 0;
				cellFx[curChan] = -1;
				cellParam[curChan] = 0;
				gbVol = -1;

				if (row[curChan].ins >= 0)
				{
					curIns[curChan] = row[curChan].ins;
				}

				/*Note: ch4's note byte is ignored by the driver (the pitch is in NR43), so it
				  plays a fixed note. A note or instrument without a volume sets the volume to
				  $3F; writing the sample number (default volume 63) does the same in MOD.*/
				if (row[curChan].note >= 0)
				{
					if (curChan == 3)
					{
						cellNote[curChan] = 12;
					}
					else
					{
						n = row[curChan].note - noteOfs[curChan];
						while (n < 0)
						{
							n += 12;
							folded++;
						}
						while (n > 35)
						{
							n -= 12;
							folded++;
						}
						cellNote[curChan] = n;
					}
					curPeriod[curChan] = TokiTorimodPeriods[cellNote[curChan]];
				}
				if (row[curChan].note >= 0 || row[curChan].ins >= 0)
				{
					cellIns[curChan] = TokiToriGetSlot(curChan, curIns[curChan], noiseVals, &numNoise);
					modVol[curChan] = sampVol[cellIns[curChan]];
					gbVol = 63;
				}
				if (row[curChan].vol >= 0)
				{
					gbVol = row[curChan].vol;
					if (gbVol > 64)
					{
						gbVol = 64;
					}
				}
				needVol = (gbVol >= 0 && gbVol != modVol[curChan]);

				/*Effects: the driver uses ProTracker numbering*/
				mfx = -1;
				mpar = 0;
				isGlobal = 0;
				if (row[curChan].fx >= 0)
				{
					k = row[curChan].param;
					switch (row[curChan].fx)
					{
					case 0x00:	/*Arpeggio (00 = nothing)*/
						if (k != 0)
						{
							mfx = 0x00;
							mpar = k;
						}
						break;
					case 0x01:	/*Slide up: param/16 semitone per tick*/
					case 0x02:	/*Slide down*/
					case 0x03:	/*Tone portamento*/
						mfx = row[curChan].fx;
						mpar = TokiToriSlideParam(curPeriod[curChan], k, 255);
						break;
					case 0x04:	/*Vibrato: depth y is about y/16 semitone; MOD depth y is 2y period units*/
						mfx = 0x04;
						if (k != 0)
						{
							j = TokiToriSlideParam(curPeriod[curChan], k & 0x0F, 31) / 2;
							if (j < 1)
							{
								j = 1;
							}
							if (j > 15)
							{
								j = 15;
							}
							mpar = (k & 0xF0) | j;
						}
						break;
					case 0x05:	/*Tone portamento + volume slide*/
					case 0x06:	/*Vibrato + volume slide*/
					case 0x07:	/*Tremolo*/
					case 0x0A:	/*Volume slide*/
						mfx = row[curChan].fx;
						mpar = k;
						break;
					case 0x0B:	/*Position jump: order entry number = MOD position*/
						mfx = 0x0B;
						mpar = k;
						isGlobal = 1;
						haveBreak = 1;
						break;
					case 0x0C:	/*Set volume*/
						gbVol = (k > 64) ? 64 : k;
						needVol = 1;
						break;
					case 0x0D:	/*Pattern break (the driver ignores the parameter)*/
						mfx = 0x0D;
						mpar = 0;
						isGlobal = 1;
						haveBreak = 1;
						break;
					case 0x0F:	/*Speed*/
						if (k != 0)
						{
							mfx = 0x0F;
							mpar = k;
							isGlobal = 1;
						}
						break;
					case 0x11:	/*E1x fine slide up*/
					case 0x12:	/*E2x fine slide down*/
						mfx = 0x0E;
						mpar = ((row[curChan].fx & 0x0F) << 4) | TokiToriSlideParam(curPeriod[curChan], k & 0x0F, 15);
						break;
					case 0x1A:	/*EAx/EBx: the driver treats the param as an xy volume slide*/
					case 0x1B:
						mfx = 0x0E;
						mpar = (k & 0xF0) ? (0xA0 | (k >> 4)) : (0xB0 | (k & 0x0F));
						break;
					default:	/*8, 9, E and the other Ex: nothing in the driver*/
						break;
					}
				}

				/*Global effects (speed, jump, break) can go in any free channel*/
				if (isGlobal)
				{
					gFx[numGlobal] = mfx;
					gParam[numGlobal] = mpar;
					gChan[numGlobal] = curChan;
					numGlobal++;
					mfx = -1;
				}

				/*One effect column: a real effect wins over a volume change, unless -v or the
				  volume is 0 (a note-off: an effect on a silent note doesn't matter).
				  Speed/jump/break never lose: they were moved to the global list above.*/
				if (mfx >= 0 && needVol && (preferVol || gbVol == 0))
				{
					cellFx[curChan] = 0x0C;
					cellParam[curChan] = gbVol;
					modVol[curChan] = gbVol;
					lostFx++;
					mfx = -1;
				}
				else if (mfx >= 0)
				{
					cellFx[curChan] = mfx;
					cellParam[curChan] = mpar;
					if (needVol)
					{
						/*modVol keeps what the MOD channel really has*/
						lostVol++;
					}
				}
				else if (needVol)
				{
					cellFx[curChan] = 0x0C;
					cellParam[curChan] = gbVol;
					modVol[curChan] = gbVol;
				}

				/*After a volume effect the MOD volume isn't known any more*/
				if (mfx == 0x05 || mfx == 0x06 || mfx == 0x07 || mfx == 0x0A ||
					(mfx == 0x0E && (mpar >> 4) >= 0x0A))
				{
					modVol[curChan] = -1;
				}
			}

			/*Shorter order entries: end the pattern early*/
			if (orders[curPat].rows < 64 && curRow == orders[curPat].rows - 1 && !haveBreak)
			{
				gFx[numGlobal] = 0x0D;
				gParam[numGlobal] = 0;
				gChan[numGlobal] = 0;
				numGlobal++;
			}

			/*Place the global effects: their own channel first, then any free one*/
			for (k = 0; k < numGlobal; k++)
			{
				j = -1;
				if (cellFx[gChan[k]] < 0)
				{
					j = gChan[k];
				}
				else
				{
					for (n = 0; n < TT_CHANNELS; n++)
					{
						if (cellFx[n] < 0)
						{
							j = n;
							break;
						}
					}
				}
				if (j >= 0)
				{
					cellFx[j] = gFx[k];
					cellParam[j] = gParam[k];
				}
				else
				{
					/*No room: a speed or jump beats a volume change, so take a volume slot*/
					for (n = 0; n < TT_CHANNELS; n++)
					{
						if (cellFx[n] == 0x0C)
						{
							j = n;
							break;
						}
					}
					if (j >= 0)
					{
						cellFx[j] = gFx[k];
						cellParam[j] = gParam[k];
						modVol[j] = -1;		/*its Cxx is gone; the real value is the one before*/
						lostVol++;
					}
					else
					{
						lostFx++;
					}
				}
			}

			/*Write the row. MOD cell: iiiiPPPP PPPPPPPP iiiieeee pppppppp*/
			for (curChan = 0; curChan < TT_CHANNELS; curChan++)
			{
				cell = patPos + (curRow * MOD_ROW_SIZE) + (curChan * 4);
				period = (cellNote[curChan] >= 0) ? TokiTorimodPeriods[cellNote[curChan]] : 0;
				mfx = (cellFx[curChan] >= 0) ? cellFx[curChan] : 0;
				mpar = (cellFx[curChan] >= 0) ? cellParam[curChan] : 0;
				modData[cell + 0] = (cellIns[curChan] & 0x10) | ((period >> 8) & 0x0F);
				modData[cell + 1] = period & 0xFF;
				modData[cell + 2] = ((cellIns[curChan] & 0x0F) << 4) | (mfx & 0x0F);
				modData[cell + 3] = mpar;
			}
		}
	}

	/*The song loop: the driver restarts at songLoopPtr. MOD restarts at position 0, so
	  any other loop point gets a Bxx on the last row*/
	if (songLoopPtr != songPtr && numOrders > 0)
	{
		curPat = numOrders - 1;
		curRow = (orders[curPat].rows < 64 ? orders[curPat].rows : 64) - 1;
		patPos = modPos + (curPat * MOD_PAT_SIZE);
		for (curChan = 0; curChan < TT_CHANNELS; curChan++)
		{
			cell = patPos + (curRow * MOD_ROW_SIZE) + (curChan * 4);
			if ((modData[cell + 2] & 0x0F) == 0 && modData[cell + 3] == 0)
			{
				modData[cell + 2] |= 0x0B;
				modData[cell + 3] = (unsigned char)((songLoopPtr - songPtr) / 16);
				break;
			}
		}
	}
	modPos += numOrders * MOD_PAT_SIZE;

	/*
	if (folded)
	{
		printf("  WARNING: %i notes were moved by an octave to fit C-1..B-3\n", folded);
	}
	if (lostVol)
	{
		printf("  WARNING: %i volume changes dropped (row already has an effect)\n", lostVol);
	}
	if (lostFx)
	{
		printf("  WARNING: %i effects dropped (no free effect column)\n", lostFx);
	}
	*/

	/*Sample data: the 64-byte wave resampled to each sample's length*/
	for (i = 1; i <= 31; i++)
	{
		if (!insUsed[i])
		{
			continue;
		}
		for (j = 0; j < sampLen[i]; j++)
		{
			modData[modPos + j] = TokiTorisineWave[(j * 64) / sampLen[i]];
		}
		modPos += sampLen[i];
	}

	fwrite(modData, modPos, 1, mod);
	free(modData);
	fclose(mod);
}