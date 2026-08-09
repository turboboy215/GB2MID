/*Martin Walker*/
/*For Metal Walker, see TOSE.*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define MWALKER_VER1			1
#define MWALKER_VER_STD			2
#define MWALKER_SYSTEM_GB		1
#define MWALKER_SYSTEM_GG		2

enum MWEventTypes
{
	MWALKER_EVENT_UNKNOWN0 = 0,
	MWALKER_EVENT_PROG_CHANGE,
	MWALKER_EVENT_REST,
	MWALKER_EVENT_BEND,
	MWALKER_EVENT_TEMPO,
	MWALKER_EVENT_TIE,
	MWALKER_EVENT_GATE,
	MWALKER_EVENT_GLOBAL_TRANSPOSE,
	MWALKER_EVENT_RETURN,
};

unsigned int MWALKER_STATUS_NOTE_MIN;
unsigned int MWALKER_STATUS_NOTE_MAX;
unsigned int MWALKER_STATUS_SEQ_MIN;
unsigned int MWALKER_STATUS_SEQ_MAX;
unsigned int MWALKER_STATUS_LOOP_STOP;
unsigned int MWALKER_STATUS_LOOP_START_MIN;
unsigned int MWALKER_STATUS_LOOP_START_MAX;
unsigned int MWALKER_STATUS_FADE;
unsigned int MWALKER_STATUS_STOP;
unsigned int MWALKER_STATUS_JUMP;

unsigned int EventMap[256];

/*Function prototypes*/
extern void MWProc(int bank);
