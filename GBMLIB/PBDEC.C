/*Pandora Box decompressor*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define bankSize 16384

typedef struct {
	uint8_t accumulator; /* 8 - bit accumulator*/
	uint8_t carry;      /* Carry flag(0 or 1)*/
} CPU;

FILE* cmp, * bin;
char outfile[1000000];
long cmpSize;
long decmpSize;
int endData;
int counter;
int mask;
CPU PBmask1 = { 0, 0 };
CPU PBmask2 = { 0, 0 };
int compPos;
int decompPos;
int carry;
int tempByte = 0;
int tempByte2 = 0;
int16_t tempByteFF = 0;
int bytesCopy = 0;
CPU PBTempCPU = { 0, 0 };
signed char backAmt = 0;
int i = 0;
int counter2 = 0;
int counter3 = 0;
int counter4 = 0;
unsigned char a, b, c, d, e, f, h, l;
unsigned char bc;
long copyStart;
long getPos;
long tempPos;
int copyBytes;
uint16_t PBTestVal = 0;
int cmpMethod;
long tempPtr;

unsigned char* compData;
unsigned char* decompData;

unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void O4method();
void O5method1F27();
void O5method1F41();
void O5method1F5F();
void O5method1FB7();
void PBRRA(CPU* cpu);
void PBRL(CPU* cpu);
void PBSLA(CPU* cpu);
void PBSRL(CPU* cpu);

void PBDecomp(unsigned char* compData, unsigned char* decompData, long compLen, int cmpMethod)
{
	compPos = 0;
	decompPos = 0;

	/*Oni 4*/
	if (cmpMethod == 1)
	{
		decmpSize = ReadLE16(&compData[compPos]);
		compPos += 2;
		copyStart = ReadLE16(&compData[compPos]) + 4;
		compPos += 2;

		decompData[0] = compData[copyStart];
		decompData[1] = compData[copyStart + 1];
		decompPos += 2;
		copyStart += 2;


		carry = 0;

		PBmask1.accumulator = 0x80;
		PBmask2.accumulator = 0x00;

		/*($1FC2)*/
		b = 0;
		c = 2;

	O4Seg1FC2:
		if (decompPos < decmpSize)
		{
			goto O4Seg1FCF;
		}
		else
		{
			goto O4SegEnd;
		}

		/*($1FCF)*/
	O4Seg1FCF:

		/*LD A, $01*/
		/*CALL $2086*/
		/*AND A*/
		/*JR NZ, $1FFE*/
		a = 1;
		O4method();
		if (a != 0)
		{
			goto O4Seg1FFE;
		}

		/*LD A, $02*/
		/*CALL $2086*/
		/*INC A*/
		/*CP A, $04*/
		/*JP NZ, $206C*/
		a = 2;
		O4method();
		a++;
		if (a != 4)
		{
			goto O4Seg206C;
		}

		/*LD A, $01*/
		/*CALL $2086*/
		/*AND A*/
		/*JR Z, $1FF9*/

		/*LD E, $00*/
		a = 1;
		O4method();
		if (a == 0)
		{
			goto O4Seg1FF9;
		}
		e = 0;

	O4Seg1FEC:
		/*($1FEC)*/
		/*LD A, $01*/
		/*CALL $2086*/
		a = 1;
		O4method();

		/*INC E*/
		/*PBRRA*/
		/*JR C, $1FEC*/
		/*LD A, E*/
		/*CALL $2086*/
		e++;
		PBTempCPU.accumulator = a;
		PBRRA(&PBTempCPU);
		a = PBTempCPU.accumulator;

		if (carry != 0)
		{
			goto O4Seg1FEC;
		}
		a = e;
		O4method();

	O4Seg1FF9:
		/*ADD A, $04*/
		/*JP $206C*/
		a += 4;
		goto O4Seg206C;

	O4Seg1FFE:
		/*($1FFE)*/
		/*LD L, $0C*/
		/*LD A, B*/
		/*CP A, $10*/
		/*JR Z, $201A*/
		/*JR NC, $201A*/

		/*LD DE, $0800*/
		l = 12;
		a = b;

		if (decompPos >= 0x1000)
		{
			goto O4Seg201A;
		}
		d = 8;
		e = 0;

	O4Seg200B:
		/*($200B)*/
		/*LD A, B*/
		/*AND D*/
		/*JR NZ, $201A*/

		/*LD A, C*/
		/*AND E*/
		/*JR NZ, $201A*/

		/*PBSRL D*/
		/*RR E*/
		/*DEC L*/
		/*JR $200B*/

		PBTestVal = decompPos & 0xFF00;
		PBTestVal = PBTestVal >> 8;
		b = PBTestVal;
		PBTestVal = decompPos & 0xFF;
		c = PBTestVal;
		a = b;
		a = a & d;
		if (a != 0)
		{
			goto O4Seg201A;
		}
		a = c;
		a = a & e;
		if (a != 0)
		{
			goto O4Seg201A;
		}

		PBTempCPU.accumulator = d;
		PBSRL(&PBTempCPU);
		d = PBTempCPU.accumulator;
		PBTempCPU.accumulator = e;
		PBRRA(&PBTempCPU);
		e = PBTempCPU.accumulator;
		l--;
		goto O4Seg200B;


	O4Seg201A:
		/*($201A)*/
		/*LD D, $00*/
		/*LD A, L*/
		/*POP HL*/
		/*CP A, $09*/
		/*JR C, $202A*/

		/*SUB A, $08*/
		/*CALL $2086*/
		/*LD D, A*/
		/*LD A, $08*/
		d = 0;
		a = l;

		if (a < 9)
		{
			goto O4Seg202A;
		}
		else
		{
			a -= 8;
			O4method();
			d = a;
			a = 8;
		}

	O4Seg202A:
		/*($202A)*/
		/*CALL $2086*/

		/*LD E, A*/
		/*LD A, L*/
		/*SUB E*/
		/*LD E, A*/
		/*LD A, H*/
		/*SBC D*/
		/*LD D, A*/
		/*PUSH HL*/
		/*LD A, $03*/
		/*CALL $2086*/
		/*INC A*/
		/*INC A*/
		/*CP A, $09*/
		/*JR NZ, $2059*/

		/*LD A, $01*/
		/*CALL $2086*/
		/*AND A*/
		/*JR Z, $2057*/

		/*LD L, $00*/

		O4method();
		getPos = decompPos - a;

		if (d > 0)
		{
			getPos -= (0x100 * d);
		}

		a = 3;
		O4method();

		a += 2;

		if (a != 9)
		{
			goto O4Seg2059;
		}

		a = 1;
		O4method();

		if (a == 0)
		{
			goto O4Seg2057;
		}

		l = 0;

	O4Seg204A:
		/*($204A)*/
		/*LD A, $01*/
		/*CALL $2086*/

		/*INC L*/
		/*PBRRA*/

		/*JR C, $204A*/
		/*LD A, L*/
		/*CALL $2086*/

		a = 1;
		O4method();
		l++;
		PBTempCPU.accumulator = a;
		PBRRA(&PBTempCPU);
		a = PBTempCPU.accumulator;

		if (carry != 0)
		{
			goto O4Seg204A;
		}
		a = l;
		O4method();

	O4Seg2057:
		/*($2057)*/
		/*LD A, $09*/
		a += 9;
	O4Seg2059:
		/*($2059)*/
		/*LD L, A*/
		/*LD H, $00*/
		/*ADD HL, BC*/

		/*LD C, L*/
		/*LD B, H*/
		/*POP HL*/
		/*PUSH BC*/
		/*LD C, A*/

		tempPos = decompPos + a;

	O4Seg2062:
		/*($2062)*/
		/*LD A, [DE]*/
		/*LD [HL], A*/
		/*INC DE*/
		/*DEC C*/
		/*JR NZ, $2062*/

		/*POP BC*/
		/*JP $1FC2*/
		while (decompPos < tempPos)
		{
			decompData[decompPos] = decompData[getPos];
			decompPos++;
			getPos++;
		}
		if (decompPos == 0x01B9)
		{
			decompPos = 0x01B9;
		}
		goto O4Seg1FC2;

	O4Seg206C:
		/*($206C)*/
		/*PUSH AF*/
		/*LD A, [$FF9B]*/
		/*LD E, A*/
		/*LD A, [$FF9C]*/
		/*LD D, A*/
		/*POP AF*/

	O4Seg2074:
		/*($2074)*/
		/*PUSH AF*/
		/*LD A, [DE]*/
		/*LDI [HL], A*/
		/*INC DE*/
		/*INC BC*/
		/*POP AF*/
		/*DEC A*/
		/*JR NZ, $2074*/

		/*LD A, E*/
		/*LD [$FF9B], A*/
		/*LD A, D*/
		/*LD [$FF9C], A*/
		/*JP $1FC2*/

		decompData[decompPos] = compData[copyStart];
		decompPos++;
		copyStart++;
		a--;

		if (a != 0)
		{
			goto O4Seg2074;
		}
		goto O4Seg1FC2;

	O4SegEnd:

		;
	}


	/*Oni 5*/
	else
	{
		counter = 0;
		decmpSize = ReadLE16(&compData[compPos]);
		compPos += 2;

		if (compData[compPos] < 0x80)
		{
			copyStart = (compData[compPos] + 1) + compPos + 1;
			compPos++;
		}
		else
		{
			copyStart = (ReadBE16(&compData[compPos]) - 0x8000) + 1 + compPos + 2;
			compPos += 2;
		}

		PBmask1.accumulator = compData[compPos];

	O5Seg1E87:
		if (decmpSize > 0)
		{
			a = decmpSize & 0xFF;
			goto O5Seg1EA0;
		}
		else
		{
			goto O5SegEnd;
		}

	O5Seg1EA0:
		/*($1EA0)*/
		/*CALL $1FB7*/
		/*JR NC, $1EC6*/
		/*CALL $1FB7*/
		/*JR NC, $1ED1*/
		/*CALL $1F5F*/
		/*INC A*/
		/*PUSH BC*/
		/*LD B, A*/
		/*LD A, [$FFAF]*/
		/*SUB B*/
		/*LD [$FFAF], A*/
		/*SBC A, $00*/
		/*JR C, $1EF0*/
		/*LD [$FFB0], A*/

		O5method1FB7();

		if (carry == 0)
		{
			goto O5Seg1EC6;
		}

		O5method1FB7();

		if (carry == 0)
		{
			goto O5Seg1ED1;
		}

		O5method1F5F();

		a++;
		b = a;

		decmpSize -= a;


	O5Seg1EBD:
		/*($1EBD)*/
		/*LDI A, [HL]*/
		/*LD [DE], A*/
		/*INC DE*/
		/*DEC B*/
		/*JR NZ, $1EBD*/
		/*POP BC*/
		/*JR $1E87*/
		while (b > 0)
		{
			decompData[decompPos] = compData[copyStart];
			copyStart++;
			decompPos++;
			b--;
		}
		goto O5Seg1E87;

	O5Seg1EC6:
		/*($1EC6)*/
		/*PUSH HL*/
		/*CALL $1F27*/
		/*CALL $1F5F*/
		/*ADD A, $03*/
		/*JR $1ED7*/

		O5method1F27();
		O5method1F5F();

		a += 3;
		goto O5Seg1ED7;

	O5Seg1ED1:
		/*($1ED1)*/
		/*PUSH HL*/
		/*CALL 1F27*/
		/*LD A, $02*/

		O5method1F27();
		a = 2;

	O5Seg1ED7:
		/*PUSH BC*/
		/*LD B, A*/
		/*LD A, [$FFAF]*/
		/*SUB B*/
		/*LD [$FFAF], A*/
		/*LD A, [$FFB0]*/
		/*SBC A, $00*/
		/*JR C, $1EF0*/
		/*LD [$FFB0], A*/

		b = a;
		decmpSize -= b;

	O5Seg1EE6:
		/*($1EE6)*/
		/*LDI A, [$HL]*/
		/*LD [DE], A*/
		/*INC DE*/
		/*DEC B*/
		/*JR NZ, $1EE6*/

		/*POP BC*/
		/*POP HL*/
		/*JR $1E87*/

		while (b > 0)
		{
			decompData[decompPos] = decompData[tempPtr];
			decompPos++;
			tempPtr++;
			b--;
		}
		goto O5Seg1E87;

	O5SegEnd:
		;
	}
}

void O4method()
{

	/*($2086)*/
	/*PUSH BC*/
	/*PUSH HL*/
	/*LD C, A*/
	/*LD B, $00*/
	/*LD A, [$FF9F]*/
	/*LD L, A*/
	/*LD A, [$FFA0]*/
	/*LD H, A*/
	tempByte = l;
	tempByte2 = b;
	c = a;
	b = 0;
	l = PBmask1.accumulator;
	h = PBmask2.accumulator;

Method2091:
	/*($2091)*/
	/*PBSLA L*/
	/*JR NC, $20A7*/

	/*INC L*/
	/*PUSH DE*/
	/*LD A, [$FF99]*/
	/*LD E, A*/
	/*LD A, [$FF9A]*/
	/*LD D, A*/
	/*LD A, [DE]*/
	/*LD H, A*/
	/*INC DE*/
	/*LD A, E*/
	/*LD [$FF99], A*/
	/*LD A, D*/
	/*LD [$FF9A], A*/
	/*POP DE*/

	PBTempCPU.accumulator = l;

	PBSLA(&PBTempCPU);
	l = PBTempCPU.accumulator;
	if (carry != 1)
	{
		goto Method20A7;
	}
	l++;
	h = compData[compPos];
	compPos++;


Method20A7:
	/*($20A7)*/
	/*PBSLA H*/
	/*PBRL B*/
	/*JR NZ, $2091*/

	/*LD A, L*/
	/*LD [$FF9F], A*/
	/*LD A, H*/
	/*LD [$FFA0], A*/
	/*LD A, B*/
	/*POP HL*/
	/*POP BC*/
	/*RET*/

	PBTempCPU.accumulator = h;
	PBSLA(&PBTempCPU);
	h = PBTempCPU.accumulator;
	PBTempCPU.accumulator = b;
	PBRL(&PBTempCPU);
	b = PBTempCPU.accumulator;

	c--;
	if (c != 0)
	{
		goto Method2091;
	}

	PBmask1.accumulator = l;
	PBmask2.accumulator = h;
	a = b;
	l = tempByte;
}

/*Get the "back position" (and number of bytes?) for compressed data*/
void O5method1F27()
{
	/*($1F27)*/
	/*CALL $1F41*/
	/*PUSH DE*/
	/*LD HL, $0000*/
	O5method1F41();
	h = 0;
	l = 0;

MethodSeg1F2E:
	/*CALL $1FB7*/
	/*PBRL L*/
	/*PBRL H*/
	/*DEC A*/
	/*JR NZ, $1F2E*/

	/*LD A, L*/
	/*CPL*/
	/*LD L, A*/
	/*LD A, H*/
	/*CPL*/
	/*LD H, A*/
	/*ADD HL, DE*/
	/*POP DE*/
	/*RET*/

	O5method1FB7();
	PBTempCPU.accumulator = l;
	PBRL(&PBTempCPU);
	l = PBTempCPU.accumulator;
	PBTempCPU.accumulator = h;
	PBRL(&PBTempCPU);
	h = PBTempCPU.accumulator;
	a--;

	if (a > 0)
	{
		goto MethodSeg1F2E;
	}
	else
	{
		l = ~l;
		h = ~h;

		tempByte2 = l + (h * 0x100);

		tempPtr = (decompPos + tempByte2) & 0xFFFF;


	}
}

void O5method1F41()
{
	/*($1F41)*/
	/*PUSH HL*/
	/*PUSH DE*/
	/*DEC DE*/
	/*PUSH DE*/
	/*LD HL, $FFB3*/
	/*LDI A, [HL]*/
	/*LD E, A*/
	/*LD D, [HL]*/
	/*POP HL*/
	/*ADD HL, DE*/
	/*LD A, H*/
	/*OR L*/
	/*LD A, $01*/
	/*JR Z, $1F5C*/
	/*LD A, $11*/

	tempPtr = decompPos + 0xD4EE - 1;

	d = 0x2B;
	e = 0x12;

	tempPtr = (tempPtr + 0x2B12) & 0xFFFF;

	h = (tempPtr & 0xFF00) >> 8;
	a = h;
	l = (tempPtr & 0xFF);
	a = a | l;
	a = 1;

	if (a == 0)
	{
		goto MethodSeg1F5C;
	}
	else
	{
		a = 17;
	}





MethodSeg1F55:
	/*DEC A*/
	/*PBSLA L*/
	/*PBRL H*/
	/*JR NC, $1F55*/

	a--;
	PBTempCPU.accumulator = l;
	PBSLA(&PBTempCPU);
	l = PBTempCPU.accumulator;

	PBTempCPU.accumulator = h;
	PBRL(&PBTempCPU);
	h = PBTempCPU.accumulator;

	if (carry == 0)
	{
		goto MethodSeg1F55;
	}

MethodSeg1F5C:
	/*POP DE*/
	/*POP HL*/
	/*RET*/
	;
}

void O5method1F5F()
{
	O5method1FB7();

	if (carry != 0)
	{
		goto MethodSeg1F78;
	}

	O5method1FB7();

	if (carry != 0)
	{
		goto MethodSeg1F6B;
	}
	else
	{
		a = 0;
		goto O5method1F5FEnd;
	}

	/*($1F5F)*/
	/*CALL $1FB7*/
	/*JR C, $1F78*/
	/*CALL $1FB7*/
	/*JR C, $1F6B*/
	/*XOR A*/
	/*RET*/

MethodSeg1F6B:
	a = 0;
	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;

	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;

	a++;

	goto O5method1F5FEnd;
	/*($1F6B)*/
	/*XOR A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*INC A*/
	/*RET*/

MethodSeg1F78:

	O5method1FB7();

	if (carry != 0)
	{
		goto MethodSeg1F95;
	}
	else
	{
		a = 0;
		O5method1FB7();
		PBTempCPU.accumulator = a;
		PBRL(&PBTempCPU);
		a = PBTempCPU.accumulator;
		O5method1FB7();
		PBTempCPU.accumulator = a;
		PBRL(&PBTempCPU);
		a = PBTempCPU.accumulator;
		O5method1FB7();
		PBTempCPU.accumulator = a;
		PBRL(&PBTempCPU);
		a = PBTempCPU.accumulator;
		O5method1FB7();
		PBTempCPU.accumulator = a;
		PBRL(&PBTempCPU);
		a = PBTempCPU.accumulator;
		a += 5;
		goto O5method1F5FEnd;
	}

	/*($1F78)*/
	/*CALL $1FB7*/
	/*JR C, $1F95*/
	/*XOR A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*ADD A, $05*/
	/*RET*/

MethodSeg1F95:

	a = 0;

	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;
	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;
	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;
	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;
	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;
	O5method1FB7();
	PBTempCPU.accumulator = a;
	PBRL(&PBTempCPU);
	a = PBTempCPU.accumulator;
	a += 21;
	goto O5method1F5FEnd;

	/*($1F95)*/
	/*XOR A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*CALL $1FB7*/
	/*PBRL A*/
	/*ADD A, $15*/
	/*RET*/

O5method1F5FEnd:
	;
}

void O5method1FB7()
{
	counter2 = a;
	if (counter < 8)
	{
		goto MethodSeg1FC4;
	}
	else
	{
		compPos++;
		PBmask1.accumulator = (compData[compPos]);
		counter = 0;
		a = 0;
	}
	/*($1FB7)*/
	/*LD [$FFBB], A*/
	/*LD A, [$FFB1]*/
	/*CP A, $08*/
	/*JR C, $1FC4*/
	/*INC BC*/
	/*LD A, [BC]*/
	/*LD [$FFB2], A*/
	/*XOR A*/

MethodSeg1FC4:
	counter++;
	PBSLA(&PBmask1);
	/*INC A*/
	/*LD [$FFB1], A*/
	/*LD A, [$FFB2]*/
	/*PBSLA A*/
	/*LD [$FFB2], A*/
	/*LD A, [$FFBB]*/
	a = counter2;
	/*RET*/
}

/*Rotate right with carry*/
void PBRRA(CPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	/* Rotate right: the least significant bit (LSB) goes to carry*/
	carry = cpu->accumulator & 0x01; /* Get the LSB*/
	cpu->accumulator = (cpu->accumulator >> 1) | (old_carry << 7); /* Shift right and put old carry in MSB*/
}

/*Rotate left with carry*/
void PBRL(CPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	carry = (cpu->accumulator & 0x80) >> 7;
	cpu->accumulator = (cpu->accumulator << 1) | old_carry;
}

/*Rotate left logically*/
void PBSRL(CPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;
	carry = cpu->accumulator & 0x01;
	cpu->accumulator = (cpu->accumulator >> 1);
}

/*Shift left with carry*/
void PBSLA(CPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	carry = (cpu->accumulator & 0x80) >> 7;
	cpu->accumulator = (cpu->accumulator << 1) & 0xFE;
}