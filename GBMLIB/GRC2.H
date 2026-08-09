/*Graphic Research (2nd driver)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define GRC2_VER_STD		1

enum GRC2EventTypes
{
	GRC2_EVENT_UNKNOWN0 = 0,
	GRC2_EVENT_UNKNOWN1,
	GRC2_EVENT_NOP,
	GRC2_EVENT_OCTAVE,
	GRC2_EVENT_REPEAT_START,
	GRC2_EVENT_REPEAT_END,
	GRC2_EVENT_PROG_CHANGE,
	GRC2_EVENT_SWEEP,
	GRC2_EVENT_TUNING,
	GRC2_EVENT_RESET,
	GRC2_EVENT_RETURN,
	GRC2_EVENT_CALL,
	GRC2_EVENT_JUMP,
	GRC2_EVENT_VIBRATO,
	GRC2_EVENT_ENV_SEQ,
	GRC2_EVENT_NOTE_LEN,
	GRC2_EVENT_TIE,
	GRC2_EVENT_STOP,
};

unsigned int GRC2_STATUS_NOTE_MIN;
unsigned int GRC2_STATUS_NOTE_MAX;
unsigned int GRC2_STATUS_OCTAVE_MIN;
unsigned int GRC2_STATUS_OCTAVE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
extern void GRC2Proc(int bank);