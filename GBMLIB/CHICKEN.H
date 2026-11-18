/*Lucky Chicken Games*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define CHICKEN_VER_STD		1

enum ChickenEventTypes
{
	CHICKEN_EVENT_NOTE_ON = 1,
	CHICKEN_EVENT_NOTE_OFF,
	CHICKEN_EVENT_PROG_CHANGE,
	CHICKEN_EVENT_PAN,
	CHICKEN_EVENT_SPECIAL,
	CHICKEN_EVENT_STOP,
};

unsigned int EventMap[256];

/*Function prototypes*/
void ChickenProc(int bank, char parameters[4][100]);