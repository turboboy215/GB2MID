/*Audio Visual Magic (AVM)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define AVM_VER_STD		0

/*Pattern layout*/
#define AVM_ROWS		64
#define AVM_VOICES		3
#define MOD_CHANNELS	4
#define MOD_ROW_SIZE	(MOD_CHANNELS * 4)
#define MOD_PAT_SIZE	(AVM_ROWS * MOD_ROW_SIZE)

/*Function prototypes*/
void AVMProc(int bank, char parameters[4][100]);

