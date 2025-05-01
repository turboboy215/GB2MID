/*Unknown PCM driver (Cannon Fodder)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "CANNON.H"
#include "WAV.H"

#define bankSize 16384

struct header wavHeader;

FILE* rom, * wav;
int bank;
long tableOffset;
long firstPtr;
long sampOffset;
long sampLen;
int sampBank;
long bankAmt;
int i, j;
int sampNum;

unsigned char* romData;
unsigned char* exRomData;

char outfile2[0x300000];

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
void Cannonsam2wav(int sampNum, long ptr, long size, int bank);

void CannonProc(int bank)
{
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

	tableOffset = 0x4000;
	i = tableOffset - bankAmt;

	firstPtr = ReadLE16(&romData[i]);
	sampNum = 1;

	while (sampNum <= 154)
	{
		sampOffset = ReadLE16(&romData[i]);
		sampLen = ReadLE16(&romData[i + 2]) * 0x10;
		sampBank = romData[i + 4];
		printf("Sample %i: 0x%04X, length: %04X, bank: %02X\n", sampNum, sampOffset, sampLen, sampBank);
		Cannonsam2wav(sampNum, sampOffset, sampLen, sampBank);
		i += 5;
		sampNum++;
	}
	free(romData);
}

void Cannonsam2wav(int sampNum, long ptr, long size, int bank)
{
	int sampPos = 0;
	int rawPos = 0;
	int k = 0;
	int c = 0;
	int s = 0;
	int fileSize = 0;
	int sampleRate = 8192;
	unsigned char lowNibble = 0;
	unsigned char highNibble = 0;
	unsigned char* rawData;
	int rawLength = 0x300000;

	sprintf(outfile2, "sample%i.wav", sampNum);
	if ((wav = fopen(outfile2, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file sample%i.wav!\n", sampNum);
		exit(2);
	}
	else
	{
		/*Copy the sample data's bank (x 2)*/
		fseek(rom, (sampBank * bankSize), SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 11);
		fread(exRomData, 1, (bankSize * 11), rom);

		/*Create a storage for the extracted and unpacked sample data*/
		rawData = (unsigned char*)malloc(rawLength);

		for (k = 0; k < rawLength; k++)
		{
			rawData[k] = 0;
		}
		rawPos = 0;


		/*Go to the sample's position*/
		sampPos = ptr - bankAmt;

		for (k = 0; k < size; k++)
		{
			c = exRomData[sampPos];

			/*Convert the 4-bit PCM to 8-bit*/
			lowNibble = c >> 4;
			highNibble = c & 15;
			s = (lowNibble | (lowNibble * 0x10));
			rawData[rawPos] = s;
			rawPos++;
			s = (highNibble | (highNibble * 0x10));
			rawData[rawPos] = s;
			rawPos++;
			sampPos++;
		}

		if (sampNum == 9 || sampNum == 10)
		{
			sampleRate = 9639;
		}
		else
		{
			sampleRate = 8192;
		}

		/*Fill in the header data*/
		memcpy(wavHeader.riffID, "RIFF", 4);
		memcpy(wavHeader.waveID, "WAVE", 4);
		memcpy(wavHeader.fmtID, "fmt ", 4);
		memcpy(wavHeader.dataID, "data", 4);

		wavHeader.fileSize = rawPos + 36;
		wavHeader.blockAlign = 16;
		wavHeader.dataFmt = 1;
		wavHeader.channels = 1;
		wavHeader.sampleRate = sampleRate;
		wavHeader.byteRate = sampleRate * 1 * 8 / 8;
		wavHeader.bytesPerSamp = 1 * 8 / 8;
		wavHeader.bits = 8;
		wavHeader.dataSize = rawPos + 36;

		/*Write the WAV header to the WAV file*/
		fwrite(&wavHeader, sizeof(wavHeader), 1, wav);
		fwrite(rawData, rawPos, 1, wav);
		fclose(wav);
		free(exRomData);
	}
}