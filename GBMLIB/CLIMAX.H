/*Climax (IMEDGBoy)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define IMED_VER_STD		1

unsigned int IMED_STATUS_NOP;
unsigned int IMED_STATUS_PORTO;
unsigned int IMED_STATUS_VIBRATO;
unsigned int IMED_STATUS_VOL_SLIDE;
unsigned int IMED_STATUS_PAT_JUMP;
unsigned int IMED_STATUS_VOL;
unsigned int IMED_STATUS_PAT_BREAK;
unsigned int IMED_STATUS_NOP;
unsigned int IMED_STATUS_SPEED;

/*Function prototypes*/
void IMEDProc(char parameters[4][100]);