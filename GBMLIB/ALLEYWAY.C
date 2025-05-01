/*Nintendo - Unknown (Alleyway/Baseball)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "ALLEYWAY.H"

#define bankSize 16384

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long seqPtrs[4];
long songPtr;
long bankAmt;
int foundTable;
int curInst;
long firstPtr;
int game;
int numSongs;

unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

long AWPtrs[12][3] =
{ {0x0000, 0x75E3, 0x7652},
  {0x0000, 0x76C3, 0x76D9},
  {0x0000, 0x76F0, 0x7712},
  {0x0000, 0x7733, 0x7738},
  {0x0000, 0x773B, 0x7750},
  {0x0000, 0x7765, 0x779B},
  {0x0000, 0x77D7, 0x780D},
  {0x0000, 0x7849, 0x785C},
  {0x0000, 0x7875, 0x7887},
  {0x0000, 0x789C, 0x78D6},
  {0x0000, 0x790F, 0x7919},
  {0x0000, 0x791F, 0x7988 } };

long BBPtrs[11][3] =
{ {0x0000, 0x7670, 0x76DA},
	{0x7751, 0x7794, 0x77D9},
	{0x0000, 0x798F, 0x79E6},
	{0x7A4D, 0x7A6C, 0x7A8B},
	{0x0000, 0x7699, 0x76F1},
	{0x7AAF, 0x7AEC, 0x7B24},
	{0x7B6C, 0x7BAB, 0x7BEA},
	{0x7C66, 0x7C71, 0x7C7C},
	{0x781E, 0x7834, 0x7848},
	{0x0000, 0x785E, 0x78F7},
	{0x0000, 0x7515, 0x75B3}
};

unsigned int noteLensAW[0x2C] = { 0x04, 0x08, 0x10, 0x20, 0x40, 0x0C, 0x18, 0x30, 0x05, 0x06, 0x0B,
0x0A, 0x05, 0x0A, 0x14, 0x28, 0x50, 0x0F, 0x1E, 0x3C, 0x07, 0x06, 0x02, 0x01, 0x03, 0x06, 0x0C,
0x18, 0x30, 0x09, 0x12, 0x24, 0x04, 0x04, 0x0B, 0x0A, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x12, 0x24, 0x48 };

unsigned int noteLensBB[0x2E] = { 0x04, 0x08, 0x10, 0x20, 0x40, 0x0C, 0x18, 0x30, 0x05, 0x06, 0x0B,
0x0A, 0x05, 0x0A, 0x14, 0x28, 0x50, 0x0F, 0x1E, 0x3C, 0x07, 0x06, 0x02, 0x01, 0x07, 0x0E, 0x1C,
0x38, 0x70, 0x15, 0x2A, 0x54, 0x09, 0x0A, 0x0B, 0x0A, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x12, 0x24, 0x48, 0x08, 0x03 };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Alleysong2mid(int songNum, long ptrs[4]);

void AlleyProc(int bank)
{
	foundTable = 0;
	curInst = 0;
	game = 0;

	if (bank != 1)
	{
		bankAmt = bankSize;
	}
	else
	{
		bankAmt = 0;
	}
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	if (ReadLE16(&romData[0x2801]) == 0x69E7)
	{
		game = 1;
	}

	else if (ReadLE16(&romData[0x2781]) == 0x679A)
	{
		game = 2;
	}

	if (game == 1 || game == 2)
	{
		if (game == 1)
		{
			numSongs = 12;
		}
		else if (game == 2)
		{
			numSongs = 11;
		}

		songNum = 1;
		for (i = 0; i < numSongs; i++)
		{
			if (game == 1)
			{
				seqPtrs[0] = AWPtrs[i][0];
				seqPtrs[1] = AWPtrs[i][1];
				seqPtrs[2] = AWPtrs[i][2];
			}
			else if (game == 2)
			{
				seqPtrs[0] = BBPtrs[i][0];
				seqPtrs[1] = BBPtrs[i][1];
				seqPtrs[2] = BBPtrs[i][2];
			}
			printf("Song %i, channel 1: 0x%04X\n", songNum, seqPtrs[0]);
			printf("Song %i, channel 2: 0x%04X\n", songNum, seqPtrs[1]);
			printf("Song %i, channel 3: 0x%04X\n", songNum, seqPtrs[2]);
			Alleysong2mid(songNum, seqPtrs);
			songNum++;
		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		fclose(rom);
		free(romData);
		exit(2);
	}


}

/*Convert the song data to MIDI*/
void Alleysong2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 3;
	int ticks = 120;
	int tempo = 150;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int chanSpeed = 0;
	unsigned char command[2];
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int rest = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	long startPos = 0;

	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
		ctrlMidData[j] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], trackCnt + 1);
		WriteBE16(&ctrlMidData[ctrlMidPos + 4], ticks);
		ctrlMidPos += 6;
		/*Write initial MIDI information for "control" track*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D54726B);
		ctrlMidPos += 8;
		ctrlMidTrackBase = ctrlMidPos;

		/*Set channel name (blank)*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE16(&ctrlMidData[ctrlMidPos], 0xFF03);
		Write8B(&ctrlMidData[ctrlMidPos + 2], 0);
		ctrlMidPos += 2;

		/*Set initial tempo*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
		ctrlMidPos += 3;

		/*Set time signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5804);
		ctrlMidPos += 3;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x04021808);
		ctrlMidPos += 4;

		/*Set key signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5902);
		ctrlMidPos += 4;

		for (curTrack == 0; curTrack < 3; curTrack++)
		{
			firstNote = 1;
			chanSpeed = 0;
			curDelay = 0;
			ctrlDelay = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;


			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (seqPtrs[curTrack] != 0x0000)
			{
				seqPos = seqPtrs[curTrack] - bankAmt;
				seqEnd = 0;

				while (seqEnd == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];

					/*End of channel (no loop)*/
					if (command[0] == 0x00)
					{
						seqEnd = 1;
					}

					/*Rest*/
					else if (command[0] == 0x01)
					{
						curDelay += (chanSpeed * 5);
						seqPos++;
					}

					/*Play note*/
					else if (command[0] >= 0x02 && command[0] <= 0x7E)
					{
						curNote = command[0] + 22;
						curNoteLen = chanSpeed * 5;
						tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
						firstNote = 0;
						midPos = tempPos;
						curDelay = 0;
						seqPos++;
					}

					/*End of channel (loop)*/
					else if (command[0] == 0x7F)
					{
						seqEnd = 1;
					}

					/*Set note size*/
					else if (command[0] >= 0x80)
					{
						if (game == 1)
						{
							chanSpeed = noteLensAW[command[0] - 0x80];
						}
						else if (game == 2)
						{
							chanSpeed = noteLensBB[command[0] - 0x80];
						}

						seqPos++;
					}
				}
			}

			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);
		}

		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		trackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], trackSize);

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}