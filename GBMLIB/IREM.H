/*Irem/Tamtex/Lenar*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/*Shisenshou: Match-Mania*/
#define IREM_VER_SS			0
/*Knight Quest*/
#define IREM_VER_KQ			1
/*Kid Niki*/
#define IREM_VER_KN			2
/*Hammerin' Harry: Ghost Building Company*/
#define IREM_VER_HH			3

enum IremEventTypes
{
	IREM_EVENT_UNKNOWN0 = 0,
	IREM_EVENT_UNKNOWN1,
	IREM_EVENT_NOP,
	IREM_EVENT_NOP1,
	IREM_EVENT_DUTY,
	IREM_EVENT_SWEEP,
	IREM_EVENT_TRANSPOSE,
	IREM_EVENT_ENV,
	IREM_EVENT_GLOBAL_VOL,
	IREM_EVENT_CALL,
	IREM_EVENT_RETURN,
	IREM_EVENT_STOP,
	IREM_EVENT_NRX4,
	IREM_EVENT_TIE,
	IREM_EVENT_MUS_PAN,
	IREM_EVENT_JUMP,
	IREM_EVENT_WAVEFORM,
	IREM_EVENT_ENV_SEQ,
	IREM_EVENT_VIBRATO,
	IREM_EVENT_SFX_PAN,
	IREM_EVENT_PAN_MASK,
};

unsigned int IREM_STATUS_NOTE_MIN;
unsigned int IREM_STATUS_NOTE_MAX;
unsigned int IREM_STATUS_SPEED_MIN;
unsigned int IREM_STATUS_SPEED_MAX;

unsigned int EventMap[256];

void IremProc(int bank, char parameters[4][100]);