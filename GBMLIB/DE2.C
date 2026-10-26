/*Digital Eclipse (Kevin Choong)*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include "SHARED.H"
#include "DE2.H"

#include "WAV.H"

#define bankSize 16384

struct header wavHeader;

FILE* rom, * mid, * wav;
long bank;
long offset;
long tableOffset;
int i, j;
char outfile[1000000];
int totalNum;
int curNum;
int curBank;
long songPtrs[4];
int sampStart;
int sampEnd;
long bankAmt;
int curVol;
int curInst;
int drvVers;
int mode;
int freq;
char* argv4;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void DE2song2mid(int songNum, long ptrs[4]);
void DE2sam2wav(int sampNum, long startPtr, long endPtr, int bank, int freq);
int gbFreq2Note(unsigned int freq);
unsigned char gb_read_byte(int bank, int cpu);



void DE2Proc(int bank, char parameters[4][100])
{
	drvVers = DE2_VER_PB;
	curVol = 120;
	curInst = 0;
	mode = DE2_MODE_MUS;

	tableOffset = strtol(parameters[0], NULL, 16);

	/*Extract music to MIDI*/
	if (strcmp(parameters[1], "m") == 0 || strcmp(parameters[1], "M") == 0)
	{
		mode = DE2_MODE_MUS;
	}
	/*Extract samples*/
	else if (strcmp(parameters[1], "s") == 0 || strcmp(parameters[1], "M") == 0)
	{
		mode = DE2_MODE_PCM;
	}
	else
	{
		printf("ERROR: Invalid mode switch!\n");
		exit(1);
	}

	totalNum = strtol(parameters[2], NULL, 16);

	if (bank < 0x02)
	{
		bank = 0x02;
	}

	fseek(rom, 0, SEEK_SET);
	romData = (unsigned char*)malloc(bankSize * 2);
	fread(romData, 1, bankSize, rom);
	fseek(rom, ((bank - 1) * bankSize), SEEK_SET);
	fread(romData + bankSize, 1, bankSize, rom);
	i = tableOffset;
	curNum = 1;

	if (mode == DE2_MODE_MUS)
	{
		if (ReadLE16(&romData[tableOffset + 5]) == 0x0000)
		{
			drvVers = DE2_VER_720;
		}
		else
		{
			drvVers = DE2_VER_PB;
		}

		if (drvVers == DE2_VER_720)
		{
			while (curNum <= totalNum)
			{
				songPtrs[0] = ReadLE16(&romData[i]);
				printf("Song %i channel 1: 0x%04X\n", curNum, songPtrs[0]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				printf("Song %i channel 2: 0x%04X\n", curNum, songPtrs[1]);
				songPtrs[2] = 0x0000;
				songPtrs[3] = 0x0000;
				curBank = romData[i + 4];
				/*Copy data from the song bank*/
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, (curBank * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				printf("Song %i bank: %02X\n", curNum, curBank);
				DE2song2mid(curNum, songPtrs);
				free(exRomData);
				i += 8;
				curNum++;
			}
		}
		else
		{
			while (curNum <= totalNum)
			{
				songPtrs[0] = ReadLE16(&romData[i]);
				printf("Song %i channel 1: 0x%04X\n", curNum, songPtrs[0]);
				songPtrs[1] = ReadLE16(&romData[i + 2]);
				printf("Song %i channel 2: 0x%04X\n", curNum, songPtrs[1]);
				songPtrs[2] = 0x0000;
				songPtrs[3] = ReadLE16(&romData[i + 4]);
				printf("Song %i channel 4: 0x%04X\n", curNum, songPtrs[3]);
				curBank = romData[i + 6];
				/*Copy data from the song bank*/
				fseek(rom, 0, SEEK_SET);
				exRomData = (unsigned char*)malloc(bankSize * 2);
				fread(exRomData, 1, bankSize, rom);
				fseek(rom, (curBank * bankSize), SEEK_SET);
				fread(exRomData + bankSize, 1, bankSize, rom);
				printf("Song %i bank: %02X\n", curNum, curBank);
				DE2song2mid(curNum, songPtrs);
				free(exRomData);
				i += 16;
				curNum++;
			}
		}

	}
	else
	{
		if (tableOffset == 0x35C1 || tableOffset == 0x34E4)
		{
			freq = 4000;
		}
		else
		{
			freq = 8192;
		}
		while (curNum <= totalNum)
		{
			sampStart = ReadLE16(&romData[i + 2]);
			sampEnd = ReadLE16(&romData[i + 4]);
			curBank = romData[i + 6];
			printf("Sample %i: start - 0x%04X, end - 0x%04X, bank %02X\n", curNum, sampStart, sampEnd, curBank);
			DE2sam2wav(curNum, sampStart, sampEnd, curBank, freq);
			i += 8;
			curNum++;
		}
	}

	free(romData);
}

/*Convert the song data to MIDI*/
void DE2song2mid(int songNum, long ptrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	unsigned int midPos = 0;
	int trackCnt = 4;
	int curTrack = 0;
	int curSeq = 0;
	long midTrackBase = 0;
	unsigned int curDelay = 0;
	int midChan = 0;
	int seqEnd = 0;
	int trackEnd = 0;
	int ticks = 120;
	int k = 0;
	int noteFreq = 0;

	unsigned int ctrlMidPos = 0;
	long ctrlMidTrackBase = 0;

	int valSize = 0;

	long trackSize = 0;

	unsigned int curNote = 0;
	int curVol = 0;
	int curNoteLen = 0;
	int lastNote = 0;

	int tempByte = 0;
	long tempPos = 0;

	int curInst = 0;

	unsigned long seqPos = 0;

	unsigned char command[4];
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;

	signed int transpose = 0;

	int firstNote = 1;

	int timeVal = 0;

	int holdNote = 0;
	int octave = 0;
	int noteLenAdd = 0;
	int volVal = 0;
	int tempo = 150;

	midPos = 0;
	ctrlMidPos = 0;
	unsigned int ctrlDelay = 0;
	unsigned int masterDelay = 0;

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

		tempo = 150;

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

		switch (drvVers)
		{
		case DE2_VER_720:
			/*Fall-through*/
		case DE2_VER_PB:
		default:
			DE2_STATUS_STOP = 0xBB;
			DE2_STATUS_REST = 0xFF;
			break;
			free(midData);
			free(ctrlMidData);
			fclose(mid);
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			trackEnd = 0;

			curNote = 0;
			lastNote = 0;
			curNoteLen = 0;
			ctrlDelay = 0;
			masterDelay = 0;
			holdNote = 0;
			curVol = 120;

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

			seqPos = songPtrs[curTrack];
			if (songPtrs[curTrack] != 0x0000)
			{
				seqEnd = 0;
			}
			else
			{
				seqEnd = 1;
			}

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000 && seqPos < 0x8000)
			{
				command[0] = exRomData[seqPos];
				command[1] = exRomData[seqPos + 1];
				command[2] = exRomData[seqPos + 2];

				if (command[0] == DE2_STATUS_STOP)
				{
					seqEnd = 1;
				}

				else
				{
					if (curTrack != 3)
					{
						curNoteLen = command[2] * 10;
						if (command[0] == DE2_STATUS_REST && command[1] == DE2_STATUS_REST)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						else
						{
							noteFreq = ReadBE16(&exRomData[seqPos]);
							curNote = gbFreq2Note(noteFreq);
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						seqPos += 3;

					}
					else
					{
						curNoteLen = command[1] * 10;
						if (command[0] == DE2_STATUS_REST)
						{
							curDelay += curNoteLen;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						else
						{
							curNote = command[0];
							tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;
							curDelay = 0;
							ctrlDelay += curNoteLen;
							masterDelay += curNoteLen;
						}
						seqPos += 2;

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

/*Convert the sample data to WAV*/
void DE2sam2wav(int sampNum, long startPtr, long endPtr, int bank, int freq)
{
	char name[64];
	int sampPos = 0;
	int rawPos = 0;
	int k = 0;
	int c = 0;
	int s = 0;
	int fileSize = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int endSamp;
	int cpu;
	int b;
	int sampLen = 0;
	int temp1;
	int temp2;

	sprintf(name, "sample%i.wav", sampNum);
	if ((wav = fopen(name, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file sample%i.wav!\n", sampNum);
		exit(2);
	}
	else
	{
		fileSize = 36;
		endSamp = 0;

		cpu = startPtr;
		b = bank;

		fseek(wav, 44, SEEK_SET);

		while (cpu < endPtr)
		{
			c = gb_read_byte(b, cpu);
			/*Convert the 4-bit PCM to 8-bit*/
			lowNibble = c >> 4;
			highNibble = c & 0x0F;
			s = (lowNibble | (lowNibble * 0x10));
			fputc(s, wav);
			fileSize++;
			s = (highNibble | (highNibble * 0x10));
			fputc(s, wav);
			fileSize++;
			cpu++;
		}

		/*Fill in the header data*/
		fseek(wav, 0, SEEK_SET);
		memcpy(wavHeader.riffID, "RIFF", 4);
		memcpy(wavHeader.waveID, "WAVE", 4);
		memcpy(wavHeader.fmtID, "fmt ", 4);
		memcpy(wavHeader.dataID, "data", 4);

		wavHeader.fileSize = fileSize;
		wavHeader.blockAlign = 16;
		wavHeader.dataFmt = 1;
		wavHeader.channels = 1;
		wavHeader.sampleRate = freq;
		wavHeader.byteRate = freq * 1 * 8 / 8;
		wavHeader.bytesPerSamp = 1 * 8 / 8;
		wavHeader.bits = 8;
		wavHeader.dataSize = fileSize - 36;

		fwrite(&wavHeader, sizeof(wavHeader), 1, wav);

		fclose(wav);
	}

}