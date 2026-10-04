/*Unknown (Climax?) (The Smurfs Travel the World)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define SMURFS2_VER_STD			0

enum Smurfs2EventTypes
{
	SMURFS2_EVENT_UNKNOWN0 = 0,
	SMURFS2_EVENT_UNKNOWN1,
	SMURFS2_EVENT_NOP,
	SMURFS2_EVENT_NOP1,
	SMURFS2_EVENT_STOP,
	SMURFS2_EVENT_PROG_CHANGE,
	SMURFS2_EVENT_TRANSPOSE,
};

unsigned int SMURFS2_STATUS_NOTE_MIN;
unsigned int SMURFS2_STATUS_NOTE_MAX;
unsigned int EventMap[256];

/*Function prototypes*/
void Smurfs2Proc(int bank);