/*archOnPlayer (by Stello Doussis)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define ARCHON_SONG_TABLE 0x406A

enum archOnEventTypes
{
	ARCHON_EVENT_PITCH_BEND = 1,
	ARCHON_EVENT_ADD_WAVE,
	ARCHON_EVENT_MASTER_VOLUME,
	ARCHON_EVENT_BAR_END,
};

unsigned int ARCHON_STATUS_REST;
unsigned int ARCHON_STATUS_PROG_CHANGE_MIN;
unsigned int ARCHON_STATUS_PROG_CHANGE_MAX;

unsigned int ARCHON_TRACK_TRANSPOSE_UP_MIN;
unsigned int ARCHON_TRACK_TRANSPOSE_UP_MAX;
unsigned int ARCHON_TRACK_TRANSPOSE_DOWN_MIN;
unsigned int ARCHON_TRACK_TRANSPOSE_DOWN_MAX;
unsigned int ARCHON_TRACK_JUMP;
unsigned int ARCHON_TRACK_END;
unsigned int ARCHON_MASTER_TRACK_END;

unsigned int ArchonEventMap[256];

/*Function prototypes*/
void archOnProc(int bank);