/*Taro Bando (Mole Mania/Moguranya)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define MOLE_VER_STD		1

enum MoleEventTypes
{
	MOLE_EVENT_UNKNOWN0 = 0,
	MOLE_EVENT_NOP,
	MOLE_EVENT_END,
	MOLE_EVENT_LEN_TABLE,
	MOLE_EVENT_WAVEFORM,
	MOLE_EVENT_VIBRATO_ON,
	MOLE_EVENT_VIBRATO_OFF,
	MOLE_EVENT_RELEASE_ON,
	MOLE_EVENT_RELEASE_OFF,
	MOLE_EVENT_PITCH_SLIDE_ON,
	MOLE_EVENT_ABS_NOTE_LEN,
	MOLE_EVENT_TRANSPOSE,
	MOLE_EVENT_PAN_LR,
	MOLE_EVENT_PAN_R,
	MOLE_EVENT_PAN_L,
	MOLE_EVENT_CH3_DECAY,
	MOLE_EVENT_SCOOP_ON,
	MOLE_EVENT_DUTY_SEQ,
	MOLE_EVENT_ENV_SEQ,
	MOLE_EVENT_PITCH_SLIDE_OFF,
	MOLE_EVENT_SCOOP_OFF,
};

unsigned int MOLE_STATUS_NOTE_LEN_MIN;
unsigned int MOLE_STATUS_NOTE_LEN_MAX;
unsigned int MOLE_STATUS_CH3_VOL_MIN;
unsigned int MOLE_STATUS_CH3_VOL_MAX;
unsigned int MOLE_STATUS_NOTE_DUR_MIN;
unsigned int MOLE_STATUS_NOTE_DUR_MAX;
unsigned int MOLE_STATUS_REST;
unsigned int MOLE_STATUS_NOTE_MIN;
unsigned int MOLE_STATUS_NOTE_MAX;
unsigned int MOLE_STATUS_TIE;
unsigned int MOLE_STATUS_CH4_NOTE_MIN;
unsigned int MOLE_STATUS_CH4_NOTE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void MoleProc(int bank);