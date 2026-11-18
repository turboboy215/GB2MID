/*Konami (original driver/KCE Nagoya)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <stddef.h>
#include <math.h>

/*The Castlevania Adventure*/
#define KONAMI_VER_CV1		1
/*Motocross Maniacs*/
#define KONAMI_VER_MM		2
/*Nemesis*/
#define KONAMI_VER_NEM1		3
/*Castlevania II: Belmont's Revenge*/
#define KONAMI_VER_CV2		4
/*Nemesis II: The Return of the Hero*/
#define KONAMI_VER_NEM2		5
/*Hyper Dunk*/
#define KONAMI_VER_HD		6
/*Teenage Mutant Ninja Turtles II: Back from the Sewers*/
#define KONAMI_VER_TMNT2	7
/*Ganbare Goemon: Sarawareta Ebisumaru!*/
#define KONAMI_VER_GOEMON	8
/*Parodius*/
#define KONAMI_VER_PD		9
/*Tiny Toon Adventures: Babs' Big Break*/
#define KONAMI_VER_TTA1		10
/*Kid Dracula*/
#define KONAMI_VER_KD		11
/*Tiny Toon Adventures 2: Montana's Movie Madness*/
#define KONAMI_VER_TTA2		12
/*Batman: The Animated Series*/
#define KONAMI_VER_BATMAN	13
/*Raging Fighter*/
#define KONAMI_VER_RF		14
/*Zen: Intergalactic Ninja*/
#define KONAMI_VER_ZEN		15
/*Tiny Toon Adventures: Wacky Sports*/
#define KONAMI_VER_TTA3		16
/*Castlevania Legends*/
#define KONAMI_VER_CVL		17
/*Power Pro GB*/
#define KONAMI_VER_PPGB		18
/*International Superstar Soccer '99*/
#define KONAMI_VER_ISS99	19
/*World Soccer GB2 (ISS '99 JP)*/
#define KONAMI_VER_ISS99J	20

enum KonamiEventTypes
{
	KONAMI_EVENT_UNKNOWN0 = 0,
	KONAMI_EVENT_UNKNOWN1,
	KONAMI_EVENT_NOP,
	KONAMI_EVENT_NOP1,
	KONAMI_EVENT_INS_PARAMS,
	KONAMI_EVENT_OCTAVE,
	KONAMI_EVENT_NOTE_LEN,
	KONAMI_EVENT_RELEASE,
	KONAMI_EVENT_SWEEP,
	KONAMI_EVENT_SUB_COMMAND,
	KONAMI_EVENT_TEMPO,
	KONAMI_EVENT_PAN,
	KONAMI_EVENT_ATTENUATION,
	KONAMI_EVENT_REPEAT_START,
	KONAMI_EVENT_RETURN,
	KONAMI_EVENT_CALL,
	KONAMI_EVENT_REPEAT_END,
	KONAMI_EVENT_STOP,
	KONAMI_EVENT_DUTY_STEP,
	KONAMI_EVENT_AUTOPAN_ON,
	KONAMI_EVENT_VIBRATO,
	KONAMI_EVENT_REPEAT1,
	KONAMI_EVENT_REPEAT2,
	KONAMI_EVENT_SONG_LOOP,
	KONAMI_EVENT_ED,
	KONAMI_EVENT_TUNING,
	KONAMI_EVENT_VOL1,
	KONAMI_EVENT_VOL2,
	KONAMI_EVENT_JUMP,
	KONAMI_EVENT_VOL_ENV,
	KONAMI_EVENT_COND_JUMP,
	KONAMI_EVENT_OCTAVE_SHIFT,
	KONAMI_EVENT_WAVE_ALT,
	KONAMI_EVENT_GLOBAL_VOL,
	KONAMI_EVENT_VOL,
	KONAMI_EVENT_FADE,
	KONAMI_EVENT_BREAK,
};

unsigned int KONAMI_STATUS_NOTE_MIN;
unsigned int KONAMI_STATUS_NOTE_MAX;
unsigned int KONAMI_STATUS_REST_MIN;
unsigned int KONAMI_STATUS_REST_MAX;
unsigned int KONAMI_STATUS_ATTEN_MIN;
unsigned int KONAMI_STATUS_ATTEN_MAX;

unsigned int KONAMI_SUB_COMMAND_TRANSPOSE_UP;
unsigned int KONAMI_SUB_COMMAND_TRANSPOSE_DOWN;
unsigned int KONAMI_SUB_COMMAND_VIBRATO;
unsigned int KONAMI_SUB_COMMAND_DUTY_WAVE;
unsigned int KONAMI_SUB_COMMAND_AUTOPAN;
unsigned int KONAMI_SUB_COMMAND_GLIDE;
unsigned int KONAMI_SUB_COMMAND_PAN_LR;
unsigned int KONAMI_SUB_COMMAND_PAN_R;
unsigned int KONAMI_SUB_COMMAND_PAN_L;
unsigned int KONAMI_SUB_COMMAND_AUTOPAN2;
unsigned int KONAMI_SUB_COMMAND_AUTOPAN_OFF;
unsigned int KONAMI_SUB_COMMAND_RELEASE_VOL;
unsigned int KONAMI_SUB_COMMAND_CLEAR;
unsigned int KONAMI_SUB_COMMAND_TRANSPOSE;
unsigned int KONAMI_SUB_COMMAND_TEMPO;
unsigned int KONAMI_SUB_COMMAND_VOL_RAMP;
unsigned int KONAMI_SUB_COMMAND_WOBBLE;
unsigned int KONAMI_SUB_COMMAND_HOLD;
unsigned int KONAMI_SUB_COMMAND_PANFLAGS;
unsigned int KONAMI_SUB_COMMAND_GLOBAL_VOL;
unsigned int KONAMI_SUB_COMMAND_NOP;

unsigned int EventMap[256];

/*Function prototypes*/
void KonProc(int bank, char parameters[4][100]);