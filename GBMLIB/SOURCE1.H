/*Source #1 (Pyramids of Ra)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define SOURCE1_VER_STD		1

unsigned int SOURCE1_STATUS_STOP_TRACK;
unsigned int SOURCE1_STATUS_STOP;
unsigned int SOURCE1_STATUS_PARK;
unsigned int SOURCE1_STATUS_NOTE_MIN;
unsigned int SOURCE1_STATUS_NOTE_MAX;
unsigned int SOURCE1_STATUS_PROG_CHANGE_MIN;
unsigned int SOURCE1_STATUS_PROG_CHANGE_MAX;

/*Function prototypes*/
void Source1Proc(int bank);