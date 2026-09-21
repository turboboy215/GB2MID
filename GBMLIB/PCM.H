/*PCM (General)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define PCM_FMT_4BIT_BE		0
#define PCM_FMT_4BIT_LE		1
#define PCM_FMT_1BIT		2
#define PCM_FMT_RNC_3		3

/*Function prototype*/
void PCMProc(int bank, char parameters[4][100]);