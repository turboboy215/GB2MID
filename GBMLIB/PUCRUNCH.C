/*Pucrunch decompressor*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "SHARED.H"
#include "PUCRUNCH.H"

#define bankSize 16384

typedef struct {
	uint8_t accumulator; /* 8 - bit accumulator*/
	uint8_t carry;      /* Carry flag(0 or 1)*/
} PCCPU;

FILE* cmp, * bin;
char outfile[1000000];
long cmpSize;
long decmpSize;
int counter;
int mask;
PCCPU mask1;
PCCPU mask2;
int compPos;
int decompPos;
int carry;
int tempByte;
int tempByte2;
int16_t tempByteFF;
int bytesCopy;
PCCPU FFByte;
PCCPU TempPCCPU;
signed char backAmt;
int i;
int counter2;
int counter3;
int counter4;
unsigned char a, b, c, d, e, f, h, l;
unsigned char bc;
int tempStack[2];
long copyStart;
long getPos;
long tempPos;
int copyBytes;
uint16_t TestVal;
int cmpMethod;
long tempPtr;
int escPu;
int lzPos[2];
int escBits;
int esc8Bits;
int maxGamma;
int max1Gamma;
int max2Gamma;
int max8Gamma;
int extraBits;
int tablePu[31];
int regY;
int copyPos;
int outPtrs[2];
int a1;
long a2;
long uc;

unsigned char* compData;
unsigned char* decompData;

void PCRRA(PCCPU* cpu);
void PCRL(PCCPU* cpu);
void PCSLA(PCCPU* cpu);
void PCSRL(PCCPU* cpu);
void getVal();
void get1Bit();
void getBits();
void getChk();

/*Rotate right with carry*/
void PCRRA(PCCPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	/* Rotate right: the least significant bit (LSB) goes to carry*/
	carry = cpu->accumulator & 0x01; /* Get the LSB*/
	cpu->accumulator = (cpu->accumulator >> 1) | (old_carry << 7); /* Shift right and put old carry in MSB*/
}

/*Rotate left with carry*/
void PCRL(PCCPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	carry = (cpu->accumulator & 0x80) >> 7;
	cpu->accumulator = (cpu->accumulator << 1) | old_carry;
}

/*Rotate left logically*/
void PCSRL(PCCPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;
	carry = cpu->accumulator & 0x01;
	cpu->accumulator = (cpu->accumulator >> 1);
}

/*Shift left with carry*/
void PCSLA(PCCPU* cpu)
{
	/* Store the current carry*/
	int old_carry = carry;

	carry = (cpu->accumulator & 0x80) >> 7;
	cpu->accumulator = (cpu->accumulator << 1) & 0xFE;
}

void PCProc(unsigned char* compData, unsigned char* decompData, long compLen)
{
	compPos = 0;
	decompPos = 0;
	for (i = 0; i < 31; i++)
	{
		tablePu[i] = 0;
	}
	lzPos[0] = 0;
	lzPos[1] = 0;
	outPtrs[0] = 0;
	outPtrs[1] = 0;

	/*Read the file header & setup variables*/
	escPu = compData[0x06];
	escBits = compData[0x09];
	esc8Bits = 8 - escBits;
	maxGamma = compData[0x0A];
	max8Gamma = 8 - (maxGamma - 1);
	max1Gamma = compData[0x0B];
	max2Gamma = (max1Gamma * 2) - 1;
	extraBits = compData[0x0C];
	b = compData[0x0F];
	compPos = 0x10;

	/*Copy the RLE table (maximum of 31 bytes) to array*/
	b++;
	i = 0;
	TempPCCPU.accumulator = b;
	PCSRL(&TempPCCPU);
	b = TempPCCPU.accumulator;
	if (carry == 0)
	{
		goto ORLELoop;
	}

RLELoop:
	tablePu[i] = compData[compPos];
	compPos++;
	i++;

ORLELoop:
	tablePu[i] = compData[compPos];
	compPos++;
	i++;

	b--;
	if (b != 0)
	{
		goto RLELoop;
	}
	d = 0x80;

	goto MainPart;

NewEsc:
	b = a;

	a = escPu;
	regY = a;

	a = escBits;
	e = a;

	a = b;
	e++;

	getChk();
	escPu = a;
	a = regY;

	/*Fall through and get the rest of the bits.*/

NoEsc:
	b = a;

	a = esc8Bits;
	e = a;

	a = b;

	e++;

	getChk();

	/*Write out the escaped/normal byte*/

	decompData[decompPos] = a;
	decompPos++;

	/*Fall through and check the escape bits again*/


MainPart:
	a = escBits;
	e = a;

	/*A = 0*/
	a = 0;
	regY = a;

	e++;

	/*X=2 -> X=0*/
	getChk();

	b = a;
	a = escPu;

	/*Not the escape code -> get the rest of the byte*/
	if (a != b)
	{
		a = b;
		goto NoEsc;
	}
	else
	{
		a = b;
	}

	/*Fall through to packed code*/

	/*X=0 -> X=0*/
	getVal();

	/*xstore - save the length for a later time*/
	lzPos[0] = a;

	/*cmp #1        ; LEN == 2 ? (A is never 0)*/
	TempPCCPU.accumulator = a;
	PCSRL(&TempPCCPU);
	a = TempPCCPU.accumulator;

	/*LEN != 2      -> LZ77*/
	if (a != 0)
	{
		goto LZ77;
	}

	/* X=0 -> X=0*/
	get1Bit();

	/*bit -> C, A = 0*/
	TempPCCPU.accumulator = a;
	PCSRL(&TempPCCPU);
	a = TempPCCPU.accumulator;

	/*A=0 -> LZPOS+1        LZ77, len=2*/
	if (carry == 0)
	{
		goto LZ77_2;
	}

	/*e..e01*/
	get1Bit();
	/*bit -> C, A = 0*/
	TempPCCPU.accumulator = a;
	PCSRL(&TempPCCPU);
	a = TempPCCPU.accumulator;
	/*e..e010               New Escape*/
	if (carry == 0)
	{
		goto NewEsc;
	}

	/*e..e011				Short/Long RLE*/
	/*Y is 1 bigger than MSB loops*/
	regY++;

	/*Y is 1, get len,  X=0 -> X=0*/
	getVal();
	/*xstore - Save length LSB*/
	lzPos[0] = a;

	c = a;

	a = max1Gamma;
	b = a;
	a = c;

	/*** PARAMETER 63-64 -> C set, 64-64 -> C clear..*/
	if (a < b)
	{
		/*short RLE, get bytecode*/
		goto CHRCode;
	}


	/*Otherwise it's long RLE*/
LongRLE:
	b = a;
	a = max8Gamma;
	/*** PARAMETER  111111xxxxxx*/
	e = a;
	a = b;

	/*get 3/2/1 more bits to get a full byte,  X=2 -> X=0*/
	getBits();
	/*xstore - Save length LSB*/
	lzPos[0] = a;

	/*length MSB, X=0 -> X=0*/
	getVal();

	/*Y is 1 bigger than MSB loops*/
	regY = a;

CHRCode:
	/*Byte Code,  X=0 -> X=0*/
	getVal();
	e = a;

	a = e;
	/*31-32 -> C set, 32-32 -> C clear..*/
	if (a < 32)
	{
		a = tablePu[a - 1];
		goto Less32;
	}
	else
	{
		a = tablePu[a - 1];
	}

	/*Not ranks 1..31, -> 11111°xxxxx (32..64), get byte..*/

	/*get back the value (5 valid bits)*/
	a = e;

	e = 3;

	/*get 3 more bits to get a full byte, X=3 -> X=0*/
	getBits();

Less32:
	a1 = a;
	a = lzPos[0];
	/*xstore - get length LSB*/
	e = a;

	b = e;
	/*adjust for cpx#$ff;bne -> bne*/
	b++;

	a = regY;
	c = a;

DoRLE:
	a = a1;
	decompData[decompPos] = a;
	decompPos++;

	b--;
	/*xstore 0..255 -> 1..256*/
	if (b != 0)
	{
		goto DoRLE;
	}

	c--;
	/*Y was 1 bigger than wanted originally*/
	if (c != 0)
	{
		goto DoRLE;
	}

	goto MainPart;

LZ77:
	/*X=0 -> X=0*/
	getVal();

	b = a;

	a = max2Gamma;
	/*end of file ?*/
	if (a == b)
	{
		/*yes, exit*/
		goto EndOfFile;
	}

	/*** PARAMETER (more bits to get)*/
	a = extraBits;
	e = a;

	a = b;

	/*subtract 1  (1..126 -> 0..125)*/
	a--;

	e++;

	/*f        ; clears Carry, X=0 -> X=0*/
	getChk();

LZ77_2:
	/*offset MSB*/
	lzPos[1] = a;

	e = 8;

	/*clears Carry, X=8 -> X=0*/
	getBits();

	/*Note: Already eor:ed in the compressor..*/

	b = a;

	a = lzPos[0];
	/*xstore - LZLEN (read before it's overwritten)*/
	e = a;

	outPtrs[0] = decompPos & 0xFF;
	outPtrs[1] = (decompPos & 0xFF00) >> 8;
	a = outPtrs[0];

	uc = b;

	/*-offset -1 + curpos (C is clear)*/
	a2 = a;
	a += b;
	a2 += uc;

	if (a2 > 255)
	{
		carry = 1;
	}
	else
	{
		carry = 0;
	}

	lzPos[0] = a;

	a = lzPos[1];
	b = a;

	a = outPtrs[1];
	if (carry == 0)
	{
		carry = 1;
	}
	else
	{
		carry = 0;
	}
	a -= b;

	a -= carry;

	lzPos[1] = a;

	copyPos = lzPos[0] + (lzPos[1] * 0x100);

	/*copy X+1 number of chars from LZPOS to OUTPOS*/
	lzPos[1] = a;

	/*adjust for cpx#$ff;bne -> bne*/
	e++;

	/*Write decompressed bytes out to RAM*/
	b = e;

	a = b;
	/*Is it zero?*/
	if (a == 0)
	{
		/*yes*/
		goto Zero;
	}

	b++;
	TempPCCPU.accumulator = b;
	PCSRL(&TempPCCPU);
	b = TempPCCPU.accumulator;

	if (carry == 0)
	{
		goto OLZLoop;
	}

LZLoop:
	/*Note: Must be copied forward*/
	decompData[decompPos] = decompData[copyPos];
	decompPos++;
	copyPos++;
OLZLoop:
	/*Note: Must be copied forward*/
	decompData[decompPos] = decompData[copyPos];
	decompPos++;
	copyPos++;

	b--;
	/*X loops, (256,1..255)*/
	if (b > 0)
	{
		goto LZLoop;
	}

	goto MainPart;

Zero:
	b = 128;
	goto LZLoop;


EndOfFile:
	;
}

/*getval : Gets a 'static huffman coded' value*/
/*** Scratches X, returns the value in A ***/
/*X must be 0 when called!*/
void getVal()
{
	a = 1;
	e = a;
Loop0:
	TempPCCPU.accumulator = d;
	PCSLA(&TempPCCPU);
	d = TempPCCPU.accumulator;

	if (d != 0)
	{
		goto Loop1;
	}

	/*Shift in C=1 (last bit marker)*/
	/*bitstr initial value = $80 == empty*/
	d = compData[compPos];
	compPos++;
	TempPCCPU.accumulator = d;
	PCRL(&TempPCCPU);
	d = TempPCCPU.accumulator;

Loop1:
	/*got 0-bit*/
	if (carry == 0)
	{
		goto GetCheck;
	}

	e++;

	/*save a*/
	b = a;

	a = maxGamma;

	if (a != e)
	{
		a = b;
		goto Loop0;
	}
	else
	{
		a = b;
		goto GetCheck;
	}






GetCheck:
	e--;
	if (e != 0)
	{
		goto GetChkBits;
	}
	carry = 0;
	goto EndMethod;

GetChkBits:
	TempPCCPU.accumulator = d;
	PCSLA(&TempPCCPU);
	d = TempPCCPU.accumulator;
	if (d != 0)
	{
		goto Loop3;
	}
	d = compData[compPos];
	compPos++;
	TempPCCPU.accumulator = d;
	PCRL(&TempPCCPU);
	d = TempPCCPU.accumulator;
Loop3:
	TempPCCPU.accumulator = a;
	PCRL(&TempPCCPU);
	a = TempPCCPU.accumulator;
	goto GetCheck;

EndMethod:
	;
}

void get1Bit()
{
Get1Bit:
	e++;
GetBits:
	TempPCCPU.accumulator = d;
	PCSLA(&TempPCCPU);
	d = TempPCCPU.accumulator;
	if (d != 0)
	{
		goto Loop3;
	}
	d = compData[compPos];
	compPos++;
	TempPCCPU.accumulator = d;
	PCRL(&TempPCCPU);
	d = TempPCCPU.accumulator;

Loop3:
	TempPCCPU.accumulator = a;
	PCRL(&TempPCCPU);
	a = TempPCCPU.accumulator;
GetCheck:
	e--;
	if (e != 0)
	{
		goto GetBits;
	}
	carry = 0;
}

void getBits()
{
GetBits:
	TempPCCPU.accumulator = d;
	PCSLA(&TempPCCPU);
	d = TempPCCPU.accumulator;
	if (d != 0)
	{
		goto Loop3;
	}
	d = compData[compPos];
	compPos++;
	TempPCCPU.accumulator = d;
	PCRL(&TempPCCPU);
	d = TempPCCPU.accumulator;

Loop3:
	TempPCCPU.accumulator = a;
	PCRL(&TempPCCPU);
	a = TempPCCPU.accumulator;
GetCheck:
	e--;
	if (e != 0)
	{
		goto GetBits;
	}
	carry = 0;
}

void getChk()
{
GetCheck:
	e--;
	if (e != 0)
	{
		goto GetChkBits;
	}
	carry = 0;
	goto EndMethod;

GetChkBits:
	TempPCCPU.accumulator = d;
	PCSLA(&TempPCCPU);
	d = TempPCCPU.accumulator;
	if (d != 0)
	{
		goto Loop3;
	}
	d = compData[compPos];

	compPos++;
	TempPCCPU.accumulator = d;
	PCRL(&TempPCCPU);
	d = TempPCCPU.accumulator;
Loop3:
	TempPCCPU.accumulator = a;
	PCRL(&TempPCCPU);
	a = TempPCCPU.accumulator;
	goto GetCheck;

EndMethod:
	;
}