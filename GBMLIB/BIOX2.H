/*Biox #2*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/*Burger Burger Pocket*/
#define BIOX2_VER_BBP	1
/*Daiku no Gensan: Kachikachi no Tonkachi ga Kachi*/
#define BIOX2_VER_GS	2

enum Biox2EventTypes
{
	BIOX2_EVENT_UNKNOWN0 = 0,
	BIOX2_EVENT_NOP,
	BIOX2_EVENT_NOISE,
	BIOX2_EVENT_TIE,
	BIOX2_EVENT_REST,
	BIOX2_EVENT_STOP,
	BIOX2_EVENT_JUMP,
	BIOX2_EVENT_CALL,
	BIOX2_EVENT_RETURN,
	BIOX2_EVENT_REPEAT_START,
	BIOX2_EVENT_REPEAT_END,
	BIOX2_EVENT_TEMPO,
	BIOX2_EVENT_ENV,
	BIOX2_EVENT_PITCH_ENV,
	BIOX2_EVENT_DUTY,
	BIOX2_EVENT_PAN,
	BIOX2_EVENT_TRANSPOSE,
	BIOX2_EVENT_NRX2_HIGH,
	BIOX2_EVENT_NRX2_LOW,
	BIOX2_EVENT_OCTAVE_ENV,
	BIOX2_EVENT_WAVEFORM,
	BIOX2_EVENT_WAVEFORM_RESTORE,
	BIOX2_EVENT_CH3_LEN,
};

unsigned int BIOX2_STATUS_NOTE_LEN_MIN;
unsigned int BIOX2_STATUS_NOTE_LEN_MAX;
unsigned int BIOX2_STATUS_NOTE_MIN;
unsigned int BIOX2_STATUS_NOTE_MAX;
unsigned int BIOX2_STATUS_DRUM_MIN;
unsigned int BIOX2_STATUS_DRUM_MAX;

unsigned int EventMap[256];

void Biox2Proc(int bank, char parameters[4][100]);