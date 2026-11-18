/*Xantera*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define XANTERA_VER_STD		1

enum XanteraEventTypes
{
	XANTERA_EVENT_CH1_NOTE,
	XANTERA_EVENT_CH2_NOTE,
	XANTERA_EVENT_CH3_NOTE,
	XANTERA_EVENT_CH4_NOTE,
	XANTERA_EVENT_CH3_OFF,
	XANTERA_EVENT_CH3_VOL,
	XANTERA_EVENT_CH3_WAVEFORM,
	XANTERA_EVENT_PITCH_MOD,
	XANTERA_EVENT_STOP,
};

unsigned int EventMap[256];

/*Function prototypes*/
void XanteraProc(int bank, char parameters[4][100]);