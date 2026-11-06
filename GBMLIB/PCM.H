/*PCM (General)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define PCM_FMT_4BIT_BE		0
#define PCM_FMT_4BIT_LE		1
#define PCM_FMT_1BIT		2
#define PCM_FMT_RNC_3		3
#define PCM_FMT_2BIT		4
#define PCM_FMT_3BIT_BE		5
#define PCM_FMT_3BIT_LE		6
#define PCM_FMT_P5			7
#define PCM_FMT_HHCMP		8
#define PCM_FMT_G2HUFF		9
#define PCM_FMT_NR50		10

/*Function prototype*/
void PCMProc(int bank, char parameters[4][100]);