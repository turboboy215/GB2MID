/*HAL Laboratory*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define HAL_VER_KDL		1
#define HAL_VER_KDL2	2
#define HAL_VER_KSS		3
#define HAL_VER_LOLO	4
#define HAL_VER_VS		5
#define HAL_VER_GB2		6

enum HALEventTypes
{
	HAL_EVENT_UNKNOWN0 = 0,
	HAL_EVENT_NOP,
	HAL_EVENT_SYNC,
	HAL_EVENT_PERIOD,
	HAL_EVENT_TUNING,
	HAL_EVENT_VOL,
	HAL_EVENT_VOL_ADD,
	HAL_EVENT_NOTE_LEN_TAB,
	HAL_EVENT_TIE,
	HAL_EVENT_GATE,
	HAL_EVENT_BASE_NOTE,
	HAL_EVENT_PROG_CHANGE,
	HAL_EVENT_TUNING_TABLE,
	HAL_EVENT_JUMP,
	HAL_EVENT_CALL,
	HAL_EVENT_RETURN,
	HAL_EVENT_REPEAT_START,
	HAL_EVENT_REPEAT_END,
	HAL_EVENT_PAN,
	HAL_EVENT_STOP,
	HAL_EVENT_ENV,
	HAL_EVENT_DUTY,
	HAL_EVENT_ATTENUATE,
	HAL_EVENT_TRANSPOSE,
	HAL_EVENT_SWEEP_ON,
	HAL_EVENT_SWEEP_OFF,
	HAL_EVENT_SFX,
};

unsigned int HAL_STATUS_NOTE_MIN;
unsigned int HAL_STATUS_NOTE_MAX;
unsigned int HAL_STATUS_REST;
unsigned int HAL_STATUS_NOTE_LEN_MIN;
unsigned int HAL_STATUS_NOTE_LEN_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void HALProc(int bank, char parameters[4][100]);