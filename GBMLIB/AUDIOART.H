/*AudioArts*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define AA_VER_CARMA	0
#define AA_VER_OLD		1
#define AA_VER_NEW		2
#define AA_VER_CASPU	3

#define AA_TRACK_VER8	1
#define AA_TRACK_VER16	2

enum AAEventTypes
{
	AA_EVENT_UNKNOWN0 = 0,
	AA_EVENT_HOLD,
	AA_EVENT_KEY_OFF,
	AA_EVENT_KEY_ON,
};

unsigned int AA_STATUS_END;
unsigned int AA_STATUS_JUMP;
unsigned int AA_STATUS_NOTE_MIN;
unsigned int AA_STATUS_NOTE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void AAProc(int bank);