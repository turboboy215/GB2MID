/*Hirotomo Nakamura (Giraffe Soft)*/

#define MM2_VER_EARLY	0
#define MM2_VER_STD		1

enum MM2EventTypes
{
	MM2_EVENT_UNKNOWN0 = 0,
	MM2_EVENT_UNKNOWN1,
	MM2_EVENT_TEMPO,
	MM2_EVENT_DUTY,
	MM2_EVENT_VOLUME,
	MM2_EVENT_JUMP,
	MM2_EVENT_PAN,
	MM2_EVENT_RETURN,
	MM2_EVENT_CALL,
	MM2_EVENT_STOP,
};

unsigned int MM2_STATUS_NOTE_MIN;
unsigned int MM2_STATUS_NOTE_MAX;
unsigned int MM2_STATUS_TIE_MIN;
unsigned int MM2_STATUS_TIE_MAX;
unsigned int MM2_STATUS_REST_MIN;
unsigned int MM2_STATUS_REST_MAX;
unsigned int MM2_STATUS_OCTAVE_MIN;
unsigned int MM2_STATUS_OCTAVE_MAX;

unsigned int EventMap[256];

/*Function prototypes*/
void MM2Proc(int bank, char parameters[4][100]);