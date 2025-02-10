/*Absolute/Imagineering*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "IMAGNRNG.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
long patOffset;
long tempoLoc;
long tempoOffset;
int i, j;
char outfile[1000000];
int format;
int cmdFmt;
int songNum;
long songPtrs[4];
long patPtrs[4];
long patList[4][50];
long bankAmt;
int seqDiff;
int foundTable;
int foundTempoTable;
int songTempo;
int highestSeq;
int curVol;

unsigned long seqList[500];
unsigned char* romData;
unsigned char* midData;
unsigned char* ctrlMidData;
long midLength;
int totalSeqs;

const char ImgnTableFind[5] = { 0x0E, 0x08, 0x2A, 0x12 };
const char ImgnFindTempo[5] = { 0x22, 0x22, 0x22, 0x77, 0xEA };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Imgnsong2mid(int songNum, long ptrs[4], int tempo);
void ImgnProc(int bank);

void ImgnProc(int bank)
{
	foundTable = 0;
	foundTempoTable = 0;
	if (bank != 1)
	{
		bankAmt = bankSize;
		fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
		romData = (unsigned char*)malloc(bankSize);
		fread(romData, 1, bankSize, rom);
	}
	else
	{
		bankAmt = 0;
		/*Banks 1 and 8 are used together for Berlitz Translator series*/
		fseek(rom, 0, SEEK_SET);
		romData = (unsigned char*)malloc(bankSize * 2);
		fread(romData, 1, bankSize, rom);
		fseek(rom, (7 * bankSize), SEEK_SET);
		fread(romData + bankSize, 1, bankSize, rom);
	}

	/*Try to search the bank for song pattern table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], ImgnTableFind, 4)) && foundTable == 0 && romData[i - 3] == 0x11)
		{
			tablePtrLoc = bankAmt + i - 6;
			printf("Found pointer to song table at address 0x%04x!\n", tablePtrLoc);
			tableOffset = ReadLE16(&romData[tablePtrLoc - bankAmt]);
			printf("Song table starts at 0x%04x...\n", tableOffset);
			foundTable = 1;
			break;
		}
	}

	/*Now find the tempos*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], ImgnFindTempo, 5)) && foundTempoTable == 0)
		{
			tempoLoc = bankAmt + i + 20;
			printf("Found pointer to song tempo table at address 0x%04x!\n", tempoLoc);
			tempoOffset = ReadLE16(&romData[tempoLoc - bankAmt]);
			printf("Song tempo table starts at 0x%04x...\n", tempoOffset);
			foundTempoTable = 1;
			break;
		}
	}


	if (foundTable == 1 && foundTempoTable == 1)
	{
		i = tableOffset + 8 - bankAmt;
		j = tempoOffset + 1 - bankAmt;
		songNum = 1;
		while ((ReadLE16(&romData[i]) > 0x100 && ReadLE16(&romData[i]) < bankSize * 2) && (ReadLE16(&romData[i + 2]) > 0x100 && ReadLE16(&romData[i + 2]) < bankSize * 2) && (ReadLE16(&romData[i + 4]) > 0x100 && ReadLE16(&romData[i + 4]) < bankSize * 2) && (ReadLE16(&romData[i + 6]) > 0x100 && ReadLE16(&romData[i + 6]) < bankSize * 2))
		{
			j = tempoOffset + songNum - bankAmt;
			songPtrs[0] = ReadLE16(&romData[i]);
			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			songPtrs[1] = ReadLE16(&romData[i + 2]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			songPtrs[2] = ReadLE16(&romData[i + 4]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			songPtrs[3] = ReadLE16(&romData[i + 6]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			songTempo = romData[j];
			printf("Song %i tempo: %i\n", songNum, songTempo);
			Imgnsong2mid(songNum, songPtrs, songTempo);
			i += 8;
			j++;
			songNum++;
		}
		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}

}

void Imgnsong2mid(int songNum, long ptrs[4], int tempo)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int chanPos = 0;
	int patPos = 0;
	int curPat = 0;
	int seqPos = 0;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int trackCnt = 4;
	int ticks = 120;
	long curSeq = 0;
	int curNoteSize = 0;
	long command[3];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int curTrack = 0;
	int endSeq = 0;
	int endChan = 0;
	int curTempo = tempo;
	int transpose = 0;
	int globalTranspose = 0;
	unsigned int curNote = 0;
	int curNoteLen = 0;
	unsigned int curDelay = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;
	long jumpPos = 0;
	int firstNote = 1;
	int timeVal = 0;
	int holdNote = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	int curInst = 0;

	int valSize = 0;

	long trackSize = 0;

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

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / (tempo * 1.1));
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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			endChan = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;

			masterDelay = 0;

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

			/*Get pointers to each sequence from pattern*/
			endChan = 0;

			chanPos = ptrs[curTrack] - bankAmt;

			while (ReadLE16(&romData[chanPos]) != 0x0000 && ReadLE16(&romData[chanPos]) != 0x00FF && ReadLE16(&romData[chanPos]) != 0x0001 && ReadLE16(&romData[chanPos]) != 0x0002 && ReadLE16(&romData[chanPos]) != 0x0003)
			{
				/*All games with this configuration only use channels 1 and 2*/
				if (bank == 1 && curTrack > 1)
				{
					break;
				}

				patPos = ReadLE16(&romData[chanPos]);
				while (ReadLE16(&romData[patPos - bankAmt]) != 0x0000)
				{
					curSeq = ReadLE16(&romData[patPos - bankAmt]);
					seqPos = curSeq;
					endSeq = 0;

					if (curSeq == 0x010F)
					{
						curDelay = 960;
						endSeq = 1;
					}

					if (curSeq < 0x1000)
					{
						break;
					}

					while (endSeq == 0 && seqPos < bankSize * 2 && seqPos > 0x1000)
					{
						command[0] = romData[seqPos - bankAmt];
						command[1] = romData[seqPos + 1 - bankAmt];
						command[2] = romData[seqPos + 2 - bankAmt];

						/*End of sequence*/
						if (command[0] == 0x00)
						{
							endSeq = 1;
						}

						/*Change instrument, effect*/
						else if (command[0] == 0xFF)
						{
							curInst = command[1];
							if (command[1] >= 128)
							{
								curInst = 127;
							}
							firstNote = 1;
							seqPos += 3;
						}

						/*Play note*/
						else
						{
							curNote = command[1] + 9;
							if (curTrack == 3)
							{
								curNote += 9;
							}
							curNoteLen = command[0] * 10;
							curNoteSize = command[2] * 10;

							/*Rest*/
							if (command[1] == 0x3F)
							{
								curDelay += (command[0] * 10);
							}

							else
							{
								/*Fix for A Boy and His Blob title theme channel 2 chord*/
								if (curNote > 113)
								{
									curNote -= 40;
								}
								if (curNoteLen >= curNoteSize)
								{
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteSize, curDelay, firstNote, curTrack, curInst);
									curDelay = curNoteLen - curNoteSize;
								}
								else
								{
									tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
									curDelay = 0;
								}

								firstNote = 0;
								midPos = tempPos;


							}
							seqPos += 3;

						}
					}
					patPos += 2;
				}
				chanPos += 2;
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