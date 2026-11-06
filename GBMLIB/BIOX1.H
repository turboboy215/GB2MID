/*Biox #1 (Japan System House)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/*Noobow*/
#define BIOX1_VER_NB		1
/*Pocket Bowling*/
#define BIOX1_VER_PB		2

enum Biox1EventTypes
{
	BIOX1_EVENT_UNKNOWN0 = 0,
	BIOX1_EVENT_UNKNOWN1,
	BIOX1_EVENT_UNKNOWN2,
	BIOX1_EVENT_NOP,
	BIOX1_EVENT_NOP1,
	BIOX1_EVENT_NOP2,
	BIOX1_EVENT_SWEEP,
	BIOX1_EVENT_ENV_SEQ,
	BIOX1_EVENT_DUTY,
	BIOX1_EVENT_VIBRATO,
	BIOX1_EVENT_ENV,
	BIOX1_EVENT_TEMPO,
	BIOX1_EVENT_PAN,
	BIOX1_EVENT_CALL,
	BIOX1_EVENT_RETURN,
	BIOX1_EVENT_REPEAT,
	BIOX1_EVENT_JUMP,
	BIOX1_EVENT_GLOBAL_VOL,
	BIOX1_EVENT_TIE,
	BIOX1_EVENT_RAW_NOTE,
	BIOX1_EVENT_SLUR,
	BIOX1_EVENT_NOTE_LEN,
	BIOX1_EVENT_STOP,
};

unsigned int BIOX1_STATUS_NOTE_MIN;
unsigned int BIOX1_STATUS_NOTE_MAX;
unsigned int BIOX1_STATUS_NOTE_LEN_MIN;
unsigned int BIOX1_STATUS_NOTE_LEN_MAX;

unsigned int EventMap[256];

void Biox1Proc(int bank, char parameters[4][100]);