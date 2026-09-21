/*Mortal Kombat 4 (Samples)*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "MK4PCM.H"
#include "SHARED.H"
#include "WAV.H"

#define bankSize 16384

struct header wavHeader;

FILE* rom, * wav;
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

unsigned char* romData;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void MK4sam2wav(int sampNum, long ptr, int bank, int freq);
unsigned char gb_read_byte(int bank, int cpu);

void MK4PCMProc(int bank, char parameters[4][100])
{
	freq = 8192;

	tableOffset = strtol(parameters[0], NULL, 16);

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

	while (sampNum <= 17)
	{
		sampleBank = romData[i];
		offset = ReadLE16(&romData[i + 1]);

		if (offset != 0x0000)
		{
			printf("Sample %i: 0x%04X, bank %02X\n", sampNum, offset, sampleBank);
			MK4sam2wav(sampNum, offset, sampleBank, freq);
		}
		else
		{
			sampleBank = 0x0E;
			offset = 0x4300;
			printf("Sample %i: 0x%04X, bank %02X (unused)\n", sampNum, offset, sampleBank);
			MK4sam2wav(sampNum, offset, sampleBank, freq);
		}
		i += 3;
		sampNum++;
	}
	free(romData);

}

void MK4sam2wav(int sampNum, long ptr, int bank, int freq)
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

		temp1 = gb_read_byte(b, cpu);
		cpu++;
		temp2 = gb_read_byte(b, cpu);
		sampLen = (temp1 + (temp2 * 0x100)) * 0x10;
		sampPos = 0;

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
