/*Tyrian 2000 decompressor*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "SHARED.H"
#include "T2KDEC.H"

#define bankSize 16384

FILE* cmp, * bin;
char outfile[1000000];
long cmpSize;
long decmpSize;
int counter;
unsigned int mask;
unsigned int mask1;
unsigned int mask2;
unsigned int tempByte1;
unsigned int tempByte2;
unsigned int copyByte;
int bytesCopy;
int bytesRep;
int compPos;
int decompPos;
int tempDecompPos;
int prevPos;
int i;
int j;
int carry;
int endFile;

unsigned char* compData;
unsigned char* decompData;

unsigned short T2KReadLE16(unsigned char* Data);

/*Convert little-endian pointer to big-endian*/
unsigned short T2KReadLE16(unsigned char* Data)
{
	return (Data[0] << 0) | (Data[1] << 8);
}
void T2KProc(unsigned char* compData, unsigned char* decompData, long compLen)
{
	compPos = 0;
	decompPos = 0;

	endFile = 0;

	while (endFile == 0)
	{
		mask = compData[compPos];

		mask1 = mask & 0xC0;

		/*Copy uncompressed bytes*/
		if (mask1 == 0xC0)
		{
			bytesCopy = (mask & 0x3F) + 1;
			compPos++;
			for (i = 0; i < bytesCopy; i++)
			{
				decompData[decompPos] = compData[compPos];
				decompPos++;
				compPos++;
			}
		}

		/*Reference to previous data*/
		else if (mask1 == 0x80)
		{
			bytesCopy = (mask & 0x3F) + 1;
			compPos++;
			prevPos = (T2KReadLE16(&compData[compPos]));
			prevPos = (prevPos + decompPos) & 0xFFFF;
			compPos += 2;
			for (i = 0; i < bytesCopy; i++)
			{
				decompData[decompPos] = decompData[prevPos];
				decompPos++;
				prevPos++;
			}
		}

		/*Copy 2 bytes multiple times*/
		else if (mask1 == 0x40)
		{
			bytesCopy = (mask & 0x3F) + 1;
			compPos++;
			for (i = 0; i < bytesCopy; i++)
			{
				decompData[decompPos] = compData[compPos];
				decompData[decompPos + 1] = compData[compPos + 1];
				decompPos += 2;
			}
			compPos += 2;
		}

		/*End of file*/
		else if (mask == 0x00)
		{
			endFile = 1;
		}

		/*Copy 1 byte multiple times*/
		else
		{
			bytesCopy = (mask & 0x3F) + 1;
			compPos++;
			for (i = 0; i < bytesCopy; i++)
			{
				decompData[decompPos] = compData[compPos];
				decompPos++;
			}
			compPos++;
		}
	}
}