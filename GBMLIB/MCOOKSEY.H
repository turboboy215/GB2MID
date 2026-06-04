/*Mark Cooksey*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define MC_VER_STD	1

#define SYSTEM_GB	1
#define SYSTEM_GG	2

enum MCEventTypes
{
	MC_EVENT_TIE = 0,
	MC_EVENT_STOP,
	MC_EVENT_JUMP,
	MC_EVENT_NOISE,
	MC_EVENT_CALL,
	MC_EVENT_RETURN,
	MC_EVENT_SET_LOOP_FLAG,
	MC_EVENT_GLOBAL_PAN,
	MC_EVENT_SET_NOTE_LENS,
	MC_EVENT_TEMPO,
	MC_EVENT_PAN1,
	MC_EVENT_PAN2,
	MC_EVENT_PAN3,
	MC_EVENT_PAN4,
};

unsigned int MC_STATUS_NOTE_MIN;
unsigned int MC_STATUS_NOTE_MAX;
unsigned int MC_STATUS_ALT_NOTE_MIN;
unsigned int MC_STATUS_ALT_NOTE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void MCProc(int bank);