/*Titus (LZSS derivative) decompressor*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "SHARED.H"

#define bankSize 16384

typedef struct {
	uint8_t accumulator; /* 8 - bit accumulator*/
	uint8_t carry;      /* Carry flag(0 or 1)*/
} CPU;


FILE* cmp, * bin;
char outfile[1000000];
long cmpSize;
long decmpSize;
int counter;
int mask;
CPU mask1 = { 0, 0 };
CPU mask2 = { 0, 0 };
int compPos;
int decompPos;
int carry;
int tempByte;
int16_t tempByteFF;
int bytesCopy;
CPU FFByte = { 0xFF, 0 };
CPU TempCPU = { 0, 0 };
signed char backAmt;
int i;
int counter2;
int counter3;
int counter4;
unsigned char a;
unsigned char b;
unsigned char c;
int tempStack[2] = { 0, 0 };
uint16_t TestVal;

unsigned char* compData;
unsigned char* decompData;

unsigned short ReadBE162(unsigned char* Data);
void method();
void RRA(CPU* cpu);
void RL(CPU* cpu);

unsigned short ReadBE162(unsigned char* Data)
{
	return (Data[0] << 8) | (Data[1] << 0);
}

void TitusDecomp(unsigned char* compData, unsigned char* decompData, long compLen)
{
	tempByte = 0;
	tempByteFF = 0;
	bytesCopy = 0;
	backAmt = 0;
	i = 0;
	counter2 = 0;
	counter3 = 0;
	counter4 = 0;
	TestVal = 0;

	compPos = 0;
	decompPos = 0;

	carry = 0;

	mask1.accumulator = compData[compPos];
	mask2.accumulator = compData[compPos + 1];
	counter = 16;
	compPos += 2;
	endData = 0;

	for (i = 0; i < bankSize; i++)
	{
		decompData[i] = 0;
	}

	while (endData == 0)
	{

	LoopMethod:
		method();

		if (endData == 0)
		{
			/*If carry, then copy next byte (uncompressed)*/
			if (carry == 1)
			{
				tempByte = compData[compPos];
				c = tempByte;
				decompData[decompPos] = tempByte;
				decompPos++;
				compPos++;
			}
			/*Otherwise, next byte is a reference to earlier bytes*/
			else
			{
				method();
				tempByte = compData[compPos];
				c = tempByte;
				backAmt = (signed char)compData[compPos];
				compPos++;
				TestVal = tempByte + 0xFF00;
				tempByteFF = (int16_t)TestVal;

				if (carry != 1)
				{
					method();
					if (carry != 1)
					{
						/*End of compressed file*/
						if (tempByte == 0xFF)
						{
							endData = 1;
						}
						/*Copy 2 bytes*/
						else
						{
							for (i = 0; i < 2; i++)
							{
								decompData[decompPos + i] = decompData[decompPos + tempByteFF + i];
							}
							decompPos += 2;
						}
					}

					/*$20C4*/
					/*If distance length is more than 1 byte...*/
					else if (carry == 1)
					{
						FFByte.accumulator = 0xFF;
						method();
						RL(&FFByte);
						method();
						RL(&FFByte);
						method();
						RL(&FFByte);
						FFByte.accumulator -= 1;
						TestVal = c + (FFByte.accumulator * 0x100);
						tempByteFF = (int16_t)TestVal;
						for (i = 0; i < 2; i++)
						{
							decompData[decompPos + i] = decompData[decompPos + tempByteFF + i];
						}
						decompPos += 2;
					}
				}

				/*20D6*/
				/*Copy a custom amount of bytes*/
				else if (carry == 1)
				{
					/*CALL $2162*/
					/*RL B*/
					/*CALL $2162*/
					/*JR C, $2102*/
					FFByte.accumulator = 0xFF;
					method();
					RL(&FFByte);
					method();

					if (carry == 1)
					{
						goto Method2102;
					}

					else
					{
						counter2 = 2;
						/*LD A, 02*/
						/*LD [$FFF9], A*/
						/*LD A, 03*/
						a = 3;
						/*PUSH AF*/
						tempStack[1] = carry;
						goto Method20E6;
					}

				}
			}
		}


	}

	return 0;

Method20E6:
	/*CALL $2162*/
	/*JR C, $20FB*/
	method();
	if (carry == 1)
	{
		goto Method20FB;
	}
	else
	{
		/*CALL $2162*/
		/*RL B*/
		/*LD A, [$FFF9]*/
		/*ADD A*/
		/*LD [$FFF9], A*/
		/*POP AF*/
		/*DEC A*/
		/*JR NZ, $20E6*/
		method();
		RL(&FFByte);
		counter2 *= 2;
		carry = tempStack[1];
		a--;

		if (a > 0)
		{
			goto Method20E6;
		}
		else
		{
			goto Method20FB;
		}
	}

	/*$20FA = PUSH AF (unused?)*/
Method20FB:
	/*POP AF*/
	/*LD A, [$FFF9]*/
	/*SUB B*/
	counter2 -= (signed char)FFByte.accumulator;
	/*CPL*/
	FFByte.accumulator = counter2 ^ 0xFF;
	/*INC A*/
	/*LD B, A*/
	FFByte.accumulator++;

	goto Method2102;

Method2102:
	/*LD A, 02*/
	/*LD [$FFF9], A*/
	/*LD A, $04*/
	counter2 = 2;
	a = 4;
	goto Method2108;

Method2108:
	tempStack[0] = a;
	tempStack[1] = carry;
	/*PUSH AF*/
	/*LD A, [$FFF9]*/
	/*INC A*/
	/*LD [$FFF9], A*/
	/*CALL $2162*/
	counter2++;
	method();

	/*JR C, $2126*/
	if (carry == 1)
	{
		goto Method2126;
	}

	else
	{
		/*POP AF*/
		/*DEC A*/
		/*JR NZ, $2108*/
		/*CALL $2162*/
		/*JR NC, $2131*/

		/*CALL $2162*/
		/*LD A, [$FFF9]*/
		/*ADC A, $01*/
		/*LD [$FFF9], A*/
		/*PUSH AF*/
		carry = tempStack[1];
		a--;
		if (a > 0)
		{
			goto Method2108;
		}
		else
		{
			method();
			if (carry == 0)
			{
				goto Method2131;
			}
			else
			{
				method();
				counter2 += (1 + carry);
			}
		}
		tempStack[1] = carry;
		goto Method2126;

	}

Method2126:
	/*POP AF*/
	/*LD A, [$FFF9]*/
	/*LD A, [$FFFA], A*/
	/*XOR A*/
	/*LD A, [$FFFB]*/
	/*JP $20AC*/

	carry = tempStack[1];
	a = counter2;
	counter3 = a;
	carry = 0;


	TestVal = c + (FFByte.accumulator * 0x100);
	tempByteFF = (int16_t)TestVal;
	for (i = 0; i < counter2; i++)
	{
		decompData[decompPos + i] = decompData[decompPos + tempByteFF + i];
	}
	decompPos += counter2;
	goto LoopMethod;


Method2131:
	/*CALL $2162*/
	/*JR C, $2154*/
	/*XOR A*/
	/*LD [$FFF9], A*/
	/*LD A, $03*/
	method();

	if (carry == 1)
	{
		goto Method2154;
	}
	else
	{
		counter2 = 0;
		carry = 0;
		a = 3;
		goto Method213B;
	}

Method213B:
	/*PUSH AF*/
	/*CALL $2162*/
	/*LD A, [$FFF9]*/
	/*RLA*/
	/*LD [$FFF9], A*/
	/*POP AF*/
	/*DEC A*/
	/*JR NZ, $213B*/

	/*LD A, [$FFF9]*/
	/*ADD A, $09*/
	/*LD [$FFFA], A*/
	/*XOR A*/
	/*LD [$FFFB], A*/
	/*JP $20AC*/
	tempStack[1] = carry;
	method();
	TempCPU.accumulator = counter2;
	RL(&TempCPU);
	counter2 = TempCPU.accumulator;
	carry = tempStack[1];
	a--;
	if (a > 0)
	{
		goto Method213B;
	}
	else
	{
		counter2 = counter2 + 9;
		TestVal = c + (FFByte.accumulator * 0x100);
		tempByteFF = (int16_t)TestVal;
		for (i = 0; i < counter2; i++)
		{
			decompData[decompPos + i] = decompData[decompPos + tempByteFF + i];
		}
		decompPos += counter2;
		goto LoopMethod;
	}

Method2154:
	/*LD A, [DE]*/
	/*INC DE*/
	/*ADD A, $11*/
	/*LD [$FFFA], A*/
	/*LD A, $00*/
	/*RLA*/
	/*LD [$FFFB], A*/
	/*JP $20AC*/
	tempByte = compData[compPos];
	compPos++;
	if (tempByte <= 238)
	{
		carry = 0;
	}
	else
	{
		carry = 1;
	}
	tempByte += 17;
	counter3 = tempByte;
	TempCPU.accumulator = 0;
	RL(&TempCPU);
	counter4 = TempCPU.accumulator;
	TestVal = c + (FFByte.accumulator * 0x100);
	tempByteFF = (int16_t)TestVal;
	for (i = 0; i < counter3; i++)
	{
		decompData[decompPos + i] = decompData[decompPos + tempByteFF + i];
	}
	decompPos += counter3;
	goto LoopMethod;

}

void method()
{
	RRA(&mask2);
	RRA(&mask1);
	counter--;

	if (counter <= 0)
	{
		counter = 16;
		mask1.accumulator = compData[compPos];
		mask2.accumulator = compData[compPos + 1];
		compPos += 2;
	}
}

/*Rotate right with carry*/
void RRA(CPU* cpu)

{
	/* Store the current carry*/
	int old_carry = carry;

	/* Rotate right: the least significant bit (LSB) goes to carry*/
	carry = cpu->accumulator & 0x01; /* Get the LSB*/
	cpu->accumulator = (cpu->accumulator >> 1) | (old_carry << 7); /* Shift right and put old carry in MSB*/
}

/*Rotate left with carry*/
void RL(CPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	carry = (cpu->accumulator & 0x80) >> 7;
	cpu->accumulator = (cpu->accumulator << 1) | old_carry;
}