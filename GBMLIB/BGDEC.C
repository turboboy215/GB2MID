/*Opera House (Bubble Ghost) decompression*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define bankSize 16384

FILE* cmp, * bin;
char outfile[1000000];
long cmpSize;
long decmpSize;
int endData;
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

unsigned char RR(unsigned char cpu);

void BGDecomp(unsigned char* compData, unsigned char* decompData)
{
	compPos = 0;
	decompPos = 0;

	/*Get the compressed size of the file*/
	compPos += 2;
	cmpSize = ReadLE16(&compData[compPos]);
	/*printf("Size (compressed): 0x%04X\n", cmpSize);*/

	compPos += 2;
	while (compPos < (cmpSize + 2))
	{
		mask = compData[compPos];

		if (mask == 0x10)
		{
			mask = 0x10;
		}
		tempByte1 = mask & 0x80;
		tempByte2 = mask & 0x40;

		if (tempByte1 != 0)
		{
			/*Repeat byte(s)*/
			if (tempByte2 != 0)
			{

				bytesCopy = (mask & 0x07) + 2;

				mask1 = mask;
				mask1 = RR(mask1);
				mask1 = RR(mask1);
				mask1 = RR(mask1);
				bytesRep = (mask1 & 0x03) + 1;

				tempDecompPos = decompPos;
				compPos++;
				for (i = 0; i < bytesCopy; i++)
				{
					for (j = 0; j < bytesRep; j++)
					{
						decompData[decompPos] = compData[compPos + j];
						decompPos++;
					}

				}
				compPos += bytesRep;
			}

			/*Copy data*/
			else
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
		}

		/*Copy earlier data*/
		else
		{
			/*Get the number of bytes back*/
			mask2 = compData[compPos + 1];

			mask1 = mask * 0x10;

			copyByte = mask2;
			copyByte = RR(copyByte);
			copyByte = RR(copyByte);
			copyByte = RR(copyByte);
			copyByte = RR(copyByte);
			copyByte = copyByte & 0x0F;

			prevPos = mask1 | copyByte;

			/*Get the number of bytes to copy*/
			copyByte = mask2;
			bytesCopy = (copyByte & 0x0F) + 2;

			for (i = 0; i < bytesCopy; i++)
			{
				decompData[decompPos] = decompData[decompPos - prevPos];
				decompPos++;
			}
			compPos += 2;
		}
	}
}

/*Rotate right with carry*/
unsigned char RR(unsigned char cpu)
{
	/* Store the current carry*/
	int oldCarry = carry;

	/* Rotate right: the least significant bit (LSB) goes to carry*/
	carry = cpu & 0x01;
	cpu = (cpu >> 1) | (oldCarry << 7);
	/* Shift right and put old carry in MSB*/
	return cpu;
}
