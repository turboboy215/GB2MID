/*UGB Player (by Thalamus/Jon Wells)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define UGB_VER_COMMON			1
#define UGB_VER_ALT				2

enum UGBEventTypes
{
	UGB_EVENT_UNKNOWN0 = 1,
	UGB_EVENT_NOP,
	UGB_EVENT_PITCH_BEND,
	UGB_EVENT_PROG_CHANGE,
	UGB_EVENT_TIE,
	UGB_EVENT_REST,
	UGB_EVENT_STOP,
};

unsigned int UGB_STATUS_NOTE_MIN;
unsigned int UGB_STATUS_NOTE_MAX;
unsigned int UGB_TRACK_EVENT_STOP;
unsigned int UGB_TRACK_EVENT_RESTART;

unsigned int EventMap[256];

/*Function prototypes*/
void UGBProc(char parameters[4][100]);