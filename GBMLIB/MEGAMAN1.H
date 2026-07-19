/*Hiroshi Wada*/
/*(Mega Man 1)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define MM1_VER_SS		0
#define MM1_VER_MM1		1

/*Game numbers*/
#define GAME_UNKNOWN	0
#define GAME_MM1		1
#define GAME_SS			2
#define GAME_QIX		3

enum MM1EventTypes
{
	MM1_EVENT_UNKNOWN0 = 0,
	MM1_EVENT_UNKNOWN1,
	MM1_EVENT_STOP,
	MM1_EVENT_VOL,
	MM1_EVENT_VOL2,
	MM1_EVENT_ENV_SEQ,
	MM1_EVENT_SWEEP_PACE,
	MM1_EVENT_ENV_SPEED,
	MM1_EVENT_CHAN_LEN,
	MM1_EVENT_NOTE_DUR,
	MM1_EVENT_VIBRATO,
	MM1_EVENT_VIB_SPEED,
	MM1_EVENT_SWEEP,
	MM1_EVENT_DUTY,
	MM1_EVENT_WAVEFORM,
	MM1_EVENT_MASTER_VOLUME,
	MM1_EVENT_PAN,
	MM1_EVENT_VIB_DELAY,
	MM1_EVENT_SLUR,
	MM1_EVENT_TRANSPOSE,
	MM1_EVENT_AUTO_LEN_ON,
	MM1_EVENT_AUTO_LEN_OFF,
	MM1_EVENT_DEC_POS,
	MM1_EVENT_CALL,
	MM1_EVENT_RETURN,
	MM1_EVENT_RNG_VAL,
	MM1_EVENT_RNG_START,
};

unsigned int MM1_STATUS_NOTE_MIN;
unsigned int MM1_STATUS_NOTE_MAX;
unsigned int MM1_STATUS_JUMP1_MIN;
unsigned int MM1_STATUS_JUMP1_MAX;
unsigned int MM1_STATUS_JUMP2_MIN;
unsigned int MM1_STATUS_JUMP2_MAX;
unsigned int MM1_STATUS_JUMP3_MIN;
unsigned int MM1_STATUS_JUMP3_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void MM1Proc(int bank);