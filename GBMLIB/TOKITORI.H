/*Unknown (Toki Tori)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define TOKITORI_VER_STD			0

#define TT_CHANNELS		4
#define TT_MAX_ROWS		64
#define MOD_ROW_SIZE	(TT_CHANNELS * 4)
#define MOD_PAT_SIZE	(MOD_ROW_SIZE * 64)
#define MOD_MAX_PATS	64

/*Sample slots: each channel type gets its own samples, so each can have its own length
  (= its own octave range)*/
#define SMP_CH1		1		/*1-4: ch1 duty 12.5/25/50/75 %*/
#define SMP_CH2		5		/*5-8: ch2 duty*/
#define SMP_CH3		9		/*9-24: ch3 wave 0-15*/
#define SMP_CH4		25		/*25-31: ch4, one per NR43 value*/
#define SMP_CH4_NUM	7

enum TokiToriEventTypes
{
	TOKITORI_EVENT_UNKNOWN0 = 0,
	TOKITORI_EVENT_NOP,
	TOKITORI_EVENT_ARPEGGIO,
	TOKITORI_EVENT_SLIDE_UP,
	TOKITORI_EVENT_SLIDE_DOWN,
	TOKITORI_EVENT_TONE_PORTAMENTO,
	TOKITORI_EVENT_VIBRATO,
	TOKITORI_EVENT_PORTAMENTO_VOL_SLIDE,
	TOKITORI_EVENT_VIBRATO_VOL_SLIDE,
	TOKITORI_EVENT_TREMOLO,
	TOKITORI_EVENT_VOL_SLIDE,
	TOKITORI_EVENT_JUMP,
	TOKITORI_EVENT_VOL,
	TOKITORI_EVENT_PAT_BREAK,
	TOKITORI_EVENT_FINE_SLIDE,
	TOKITORI_EVENT_SPEED,
};

unsigned int EventMap[256];

/*Function prototypes*/
void TokiToriProc(int bank);