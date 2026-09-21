/*Jaleco*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/*Hero Shuugou!! Pinball Party*/
#define JALECO_VER_HSPP				1
/*Avenging Spirit*/
#define JALECO_VER_AS				2
/*Soldam*/
#define JALECO_VER_SD				3

enum JalecoEventTypes
{
	JALECO_EVENT_UNKNOWN0 = 0,
	JALECO_EVENT_PAN,
	JALECO_EVENT_ENV_PARAMS,
	JALECO_EVENT_VOL,
	JALECO_EVENT_ENV_SEQ,
	JALECO_EVENT_VIB_SEQ,
	JALECO_EVENT_DELAY,
	JALECO_EVENT_NOTE_DUR_FRAC,
	JALECO_EVENT_NOTE_DUR_ABS,
	JALECO_EVENT_TRANSPOSE,
	JALECO_EVENT_SPEED,
	JALECO_EVENT_JUMP,
	JALECO_EVENT_SWEEP_ON,
	JALECO_EVENT_WAVEFORM,
	JALECO_EVENT_REPEAT_START,
	JALECO_EVENT_REPEAT_END,
	JALECO_EVENT_STOP,
	JALECO_EVENT_CALL,
	JALECO_EVENT_RETURN,
	JALECO_EVENT_TUNING,
	JALECO_EVENT_SWEEP_OFF,
	JALECO_EVENT_ENV,
	JALECO_EVENT_TREMOLO,
};

unsigned int JALECO_STATUS_NOTE_MIN;
unsigned int JALECO_STATUS_NOTE_MAX;
unsigned int JALECO_STATUS_REST;
unsigned int JALECO_STATUS_COMMAND;

unsigned int EventMap[256];

/*Function prototypes*/
void JalecoProc(int bank, char parameters[4][100]);