/*Konami (original driver/KCE Nagoya)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <stddef.h>
#include <math.h>
#include "SHARED.H"
#include "KONAMI.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;

int i, j;
char outfile[1000000];
int songNum;
long songPtr;
long seqPtrs[4];
int songBank;
long bankAmt;
int curVol;
int curInst;
int drvVers;
int drvGen;
int foundTable;
int progBank;
int verOverride;
int multiplier = 5;
int isSFX;		/*the whole sound is SFX (no music tracks)*/
int trkSFX;		/*the track being converted is an SFX track (own tick rate, no tempo)*/
int trkSFXMixed;	/*...and it plays inside a music song (its times go through the tempo map)*/

/*Noise SFX state (TMNT II: notes write NR43 without a retrigger, so the NR42 envelope keeps running)*/
int noiseHold;
long noiseTrigTime;
int noiseEnvVol;
int noiseEnvDir;
int noiseEnvPace;

/*Goemon raw frames ($E0 u|$80 on a tone/wave track): driver state and the MIDI note being held
  (frames with the same note and velocity and no retrigger are merged into one note)*/
int rawSlide;		/*TRK_FLAGS bit 6: slide frames*/
int rawSlideCnt;	/*TRK_SLIDECNT (also counts the $EE repeats)*/
int rawSlideVol;	/*TRK_SLIDEVOL (8-bit)*/
int rawSlideDelta;	/*TRK_SLIDEDELTA (16-bit)*/
int rawVolOfs;		/*TRK_RAWVOL*/
int rawPerOfs;		/*TRK_RAWOFS (16-bit)*/
long rawMark;		/*TRK_RAWMARK*/
int rawLevel;		/*TRK_LEVEL = NRx2/NR32 as last written (-1 = unknown)*/
int rawPeriod;		/*TRK_CURPERIOD*/
int rawNR10;		/*NR10 (CH1 retriggers every frame while it isn't 8)*/
int rawNote;		/*held MIDI note, -1 = none*/
int rawVel;
long rawStart;
long rawEnd;

int multiBanks;
int curBank;

char folderName[100];

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char KonMagicBytes1[3] = { 0x40, 0x2A, 0xEA };
const char KonMagicBytes1b[3] = { 0x1E, 0x2A, 0xEA };
const char KonMagicBytes2[3] = { 0x40, 0x7E, 0xEA };
const char KonMagicBytes3[5] = { 0x16, 0x00, 0x19, 0x5E, 0x23 };
const char KonMagicBytes4[3] = { 0xCF, 0x5D, 0x54 };
const char KonMagicBytes5[3] = { 0xCF, 0x2A, 0xEA };
const char KonMagicBytes6[4] = { 0xCD, 0x81, 0x4C, 0x2A };
const char KonMagicBytes6b[4] = { 0xCD, 0x8B, 0x4C, 0x2A };
const char KonMagicBytes6c[4] = { 0xCD, 0x03, 0x4D, 0x2A };
const char KonMagicBytes7[3] = { 0x61, 0x7E, 0xEA };
const char KonMagicBytes7b[3] = { 0x64, 0x7E, 0xEA };
const char KonMagicBytes7c[3] = { 0x5F, 0x7E, 0xEA };
const char KonMagicBytes8[4] = { 0xEA, 0xCF, 0xC0, 0x21 };
const char KonMagicBytes9[3] = { 0x48, 0x2A, 0xEA };
const char KonMagicBytes10[3] = { 0x55, 0x2A, 0xEA };
const char KonMagicBytes11[3] = { 0xF5, 0xEF, 0xF1 };
const char KonMagicBytes12[3] = { 0xCB, 0x7F, 0x28 };
const char KonMagicBytes13[3] = { 0x49, 0x2A, 0xEA };

long midLength;

/*Loop detection: control state of the stream reader at every note/rest*/
#define MAX_LOOP_STATES 8192
long loopStates[MAX_LOOP_STATES][7];
long loopStateTimes[MAX_LOOP_STATES];
int numLoopStates;

/*Tempo changes collected from the music tracks (written to the control track at the end)*/
#define MAX_TEMPO_EVTS 1024
long tempoTimes[MAX_TEMPO_EVTS];
int tempoVals[MAX_TEMPO_EVTS];
int numTempoEvts;

/*Velocity for GB volume 0-15 (amplitude is linear in the GB volume, MIDI velocity is roughly
  amplitude = (vel/127)^2, so vel = 127 * sqrt(vol/15))*/
const int VolToVel[16] = { 0, 33, 46, 57, 66, 73, 80, 87, 93, 98, 104, 108, 113, 118, 122, 127 };
/*Velocity for CH3 level code: 0 = off, 1 = 100%, 2 = 50%, 3 = 25%*/
const int Ch3LvlToVel[4] = { 0, 127, 90, 64 };
/*Velocity for the music drum track (its notes start drum SFX with their own volumes)*/
#define DRUM_VEL 120

/*Function prototypes*/
double KonMusicRate(void);
double KonTempoRate(int tempoVal);
double KonSfxRate(void);
long KonTempoToMicros(int tempoVal, int ticksPerQN, int sfx);
void KonSortTempoEvts(void);
long KonSfxToMidi(long sfxTime);
long KonTrackToMidi(long trkTime);
int KonGetNoteLen(int nibble, int unit);
int KonNoiseVolAt(long trkTime);
int KonCheckLoopState(long pos, long segno, long loopCnt, long ret, long unit, long oct, long trans, long time);
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int KonWriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int KonWriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int KonWriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int KonWriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
int KonRawPeriodToNote(int period, int curChan);
unsigned int KonFlushRawNote(unsigned char* buffer, unsigned int pos, long* lastTime, int* firstNote, int curChan, int inst);
unsigned int KonRawFrame(unsigned char* buffer, unsigned int pos, int period, int level, long time, long len, long* lastTime, int* firstNote, int curChan, int inst);
void Konsong2mid(int songNum, long songPtr);

unsigned int KonWriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	deltaValue = KonWriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0xC0 | curChan);
		}
		else
		{
			Write8B(&buffer[pos], 0xC9);
		}

		Write8B(&buffer[pos + 1], inst);
		Write8B(&buffer[pos + 2], 0);

		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			Write8B(&buffer[pos + 3], 0x99);
		}

		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	deltaValue = KonWriteDeltaTime(buffer, pos, length);
	pos += deltaValue;

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}

/*Tick rates. Music rate = music tick rate * (1 - tempo/256): the tempo accumulator skips that
  share of music ticks. SFX tracks run every update and are never skipped.
  - CVA: 64 Hz timer, music only on every 2nd tick (32 Hz); the skips land on the music ticks
  - Motocross Maniacs, Nemesis: 64 Hz timer, music and SFX on every tick
  - Belmont's Revenge, Hyper Dunk: VBlank (59.7275 Hz), all tracks every tick
  - Nemesis II: 128 Hz timer, SFX every tick, music in alternating pairs (64 Hz per track)
  - TMNT II: 256 Hz timer, SFX every tick, one music track per tick in turn (64 Hz per track)
  One driver tick = multiplier MIDI ticks.*/
double KonMusicRate(void)
{
	switch (drvVers)
	{
	case KONAMI_VER_CV1:
		return 32.0;
	case KONAMI_VER_CV2:
	case KONAMI_VER_HD:
	case KONAMI_VER_PD:
	case KONAMI_VER_TTA1:
	case KONAMI_VER_KD:
	case KONAMI_VER_TTA2:
	case KONAMI_VER_RF:
	case KONAMI_VER_ZEN:
	case KONAMI_VER_TTA3:
	case KONAMI_VER_CVL:
	case KONAMI_VER_PPGB:
	case KONAMI_VER_ISS99:
	case KONAMI_VER_ISS99J:
		return 59.7275;
	case KONAMI_VER_GOEMON:
	default:
		return 64.0;
	}
}

double KonSfxRate(void)
{
	switch (drvVers)
	{
	case KONAMI_VER_CV2:
	case KONAMI_VER_HD:
	case KONAMI_VER_TTA1:
	case KONAMI_VER_TTA2:
	case KONAMI_VER_RF:
	case KONAMI_VER_ZEN:
	case KONAMI_VER_TTA3:
	case KONAMI_VER_CVL:
	case KONAMI_VER_PPGB:
	case KONAMI_VER_ISS99:
	case KONAMI_VER_ISS99J:
		return 59.7275;
	case KONAMI_VER_NEM2:
		return 128.0;
	case KONAMI_VER_TMNT2:
	case KONAMI_VER_GOEMON:
		return 256.0;
	case KONAMI_VER_CV1:
	case KONAMI_VER_MM:
	case KONAMI_VER_NEM1:
	case KONAMI_VER_BATMAN:
	default:
		return 64.0;
	}
}

/*MIDI tempo (microseconds per quarter note) for a driver TEMPO value (music) or for the plain
  SFX tick rate (sfx = 1; the tempo doesn't apply)*/
  /*Music tick rate for a driver TEMPO value. Normally the tempo carry skips music ticks: rate x (256 - t) / 256.
	Cyborg Kuro-chan / The Mummy (2000 driver, run as ISS99J): $EE $80|n instead takes one extra tick off every
	music track's length every n updates (postponed while a track is about to read), so the music runs n + 1
	ticks per n updates; $EE $80 itself leaves the old tempo (taken as the plain rate here)*/
double KonTempoRate(int tempoVal)
{
	if (drvVers == KONAMI_VER_ISS99J && (tempoVal & 0x80))
	{
		if ((tempoVal & 0x7F) == 0)
		{
			return KonMusicRate();
		}
		return KonMusicRate() * ((tempoVal & 0x7F) + 1) / (double)(tempoVal & 0x7F);
	}
	return KonMusicRate() * (256 - (tempoVal & 0xFF)) / 256.0;
}

long KonTempoToMicros(int tempoVal, int ticksPerQN, int sfx)
{
	double rate;

	if (sfx == 1)
	{
		rate = KonSfxRate();
	}
	else
	{
		rate = KonTempoRate(tempoVal);
	}

	return (long)(((double)ticksPerQN / multiplier) * 1000000.0 / rate + 0.5);
}

/*Sort the collected tempo changes by time (stable, so a later track wins on the same tick, as in the driver)*/
void KonSortTempoEvts(void)
{
	int a, b;
	long tmpTime;
	int tmpVal;

	for (a = 1; a < numTempoEvts; a++)
	{
		tmpTime = tempoTimes[a];
		tmpVal = tempoVals[a];
		b = a - 1;
		while (b >= 0 && tempoTimes[b] > tmpTime)
		{
			tempoTimes[b + 1] = tempoTimes[b];
			tempoVals[b + 1] = tempoVals[b];
			b--;
		}
		tempoTimes[b + 1] = tmpTime;
		tempoVals[b + 1] = tmpVal;
	}
}

/*An SFX track inside a music song runs at the SFX rate, untouched by the tempo, while the MIDI
  file's ticks follow the music. Convert an SFX track time (SFX ticks * multiplier) to MIDI ticks
  through the music's tempo map (music starts at TEMPO 0; the map must be sorted).*/
long KonSfxToMidi(long sfxTime)
{
	double secs;
	double midiTime;
	double tps;
	double segLen;
	int curT;
	int e;

	secs = (double)sfxTime / (multiplier * KonSfxRate());
	midiTime = 0.0;
	curT = 0;

	for (e = 0; e < numTempoEvts; e++)
	{
		tps = multiplier * KonTempoRate(curT);
		segLen = (tempoTimes[e] - midiTime) / tps;
		if (secs <= segLen)
		{
			break;
		}
		secs -= segLen;
		midiTime = tempoTimes[e];
		curT = tempoVals[e] & 0xFF;
	}
	tps = multiplier * KonTempoRate(curT);

	return (long)(midiTime + secs * tps + 0.5);
}

/*Track time -> MIDI time*/
long KonTrackToMidi(long trkTime)
{
	if (trkSFXMixed == 1)
	{
		return KonSfxToMidi(trkTime);
	}
	return trkTime;
}

/*Note/rest length in driver ticks for a length nibble.
  Gen 1: nibble * unit, 8-bit, 0 = 256.
  Gen 2/3: nibble 0 = (unit & 15) * 16; 8-bit, 0 = 256 (the length counter is decremented first)*/
int KonGetNoteLen(int nibble, int unit)
{
	int len;

	if (drvGen == 1 || nibble != 0)
	{
		len = (nibble * unit) & 0xFF;
	}
	else
	{
		len = ((unit & 0x0F) << 4) & 0xFF;
	}

	if (len == 0)
	{
		len = 256;
	}
	return len;
}

/*TMNT II noise SFX: CH4 volume at a time (track time), following the NR42 envelope started at
  the last trigger (envelope clock 64 Hz)*/
int KonNoiseVolAt(long trkTime)
{
	long steps;
	int v;

	if (noiseEnvPace == 0)
	{
		return noiseEnvVol;
	}

	steps = (long)(((double)(trkTime - noiseTrigTime) / multiplier) / (noiseEnvPace * KonSfxRate() / 64.0));
	v = noiseEnvVol;
	if (noiseEnvDir == 0)
	{
		v -= steps;
		if (v < 0)
		{
			v = 0;
		}
	}
	else
	{
		v += steps;
		if (v > 15)
		{
			v = 15;
		}
	}
	return v;
}

/*Returns 1 if the reader was already in this exact state (= the track has looped),
  otherwise remembers the state and returns 0*/
int KonCheckLoopState(long pos, long segno, long loopCnt, long ret, long unit, long oct, long trans, long time)
{
	int s;
	for (s = 0; s < numLoopStates; s++)
	{
		if (loopStates[s][0] == pos && loopStates[s][1] == segno && loopStates[s][2] == loopCnt && loopStates[s][3] == ret
			&& loopStates[s][4] == unit && loopStates[s][5] == oct && loopStates[s][6] == trans)
		{
			/*printf("  loop to 0x%04X after %ld ticks (loop start at tick %ld)\n", pos, time / multiplier, loopStateTimes[s] / multiplier);*/
			return 1;
		}
	}
	if (numLoopStates < MAX_LOOP_STATES)
	{
		loopStates[numLoopStates][0] = pos;
		loopStates[numLoopStates][1] = segno;
		loopStates[numLoopStates][2] = loopCnt;
		loopStates[numLoopStates][3] = ret;
		loopStates[numLoopStates][4] = unit;
		loopStates[numLoopStates][5] = oct;
		loopStates[numLoopStates][6] = trans;
		loopStateTimes[numLoopStates] = time;
		numLoopStates++;
	}
	return 0;
}

int KonWriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value)
{
	unsigned char valSize;
	unsigned char* valData;
	unsigned int tempLen;
	unsigned int curPos;

	valSize = 0;
	tempLen = value;

	while (tempLen != 0)
	{
		tempLen >>= 7;
		valSize++;
	}

	valData = &buffer[pos];
	curPos = valSize;
	tempLen = value;

	while (tempLen != 0)
	{
		curPos--;
		valData[curPos] = 128 | (tempLen & 127);
		tempLen >>= 7;
	}

	valData[valSize - 1] &= 127;

	pos += valSize;

	if (value == 0)
	{
		valSize = 1;
	}
	return valSize;
}

unsigned int KonWriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	int midiBank;
	int midiProg;
	int bankChanged;
	unsigned char bankStatus;
	midiBank = inst / 128;
	midiProg = inst % 128;
	bankChanged = (midiBank != progBank);
	deltaValue = KonWriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1 || bankChanged == 1)
	{
		if (bankChanged == 1)
		{
			if (curChan != 3)
			{
				bankStatus = 0xB0 | curChan;
			}
			else
			{
				bankStatus = 0xB9;
			}

			/*Bank Select MSB (CC0)*/
			Write8B(&buffer[pos], bankStatus);
			Write8B(&buffer[pos + 1], 0x00);
			Write8B(&buffer[pos + 2], midiBank);
			pos += 3;
			/*Delta 0*/
			Write8B(&buffer[pos], 0);
			pos++;

			/*Bank Select LSB (CC32)*/
			Write8B(&buffer[pos], bankStatus);
			Write8B(&buffer[pos + 1], 0x20);
			Write8B(&buffer[pos + 2], 0x00);
			pos += 3;
			/*Delta 0*/
			Write8B(&buffer[pos], 0);
			pos++;

			progBank = midiBank;
		}
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0xC0 | curChan);
		}
		else
		{
			Write8B(&buffer[pos], 0xC9);
		}


		Write8B(&buffer[pos + 1], midiProg);
		Write8B(&buffer[pos + 2], 0);

		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			Write8B(&buffer[pos + 3], 0x99);
		}


		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	return pos;

}

unsigned int KonWriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;

	deltaValue = KonWriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			Write8B(&buffer[pos + 3], 0x99);
		}
		pos++;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}

/*Goemon raw frames: MIDI note for a period (CH1/CH2: 131072 / (2048 - p) Hz, CH3 half of that;
  this puts period table entry 0 on note 36 for CH1/CH2 and 24 for CH3, as the normal notes)*/
int KonRawPeriodToNote(int period, int curChan)
{
	double freq;
	int n;

	freq = 131072.0 / (2048 - (period & 0x7FF));
	if (curChan == 2)
	{
		freq /= 2.0;
	}
	n = (int)floor(69.0 + 12.0 * log(freq / 440.0) / log(2.0) + 0.5);
	if (n < 0)
	{
		n = 0;
	}
	if (n > 127)
	{
		n = 127;
	}
	return n;
}

/*Goemon raw frames: write the held note (if any)*/
unsigned int KonFlushRawNote(unsigned char* buffer, unsigned int pos, long* lastTime, int* firstNote, int curChan, int inst)
{
	long nStart;
	long nEnd;

	if (rawNote < 0)
	{
		return pos;
	}
	nStart = KonTrackToMidi(rawStart);
	nEnd = KonTrackToMidi(rawEnd);
	if (nStart < *lastTime)
	{
		nStart = *lastTime;
	}
	if (nEnd <= nStart)
	{
		nEnd = nStart + 1;
	}
	curVol = rawVel;
	pos = KonWriteNoteEvent(buffer, pos, rawNote, nEnd - nStart, nStart - *lastTime, *firstNote, curChan, inst);
	*firstNote = 0;
	*lastTime = nEnd;
	rawNote = -1;
	return pos;
}

/*Goemon raw frames: one frame (or one slide unit) of len ticks at a track time. As the driver:
  WritePeriod changes the period without a retrigger, WriteVolume retriggers when NRx2 changes
  (CH1 also on every frame while NR10 isn't 8), CH3 never retriggers. The held note goes on while
  the MIDI note and velocity stay the same and nothing retriggers.*/
unsigned int KonRawFrame(unsigned char* buffer, unsigned int pos, int period, int level, long time, long len, long* lastTime, int* firstNote, int curChan, int inst)
{
	int note;
	int vel;
	int retrig;

	level &= 0xFF;
	retrig = 0;
	if (curChan != 2 && (level != rawLevel || (curChan == 0 && (rawNR10 & 0x7F) != 0x08)))
	{
		retrig = 1;
	}
	rawLevel = level;
	rawPeriod = period & 0x7FF;

	if (curChan == 2)
	{
		/*NR32 bits 6-5: 0 mute, 1 100%, 2 50%, 3 25%*/
		vel = Ch3LvlToVel[(level >> 5) & 3];
	}
	else
	{
		/*NRx2 = vol << 4 (no envelope), vol 0 = DAC off*/
		vel = VolToVel[(level >> 4) & 0x0F];
	}
	note = KonRawPeriodToNote(rawPeriod, curChan);

	if (vel == 0)
	{
		return KonFlushRawNote(buffer, pos, lastTime, firstNote, curChan, inst);
	}
	if (rawNote >= 0 && (retrig == 1 || note != rawNote || vel != rawVel || rawEnd != time))
	{
		pos = KonFlushRawNote(buffer, pos, lastTime, firstNote, curChan, inst);
	}
	if (rawNote < 0)
	{
		rawNote = note;
		rawVel = vel;
		rawStart = time;
	}
	rawEnd = time + len;
	return pos;
}

void KonProc(int bank, char parameters[4][100])
{
	drvVers = KONAMI_VER_CV1;
	curVol = 120;
	curInst = 0;
	foundTable = 0;
	verOverride = 0;
	progBank = 0;

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	if (parameters[0][0] != 0)
	{
		verOverride = 1;
		drvVers = strtol(parameters[0], NULL, 16);
		if (drvVers < KONAMI_VER_CV1 || drvVers > KONAMI_VER_ISS99J)
		{
			fclose(rom);
			printf("ERROR: Invalid version number!\n");
			exit(-1);
		}

	}

	bankAmt = bankSize;
	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);

	/*Try to search the bank for song pattern table loader - Method 1: Castlevania 1/2*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes1, 3) && foundTable != 1)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 2: Nemesis*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes2, 3) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 3: Motocross Maniacs*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes3, 5) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 4: NFL Football*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes2, 3) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 5: Skate or Die*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes5, 3) && foundTable != 1 && ReadLE16(&romData[i - 4]) < (bankSize * 2))
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 6: Konami Golf (E)*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes6, 4) && foundTable != 1 && ReadLE16(&romData[i - 4]) < (bankSize * 2))
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 6b: Konami Golf (J)*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes6b, 4) && foundTable != 1 && ReadLE16(&romData[i - 4]) < (bankSize * 2))
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 6c: Ultra Golf (U)*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes6c, 4) && foundTable != 1 && ReadLE16(&romData[i - 4]) < (bankSize * 2))
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 7: TMNT2*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if (!memcmp(&romData[i], KonMagicBytes8, 4) && foundTable != 1)
		{
			tablePtrLoc = i + 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Method 8: Batman*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes9, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Method 9: Tiny Toon Adventures 2*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes10, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Method 10: TTA Wacky Sports*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes13, 3)) && foundTable != 1 && ReadLE16(&romData[i - 4]) < bankSize * 2)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Try to search the bank for song table loader - Block Game (Collection 2)*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes1b, 3)) && foundTable != 1 && bank == 0x13)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Multi-banked method 1: Nemesis 2*/
	if (ReadLE16(&romData[0x4000]) >= bankSize && ReadLE16(&romData[0x4000]) < (bankSize * 2) && foundTable != 1)
	{
		tableOffset = 0x4000;
		printf("Song table starts at 0x%04x...\n", tableOffset);
		foundTable = 1;
	}

	/*Multi-banked method 1b: Goemon 1*/
	if (ReadLE16(&romData[0x7000]) > bankAmt && ReadLE16(&romData[0x7000]) < (bankSize * 2) && foundTable != 1 && drvVers != KONAMI_VER_PD)
	{
		tableOffset = 0x7000;
		printf("Song table starts at 0x%04x...\n", tableOffset);
		foundTable = 1;
	}

	/*Multi-banked method 2: Parodius*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes7, 3)) && foundTable != 1)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;

		}
	}

	/*Multi-banked method 2b: Variation of above for Parodius Da! (J)*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes7b, 3)) && foundTable != 1)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;
		}
	}

	/*Multi-banked method 2c: Variation of above for Konami GB Collection Vol. 2*/
	for (i = 0; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes7c, 3)) && foundTable != 1 && drvVers == KONAMI_VER_PD)
		{
			tablePtrLoc = i - 4;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;

		}
	}

	/*Multi-banked method 3: Kid Dracula*/
	for (i = bankSize; i < (bankSize * 2); i++)
	{
		if ((!memcmp(&romData[i], KonMagicBytes11, 3)) && foundTable != 1)
		{
			tablePtrLoc = i - 2;
			printf("Found pointer to song table at address 0x%04X!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc]);
			printf("Song table starts at 0x%04X...\n", tableOffset);
			foundTable = 1;

		}
	}



	if (foundTable == 1)
	{
		i = tableOffset;
		songNum = 1;

		if (ReadLE16(&romData[i]) >= (bankSize * 2) || ReadLE16(&romData[i]) <= bankSize || drvVers == KONAMI_VER_NEM1)
		{
			i += 2;
		}

		/*World Soccer GB2: ids $0D-$0F of its single table are null words*/
		while ((ReadLE16(&romData[i]) >= bankAmt && ReadLE16(&romData[i]) < 0x8000) || (drvVers == KONAMI_VER_ISS99J && ReadLE16(&romData[i]) == 0x0000))
		{
			songPtr = ReadLE16(&romData[i]);
			if (songPtr == 0x4000 || songPtr == 0x4002)
			{
				i += 2;
			}
			if (tableOffset == 0x4000 && songPtr > 0x7000)
			{
				break;
			}

			if (songPtr == 0x7840)
			{
				break;
			}
			if (songPtr != 0x0000)
			{
				printf("Song %i: 0x%04X\n", songNum, songPtr);
				if (drvVers == KONAMI_VER_PD || drvVers == KONAMI_VER_KD)
				{
					songBank = romData[songPtr + 1];
				}
				else if (drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J)
				{
					if (bank == 0x1F || bank == 0x20)
					{
						/*ISS '99 / World Soccer GB2: the streams are in bank $1E + the high nibble of the
						  header's first (priority) byte, whatever bank the header is in*/
						songBank = 0x1E + (romData[songPtr] >> 4);
					}
					else if (bank == 0x1E)
					{
						/*Pocket GI Stable*/
						songBank = 0x1D + (romData[songPtr] >> 4);
					}
					else if (bank == 0x0F)
					{
						/*International Track & Field*/
						songBank = 0x0E + (romData[songPtr] >> 4);
					}
					else if (bank == 0x0E)
					{
						/*Azure Dreams*/
						songBank = 0x0D + (romData[songPtr] >> 4);
					}
					else if (bank == 0x3E)
					{
						/*Cross Country Racing*/
						songBank = 0x3D + (romData[songPtr] >> 4);
					}
					else if (bank == 0x0D)
					{
						/*Cyborg Kuro-chan: Devil Fukkatsu*/
						songBank = 0x0C + (romData[songPtr] >> 4);
					}
					else if (bank == 0x7C)
					{
						/*Battle Fishers*/
						songBank = 0x7B + (romData[songPtr] >> 4);
					}

				}
				else
				{
					songBank = bank - 1;
				}
				bankAmt = bankSize;
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, (songBank * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				Konsong2mid(songNum, songPtr);
				free(exRomData);

			}
			else
			{
				printf("Song %i: 0x%04X (empty, skipped)\n", songNum, songPtr);
			}
			i += 2;
			songNum++;
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
void Konsong2mid(int songNum, long songPtr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int activeChan[4];
	int maskArray[4];
	unsigned char mask = 0;
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int trackEnd = 0;
	int noteTrans = 0;
	int ticks = 120;
	int k = 0;
	int macros[8][2];
	int repeats[8][2];
	int inMacro;
	int curSpeed;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	long tempo = 0;

	int chanSpeed = 1;

	int curInst = 0;

	unsigned long seqPos = 0;

	unsigned char command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	signed int transpose = 0;
	signed int octaveShift = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;
	int octave = 0;
	int noteLenAdd = 0;
	int volVal = 0;
	int rest = 0;
	int attenuate = 0;
	long initTempoPos = 0;
	long cmdCount = 0;
	int curTempoVal = 0;
	long lastTempoTime = 0;
	long tmpTime = 0;
	int tmpVal = 0;
	int lvl = 0;
	int ch3Code = 0;
	int repeat1Set = 0;
	int repeat2Set = 0;
	int songLoop = 0;
	int jumpCnt = 0;
	int noteVal = 0;
	int rawMode = 0;

	/*Gen 2+ header: which header pointer feeds each MIDI track, and whether it is an SFX track*/
	long hdrPtrs[7];
	int srcBit[4];
	int srcSFX[4];
	int sfx2Chan;
	int trkOrder[4];
	int ordIdx;
	int sortedTempo = 0;
	unsigned char* trkData[4];
	unsigned int trkLen[4];
	long lastEvtTime = 0;
	long noteStart = 0;
	long noteEnd = 0;
	int nr43 = 0;
	int volSet = 0;
	int pass = 0;
	int numPasses = 1;
	long loopTarget = 0;
	int trkLooped = 0;
	long segnoPos = 0;

	/*Fourth generation (Tiny Toon 2 on) state*/
	int sfxHalf = 0;		/*packed tone SFX: which nibble comes next (see the SFX note code)*/
	int nib = 0;			/*packed tone SFX: the nibble being played (-1 = none)*/
	int vmMode = 0;			/*Castlevania Legends note-volume mode ($E0 u|$80): lo = volume, one unit*/
	int noiseLast = 0;		/*noise SFX: NR42 as the driver keeps it (TRK_LASTVOL)*/
	int noisePend = 0;		/*noise SFX: retrigger at the next NR43 byte*/
	int noiseAlways = 0;	/*noise SFX (Castlevania Legends, $E0 u|$80): retrigger on every byte*/
	int brkCnt[2];			/*Castlevania Legends $F9 loop break: passes left for loops 1 and 2*/
	long brkExit[2];		/*...and where the break goes*/
	int eeOn = 0;			/*Castlevania Legends note-volume $EE repeat: mark set, mark, pass counter*/
	long eeMark = 0;
	int eeCnt = 0;
	int rampMode = 0;		/*Batman $EA Fx volume ramp: 0 off, 1 louder, 2 quieter; notes per step; countdown*/
	int rampStep = 0;
	int rampCnt = 0;
	int skip = 0;

	midPos = 0;
	ctrlMidPos = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;

	if (multiBanks != 0)
	{
		snprintf(folderName, sizeof(folderName), "Bank %i", (curBank + 1));
		_mkdir(folderName);
	}

	midLength = 0x10000;

	/*One buffer per track: music tracks are converted before SFX tracks (an SFX track in a music
	  song needs the music's tempo map), and the tracks are written in channel order at the end*/
	for (k = 0; k < 4; k++)
	{
		trkData[k] = (unsigned char*)calloc(midLength, 1);
		trkLen[k] = 0;
	}
	midData = trkData[0];

	ctrlMidData = (unsigned char*)calloc(midLength, 1);

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

		/*Placeholder; the real initial tempo is written once we know whether this is music or SFX*/
		tempo = 150;
		numTempoEvts = 0;

		/*Write initial MIDI information for "control" track*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D54726B);
		ctrlMidPos += 8;
		ctrlMidTrackBase = ctrlMidPos;

		/*Set channel name (blank)*/
		KonWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE16(&ctrlMidData[ctrlMidPos], 0xFF03);
		Write8B(&ctrlMidData[ctrlMidPos + 2], 0);
		ctrlMidPos += 2;

		/*Set initial tempo*/
		KonWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		initTempoPos = ctrlMidPos;
		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
		ctrlMidPos += 3;

		/*Set time signature*/
		KonWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5804);
		ctrlMidPos += 3;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x04021808);
		ctrlMidPos += 4;

		/*Set key signature*/
		KonWriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5902);
		ctrlMidPos += 4;

		switch (drvVers)
		{
		case KONAMI_VER_CV1:
		case KONAMI_VER_NEM1:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_NOP1;
			EventMap[0xEC] = KONAMI_EVENT_NOP1;
			EventMap[0xED] = KONAMI_EVENT_NOP1;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_PAN;
			for (j = 0xF0; j <= 0xFA; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xFA;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT_START;
			EventMap[0xFC] = KONAMI_EVENT_RETURN;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_REPEAT_END;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x00;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x10;
			KONAMI_SUB_COMMAND_VIBRATO = 0x20;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			break;
		case KONAMI_VER_MM:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_NOP1;
			EventMap[0xEC] = KONAMI_EVENT_NOP1;
			EventMap[0xED] = KONAMI_EVENT_NOP1;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_PAN;
			for (j = 0xF0; j <= 0xFA; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xFA;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT_START;
			EventMap[0xFC] = KONAMI_EVENT_RETURN;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_REPEAT_END;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x00;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x10;
			KONAMI_SUB_COMMAND_VIBRATO = 0x20;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x30;
			KONAMI_SUB_COMMAND_NOP = 0x40;
			break;
		case KONAMI_VER_CV2:
		case KONAMI_VER_PD:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_NOP1;
			EventMap[0xEC] = KONAMI_EVENT_NOP1;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_PAN;
			for (j = 0xF0; j <= 0xFA; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xFA;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT_START;
			EventMap[0xFC] = KONAMI_EVENT_RETURN;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_REPEAT_END;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x00;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x10;
			KONAMI_SUB_COMMAND_VIBRATO = 0x20;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			break;
		case KONAMI_VER_NEM2:
		case KONAMI_VER_HD:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_NOP1;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_PAN;
			for (j = 0xF0; j <= 0xFA; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xFA;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x00;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x10;
			KONAMI_SUB_COMMAND_NOP = 0x20;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			break;
		case KONAMI_VER_TMNT2:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_NOP1;
			EventMap[0xED] = KONAMI_EVENT_ED;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_TUNING;
			for (j = 0xF0; j <= 0xF7; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xF7;
			EventMap[0xF8] = KONAMI_EVENT_VOL1;
			EventMap[0xF9] = KONAMI_EVENT_VOL2;
			EventMap[0xFA] = KONAMI_EVENT_JUMP;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x00;
			KONAMI_SUB_COMMAND_PAN_LR = 0x10;
			KONAMI_SUB_COMMAND_PAN_R = 0x20;
			KONAMI_SUB_COMMAND_PAN_L = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			KONAMI_SUB_COMMAND_AUTOPAN2 = 0x50;
			KONAMI_SUB_COMMAND_AUTOPAN_OFF = 0x60;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x70;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x80;
			break;
		case KONAMI_VER_GOEMON:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_VOL_ENV;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_TUNING;
			for (j = 0xF0; j <= 0xF7; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xF7;
			EventMap[0xF8] = KONAMI_EVENT_VOL1;
			EventMap[0xF9] = KONAMI_EVENT_COND_JUMP;
			EventMap[0xFA] = KONAMI_EVENT_JUMP;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x00;
			KONAMI_SUB_COMMAND_PAN_LR = 0x10;
			KONAMI_SUB_COMMAND_PAN_R = 0x20;
			KONAMI_SUB_COMMAND_PAN_L = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			KONAMI_SUB_COMMAND_AUTOPAN2 = 0x50;
			KONAMI_SUB_COMMAND_AUTOPAN_OFF = 0x60;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x70;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x80;
			break;
		case KONAMI_VER_TTA1:
		case KONAMI_VER_KD:
		case KONAMI_VER_ZEN:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xCF;
			for (j = 0xD0; j <= 0xD8; j++)
			{
				EventMap[j] = KONAMI_EVENT_NOP1;
			}
			EventMap[0xD9] = KONAMI_EVENT_SWEEP;
			EventMap[0xDA] = KONAMI_EVENT_SUB_COMMAND;
			for (j = 0xDB; j <= 0xDE; j++)
			{
				EventMap[j] = KONAMI_EVENT_NOP1;
			}
			EventMap[0xDF] = KONAMI_EVENT_PAN;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEA] = KONAMI_EVENT_OCTAVE_SHIFT;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_VOL_ENV;
			EventMap[0xED] = KONAMI_EVENT_TUNING;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_NOP1;
			for (j = 0xF0; j <= 0xFA; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xFA;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT_START;
			EventMap[0xFC] = KONAMI_EVENT_RETURN;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_REPEAT_END;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x00;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x10;
			KONAMI_SUB_COMMAND_NOP = 0x20;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			break;
		case KONAMI_VER_TTA2:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_WAVE_ALT;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_TUNING;
			for (j = 0xF0; j <= 0xF8; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xF8;
			EventMap[0xF9] = KONAMI_EVENT_COND_JUMP;
			EventMap[0xFA] = KONAMI_EVENT_JUMP;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x00;
			KONAMI_SUB_COMMAND_PAN_LR = 0x10;
			KONAMI_SUB_COMMAND_PAN_R = 0x20;
			KONAMI_SUB_COMMAND_PAN_L = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			KONAMI_SUB_COMMAND_AUTOPAN2 = 0x50;
			KONAMI_SUB_COMMAND_NOP = 0x60;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x70;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x80;
			KONAMI_SUB_COMMAND_RELEASE_VOL = 0x90;
			break;
		case KONAMI_VER_BATMAN:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_GLOBAL_VOL;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_TUNING;
			for (j = 0xF0; j <= 0xF7; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xF7;
			EventMap[0xF8] = KONAMI_EVENT_VOL;
			EventMap[0xF9] = KONAMI_EVENT_COND_JUMP;
			EventMap[0xFA] = KONAMI_EVENT_JUMP;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x00;
			KONAMI_SUB_COMMAND_PAN_LR = 0x10;
			KONAMI_SUB_COMMAND_PAN_R = 0x20;
			KONAMI_SUB_COMMAND_PAN_L = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			KONAMI_SUB_COMMAND_AUTOPAN2 = 0x50;
			KONAMI_SUB_COMMAND_NOP = 0x60;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x70;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x80;
			KONAMI_SUB_COMMAND_RELEASE_VOL = 0x90;
			KONAMI_SUB_COMMAND_CLEAR = 0xA0;
			KONAMI_SUB_COMMAND_TRANSPOSE = 0xD0;
			KONAMI_SUB_COMMAND_TEMPO = 0xE0;
			KONAMI_SUB_COMMAND_VOL_RAMP = 0xF0;
			break;
		case KONAMI_VER_RF:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_FADE;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_TUNING;
			for (j = 0xF0; j <= 0xF7; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xF7;
			EventMap[0xF8] = KONAMI_EVENT_VOL;
			EventMap[0xF9] = KONAMI_EVENT_COND_JUMP;
			EventMap[0xFA] = KONAMI_EVENT_JUMP;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x00;
			KONAMI_SUB_COMMAND_PAN_LR = 0x10;
			KONAMI_SUB_COMMAND_PAN_R = 0x20;
			KONAMI_SUB_COMMAND_PAN_L = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			KONAMI_SUB_COMMAND_AUTOPAN2 = 0x50;
			KONAMI_SUB_COMMAND_NOP = 0x60;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x70;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x80;
			KONAMI_SUB_COMMAND_RELEASE_VOL = 0x90;
			KONAMI_SUB_COMMAND_CLEAR = 0xA0;
			break;
		case KONAMI_VER_TTA3:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_GLOBAL_VOL;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_TUNING;
			for (j = 0xF0; j <= 0xF7; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xF7;
			EventMap[0xF8] = KONAMI_EVENT_VOL;
			EventMap[0xF9] = KONAMI_EVENT_COND_JUMP;
			EventMap[0xFA] = KONAMI_EVENT_JUMP;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x00;
			KONAMI_SUB_COMMAND_PAN_LR = 0x10;
			KONAMI_SUB_COMMAND_PAN_R = 0x20;
			KONAMI_SUB_COMMAND_PAN_L = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			KONAMI_SUB_COMMAND_AUTOPAN2 = 0x50;
			KONAMI_SUB_COMMAND_GLOBAL_VOL = 0x60;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x70;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x80;
			KONAMI_SUB_COMMAND_NOP = 0x90;
			break;
		case KONAMI_VER_CVL:
		case KONAMI_VER_PPGB:
		case KONAMI_VER_ISS99:
		case KONAMI_VER_ISS99J:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_VIBRATO;
			EventMap[0xEC] = KONAMI_EVENT_VOL_ENV;
			EventMap[0xED] = KONAMI_EVENT_DUTY_STEP;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_TUNING;
			/*$F8 is one more attenuation step here (Batman/Raging Fighter: volume)*/
			for (j = 0xF0; j <= 0xF8; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xF8;
			EventMap[0xF9] = KONAMI_EVENT_BREAK;
			EventMap[0xFA] = KONAMI_EVENT_JUMP;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT1;
			EventMap[0xFC] = KONAMI_EVENT_REPEAT2;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_SONG_LOOP;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x00;
			KONAMI_SUB_COMMAND_PAN_LR = 0x10;
			KONAMI_SUB_COMMAND_PAN_R = 0x20;
			KONAMI_SUB_COMMAND_PAN_L = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			KONAMI_SUB_COMMAND_AUTOPAN2 = 0x50;
			KONAMI_SUB_COMMAND_NOP = 0x60;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x70;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x80;
			KONAMI_SUB_COMMAND_RELEASE_VOL = 0x90;
			KONAMI_SUB_COMMAND_WOBBLE = 0xA0;
			KONAMI_SUB_COMMAND_HOLD = 0xB0;
			KONAMI_SUB_COMMAND_PANFLAGS = 0xC0;
			break;
		default:
			KONAMI_STATUS_REST_MIN = 0x00;
			KONAMI_STATUS_REST_MAX = 0x0F;
			KONAMI_STATUS_NOTE_MIN = 0x10;
			KONAMI_STATUS_NOTE_MAX = 0xDF;
			EventMap[0xE0] = KONAMI_EVENT_INS_PARAMS;
			for (j = 0xE1; j <= 0xE6; j++)
			{
				EventMap[j] = KONAMI_EVENT_OCTAVE;
			}
			EventMap[0xE7] = KONAMI_EVENT_NOTE_LEN;
			EventMap[0xE8] = KONAMI_EVENT_RELEASE;
			EventMap[0xE9] = KONAMI_EVENT_SWEEP;
			EventMap[0xEA] = KONAMI_EVENT_SUB_COMMAND;
			EventMap[0xEB] = KONAMI_EVENT_NOP1;
			EventMap[0xEC] = KONAMI_EVENT_NOP1;
			EventMap[0xED] = KONAMI_EVENT_NOP1;
			EventMap[0xEE] = KONAMI_EVENT_TEMPO;
			EventMap[0xEF] = KONAMI_EVENT_PAN;
			for (j = 0xF0; j <= 0xFA; j++)
			{
				EventMap[j] = KONAMI_EVENT_ATTENUATION;
			}
			KONAMI_STATUS_ATTEN_MIN = 0xF0;
			KONAMI_STATUS_ATTEN_MAX = 0xFA;
			EventMap[0xFB] = KONAMI_EVENT_REPEAT_START;
			EventMap[0xFC] = KONAMI_EVENT_RETURN;
			EventMap[0xFD] = KONAMI_EVENT_CALL;
			EventMap[0xFE] = KONAMI_EVENT_REPEAT_END;
			EventMap[0xFF] = KONAMI_EVENT_STOP;
			KONAMI_SUB_COMMAND_TRANSPOSE_UP = 0x00;
			KONAMI_SUB_COMMAND_TRANSPOSE_DOWN = 0x10;
			KONAMI_SUB_COMMAND_VIBRATO = 0x20;
			KONAMI_SUB_COMMAND_DUTY_WAVE = 0x30;
			KONAMI_SUB_COMMAND_AUTOPAN = 0x40;
			break;
		}

		switch (drvVers)
		{
		case KONAMI_VER_CV1:
		case KONAMI_VER_MM:
		case KONAMI_VER_NEM1:
		case KONAMI_VER_CV2:
		case KONAMI_VER_PD:
		case KONAMI_VER_TTA1:
		case KONAMI_VER_KD:
		case KONAMI_VER_ZEN:
		default:
			drvGen = 1;
			break;
		case KONAMI_VER_NEM2:
		case KONAMI_VER_HD:
			drvGen = 2;
			break;
		case KONAMI_VER_TMNT2:
		case KONAMI_VER_GOEMON:
			drvGen = 3;
			break;
		case KONAMI_VER_TTA2:
		case KONAMI_VER_BATMAN:
		case KONAMI_VER_RF:
		case KONAMI_VER_TTA3:
		case KONAMI_VER_CVL:
		case KONAMI_VER_PPGB:
		case KONAMI_VER_ISS99:
		case KONAMI_VER_ISS99J:
			drvGen = 4;
			break;
		}

		/*Try to get active channels for sound effects*/
		romPos = songPtr;
		/*Byte 1 is priority*/
		romPos++;

		if (drvVers == KONAMI_VER_PD || drvVers == KONAMI_VER_KD)
		{
			romPos++;
		}
		mask = romData[romPos];
		if (drvVers == KONAMI_VER_CV1)
		{
			for (k = 0; k < 4; k++)
			{
				maskArray[k] = 0;
			}


			maskArray[3] = mask >> 1 & 1;
			maskArray[0] = mask & 1;
			isSFX = 1;

			if (maskArray[0] == 0 && maskArray[3] == 0)
			{
				maskArray[3] = mask >> 5 & 1;
				maskArray[2] = mask >> 4 & 1;
				maskArray[1] = mask >> 3 & 1;
				maskArray[0] = mask >> 2 & 1;
				isSFX = 0;
			}

			else
			{
				;
			}
			romPos++;
		}

		else if (drvVers == KONAMI_VER_MM)
		{
			for (k = 0; k < 4; k++)
			{
				maskArray[k] = 0;
			}


			/*Try to get active channels for sound effects*/
			maskArray[3] = mask >> 2 & 1;
			maskArray[0] = mask >> 1 & 1;
			isSFX = 1;

			if (maskArray[0] == 0 && maskArray[3] == 0)
			{
				maskArray[3] = mask >> 6 & 1;
				maskArray[1] = mask >> 5 & 1;
				maskArray[2] = mask >> 4 & 1;
				maskArray[0] = mask >> 3 & 1;
				isSFX = 0;
			}
			romPos++;
		}

		else
		{
			/*Mask: bit 0 = Sfx1 (CH1), bit 1 = Sfx2 (CH2; CH3 in TMNT II), bit 2 = SfxN (CH4),
			  bits 3-6 = Ch1, Ch2, Ch3, Drm; one header pointer per set bit, in bit order.
			  Many songs also start SFX tracks (a noise part on SfxN, or all seven tracks in some
			  jingles). The music wins: each MIDI track takes the music track of its channel, and
			  an SFX track only fills a channel the music leaves free.*/
			romPos++;
			for (k = 0; k < 7; k++)
			{
				hdrPtrs[k] = 0x0000;
				if ((mask >> k) & 1)
				{
					hdrPtrs[k] = ReadLE16(&romData[romPos]);
					romPos += 2;
				}
			}

			/*A stream that is only $FF just stops that track (TMNT II's $64/$73 start four empty
			  music tracks to cut the music; their sound is on the SFX tracks): ignore it*/
			for (k = 0; k < 7; k++)
			{
				if (((mask >> k) & 1) && hdrPtrs[k] >= bankSize && hdrPtrs[k] < (bankSize * 2) && exRomData[hdrPtrs[k]] == 0xFF)
				{
					mask &= ~(1 << k);
				}
			}

			/*Sfx2 plays on CH3 from TMNT II on (Tiny Toon 2, Batman, Raging Fighter and Castlevania
			  Legends too); in the Tiny Toon/Zen line it is CH2*/
			if (drvVers == KONAMI_VER_TMNT2 || drvVers == KONAMI_VER_GOEMON || drvGen >= 4)
			{
				sfx2Chan = 2;
			}
			else
			{
				sfx2Chan = 1;
			}

			for (k = 0; k < 4; k++)
			{
				srcBit[k] = -1;
				srcSFX[k] = 0;
			}

			if ((mask & 0x78) != 0)
			{
				isSFX = 0;
				for (k = 0; k < 4; k++)
				{
					if ((mask >> (k + 3)) & 1)
					{
						srcBit[k] = k + 3;
					}
				}
			}
			else
			{
				isSFX = 1;
			}

			/*SFX tracks on free channels*/
			if (srcBit[0] < 0 && (mask & 0x01))
			{
				srcBit[0] = 0;
				srcSFX[0] = 1;
			}
			if (srcBit[sfx2Chan] < 0 && (mask & 0x02))
			{
				srcBit[sfx2Chan] = 1;
				srcSFX[sfx2Chan] = 1;
			}
			if (srcBit[3] < 0 && (mask & 0x04))
			{
				srcBit[3] = 2;
				srcSFX[3] = 1;
			}

			for (k = 0; k < 4; k++)
			{
				maskArray[k] = (srcBit[k] >= 0);
				if (srcBit[k] >= 0)
				{
					seqPtrs[k] = hdrPtrs[srcBit[k]];
				}
				else
				{
					seqPtrs[k] = 0x0000;
				}
			}
		}

		/*CV1/MM: every track is music or every track is SFX, pointers in track order*/
		if (drvVers == KONAMI_VER_CV1 || drvVers == KONAMI_VER_MM)
		{
			for (curTrack = 0; curTrack < 4; curTrack++)
			{
				srcSFX[curTrack] = isSFX;
				if (maskArray[curTrack] == 0)
				{
					seqPtrs[curTrack] = 0x0000;
				}
				else
				{
					seqPtrs[curTrack] = ReadLE16(&romData[romPos]);
					romPos += 2;
				}
			}
		}

		/*Initial tempo: TEMPO 0 (music), or the plain SFX tick rate*/
		WriteBE24(&ctrlMidData[initTempoPos], KonTempoToMicros(0, ticks, isSFX));
		curTempoVal = 0;

		/*Music tracks first, then SFX tracks*/
		ordIdx = 0;
		for (k = 0; k < 4; k++)
		{
			if (srcSFX[k] == 0)
			{
				trkOrder[ordIdx++] = k;
			}
		}
		for (k = 0; k < 4; k++)
		{
			if (srcSFX[k] != 0)
			{
				trkOrder[ordIdx++] = k;
			}
		}

		/*Gen 2+: every track ends at its own $FE jump, so a short loop (often the drums) would
		  play only once against the longer ones. Pass 0 measures where each looping track's first
		  pass ends; pass 1 converts for real and keeps looping each track until the longest one.*/
		if (drvGen >= 2)
		{
			numPasses = 2;
		}
		else
		{
			numPasses = 1;
		}
		loopTarget = 0;

		for (pass = 0; pass < numPasses; pass++)
		{
			numTempoEvts = 0;
			sortedTempo = 0;

			for (ordIdx = 0; ordIdx < 4; ordIdx++)
			{
				curTrack = trkOrder[ordIdx];
				trkSFX = srcSFX[curTrack];
				trkSFXMixed = (trkSFX == 1 && isSFX == 0);
				if (trkSFXMixed == 1 && sortedTempo == 0)
				{
					KonSortTempoEvts();
					sortedTempo = 1;
				}
				midData = trkData[curTrack];
				memset(midData, 0, midLength);
				midPos = 0;
				trkLooped = 0;
				segnoPos = 0;
				lastEvtTime = 0;
				noiseHold = 0;
				noiseTrigTime = 0;
				noiseEnvVol = 0;
				noiseEnvDir = 0;
				noiseEnvPace = 0;

				firstNote = 1;
				octave = 1;
				chanSpeed = 1;
				/*Write MIDI chunk header with "MTrk"*/
				WriteBE32(&midData[midPos], 0x4D54726B);
				midPos += 8;
				midTrackBase = midPos;

				curDelay = 0;
				ctrlDelay = 0;
				masterDelay = 0;
				trackEnd = 0;

				curNote = 0;
				curNoteLen = 0;
				ctrlDelay = 0;
				masterDelay = 0;
				holdNote = 0;
				curVol = 120;
				noteLenAdd = 0;
				inMacro = 0;
				volVal = 0xF0;
				volSet = 0;
				transpose = 0;
				octaveShift = 0;
				curSpeed = 1;
				rest = 0;
				attenuate = 0;
				repeat1Set = 0;
				repeat2Set = 0;
				songLoop = 0;
				jumpCnt = 0;
				noteVal = 0;
				rawMode = 0;
				rawSlide = 0;
				rawSlideCnt = 0;
				rawSlideVol = 0;
				rawSlideDelta = 0;
				rawVolOfs = 0;
				rawPerOfs = 0;
				rawMark = 0;
				rawLevel = -1;
				rawPeriod = 0;
				rawNR10 = 0x08;
				rawNote = -1;
				rawVel = 0;
				rawStart = 0;
				rawEnd = 0;
				sfxHalf = 0;
				vmMode = 0;
				noiseLast = 0;
				noisePend = 0;
				noiseAlways = 0;
				brkCnt[0] = 0;
				brkCnt[1] = 0;
				brkExit[0] = 0;
				brkExit[1] = 0;
				eeOn = 0;
				eeMark = 0;
				eeCnt = 0;
				rampMode = 0;
				rampStep = 0;
				rampCnt = 0;
				for (j = 0; j < 8; j++)
				{
					macros[j][0] = 0;
					macros[j][1] = 0;
				}
				for (j = 0; j < 8; j++)
				{
					repeats[j][0] = -1;
					repeats[j][1] = 0x0000;
				}
				/*repeats[0][0] = segno, repeats[0][1] = repeat counter, macros[0][1] = the (single) return address*/
				repeats[0][0] = 0;
				numLoopStates = 0;
				cmdCount = 0;

				/*Add track header*/
				valSize = KonWriteDeltaTime(midData, midPos, 0);
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


				seqPos = seqPtrs[curTrack];
				seqEnd = 0;

				if (seqPos == 0x0000)
				{
					seqEnd = 1;
				}

				while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000 && cmdCount < 1000000)
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];

					if (songNum == 84)
					{
						songNum = 84;
					}
					cmdCount++;

					lowNibble = (command[0] >> 4);
					highNibble = (command[0] & 15);

					/*Gen 2+, pass 1: a looping track stops where the longest track's first pass ends*/
					if (pass == 1 && songLoop == 1 && loopTarget > 0 && KonTrackToMidi(masterDelay) >= loopTarget)
					{
						trkLooped = 1;
						seqEnd = 1;
						break;
					}

					/*Stop when the reader comes back to a note/rest in exactly the same state as before:
					  everything from here on would repeat, so the track has played its intro + one loop*/
					if (drvGen == 1)
					{
						if (command[0] <= KONAMI_STATUS_NOTE_MAX)
						{
							if (KonCheckLoopState(seqPos, repeats[0][0], repeats[0][1], macros[0][1], curSpeed, octave, transpose, masterDelay) == 1)
							{
								/*printf("  (%s)\n", TRK_NAMES[curTrack]);*/
								seqEnd = 1;
								break;
							}
						}
					}
					/*Gen 2+ noise SFX track: every byte below $E0 is NR43, one unit long.
					  Nemesis II and its relatives retrigger every byte. TMNT II writes NR43 without a
					  retrigger (the NR42 envelope from $E0 keeps running), $00 is a rest that silences
					  CH4 (NR42 = 8), and in hold mode ($E0 u|$80) a note is two bytes: volume, NR43.*/
					if (trkSFX == 1 && curTrack == 3 && drvGen >= 2 && drvGen <= 3 && command[0] < 0xE0)
					{
						curNoteLen = curSpeed & 0xFF;
						if (curNoteLen == 0)
						{
							curNoteLen = 256;
						}
						curNoteLen *= multiplier;

						if (drvGen == 3 && command[0] == 0x00)
						{
							/*Rest*/
							noiseEnvVol = 0;
							noiseEnvPace = 0;
							seqPos++;
						}
						else
						{
							nr43 = command[0];
							if (drvGen == 3)
							{
								if (noiseHold == 1)
								{
									/*Volume byte (retrigger only if it changed), then NR43*/
									if ((command[0] & 0x0F) != noiseEnvVol || noiseEnvPace != 0)
									{
										noiseTrigTime = masterDelay;
									}
									noiseEnvVol = command[0] & 0x0F;
									noiseEnvDir = 0;
									noiseEnvPace = 0;
									nr43 = command[1];
									seqPos += 2;
								}
								else
								{
									seqPos++;
								}
								lvl = KonNoiseVolAt(masterDelay);
							}
							else
							{
								/*Retriggered at the NR42 volume*/
								lvl = volVal & 0x0F;
								seqPos++;
							}

							curNote = ((nr43 >> 4) - 1) + (octave * 12) + transpose + octaveShift + 24;
							curVol = VolToVel[lvl & 0x0F];
							if (curVol != 0)
							{
								noteStart = KonTrackToMidi(masterDelay);
								noteEnd = KonTrackToMidi(masterDelay + curNoteLen);
								if (noteStart < lastEvtTime)
								{
									noteStart = lastEvtTime;
								}
								if (noteEnd <= noteStart)
								{
									noteEnd = noteStart + 1;
								}
								tempPos = KonWriteNoteEvent(midData, midPos, curNote, noteEnd - noteStart, noteStart - lastEvtTime, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								lastEvtTime = noteEnd;
							}
						}
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
					}

					/*Goemon raw frames: bytes below $E0, $E1-$E6 and $F0-$F7 are frames (one unit each),
					  $EB-$EE are slide on/off, mark and repeat; the rest are the normal commands*/
					else if (rawMode == 1 && (command[0] < 0xE0 || (command[0] >= 0xE1 && command[0] <= 0xE6)
						|| (command[0] >= 0xEB && command[0] <= 0xEE) || (command[0] >= 0xF0 && command[0] <= 0xF7)))
					{
						curNoteLen = curSpeed & 0xFF;
						if (curNoteLen == 0)
						{
							curNoteLen = 256;
						}
						curNoteLen *= multiplier;

						if (command[0] == 0xEB)
						{
							/*Slide frames on*/
							rawSlide = 1;
							rawSlideCnt = 0;
							seqPos++;
						}
						else if (command[0] == 0xEC)
						{
							/*Slide frames off*/
							rawSlide = 0;
							seqPos++;
						}
						else if (command[0] == 0xED)
						{
							/*Mark*/
							rawMark = seqPos + 1;
							rawSlideCnt = 0;
							seqPos++;
						}
						else if (command[0] == 0xEE)
						{
							/*$EE n v p: back to the mark until the n-th pass, adding v to the volume offset and
							  p (signed) to the period offset each time; on the n-th pass clear them and go on*/
							rawSlideCnt = (rawSlideCnt + 1) & 0xFF;
							if (rawSlideCnt == command[1])
							{
								rawVolOfs = 0;
								rawPerOfs = 0;
								seqPos += 4;
							}
							else
							{
								rawVolOfs = (rawVolOfs + command[2]) & 0xFF;
								rawPerOfs = (rawPerOfs + (signed char)command[3]) & 0xFFFF;
								seqPos = rawMark;
							}
						}
						else if (command[0] == 0x00)
						{
							/*Rest: NRx2 = 8*/
							midPos = KonFlushRawNote(midData, midPos, &lastEvtTime, &firstNote, curTrack, curInst);
							rawLevel = 0x08;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos++;
						}
						else if (rawSlide == 1)
						{
							/*Slide frame: count, flags (bit 7 negative period, bit 4 negative level, bits 0-3
							  level step), delta: for count units the period and the level move from the last
							  written ones. The values are loaded only when the counter is 0.*/
							if (rawSlideCnt == 0)
							{
								rawSlideCnt = command[0];
								rawSlideVol = (command[1] & 0x0F) << 4;
								if (command[1] & 0x10)
								{
									rawSlideVol = (-rawSlideVol) & 0xFF;
								}
								if (command[1] & 0x80)
								{
									/*cpl/inc of the low byte, high byte $FF (so a delta of 0 gives -256)*/
									rawSlideDelta = 0xFF00 | ((-command[2]) & 0xFF);
								}
								else
								{
									rawSlideDelta = command[2];
								}
							}
							for (k = 0; k < rawSlideCnt; k++)
							{
								midPos = KonRawFrame(midData, midPos, rawPeriod + rawSlideDelta, (rawLevel < 0 ? 0 : rawLevel) + rawSlideVol,
									masterDelay, curNoteLen, &lastEvtTime, &firstNote, curTrack, curInst);
								ctrlDelay += curNoteLen;
								masterDelay += curNoteLen;
							}
							rawSlideCnt = 0;
							seqPos += 3;
						}
						else
						{
							/*Frame: vol << 4 | period >> 8, period & $FF; the period offset is added to the period,
							  the volume offset to the volume nibble (wrapping)*/
							midPos = KonRawFrame(midData, midPos, (((command[0] & 0x0F) << 8) | command[1]) + rawPerOfs,
								((command[0] & 0xF0) + ((rawVolOfs & 0x0F) << 4)) & 0xF0,
								masterDelay, curNoteLen, &lastEvtTime, &firstNote, curTrack, curInst);
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos += 2;
						}
					}

					/*Fourth generation noise SFX (Tiny Toon 2, Batman, Raging Fighter, Castlevania Legends):
					  $00-$BF = NR43, one unit, written without a retrigger unless one is pending (after a
					  voice or a volume byte; Castlevania Legends' $E0 u|$80 retriggers every byte), so the
					  NR42 envelope keeps running across the bytes. $C0-$CF set the volume nibble and mark a
					  retrigger, no time (Tiny Toon 2/Batman: $C0 only marks); $D0-$DF set the volume nibble
					  and retrigger at once for one unit with the last NR43 ($D0: NR42 = 8, silent).*/
					else if (trkSFX == 1 && curTrack == 3 && drvGen >= 4 && command[0] < 0xE0)
					{
						curNoteLen = curSpeed & 0xFF;
						if (curNoteLen == 0)
						{
							curNoteLen = 256;
						}
						curNoteLen *= multiplier;

						if (command[0] >= 0xC0 && command[0] < 0xD0)
						{
							if (command[0] != 0xC0 || drvVers == KONAMI_VER_CVL || drvVers == KONAMI_VER_PPGB || drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J)
							{
								noiseLast = (noiseLast & 0x0F) | ((command[0] & 0x0F) << 4);
							}
							noisePend = 1;
							seqPos++;
						}
						else
						{
							if (command[0] == 0xD0)
							{
								noiseLast = 0x08;
								noisePend = 1;
							}
							else if (command[0] > 0xD0)
							{
								noiseLast = (noiseLast & 0x0F) | ((command[0] & 0x0F) << 4);
								noisePend = 1;
							}
							else
							{
								nr43 = command[0];
							}
							if (noisePend == 1 || noiseAlways == 1)
							{
								noiseTrigTime = masterDelay;
								noiseEnvVol = noiseLast >> 4;
								noiseEnvDir = (noiseLast >> 3) & 1;
								noiseEnvPace = noiseLast & 0x07;
								noisePend = 0;
							}
							lvl = KonNoiseVolAt(masterDelay);

							curNote = ((nr43 >> 4) - 1) + (octave * 12) + transpose + octaveShift + 24;
							curVol = VolToVel[lvl & 0x0F];
							if (curVol != 0)
							{
								noteStart = KonTrackToMidi(masterDelay);
								noteEnd = KonTrackToMidi(masterDelay + curNoteLen);
								if (noteStart < lastEvtTime)
								{
									noteStart = lastEvtTime;
								}
								if (noteEnd <= noteStart)
								{
									noteEnd = noteStart + 1;
								}
								tempPos = KonWriteNoteEvent(midData, midPos, curNote, noteEnd - noteStart, noteStart - lastEvtTime, firstNote, curTrack, curInst);
								firstNote = 0;
								midPos = tempPos;
								lastEvtTime = noteEnd;
							}
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
							seqPos++;
						}
					}

					/*Packed tone SFX (Tiny Toon 2, Batman, Raging Fighter; Sfx1 and Sfx2): two notes per
					  byte, one unit each, pitch 0 = rest. A low nibble $E is not a note: the next byte is
					  the new unit; a low nibble $F is not a note either and the next byte is read as a
					  command. Tiny Toon 2/Batman: a voice makes the next byte start with its high nibble,
					  after which the nibbles alternate (before any voice only low nibbles play).
					  Raging Fighter: every byte plays its high nibble, then its low nibble.*/
					else if (trkSFX == 1 && curTrack < 3 && command[0] < 0xE0 && (drvVers == KONAMI_VER_TTA2 || drvVers == KONAMI_VER_TTA3
						|| drvVers == KONAMI_VER_BATMAN || drvVers == KONAMI_VER_RF))
					{
						nib = -1;
						if ((drvVers == KONAMI_VER_RF && sfxHalf == 0) || (drvVers != KONAMI_VER_RF && sfxHalf == 1))
						{
							/*High nibble; the same byte is read again for the low nibble*/
							nib = command[0] >> 4;
							sfxHalf = (drvVers == KONAMI_VER_RF);
						}
						else
						{
							sfxHalf = (drvVers != KONAMI_VER_RF);
							if ((command[0] & 0x0F) == 0x0E)
							{
								curSpeed = command[1];
								seqPos += 2;
							}
							else if ((command[0] & 0x0F) == 0x0F)
							{
								seqPos++;
							}
							else
							{
								nib = command[0] & 0x0F;
								seqPos++;
							}
						}

						if (nib >= 0)
						{
							curNoteLen = curSpeed & 0xFF;
							if (curNoteLen == 0)
							{
								curNoteLen = 256;
							}
							curNoteLen *= multiplier;

							if (nib != 0)
							{
								if (nib == 0x0D)
								{
									/*Pitch 13: the last pitch an octave up*/
									curNote = (noteVal + 12) + transpose + octaveShift + 24;
								}
								else
								{
									noteVal = (nib - 1) + (octave * 12);
									curNote = noteVal + transpose + octaveShift + 24;
								}
								if (curTrack < 2)
								{
									curNote += 12;
								}

								/*Batman's volume ramp steps on every note*/
								if (rampMode != 0)
								{
									rampCnt = (rampCnt - 1) & 0xFF;
									if (rampCnt == 0)
									{
										rampCnt = rampStep;
										attenuate += (rampMode == 1) ? -1 : 1;
									}
								}

								if (curTrack == 2)
								{
									ch3Code = (((volVal & 0x0F) >> 1) + ((attenuate == 8) ? 0 : attenuate)) & 3;
									curVol = Ch3LvlToVel[ch3Code];
								}
								else
								{
									curVol = VolToVel[((volVal & 0x0F) - attenuate) & 0x0F];
								}

								if (curVol != 0)
								{
									noteStart = KonTrackToMidi(masterDelay);
									noteEnd = KonTrackToMidi(masterDelay + curNoteLen);
									if (noteStart < lastEvtTime)
									{
										noteStart = lastEvtTime;
									}
									if (noteEnd <= noteStart)
									{
										noteEnd = noteStart + 1;
									}
									tempPos = KonWriteNoteEvent(midData, midPos, curNote, noteEnd - noteStart, noteStart - lastEvtTime, firstNote, curTrack, curInst);
									firstNote = 0;
									midPos = tempPos;
									lastEvtTime = noteEnd;
								}
							}
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
					}

					else if (command[0] >= KONAMI_STATUS_REST_MIN && command[0] <= KONAMI_STATUS_REST_MAX)
					{
						if (vmMode == 1)
						{
							/*Note-volume mode: one unit*/
							curNoteLen = curSpeed & 0xFF;
							if (curNoteLen == 0)
							{
								curNoteLen = 256;
							}
						}
						else
						{
							curNoteLen = KonGetNoteLen(highNibble, curSpeed);
						}
						curNoteLen *= multiplier;
						curDelay += curNoteLen;
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;
					}

					else if (command[0] >= KONAMI_STATUS_NOTE_MIN && command[0] <= KONAMI_STATUS_NOTE_MAX)
					{
						if ((drvVers == KONAMI_VER_PPGB || drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J) && curTrack == 3 && trkSFX != 1 && lowNibble == 0x0D)
						{
							/*Power Pro GB drum track: $Dx n starts sound n (any bank-14 id, e.g. the crowd
							  sounds) instead of hit $D; x is the length as for any drum note. 2 bytes.*/
							noteVal = (command[1] - 1) + (octave * 12);
							curNote = noteVal + transpose + octaveShift + 24;
							if (curNote > 127)
							{
								curNote = 127;
							}
							seqPos++;
						}
						else if (drvGen >= 3 && lowNibble == 0x0D)
						{
							curNote = (noteVal + 12) + transpose + octaveShift + 24;
							if (curTrack < 2)
							{
								curNote += 12;
							}
						}
						else
						{
							noteVal = (lowNibble - 1) + (octave * 12);
							curNote = noteVal + transpose + octaveShift + 24;
							if (curTrack < 2)
							{
								curNote += 12;
							}
						}
						if ((curTrack == 3 && trkSFX == 1) || vmMode == 1)
						{
							/*Gen 1 noise SFX: the byte is NR43, the length is always the unit.
							  Castlevania Legends note-volume mode: one unit, lo = the volume.*/
							curNoteLen = curSpeed & 0xFF;
							if (curNoteLen == 0)
							{
								curNoteLen = 256;
							}
							if (vmMode == 1)
							{
								volVal = highNibble;
							}
						}
						else
						{
							curNoteLen = KonGetNoteLen(highNibble, curSpeed);
						}

						curNoteLen *= multiplier;

						/*Batman's volume ramp steps on every note (not on the drum track)*/
						if (rampMode != 0 && curTrack < 3)
						{
							rampCnt = (rampCnt - 1) & 0xFF;
							if (rampCnt == 0)
							{
								rampCnt = rampStep;
								attenuate += (rampMode == 1) ? -1 : 1;
							}
						}

						/*Volume as the driver sets it at note-on (no envelope at the attack)*/
						if (curTrack == 2 && drvGen >= 4)
						{
							/*Fourth generation CH3: NR32 = TRK_VOL - TRK_ATTEN with the attenuation stored
							  so that each step adds one to the level code (wrapping; Tiny Toon 2's $F8 = none)*/
							ch3Code = (((volVal & 0x0F) >> 1) + ((attenuate == 8) ? 0 : attenuate)) & 3;
							curVol = Ch3LvlToVel[ch3Code];
						}
						else if (curTrack == 2)
						{
							/*CH3: level code = ((vol >> 1) & 3) + atten; 1 = 100%, 2 = 50%, 3 = 25%, 0 or >= 4 = off*/
							ch3Code = ((volVal >> 1) & 3) + attenuate;
							if (ch3Code >= 4)
							{
								ch3Code = 0;
							}
							curVol = Ch3LvlToVel[ch3Code];
						}
						else if (curTrack == 3 && trkSFX != 1)
						{
							/*Drum track: notes start drum SFX*/
							curVol = DRUM_VEL;
						}
						else if (drvGen == 3 && volSet == 1 && (volVal & 0x0F) == 0)
						{
							/*TMNT II, volume set to 0: silent (Hyper Dunk's $EC envelopes run over a 0 base
							  volume, so earlier versions keep the old rule)*/
							curVol = 0;
						}
						else if (drvGen >= 4)
						{
							/*Fourth generation CH1/CH2: NRx2 = TRK_VOL - TRK_ATTEN (both << 4) without a borrow
							  test, so an attenuation above the volume wraps around; 0 turns the DAC off*/
							curVol = VolToVel[((volVal & 0x0F) - attenuate) & 0x0F];
						}
						else
						{
							/*CH1/CH2/noise SFX: NRx2 = (vol - atten) << 4; an attenuation above the
							  volume wraps around to a loud note*/
							lvl = (volVal & 0x0F) - attenuate;
							if (lvl <= 0)
							{
								lvl = 15;
							}
							curVol = VolToVel[lvl];
						}

						/*A silent note is a rest. Times are absolute, so the next event's delta is
						  measured from the end of the last written note.*/
						if (curVol <= 0 && drvVers >= KONAMI_VER_CVL)
						{
							curVol = 30;
						}
						if (curVol != 0)
						{
							noteStart = KonTrackToMidi(masterDelay);
							noteEnd = KonTrackToMidi(masterDelay + curNoteLen);
							if (noteStart < lastEvtTime)
							{
								noteStart = lastEvtTime;
							}
							if (noteEnd <= noteStart)
							{
								noteEnd = noteStart + 1;
							}
							tempPos = KonWriteNoteEvent(midData, midPos, curNote, noteEnd - noteStart, noteStart - lastEvtTime, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							lastEvtTime = noteEnd;
						}
						ctrlDelay += curNoteLen;
						masterDelay += curNoteLen;
						seqPos++;

					}

					else if (EventMap[command[0]] == KONAMI_EVENT_UNKNOWN0 || EventMap[command[0]] == KONAMI_EVENT_NOP)
					{
						seqPos++;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_UNKNOWN1 || EventMap[command[0]] == KONAMI_EVENT_NOP1)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_INS_PARAMS && drvGen == 3 && (command[1] & 0x80))
					{
						/*TMNT II $E0 u|$80: noise SFX -> hold mode (2-byte notes), bank 15 tone -> only the
						  unit, bank 14 tone -> raw frames ($E0 u|$80, NRx1, NR10; unused by the data)*/
						curSpeed = command[1] & 0x7F;
						if (curTrack == 3 && trkSFX == 1)
						{
							noiseHold = 1;
							seqPos += 2;
						}
						else if (drvVers == KONAMI_VER_GOEMON && curTrack < 3)
						{
							/*Goemon: raw frames on any tone/wave track ($E0 u|$80, NRx1, NR10 written on CH1 only)*/
							midPos = KonFlushRawNote(midData, midPos, &lastEvtTime, &firstNote, curTrack, curInst);
							rawMode = 1;
							rawLevel = -1;
							if (curTrack == 0)
							{
								rawNR10 = command[3];
							}
							seqPos += 4;
						}
						else if (drvVers == KONAMI_VER_TMNT2 && curTrack < 3 && (bank - 1) == 14)
						{
							seqPos += 4;
						}
						else
						{
							seqPos += 2;
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_INS_PARAMS && drvGen >= 4)
					{
						/*Fourth generation voice. Unit: 5 bits + octave code in Tiny Toon 2, 7 bits after it.
						  - tone/wave/drum tracks: $E0 u, duty<<4 | vol, gate<<4 | env (attenuation 0)
						  - noise SFX: $E0 u v; NR42 = swap(v) (Castlevania Legends: v, and u bit 7 =
							retrigger on every byte), retriggered at the next noise byte
						  - Raging Fighter: $E0 u|$80 v is 3 bytes on any track (NRx2 = swap(v) at once, the
							notes keep TRK_VOL); without bit 7 the voice is 4 bytes even on the noise track
						  - Castlevania Legends: $E0 u|$80 v x (4 bytes) starts the note-volume mode*/
						if (drvVers == KONAMI_VER_TTA2 || drvVers == KONAMI_VER_TTA3)
						{
							curSpeed = command[1] & 0x1F;
						}
						else
						{
							curSpeed = command[1] & 0x7F;
						}

						if (curTrack == 3 && trkSFX == 1 && !(drvVers == KONAMI_VER_RF && !(command[1] & 0x80)))
						{
							if (drvVers == KONAMI_VER_CVL || drvVers == KONAMI_VER_PPGB || drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J)
							{
								noiseLast = command[2];
								noiseAlways = command[1] >> 7;
							}
							else
							{
								noiseLast = ((command[2] << 4) | (command[2] >> 4)) & 0xFF;
							}
							noisePend = 1;
							seqPos += 3;
						}
						else if (drvVers == KONAMI_VER_RF && (command[1] & 0x80))
						{
							seqPos += 3;
						}
						else
						{
							if (curTrack < 3 || trkSFX != 1)
							{
								if ((drvVers == KONAMI_VER_CVL || drvVers == KONAMI_VER_PPGB || drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J) && (command[1] & 0x80))
								{
									vmMode = 1;
								}
								else
								{
									vmMode = 0;
									volVal = command[2] & 0x0F;
									volSet = 1;
									attenuate = 0;
									rampMode = 0;
									if (drvVers == KONAMI_VER_TTA2)
									{
										octave = (command[1] >> 5) - 1;
									}
								}
								/*Tiny Toon 2/Batman: the packed SFX notes start with the high nibble*/
								if (drvVers != KONAMI_VER_RF)
								{
									sfxHalf = 1;
								}
							}
							seqPos += 4;
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_INS_PARAMS)
					{
						/*(the fourth generation is handled above)*/
						curSpeed = command[1];

						if (curTrack < 3)
						{
							/*$E0 unit, duty<<4 | vol, gate<<4 | env (CH3: wave<<4 | vol); ends Goemon's raw frames*/
							if (rawMode == 1)
							{
								midPos = KonFlushRawNote(midData, midPos, &lastEvtTime, &firstNote, curTrack, curInst);
								rawMode = 0;
							}
							volVal = (command[2] & 0x0F);
							volSet = 1;
							if (drvVers != KONAMI_VER_ZEN)
							{
								seqPos += 4;
							}
							else
							{
								seqPos += 5;
							}
						}
						else
						{
							if (trkSFX != 1)
							{
								/*Drum track: $E0 unit*/
								seqPos += 2;
							}
							else
							{
								/*Noise SFX: $E0 unit, NR42 (vol<<4 | dir<<3 | pace), written with a trigger*/
								volVal = (command[2] >> 4);
								noiseHold = 0;
								noiseTrigTime = masterDelay;
								noiseEnvVol = command[2] >> 4;
								noiseEnvDir = (command[2] >> 3) & 1;
								noiseEnvPace = command[2] & 0x07;
								seqPos += 3;
							}
						}

					}

					else if (EventMap[command[0]] == KONAMI_EVENT_OCTAVE)
					{
						octave = highNibble - 1;
						seqPos++;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_NOTE_LEN)
					{
						curSpeed = command[1];
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_RELEASE)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_SWEEP)
					{
						if (curTrack == 0)
						{
							rawNR10 = command[1];
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_SUB_COMMAND)
					{
						if (drvVers == KONAMI_VER_PPGB && (command[1] == 0x0F || (command[1] & 0xF0) == 0xD0))
						{
							/*Power Pro GB: $EA 0F n = duty/wave n (escape for values above $E), and
							  $EA Dx n = play PCM sample n through the wave channel: 3 bytes each*/
							seqPos++;
						}
						else if ((drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J) && command[1] == 0x0F)
						{
							/*ISS '99 and its descendants: $EA 0F n = duty/wave n (3 bytes); used by the 2000
							  games (Cyborg Kuro-chan, The Mummy)*/
							seqPos++;
						}
						else if (drvVers == KONAMI_VER_ISS99J && command[1] == 0x11)
						{
							/*Cyborg Kuro-chan / The Mummy: $EA 11 n = instrument macro n (a timed list of wave
							  changes and period offsets started with every note; not converted), 3 bytes.
							  $EA 10 is still pan both; World Soccer GB2 never uses $EA 11*/
							seqPos++;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_TRANSPOSE_UP)
						{
							transpose = (command[1] & 0x0F);
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_TRANSPOSE_DOWN)
						{
							transpose = (command[1] & 0x0F) * -1;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_VIBRATO)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_DUTY_WAVE)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_AUTOPAN)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_PAN_LR || (command[1] & 0xF0) == KONAMI_SUB_COMMAND_PAN_R
							|| (command[1] & 0xF0) == KONAMI_SUB_COMMAND_PAN_L)
						{
							/*Tiny Toon 2/Batman: the pan commands also set the attenuation to y (and stop the
							  volume ramp)*/
							if (drvVers == KONAMI_VER_TTA2 || drvVers == KONAMI_VER_TTA3 || drvVers == KONAMI_VER_BATMAN)
							{
								attenuate = command[1] & 0x0F;
								rampMode = 0;
							}
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_AUTOPAN2)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_AUTOPAN_OFF)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_CLEAR)
						{
							/*Field clear: bit 0 = the attenuation (bits 1-3: detune, duty step, vibrato)*/
							if (command[1] & 0x01)
							{
								attenuate = 0;
							}
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_TRANSPOSE)
						{
							transpose = command[1] & 0x0F;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_TEMPO)
						{
							/*Batman $EA Ex: wTempo - 3 (no borrow test)*/
							if (trkSFX != 1 && numTempoEvts < MAX_TEMPO_EVTS)
							{
								tempoTimes[numTempoEvts] = masterDelay;
								tempoVals[numTempoEvts] = ((numTempoEvts > 0 ? tempoVals[numTempoEvts - 1] : 0) - 3) & 0xFF;
								numTempoEvts++;
							}
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_VOL_RAMP && drvVers == KONAMI_VER_BATMAN)
						{
							/*Batman $EA Fy: y bit 3 set: every (y & 7) + 1 notes one attenuation step up
							  (quieter); clear: every y + 1 notes one step down (louder)*/
							if (command[1] & 0x08)
							{
								rampMode = 2;
								rampStep = (command[1] & 0x07) + 1;
							}
							else
							{
								rampMode = 1;
								rampStep = (command[1] & 0x0F) + 1;
							}
							rampCnt = rampStep;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_WOBBLE && (drvVers == KONAMI_VER_CVL || drvVers == KONAMI_VER_PPGB || drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J))
						{
							/*Castlevania Legends $EA Ax y: NR50 wobble, 3 bytes*/
							seqPos++;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_HOLD)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_PANFLAGS)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_GLOBAL_VOL)
						{
							;
						}
						else if ((command[1] & 0xF0) == KONAMI_SUB_COMMAND_NOP)
						{
							;
						}
						else
						{
							;
						}

						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_TEMPO && vmMode == 1)
					{
						/*Castlevania Legends, note-volume mode: the first $EE marks; the next $EE x y n goes
						  back to the mark until its n-th pass (sliding the pitch by x and the attenuation
						  by y on each pass, not converted). The driver reads x, y, n from bank 14 even in a
						  bank-15 stream; no data uses this.*/
						if (eeOn == 0)
						{
							eeOn = 1;
							eeMark = seqPos + 1;
							eeCnt = 0;
							seqPos++;
						}
						else
						{
							eeCnt = (eeCnt + 1) & 0xFF;
							if (eeCnt == command[3])
							{
								eeOn = 0;
								eeCnt = 0;
								seqPos += 4;
							}
							else
							{
								seqPos = eeMark;
							}
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_TEMPO)
					{
						/*The tempo is global, so remember when it changes (absolute time) and write all
						  changes to the control track after the last track. SFX tracks ignore it.*/
						if (trkSFX != 1 && numTempoEvts < MAX_TEMPO_EVTS)
						{
							tempoTimes[numTempoEvts] = masterDelay;
							tempoVals[numTempoEvts] = command[1];
							numTempoEvts++;
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_PAN)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_ATTENUATION)
					{
						attenuate = command[0] - KONAMI_STATUS_ATTEN_MIN;
						rampMode = 0;
						seqPos++;
					}

					/*Flow commands, exactly as the driver: one segno, one repeat counter, one return slot.
					  The songs use calls as gotos and call shared blocks that lie before the caller,
					  so the direction of a jump says nothing; the loop check above ends the track.*/
					else if (EventMap[command[0]] == KONAMI_EVENT_REPEAT_START)
					{
						/*The counter is not reset here (only when a repeat finishes)*/
						repeats[0][0] = seqPos + 1;
						seqPos++;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_RETURN)
					{
						if (macros[0][1] == 0)
						{
							/*No call made yet: the driver would use a stale address*/
							seqEnd = 1;
						}
						else
						{
							seqPos = macros[0][1];
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_CALL)
					{
						if (songNum == 2)
						{
							songNum = 2;
						}
						if (drvGen > 1)
						{
							if (inMacro == 0)
							{
								macros[0][0] = ReadLE16(&exRomData[seqPos + 1]);
								macros[0][1] = seqPos + 3;
								seqPos = macros[0][0];
								inMacro = 1;
							}
							else
							{
								if (macros[0][1] == 0)
								{
									/*No call made yet: the driver would use a stale address*/
									seqEnd = 1;
								}
								else
								{
									seqPos = macros[0][1];
									inMacro = 0;
								}
							}

						}
						else
						{
							macros[0][0] = ReadLE16(&exRomData[seqPos + 1]);
							macros[0][1] = seqPos + 3;
							seqPos = macros[0][0];
						}

					}

					else if (EventMap[command[0]] == KONAMI_EVENT_REPEAT_END)
					{
						if (command[1] == 0xFF)
						{
							/*$FE $FF addr: jump*/
							seqPos = ReadLE16(&exRomData[seqPos + 2]);
						}
						else
						{
							/*$FE n: play the part after the segno n times in all (8-bit counter)*/
							repeats[0][1] = (repeats[0][1] + 1) & 0xFF;
							if (repeats[0][1] == command[1])
							{
								repeats[0][1] = 0;
								seqPos += 2;
							}
							else
							{
								seqPos = repeats[0][0];
							}
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_STOP)
					{
						seqEnd = 1;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_DUTY_STEP)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_AUTOPAN_ON)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_VIBRATO)
					{
						if (drvVers == KONAMI_VER_HD || (drvGen >= 2 && drvGen <= 3))
						{
							if (command[1] != 0)
							{
								seqPos += 3;
							}
							else
							{
								seqPos += 2;
							}
						}
						else if (drvGen >= 4)
						{
							if (command[1] >= 0x80)
							{
								seqPos += 2;
							}
							else if (command[1] == 0x00)
							{
								seqPos += 2;
							}
							else
							{
								seqPos += 3;
							}
						}
						else
						{
							if ((command[1] & 0x0F) != 0)
							{
								seqPos += 3;
							}
							else
							{
								seqPos += 2;
							}
						}

					}

					else if (EventMap[command[0]] == KONAMI_EVENT_REPEAT1)
					{
						if (repeat1Set == 0)
						{
							repeats[0][0] = seqPos + 1;
							repeat1Set = 1;
							if (drvGen >= 4)
							{
								repeats[0][1] = 0;
							}
							seqPos++;
						}
						else
						{
							/*Castlevania Legends: an armed loop break takes the passes left and its exit here
							  (the exit is one byte too far when this is also the last pass)*/
							skip = 2;
							if ((drvVers == KONAMI_VER_CVL || drvVers == KONAMI_VER_PPGB || drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J) && brkCnt[0] == 0xFF)
							{
								brkCnt[0] = (command[1] - 1) & 0xFF;
								brkExit[0] = seqPos + 2;
								skip = 3;
							}
							if (repeats[0][1] != (command[1] - 1))
							{
								repeats[0][1]++;
								seqPos = repeats[0][0];
							}
							else
							{
								repeats[0][1] = 0;
								seqPos += skip;
								repeat1Set = 0;
							}
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_REPEAT2)
					{
						if (repeat2Set == 0)
						{
							repeats[1][0] = seqPos + 1;
							repeat2Set = 1;
							if (drvGen >= 4)
							{
								repeats[1][1] = 0;
							}
							seqPos++;
						}
						else
						{
							skip = 2;
							if ((drvVers == KONAMI_VER_CVL || drvVers == KONAMI_VER_PPGB || drvVers == KONAMI_VER_ISS99 || drvVers == KONAMI_VER_ISS99J) && brkCnt[1] == 0xFF)
							{
								brkCnt[1] = (command[1] - 1) & 0xFF;
								brkExit[1] = seqPos + 2;
								skip = 3;
							}
							if (repeats[1][1] != (command[1] - 1))
							{
								repeats[1][1]++;
								seqPos = repeats[1][0];
							}
							else
							{
								repeats[1][1] = 0;
								seqPos += skip;
								repeat2Set = 0;
							}
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_SONG_LOOP)
					{
						if (songLoop == 0)
						{
							/*Mark*/
							songLoop = 1;
							segnoPos = seqPos + 1;
							seqPos++;
						}
						else if (pass == 1 && KonTrackToMidi(masterDelay) < loopTarget && segnoPos != 0)
						{
							/*Jump back to the mark until the longest track has played its first pass*/
							seqPos = segnoPos;
						}
						else
						{
							trkLooped = 1;
							seqEnd = 1;
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_ED)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_TUNING)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_VOL1 || EventMap[command[0]] == KONAMI_EVENT_VOL2)
					{
						/*TMNT II $F8/$F9 x: TRK_VOL = swap(x), so the volume is x & 15. Tone/wave tracks:
						  the attenuation is reset. Noise SFX: NR42 written with a trigger (not in hold mode).*/
						volVal = command[1] & 0x0F;
						volSet = 1;
						if (curTrack == 3 && trkSFX == 1)
						{
							if (noiseHold == 0)
							{
								noiseTrigTime = masterDelay;
								noiseEnvVol = command[1] & 0x0F;
								noiseEnvDir = (command[1] >> 7) & 1;
								noiseEnvPace = (command[1] >> 4) & 0x07;
							}
						}
						else
						{
							attenuate = 0;
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_JUMP)
					{
						if ((ReadLE16(&exRomData[seqPos + 1]) > seqPos))
						{
							seqPos = ReadLE16(&exRomData[seqPos + 1]);
						}
						else if (drvGen >= 4)
						{
							/*Most jumps back go to a shared tail or block. A jump reached again in the same
							  reader state loops forever (like $FE): keep going until the longest track's
							  first pass ends.*/
							if (KonCheckLoopState(seqPos, repeats[0][1] | (repeats[1][1] << 8), jumpCnt | (inMacro << 8) | (repeat1Set << 9) | (repeat2Set << 10) | (songLoop << 11),
								macros[0][1], curSpeed, octave, transpose, masterDelay) == 0
								|| (pass == 1 && KonTrackToMidi(masterDelay) < loopTarget))
							{
								seqPos = ReadLE16(&exRomData[seqPos + 1]);
							}
							else
							{
								trkLooped = 1;
								seqEnd = 1;
							}
						}
						else
						{
							seqEnd = 1;
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_VOL_ENV)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_COND_JUMP)
					{
						if ((jumpCnt + 1) != command[1])
						{
							seqPos += 4;
							jumpCnt++;
						}
						else
						{
							seqPos = ReadLE16(&exRomData[seqPos + 2]);
							jumpCnt = 0;
						}
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_OCTAVE_SHIFT)
					{
						if (octave != 0)
						{
							if ((command[1] >> 4) < 8)
							{
								octaveShift = (((command[1] >> 4)) * 12) + (command[1] & 0x0F);
							}
							else
							{
								switch (command[1] >> 4)
								{
								case 9:
									octaveShift = -12;
									break;
								case 10:
									octaveShift = -24;
									break;
								case 11:
									octaveShift = -36;
									break;
								case 12:
									octaveShift = -48;
									break;
								case 13:
									octaveShift = -60;
									break;
								case 14:
									octaveShift = -72;
									break;
								case 15:
									octaveShift = -84;
									break;
								default:
									octaveShift = -12;
									break;
								}
								octaveShift -= (command[1] & 0x0F);
							}
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_WAVE_ALT)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_GLOBAL_VOL)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_VOL)
					{
						/*Batman/Raging Fighter $F8 x: TRK_VOL = swap(x), attenuation 0 (the noise SFX
						  track keeps its own NR42 value)*/
						if (!(curTrack == 3 && trkSFX == 1))
						{
							volVal = command[1] & 0x0F;
							volSet = 1;
							attenuate = 0;
						}
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_FADE)
					{
						seqPos += 2;
					}

					else if (EventMap[command[0]] == KONAMI_EVENT_BREAK)
					{
						/*Castlevania Legends $F9 x: break out of loop 1 (x odd) or loop 2 (x even, also ends
						  loop 1) on its last pass, to the byte after its $FB n / $FC n. The first $F9 arms a
						  counter (0 -> $FF); the next loop end stores the passes left and the exit there;
						  every later $F9 counts down and breaks at 0.*/
						k = (command[1] & 1) ? 0 : 1;
						if ((k == 0 && repeat1Set == 1) || (k == 1 && repeat2Set == 1))
						{
							brkCnt[k] = (brkCnt[k] - 1) & 0xFF;
							if (brkCnt[k] == 0)
							{
								repeats[k][1] = 0;
								if (k == 1)
								{
									repeat2Set = 0;
								}
								repeat1Set = 0;
								seqPos = brkExit[k];
							}
							else
							{
								seqPos += 2;
							}
						}
						else
						{
							seqPos += 2;
						}
					}

					/*Unknown command*/
					else
					{
						seqPos++;
					}
				}

				/*Goemon raw frames: the last held note*/
				midPos = KonFlushRawNote(midData, midPos, &lastEvtTime, &firstNote, curTrack, curInst);

				/*End of track*/
				WriteBE32(&midData[midPos], 0xFF2F00);
				midPos += 4;

				/*Calculate MIDI channel size*/
				trackSize = midPos - midTrackBase;
				WriteBE16(&midData[midTrackBase - 2], trackSize);
				trkLen[curTrack] = midPos;

				if (pass == 0 && trkLooped == 1 && KonTrackToMidi(masterDelay) > loopTarget)
				{
					loopTarget = KonTrackToMidi(masterDelay);
				}

			}
		}

		/*Tempo changes: sort by time and write the ones that change the tempo*/
		if (sortedTempo == 0)
		{
			KonSortTempoEvts();
		}
		lastTempoTime = 0;
		for (j = 0; j < numTempoEvts; j++)
		{
			if (tempoVals[j] == curTempoVal)
			{
				continue;
			}
			curTempoVal = tempoVals[j];
			ctrlMidPos++;
			valSize = KonWriteDeltaTime(ctrlMidData, ctrlMidPos, tempoTimes[j] - lastTempoTime);
			lastTempoTime = tempoTimes[j];
			ctrlMidPos += valSize;
			WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5103);
			ctrlMidPos += 3;
			WriteBE24(&ctrlMidData[ctrlMidPos], KonTempoToMicros(curTempoVal, ticks, 0));
			ctrlMidPos += 2;
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
		for (k = 0; k < 4; k++)
		{
			fwrite(trkData[k], trkLen[k], 1, mid);
			free(trkData[k]);
		}
		midData = NULL;
		free(ctrlMidData);
		fclose(mid);
	}
}
