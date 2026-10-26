/*Digital Eclipse (Kevin Choong)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define DE2_VER_720		1
#define DE2_VER_PB		2

#define DE2_MODE_MUS	1
#define DE2_MODE_PCM	2

unsigned int DE2_STATUS_STOP;
unsigned int DE2_STATUS_REST;

unsigned int EventMap[256];

/*Function prototypes*/
void DE2Proc(int bank, char parameters[4][100]);