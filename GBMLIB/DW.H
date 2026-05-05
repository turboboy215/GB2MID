/*David Whittaker*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/*David Whittaker*/

#define DW_VER_STD	1

enum EventTypes
{
	DW_EVENT_TEMPO = 0,
	DW_EVENT_LOOP,
	DW_EVENT_ENV,
	DW_EVENT_VIBRATO,
	DW_EVENT_REST,
	DW_EVENT_TIE,
	DW_EVENT_DUTY,
	DW_EVENT_GLOBAL_TRANSPOSE,
	DW_EVENT_LOCAL_TRANSPOSE,
	DW_EVENT_SWEEP,
	DW_EVENT_STOP,
	DW_EVENT_RETURN,
};

unsigned int DW_STATUS_NOTE_MIN;
unsigned int DW_STATUS_NOTE_MAX;
unsigned int DW_STATUS_NOTE_LEN_MIN;
unsigned int DW_STATUS_NOTE_LEN_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
extern void DWProc(int bank);