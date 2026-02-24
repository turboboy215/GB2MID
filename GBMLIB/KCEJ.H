/*Konami (KCE Japan)*/

#define KCEJ_VER_HXH	1
#define KCEJ_VER_YGO	2

enum KCEJEventTypes
{
	KCEJ_EVENT_UNKNOWN0 = 1,
	KCEJ_EVENT_NOP,
	KCEJ_EVENT_NOTE_ON,
	KCEJ_EVENT_NOTE_OFF,
	KCEJ_EVENT_VOLUME,
	KCEJ_EVENT_SONG_LOOP,
	KCEJ_EVENT_STOP,
	KCEJ_EVENT_GOTO_LOOP,
};

unsigned int KCEJ_STATUS_DUR_MIN;
unsigned int KCEJ_STATUS_DUR_MAX;

unsigned int EventMap[256];

void KCEJProc(int bank, char parameters[4][100]);