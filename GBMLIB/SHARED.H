#include <stdio.h>

/*Sound engine/format codes*/
#define MIDI                0x00
#define Mark_Cooksey        0x01
#define Tiertex             0x02
#define GHX                 0x03
#define AudioArts           0x04
#define Climax              0x05
#define Hirokazu_Tanaka     0x06
#define Martin_Walker       0x07
#define Hiroshi_Wada        0x08
#define David_Whittaker     0x09
#define Paragon_5           0x0B
#define Konami              0x0C
#define David_Warhol		0x0D
#define AJ_Gonzalez			0x0E
#define RARE				0x0F
#define Hirotomo_Nakamura	0x10
#define Kouji_Murata		0x11
#define Capcom				0x12
#define	HAL_Laboratory		0x13
#define Ryohji_Yoshitomi	0x14
#define Kozue_Ishikawa		0x15
#define Imagineering		0x16
#define Ocean				0x17
#define Game_Freak			0x18
#define Jeroen_Tel			0x19
#define Technos_Japan		0x1A
#define	NMK					0x1B
#define Probe_Software		0x1C
#define TOSE				0x1D
#define David_Shea			0x1E
#define Natsume				0x1F
#define Beam_Software		0x20
#define Hudson_Soft			0x21
#define MusyX				0x22
#define Sunsoft				0x23
#define Titus_1				0x24
#define Kyouhei_Sada		0x25
#define Titus_2				0x26
#define Square				0x27
#define Digital_Eclipse_1	0x28
#define Lufia				0x29
#define Cube				0x2A
#define MPlay				0x2B
#define Junichi_Saito		0x2C
#define Carillon_Player		0x2D
#define Sheep				0x2E
#define Cosmigo				0x2F
#define Make_Software		0x30
#define Tarantula_Studios	0x31
#define Ed_Magnin			0x32

extern FILE * rom, * mid;
extern long masterBank;
extern long banks[50];
extern int i, j, k;
extern char outfile[1000000];
extern int songNum;
extern int foundTable;
extern int curInst;
extern int curVol;
extern long fileSize;
extern int multiBanks;
extern int curBank;

extern unsigned char* romData;
extern unsigned char* exRomData;
extern unsigned char* midData;
extern unsigned char* ctrlMidData;
extern unsigned char* xmData;
extern unsigned char* endData;

extern long midLength;
extern long xmLength;

/*Function prototypes*/
extern unsigned short ReadLE16(unsigned char* Data);
extern unsigned short ReadBE16(unsigned char* Data);
extern void Write8B(unsigned char* buffer, unsigned int value);
extern void WriteBE32(unsigned char* buffer, unsigned long value);
extern void WriteBE24(unsigned char* buffer, unsigned long value);
extern void WriteBE16(unsigned char* buffer, unsigned int value);
extern void WriteLE16(unsigned char* buffer, unsigned int value);
extern void WriteLE24(unsigned char* buffer, unsigned long value);
extern void WriteLE32(unsigned char* buffer, unsigned long value);
extern unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
extern unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
extern unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
extern unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
extern int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
extern void gb2MID(FILE* rom, long banks[50], int numBanks, long format, char parameters[4][50]);