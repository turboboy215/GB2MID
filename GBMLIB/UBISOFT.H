/*Ubi Soft*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define UBISOFT_VER_STD	1

enum UbiSoftEventTypes
{
	UBISOFT_EVENT_UNKNOWN0,
	UBISOFT_EVENT_NOP,
	UBISOFT_EVENT_NOTE_ON1,
	UBISOFT_EVENT_NOTE_OFF1,
	UBISOFT_EVENT_PROG_CHANGE1,
	UBISOFT_EVENT_NOTE_ON2,
	UBISOFT_EVENT_NOTE_OFF2,
	UBISOFT_EVENT_PROG_CHANGE2,
	UBISOFT_EVENT_NOTE_ON3,
	UBISOFT_EVENT_NOTE_OFF3,
	UBISOFT_EVENT_PROG_CHANGE3,
	UBISOFT_EVENT_NOTE_ON4,
	UBISOFT_EVENT_NOTE_OFF4,
	UBISOFT_EVENT_EXIT,
	UBISOFT_EVENT_JUMP,
};

unsigned int UBISOFT_STATUS_JUMP_MIN;
unsigned int UBISOFT_STATUS_JUMP_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void UbiSoftProc(char parameters[4][100]);