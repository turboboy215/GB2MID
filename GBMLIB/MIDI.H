/*General MIDI (Loriciel - Best of the Best)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/*Function prototypes*/

extern void MIDProc();
extern long extMID(int songNum, long ptr);

extern int fileEnd;
extern int midEnd;
extern long filePos;
extern long midPos;

extern const char midHeader[4];
extern const char trackEnd[3];