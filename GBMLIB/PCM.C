/*PCM (General)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "PCM.H"
#include "WAV.H"
#include "RNC.H"

#define bankSize 16384
#define G2_TREE 0x26C4	/*Daiku no Gensan 2 PCM Huffman tree (ROM0)*/

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

/*HHCMP bitstream reader (ROM0 $261A).  MSB first: the register is rotated
  left and the new low bit is the next stream bit.*/
static int hhBitBuf;
static int hhBitsLeft;
static int hhBank;
static int hhCpu;

static int hhReadBit(void)
{
	int b;

	hhBitsLeft--;
	if (hhBitsLeft == 0)
	{
		hhBitBuf = gb_read_byte(hhBank, hhCpu);
		hhCpu++;
		hhBitsLeft = 8;
	}
	hhBitBuf = ((hhBitBuf << 1) | (hhBitBuf >> 7)) & 0xFF;
	b = hhBitBuf & 1;
	return b;
}

static int hhReadBits(int n)
{
	int v = 0;

	while (n-- > 0)
	{
		v = (v << 1) | hhReadBit();
	}
	return v;
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

		fseek(wav, 44, SEEK_SET);

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

		else if (fmt == PCM_FMT_2BIT)
		{
			while (totalLen < len)
			{
				c = gb_read_byte(b, cpu);
				/* First sample: level bits 5-4, repeated 1 + bits 7-6 times */
				/* Levels are raw ch3 output with all-$F wave: 0->0, 1->15, 2->7, 3->3, scaled x17 to 8-bit */
				c1 = (c >> 4) & 0x03;
				if (c1 == 0) s = 0 * 17;
				else if (c1 == 1) s = 15 * 17;
				else if (c1 == 2) s = 7 * 17;
				else s = 3 * 17;
				for (k = 0; k < (1 + ((c >> 6) & 0x03)); k++)
				{
					fputc(s, wav);
					fileSize++;
				}
				/* Second sample: level bits 1-0, repeated 1 + bits 3-2 times */
				c1 = c & 0x03;
				if (c1 == 0) s = 0 * 17;
				else if (c1 == 1) s = 15 * 17;
				else if (c1 == 2) s = 7 * 17;
				else s = 3 * 17;
				for (k = 0; k < (1 + ((c >> 2) & 0x03)); k++)
				{
					fputc(s, wav);
					fileSize++;
				}
				cpu++;
				totalLen++;
			}
		}

		else if (fmt == PCM_FMT_3BIT_BE)
		{
			while (totalLen < len)
			{
				c = gb_read_byte(b, cpu);
				/*Convert the 3-bit PCM to 8-bit (high half first)*/
				/*Scale 0-7 to 0-255: 0->0, 7->255, centre 3/4 -> 109/146*/
				s = (c >> 4) & 0x07;
				s = (s << 5) | (s << 2) | (s >> 1);
				fputc(s, wav);
				fileSize++;
				s = c & 0x07;
				s = (s << 5) | (s << 2) | (s >> 1);
				fputc(s, wav);
				fileSize++;
				cpu++;
				totalLen++;
			}
		}

		else if (fmt == PCM_FMT_3BIT_LE)
		{
			while (totalLen < len)
			{
				c = gb_read_byte(b, cpu);
				/*Convert the 3-bit PCM to 8-bit (low half first)*/
				s = c & 0x07;
				s = (s << 5) | (s << 2) | (s >> 1);
				fputc(s, wav);
				fileSize++;
				s = (c >> 4) & 0x07;
				s = (s << 5) | (s << 2) | (s >> 1);
				fputc(s, wav);
				fileSize++;
				cpu++;
				totalLen++;
			}
		}

		else if (fmt == PCM_FMT_P5)
		{
			while (totalLen < len)
			{
				if (cpu == 0x5037)
				{
					cpu = 0x5037;
				}
				c = gb_read_byte(b, cpu);

				if (c == 0x00)
				{
					cpu += 2;
					totalLen += 2;
				}
				else if (c == 0x01)
				{
					cpu += 2;
					totalLen += 2;
				}
				else if (c == 0x02)
				{
					cpu++;
					totalLen++;
				}
				else if (c == 0x03)
				{
					cpu += 2;
					totalLen += 2;
				}
				else if (c == 0x04)
				{
					cpu += 3;
					totalLen += 3;
				}
				else if (c == 0x05)
				{
					cpu += 2;
					totalLen += 2;
				}
				else if (c == 0x06)
				{
					cpu += 4;
					totalLen += 4;
				}
				else if (c == 0x07)
				{
					cpu++;
					totalLen++;
				}
				else if (c == 0x08)
				{
					cpu++;
					totalLen++;
				}
				else if (c == 0x09)
				{
					cpu++;
					totalLen++;
				}
				else if (c == 0x0A)
				{
					cpu += 2;
					totalLen += 2;
					for (i = 0; i < 16; i++)
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
				else if (c == 0x0B)
				{
					cpu += 2;
					totalLen += 2;
				}
				else
				{
					cpu++;
					totalLen++;
				}

			}
			}

		else if (fmt == PCM_FMT_HHCMP)
		{
			/*3-bit RLE bitstream, MSB first.  hPCMCount starts at 0, so:
				1                  -> repeat the last level
				0 vvv (v != L)     -> v
				0 LLL vvv (v != L) -> v, L = v
				0 LLL LLL nnnn vvv -> v, then 11 + n raw 3-bit values
			  len is the sample count; the bitstream follows the opcode's
			  3-byte (opcode + negative count) header.*/
			int last = 0;
			int raw = 0;
			int v;

			hhBank = b;
			hhCpu = cpu;
			hhBitBuf = 0;
			hhBitsLeft = 1;

			for (totalLen = 0; totalLen < len; totalLen++)
			{
				if (raw)
				{
					raw--;
					v = hhReadBits(3);
				}
				else if (hhReadBit())
				{
					v = last;
				}
				else
				{
					v = hhReadBits(3);
					if (v == last)
					{
						v = hhReadBits(3);
						if (v == last)
						{
							raw = 11 + hhReadBits(4);
							v = hhReadBits(3);
						}
						else
						{
							last = v;
						}
					}
				}

				/*Scale the 3-bit level to 8-bit, as the other 3-bit formats*/
				s = (v << 5) | (v << 2) | (v >> 1);
				fputc(s, wav);
				fileSize++;
			}
			}

		else if (fmt == PCM_FMT_G2HUFF)
		{
			/*Daiku no Gensan: Robot Teikoku no Yabou: Huffman + RLE (ROM0 $0478),
			  script opcode $0C (dest, stream) decodes into RAM, opcode $20 plays it.
			  Same MSB-first bit reader as HHCMP.  Tree of 2-byte nodes at ROM0
			  G2_TREE; the last value starts at 0:
				1 v   leaf: output v (an NR50 byte = level * $11; $FF ends the sample)
				0 0   end of the stream
				0 x   output the last value 2 + nnn times (next 3 bits)
				a b   branch: bit 0 -> a, bit 1 -> b; next node = node + 2 * (child - 1)
			  start = the stream (opcode address + 3), len = sample count without the $FF.*/
			int last = 0;
			int rep = 0;
			int node;
			int n0;
			int n1;
			int v = 0;
			int done = 0;

			hhBank = b;
			hhCpu = cpu;
			hhBitBuf = 0;
			hhBitsLeft = 1;

			totalLen = 0;
			while (totalLen < len && !done)
			{
				if (rep)
				{
					rep--;
					v = last;
				}
				else
				{
					node = G2_TREE;
					while (1)
					{
						n0 = gb_read_byte(0, node);
						n1 = gb_read_byte(0, node + 1);
						if (n0 == 0)
						{
							if (n1 == 0)
							{
								done = 1;
							}
							else
							{
								rep = hhReadBits(3) + 1;
								v = last;
							}
							break;
						}
						else if (n0 == 1)
						{
							v = n1;
							last = n1;
							break;
						}
						else
						{
							node += 2 * ((hhReadBit() ? n1 : n0) - 1);
						}
					}
				}

				if (done || v == 0xFF)
				{
					break;
				}

				v &= 0x07;
				s = (v << 5) | (v << 2) | (v >> 1);
				fputc(s, wav);
				fileSize++;
				totalLen++;
			}
			}

		else if (fmt == PCM_FMT_NR50)
		{
			/*One NR50 byte per sample (level * $11, 0-7), ended by $FF (Samurai Kid)*/
			while (totalLen < len)
			{
				c = gb_read_byte(b, cpu);
				if (c == 0xFF)
				{
					break;
				}
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
		wavHeader.dataSize = fileSize - 36;

		fwrite(&wavHeader, sizeof(wavHeader), 1, wav);

		fclose(wav);

	}
}
