/*Tiertex*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define TT_VER_WC94		0
#define TT_VER_STD		1

enum TTEventTypes
{
	TT_EVENT_RESTART = 0,
	TT_EVENT_STOP,
	TT_EVENT_REPEAT_END,
	TT_EVENT_REPEAT_START,
	TT_EVENT_CALL,
	TT_EVENT_RETURN,
};

unsigned int TT_STATUS_REST;
unsigned int TT_STATUS_NOTE_MIN;
unsigned int TT_STATUS_NOTE_MAX;
unsigned int TT_STATUS_PROG_CHANGE_MIN;
unsigned int TT_STATUS_PROG_CHANGE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void TTProc(int bank, char parameters[4][100]);