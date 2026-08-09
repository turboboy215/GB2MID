/*Studio Cliche*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define CLICHE_VER_STD		1
#define CLICHE_VER_DM		2
#define CLICHE_VER_MMB		3

enum ClicheEventTypes
{
	CLICHE_EVENT_UNKNOWN0 = 0,
	CLICHE_EVENT_UNKNOWN1,
	CLICHE_EVENT_NOP,
	CLICHE_EVENT_JUMP,
	CLICHE_EVENT_CALL,
	CLICHE_EVENT_RETURN,
	CLICHE_EVENT_REPEAT_START,
	CLICHE_EVENT_REPEAT_END,
	CLICHE_EVENT_VIBRATO,
	CLICHE_EVENT_PROG_CHANGE,
	CLICHE_EVENT_ENV_SEQ,
	CLICHE_EVENT_TRANSPOSE,
	CLICHE_EVENT_TUNING,
	CLICHE_EVENT_PAN,
	CLICHE_EVENT_STOP,
};

unsigned int CLICHE_STATUS_NOTE_MIN;
unsigned int CLICHE_STATUS_NOTE_MAX;
unsigned int CLICHE_STATUS_REST;

unsigned int EventMap[256];

/*Function prototypes*/
extern void ClicheProc(int bank, char parameters[4][100]);