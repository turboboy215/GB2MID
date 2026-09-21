/*Digital Eclipse (Klax/Tarzan/etc. - Samples)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "KLAXPCM.H"
#include "SHARED.H"
#include "WAV.H"

#define bankSize 16384

struct header wavHeader;

FILE* rom, * wav, * txt;
int sampNum;
int freq;
int bank;
int sampleBank;
long offset;
long tableOffset;
long ptr;
long firstPtr;
int i;
int sampNum;
int numSamp;
int sampLen;
int outputTSeq;
int songTab;
int songNum;
int songPtr;
int numSongs;
int songSamList;
int samPtr;

unsigned char* romData;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void Klaxsam2wav(int sampNum, long ptr, int bank, int freq, int len);
void tarzSeq(int songNum, int ptr, int samList);
unsigned char gb_read_byte(int bank, int cpu);


void KlaxPCMProc(int bank, char parameters[4][100])
{
	freq = 8192;
	outputTSeq = 0;
	if (parameters[2][0] != 0)
	{
		outputTSeq = 1;
		tableOffset = strtol(parameters[0], NULL, 16);
		numSamp = strtol(parameters[1], NULL, 16);
	}
	else if (parameters[1][0] != 0)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
		numSamp = strtol(parameters[1], NULL, 16);
	}
	else if (parameters[0][0] != 0)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
	}

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
	sampNum = 1;

	while (sampNum <= numSamp)
	{
		sampleBank = romData[i];
		offset = ReadLE16(&romData[i + 1]);
		sampLen = ReadLE16(&romData[i + 3]);
		if (offset != 0x0000)
		{
			printf("Sample %i: 0x%04X, bank %02X (length: %04X)\n", sampNum, offset, sampleBank, sampLen);
			Klaxsam2wav(sampNum, offset, sampleBank, freq, sampLen);
		}
		else
		{
			printf("Sample %i: 0x%04X, bank %02X (length: %04X) (empty)\n", sampNum, offset, sampleBank, sampLen);
		}
		i += 5;
		sampNum++;
	}

	/*Output song sequence lists from Tarzan*/
	if (outputTSeq == 1)
	{
		songTab = tableOffset - 0x190;
		numSongs = 7;

		i = songTab;
		songNum = 1;

		while (songNum <= numSongs)
		{
			songPtr = ReadLE16(&romData[i]);
			songSamList = ReadLE16(&romData[i + (numSongs * 2)]);
			printf("Song sequence %i: 0x%04X (sample list: 0x%04X)\n", songNum, songPtr, songSamList);
			tarzSeq(songNum, songPtr, songSamList);
			i += 2;
			songNum++;
		}

	}

	free(romData);
}

void Klaxsam2wav(int sampNum, long ptr, int bank, int freq, int len)
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

		cpu = ptr;
		b = bank;
		sampLen = len * 16;

		while (sampPos < sampLen)
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
			sampPos++;
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
		wavHeader.dataSize = fileSize;

		fwrite(&wavHeader, sizeof(wavHeader), 1, wav);

		fclose(wav);
	}
}

void tarzSeq(int songNum, int ptr, int samList)
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

	sprintf(name, "song%i.txt", songNum);
	if ((txt = fopen(name, "w")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.txt!\n", songNum);
		exit(2);
	}
	else
	{
		k = songPtr;

		while (romData[k] != 0xFF)
		{
			b = romData[k];
			s = ReadLE16(&romData[samList + (b * 2)]);
			s = (s - tableOffset) / 5;
			fprintf(txt, "%02X\n", s);
			k++;
		}
		fclose(txt);
	}
}
