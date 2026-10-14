/*GHX (Shin'en)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define GHX_VER_STD		0
#define GHX_VER_TR		1

unsigned int GHX_STATUS_NONE;
unsigned int GHX_STATUS_PROG_CHANGE_VOL;
unsigned int GHX_STATUS_EFFECT;

/*Function prototypes*/
void GHXProc(int bank);