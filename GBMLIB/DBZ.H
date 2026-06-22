/*Unknown (Dragon Ball Z: Legendary Super Warriors)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define DBZ_VER_STD		1

enum DBZEventTypes
{
	DBZ_EVENT_UNKNOWN0 = 1,
	DBZ_EVENT_UNKNOWN1,
	DBZ_EVENT_OCTAVE,
	DBZ_EVENT_OCTAVE_UP,
	DBZ_EVENT_OCTAVE_DOWN,
	DBZ_EVENT_PROG_CHANGE,
	DBZ_EVENT_MASTER_VOLUME,
	DBZ_EVENT_VOLUME,
	DBZ_EVENT_PAN,
	DBZ_EVENT_TUNING,
	DBZ_EVENT_STOP,
	DBZ_EVENT_CLEAR_RAM,
	DBZ_EVENT_TEMPO,
	DBZ_EVENT_INS_PARAM,
	DBZ_EVENT_REPEAT_START,
	DBZ_EVENT_REPEAT_END,
	DBZ_EVENT_REG_WRITE,
	DBZ_EVENT_LOOP,
	DBZ_EVENT_GOTO_LOOP,
	DBZ_EVENT_SFX_F5,
	DBZ_EVENT_SFX_F6,
	DBZ_EVENT_SFX_F7,
	DBZ_EVENT_SFX_F8,
};

unsigned int DBZ_STATUS_NOTE_MIN;
unsigned int DBZ_STATUS_NOTE_MAX;

unsigned int EventMap[256];

void DBZProc(int bank);