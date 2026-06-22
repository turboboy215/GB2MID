/*Reeb*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define REEB_VER_KPSM	1
#define REEB_VER_SPPS	2
#define REEB_VER_QIX	3

enum ReebEventTypes
{
	REEB_EVENT_UNKNOWN0 = 0,
	REEB_EVENT_UNKNOWN1,
	REEB_EVENT_SET_SPEED,
	REEB_EVENT_DUTY,
	REEB_EVENT_LENGTH_CH3,
	REEB_EVENT_ENV_DIR,
	REEB_EVENT_WAVEFORM,
	REEB_EVENT_TIE_ON,
	REEB_EVENT_TIE_OFF,
	REEB_EVENT_VOLUME,
	REEB_EVENT_VIBRATO,
	REEB_EVENT_GLOBAL_VOLUME,
	REEB_EVENT_SWEEP,
	REEB_EVENT_PAN,
	REEB_EVENT_JUMP,
	REEB_EVENT_MACRO_F8,
	REEB_EVENT_RETURN,
};

unsigned int REEB_STATUS_NOTE_MIN;
unsigned int REEB_STATUS_NOTE_MAX;
unsigned int REEB_STATUS_MACRO_MIN;
unsigned int REEB_STATUS_MACRO_MAX;

unsigned int EventMap[256];

void ReebProc(int bank, char parameters[4][100]);