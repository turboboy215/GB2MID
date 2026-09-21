/*PCM (General)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "PCM.H"
#include "WAV.H"
#include "RNC.H"

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

unsigned char* romData;
unsigned char* cmpData;
unsigned char* decmpData;

char* argv3;
char string1[100];
char string2[100];
char PCMcheckStrings[6][100] = { "numSam=", "bank=", "start=", "len=", "freq=", "format=" };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void PCMsam2wav(int sampNum, long ptr, int bank, int len, int freq, int fmt);
unsigned char gb_read_byte(int bank, int cpu);
void PCMcopyData(long dataStart, int bank, int dataLen);

void PCMcopyData(long dataStart, int bank, int dataLen)
{
	int ptr = dataStart;
	int b = 0;
	int k, l = 0;
	int cpu = dataStart;

	cmpLen = dataLen;
	unsigned char getChar;
	cmpData = (unsigned char*)malloc(cmpLen);
	decmpData = (unsigned char*)malloc(bankSize);
	k = 0;
	b = bank - 1;

	while (k < dataLen)
	{
		getChar = gb_read_byte(b, cpu);

		cmpData[k] = getChar;
		k++;
		cpu++;
	}
}

void PCMProc(int bank, char parameters[4][100])
{
	fmt = PCM_FMT_4BIT_BE;
	freq = 8192;
	exitError = 0;
	fileExit = 0;

	if ((cfg = fopen(parameters[0], "r")) == NULL)
	{
		printf("ERROR: Unable to open configuration file %s!\n", parameters[0]);
		exit(1);
	}
	else
	{
		/*Get the total number of songs*/
		fgets(string1, 8, cfg);
		if (memcmp(string1, PCMcheckStrings[0], 1))
		{
			printf("ERROR: Invalid CFG data!\n");
			exit(1);

		}
		fgets(string1, 5, cfg);

		numSam = strtol(string1, NULL, 16);

		fgets(string1, 3, cfg);
		sampNum = 1;

		while (sampNum <= numSam)
		{
			/*Skip the first line*/
			fgets(string1, 9, cfg);

			/*Get the sample bank*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, PCMcheckStrings[1], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			sampleBank = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get the sample offset*/
			fgets(string1, 7, cfg);
			if (memcmp(string1, PCMcheckStrings[2], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			offset = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get the length*/
			fgets(string1, 5, cfg);
			if (memcmp(string1, PCMcheckStrings[3], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 9, cfg);

			sampLen = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			/*Get the frequency*/
			fgets(string1, 6, cfg);
			if (memcmp(string1, PCMcheckStrings[4], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 7, cfg);

			freq = strtod(string1, NULL);

			fgets(string1, 3, cfg);

			/*Get the format*/
			fgets(string1, 8, cfg);
			if (memcmp(string1, PCMcheckStrings[5], 1))
			{
				printf("ERROR: Invalid CFG data!\n");
				exit(1);
			}
			fgets(string1, 5, cfg);

			fmt = strtol(string1, NULL, 16);

			fgets(string1, 3, cfg);

			printf("Sample %i: 0x%04X, bank %02X\n", sampNum, offset, sampleBank);
			PCMsam2wav(sampNum, offset, sampleBank, sampLen, freq, fmt);
			sampNum++;
		}

		fclose(rom);
		fclose(cfg);
		printf("The operation was successfully completed!\n");
		exit(0);
	}
}

void PCMsam2wav(int sampNum, long ptr, int bank, int len, int freq, int fmt)
{
	char name[64];
	int sampPos = 0;
	int rawPos = 0;
	int k = 0;
	int c = 0;
	int c1 = 0;
	int s = 0;
	int fileSize = 0;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	int endSamp;
	int cpu;
	int b;
	int totalLen = 0;
	int bit;

	sprintf(name, "sample%i.wav", sampNum);
	if ((wav = fopen(name, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file sample%i.wav!\n", sampNum);
		exit(2);
	}
	else
	{
		fileSize = 36;

		cpu = ptr;
		b = bank - 1;

		totalLen = 0;

		if (fmt == PCM_FMT_4BIT_BE)
		{
			while (totalLen < len)
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
				totalLen++;
			}
		}

		else if (fmt == PCM_FMT_4BIT_LE)
		{
			while (totalLen < len)
			{
				c = gb_read_byte(b, cpu);
				/*Convert the 4-bit PCM to 8-bit*/
				highNibble = c >> 4;
				lowNibble = c & 0x0F;
				s = (lowNibble | (lowNibble * 0x10));
				fputc(s, wav);
				fileSize++;
				s = (highNibble | (highNibble * 0x10));
				fputc(s, wav);
				fileSize++;
				cpu++;
				totalLen++;
			}
		}

		else if (fmt == PCM_FMT_1BIT)
		{
			while (totalLen < len)
			{
				c = gb_read_byte(b, cpu);

				/*Convert the 1-bit PCM to 8-bit*/
				for (bit = 7; bit >= 0; bit--)
				{
					if (c >> bit & 1 != 0x00)
					{
						fputc(0xFF, wav);
					}
					else
					{
						fputc(0x00, wav);
					}
					fileSize++;
				}

				cpu++;
				totalLen++;
			}
		}

		else if (fmt == PCM_FMT_RNC_3)
		{
			PCMcopyData(offset, sampleBank, sampLen);
			decmpLen = ReadBE16(&cmpData[0x06]);
			procRNC(cmpData, decmpData, sampLen);
			while (totalLen < decmpLen)
			{
				/*Convert the 3-bit packed data to 8-bit*/
				c = (decmpData[totalLen] >> 4);
				s = c & 0x07;
				s = (s << 5) | (s << 2) | (s >> 1);
				fputc(s, wav);
				fileSize++;
				c = (decmpData[totalLen] & 0x0F);
				s = c & 0x07;
				s = (s << 5) | (s << 2) | (s >> 1);
				fputc(s, wav);
				fileSize++;
				cpu++;
				totalLen++;
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
