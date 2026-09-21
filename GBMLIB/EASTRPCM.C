/*Nick Eastridge (Samples)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "EASTRPCM.H"
#include "WAV.H"

#define bankSize 16384

struct header wavHeader;

FILE* rom, * wav, * cfg;
int sampNum;
int freq;
int bank;
int sampleBank;
long offset;
long ptr;
long firstPtr;
int i;
int sampNum;
int numSam;
long sampLen;
int fmt;
int exitError;
int fileExit;
int cmpLen;
int decmpLen;
int drvVers;
int tableOffset;

unsigned char* romData;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void EastrPCMsam2wav(int sampNum, long ptr, int bank, int freq);
unsigned char gb_read_byte(int bank, int cpu);

void EastrPCMProc(int bank, char parameters[4][100])
{
	freq = 8192;
	drvVers = EASTRIDGE_PCM_VER_G2;

	if (parameters[1][0] != 0)
	{
		tableOffset = strtol(parameters[0], NULL, 16);
		drvVers = strtol(parameters[1], NULL, 16);
		if (drvVers < EASTRIDGE_PCM_VER_G2 || drvVers > EASTRIDGE_PCM_VER_RS)
		{
			drvVers = EASTRIDGE_PCM_VER_G2;
		}
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

	if (drvVers == EASTRIDGE_PCM_VER_G2)
	{
		firstPtr = ReadLE16(&romData[i]) + tableOffset;
		freq = 4936;
		while (i < firstPtr)
		{
			if (ReadLE16(&romData[i]) != 0x0000)
			{
				ptr = ReadLE16(&romData[i]) + tableOffset;
				sampleBank = romData[i + 2] + bank;
				printf("Sample %i: 0x%04X, bank %02X\n", sampNum, ptr, sampleBank);
				EastrPCMsam2wav(sampNum, ptr, sampleBank, freq);
			}
			else
			{
				ptr = ReadLE16(&romData[i]);
				sampleBank = romData[i + 2] + bank;
				printf("Sample %i: 0x%04X, bank %02X (empty, skipped)\n", sampNum, ptr, sampleBank);
			}

			i += 3;
			sampNum++;
		}
	}
	else if (drvVers == EASTRIDGE_PCM_VER_PF)
	{
		firstPtr = ReadLE16(&romData[i]);
		freq = 7489;
		while (ReadLE16(&romData[i]) != 0x0000)
		{
			ptr = ReadLE16(&romData[i]);
			sampleBank = romData[i + 2] + bank;
			printf("Sample %i: 0x%04X, bank %02X\n", sampNum, ptr, sampleBank);
			EastrPCMsam2wav(sampNum, ptr, sampleBank, freq);

			i += 3;
			sampNum++;
		}
	}
	else
	{
		firstPtr = (ReadLE16(&romData[i]) + tableOffset) - 2;
		freq = 7489;
		while (sampNum < 13)
		{
			if (ReadLE16(&romData[i]) != 0x0000)
			{
				ptr = ReadLE16(&romData[i]) + tableOffset;
				sampleBank = romData[i + 2] + bank;
				printf("Sample %i: 0x%04X, bank %02X\n", sampNum, ptr, sampleBank);
				EastrPCMsam2wav(sampNum, ptr, sampleBank, freq);
			}
			else
			{
				ptr = ReadLE16(&romData[i]);
				sampleBank = romData[i + 2] + bank;
				printf("Sample %i: 0x%04X, bank %02X (empty, skipped)\n", sampNum, ptr, sampleBank);
			}

			i += 3;
			sampNum++;
		}
	}

	free(romData);
}

void EastrPCMsam2wav(int sampNum, long ptr, int bank, int freq)
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
		b = bank - 1;

		while (endSamp != 1)
		{
			c = gb_read_byte(b, cpu);
			if (c != 0x00)
			{
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
			else
			{
				endSamp = 1;
			}
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
