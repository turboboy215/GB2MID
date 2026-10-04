/*Unknown (3D Pocket Pool/3D Pool Stars)*/

#define POOL3D_VER_STD		1

#define POOL3D_MODE_MUS		1
#define POOL3D_MODE_SAMP	2

/*Number of songs in the song table (00:3D46)*/
#define POOL3D_NUM_SONGS	8

/*The second song is a set of separate cues: the game starts it with C = order
  number, and the driver then repeats only that order ($CF01). Each order is
  therefore written as its own phrase (song2_1.mid, song2_2.mid, ...).*/
#define POOL3D_PHRASE_SONG	2

  /*Rate table (bank 0): 4 bytes per entry (NR33, NR34, TIMA reload, unused), index 1-19*/
#define POOL3D_RATE_TAB		0x3B0A
#define POOL3D_RATE_MIN		1
#define POOL3D_RATE_MAX		19

/*Noise NR42 table (bank 0), indexed by noise type 0-15*/
#define POOL3D_NOISE_TAB	0x3EF6

/*Instrument table: bank 9, 5 bytes per entry (len_lo, len_hi, bank, addr_lo, addr_hi)*/
#define POOL3D_INST_BANK	0x09
#define POOL3D_INST_TAB		0x4000
#define POOL3D_MAX_INST		32

/*Tempo set by the song start routine ($3D6A) before the first pattern is loaded*/
#define POOL3D_DEFAULT_TEMPO	0x7D

/*A row lasts 896/tempo frames; one row = one 16th note*/
#define POOL3D_ROW_UNITS	896
#define POOL3D_TICKS_PER_ROW	30

/*Rate index 15 ($700 = 8192 Hz) is the samples' native pitch: map it to C5*/
#define POOL3D_NOTE_BASE	57

enum Pool3DEventTypes
{
	POOL3D_EVENT_UNKNOWN = 0,
	POOL3D_EVENT_NEXT_PAT,
	POOL3D_EVENT_NOTE,
	POOL3D_EVENT_WAIT,
	POOL3D_EVENT_NOISE,
	POOL3D_EVENT_SET_INST,
};

unsigned int EventMap[256];

void Pool3DProc(int bank, char parameters[4][100]);