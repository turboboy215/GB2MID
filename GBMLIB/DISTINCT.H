/*Distinctive Software*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/*Bill Elliott's NASCAR Fast Tracks*/
#define DIST_VER_NF		1
/*Top Gun: Guts & Glory*/
#define DIST_VER_TG		2
/*The Battle of Olympus*/
#define DIST_VER_BO		3
/*Wayne's World*/
#define DIST_VER_WW		4

enum DistEventTypes
{
	DIST_EVENT_UNKNOWN0 = 0,
	DIST_EVENT_UNKNOWN1,
	DIST_EVENT_UNKNOWN2,
	DIST_EVENT_NOP,
	DIST_EVENT_NOP1,
	DIST_EVENT_NOP2,
	DIST_EVENT_NEXT_PAT,
	DIST_EVENT_STOP,
	DIST_EVENT_RESTART,
	DIST_EVENT_PROG_CHANGE,
	DIST_EVENT_DECAY_STAGE,
	DIST_EVENT_TEMPO,
	DIST_EVENT_LOOP_TRACK,
	DIST_EVENT_STOP_TRACK,
};

unsigned int DIST_STATUS_REST;
unsigned int DIST_STATUS_NOTE_MIN;
unsigned int DIST_STATUS_NOTE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void DistProc(int bank, char parameters[4][100]);