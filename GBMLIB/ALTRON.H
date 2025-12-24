/*Altron*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define ALTRON_VER_ROX	1
#define ALTRON_VER_LM	2

enum AltronEventTypes
{
	/*Version 1 (Rox)*/
	ALTRON_EVENT_UNKNOWN0,
	ALTRON_EVENT_OCTAVE_DOWN,
	ALTRON_EVENT_OCTAVE_UP,
	ALTRON_EVENT_RESTART,
	ALTRON_EVENT_NOTE_LEN,
	ALTRON_EVENT_OCTAVE,
	ALTRON_EVENT_REST,
	ALTRON_EVENT_END,
	ALTRON_EVENT_NOTEC,
	ALTRON_EVENT_NOTED,
	ALTRON_EVENT_NOTEE,
	ALTRON_EVENT_NOTEF,
	ALTRON_EVENT_NOTEG,
	ALTRON_EVENT_NOTEA,
	ALTRON_EVENT_NOTEB,
	ALTRON_EVENT_NOTECS,
	ALTRON_EVENT_NOTEDS,
	ALTRON_EVENT_NOTEFS,
	ALTRON_EVENT_NOTEGS,
	ALTRON_EVENT_NOTEAS,

	/*Version 2 (Little Magic)*/
	ALTRON_EVENT_NOTE,
};

unsigned int ALTRON_STATUS_NOTE_MIN;
unsigned int ALTRON_STATUS_NOTE_MAX;
unsigned int ALTRON_STATUS_NOTE_LEN_MIN;
unsigned int ALTRON_STATUS_NOTE_LEN_MAX;

unsigned int AltronEventMap[256];

/*Function prototypes*/
void AltronProc(int bank, char parameters[4][100]);