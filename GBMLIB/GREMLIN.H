/*Gremlin*/

#define GREMLIN_VER_STD		1

enum GremlinEventTypes
{
	GREMLIN_EVENT_UNKNOWN0 = 0,
	GREMLIN_EVENT_VOL,
	GREMLIN_EVENT_STOP,
};

unsigned int GREMLIN_STATUS_NOTE_MIN;
unsigned int GREMLIN_STATUS_NOTE_MAX;
unsigned int GREMLIN_STATUS_PROG_CHANGE_MIN;
unsigned int GREMLIN_STATUS_PROG_CHANGE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void GremlinProc(int bank);