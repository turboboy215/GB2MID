/*C-lab*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define CLAB_VER_RAMPART	0
#define CLAB_VER_STD		1

enum ClabEventTypes
{
	CLAB_EVENT_UNKNOWN0 = 0,
	CLAB_EVENT_UNKNOWN1,
	CLAB_EVENT_NOP,
	CLAB_EVENT_NOP1,
	CLAB_EVENT_REST,
	CLAB_EVENT_OCTAVE,
	CLAB_EVENT_OCTAVE_UP,
	CLAB_EVENT_OCTAVE_DOWN,
	CLAB_EVENT_CALL,
	CLAB_EVENT_RETURN,
	CLAB_EVENT_REPEAT_START,
	CLAB_EVENT_REPEAT_END,
	CLAB_EVENT_SFX_END,
	CLAB_EVENT_CH234_END,
	CLAB_EVENT_CH1_END,
	CLAB_EVENT_DUTY,
	CLAB_EVENT_SWEEP,
	CLAB_EVENT_ENV,
	CLAB_EVENT_TEMPO,
	CLAB_EVENT_DUR,
	CLAB_EVENT_TRANSPOSE,
	CLAB_EVENT_JUMP,
	CLAB_EVENT_PAN,
};

unsigned int CLAB_STATUS_NOTE_MIN;
unsigned int CLAB_STATUS_NOTE_MAX;
unsigned int CLAB_STATUS_REST;
unsigned int CLAB_STATUS_NOTE_LEN_MIN;
unsigned int CLAB_STATUS_NOTE_LEN_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void ClabProc(int bank, char parameters[4][100]);