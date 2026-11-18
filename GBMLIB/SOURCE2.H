/*Source #2 (Workboy/Double Dragon 3)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define SOURCE2_VER_WB		1
#define SOURCE2_VER_DD3		2

unsigned int SOURCE2_STATUS_REST;
unsigned int SOURCE2_STATUS_NOTE_MIN;
unsigned int SOURCE2_STATUS_NOTE_MAX;
unsigned int SOURCE2_STATUS_RESTART;
unsigned int SOURCE2_STATUS_STOP;

/*Function prototypes*/
void Source2Proc(int bank);