#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SHARED.H"
#include "AJG.H"
#include "AUDIOART.H"
#include "BEAM.H"
#include "CAPCOM.H"
#include "CARILLON.H"
#include "CLIMAX.H"
#include "COSMIGO.H"
#include "CUBE.H"
#include "DAVDSHEA.H"
#include "DE1.H"
#include "DW.H"
#include "EDMAGNIN.H"
#include "GAMEFRK.H"
#include "GHX.H"
#include "HAL.H"
#include "HUDSON.H"
#include "IMAGNRNG.H"
#include "JEROTEL.H"
#include "JSAITO.H"
#include "KONAMI.H"
#include "KSADA.H"
#include "LUFIA.H"
#include "MAKE.H"
#include "MCOOKSEY.H"
#include "MEGAMAN1.H"
#include "MEGAMAN2.H"
#include "MEGAMAN3.H"
#include "METROID2.H"
#include "MIDI.H"
#include "MPLAY.H"
#include "MUSYX.H"
#include "MWALKER.H"
#include "NATSUME.H"
#include "NINTENDO.H"
#include "OCEAN.H"
#include "PARAGON5.H"
#include "PROBE.H"
#include "RARE.H"
#include "REALTIME.H"
#include "ROLAN.H"
#include "SHEEP.H"
#include "SQUARE.H"
#include "SUNSOFT.H"
#include "TARANTLA.H"
#include "TECHNOS.H"
#include "TIERTEX.H"
#include "TITUS1.H"
#include "TITUS2.H"
#include "TOSE.H"
#include "WARIOL2.H"
#define bankSize 16384

int foundTable = 0;
int curInst = 0;
int curVol = 100;
int curBank = 0;
int multiBanks = 0;

/*Convert little-endian pointer to big-endian*/
unsigned short ReadLE16(unsigned char* Data)
{
	return (Data[0] << 0) | (Data[1] << 8);
}

/*Store big-endian pointer*/
unsigned short ReadBE16(unsigned char* Data)
{
	return (Data[0] << 8) | (Data[1] << 0);
}

void Write8B(unsigned char* buffer, unsigned int value)
{
	buffer[0x00] = value;
}

void WriteBE32(unsigned char* buffer, unsigned long value)
{
	buffer[0x00] = (value & 0xFF000000) >> 24;
	buffer[0x01] = (value & 0x00FF0000) >> 16;
	buffer[0x02] = (value & 0x0000FF00) >> 8;
	buffer[0x03] = (value & 0x000000FF) >> 0;

	return;
}

void WriteBE24(unsigned char* buffer, unsigned long value)
{
	buffer[0x00] = (value & 0xFF0000) >> 16;
	buffer[0x01] = (value & 0x00FF00) >> 8;
	buffer[0x02] = (value & 0x0000FF) >> 0;

	return;
}

void WriteBE16(unsigned char* buffer, unsigned int value)
{
	buffer[0x00] = (value & 0xFF00) >> 8;
	buffer[0x01] = (value & 0x00FF) >> 0;

	return;
}

void WriteLE16(unsigned char* buffer, unsigned int value)
{
	buffer[0x00] = (value & 0x00FF) >> 0;
	buffer[0x01] = (value & 0xFF00) >> 8;

	return;
}

void WriteLE24(unsigned char* buffer, unsigned long value)
{
	buffer[0x00] = (value & 0x0000FF) >> 0;
	buffer[0x01] = (value & 0x00FF00) >> 8;
	buffer[0x02] = (value & 0xFF0000) >> 16;

	return;
}

void WriteLE32(unsigned char* buffer, unsigned long value)
{
	buffer[0x00] = (value & 0x000000FF) >> 0;
	buffer[0x01] = (value & 0x0000FF00) >> 8;
	buffer[0x02] = (value & 0x00FF0000) >> 16;
	buffer[0x03] = (value & 0xFF000000) >> 24;

	return;
}

unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	deltaValue = WriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		if (curChan != 3)
		{
			Write8B(&buffer[pos], 0xC0 | curChan);
		}
		else
		{
			Write8B(&buffer[pos], 0xC9);
		}

		Write8B(&buffer[pos + 1], inst);
		Write8B(&buffer[pos + 2], 0);

		if (curChan != 3)
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}
		else
		{
			Write8B(&buffer[pos + 3], 0x99);
		}

		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	deltaValue = WriteDeltaTime(buffer, pos, length);
	pos += deltaValue;

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}

unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	deltaValue = WriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		Write8B(&buffer[pos], 0xC0 | curChan);

		Write8B(&buffer[pos + 1], inst);
		Write8B(&buffer[pos + 2], 0);

		if (curChan == 10)
		{
			Write8B(&buffer[pos + 3], 0x90 | 16);
		}
		else if (curChan == 16)
		{
			Write8B(&buffer[pos + 3], 0x90 | 10);
		}
		else
		{
			Write8B(&buffer[pos + 3], 0x90 | curChan);
		}



		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	deltaValue = WriteDeltaTime(buffer, pos, length);
	pos += deltaValue;

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;

	return pos;

}

int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value)
{
	unsigned char valSize;
	unsigned char* valData;
	unsigned int tempLen;
	unsigned int curPos;

	valSize = 0;
	tempLen = value;

	while (tempLen != 0)
	{
		tempLen >>= 7;
		valSize++;
	}

	valData = &buffer[pos];
	curPos = valSize;
	tempLen = value;

	while (tempLen != 0)
	{
		curPos--;
		valData[curPos] = 128 | (tempLen & 127);
		tempLen >>= 7;
	}

	valData[valSize - 1] &= 127;

	pos += valSize;

	if (value == 0)
	{
		valSize = 1;
	}
	return valSize;
}

unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;
	deltaValue = WriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	if (firstNote == 1)
	{
		Write8B(&buffer[pos], 0xC0 | curChan);

		Write8B(&buffer[pos + 1], inst);
		Write8B(&buffer[pos + 2], 0);

		Write8B(&buffer[pos + 3], 0x90 | curChan);

		pos += 4;
	}

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], curVol);
	pos++;

	return pos;

}

unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst)
{
	int deltaValue;

	deltaValue = WriteDeltaTime(buffer, pos, delay);
	pos += deltaValue;

	Write8B(&buffer[pos], note);
	pos++;
	Write8B(&buffer[pos], 0);
	pos++;
	return pos;
}

void gb2MID(FILE* rom, long banks[50], int numBanks, long format, char parameters[4][50])
{
	if (numBanks > 1)
	{
		multiBanks = 1;
	}
	for (curBank = 0; curBank < numBanks; curBank++)
	{
		switch (format)
		{
		case AJ_Gonzalez:
			AJGProc(banks[curBank], parameters);
			break;
		case AudioArts:
			AAProc(banks[curBank]);
			break;
		case Beam_Software:
			BeamProc(banks[curBank]);
			break;
		case Capcom:
			CapProc(banks[curBank]);
			break;
		case Carillon_Player:
			CarillonProc(banks[curBank]);
			break;
		case Climax:
			IMEDProc(parameters);
			break;
		case Cosmigo:
			CosmigoProc(banks[curBank]);
			break;
		case Cube:
			CubeProc(banks[curBank], parameters);
			break;
		case David_Shea:
			DShProc(banks[curBank]);
			break;
		case David_Warhol:
			RTAProc(banks[curBank]);
			break;
		case David_Whittaker:
			DWProc(banks[curBank]);
			break;
		case Digital_Eclipse_1:
			DE1Proc(banks[curBank], parameters);
			break;
		case Ed_Magnin:
			MagninProc(banks[curBank], parameters);
			break;
		case Game_Freak:
			GFProc(banks[curBank]);
			break;
		case GHX:
			GHXProc(banks[curBank]);
			break;
		case HAL_Laboratory:
			HALProc(banks[curBank]);
			break;
		case Hirokazu_Tanaka:
			NintProc(banks[curBank], parameters);
			break;
		case Hiroshi_Wada:
			MM1Proc(banks[curBank]);
			break;
		case Hirotomo_Nakamura:
			MM2Proc(banks[curBank]);
			break;
		case Hudson_Soft:
			HSProc(banks[curBank]);
			break;
		case Imagineering:
			ImgnProc(banks[curBank]);
			break;
		case Junichi_Saito:
			JSaitoProc(banks[curBank]);
			break;
		case Jeroen_Tel:
			JTProc(parameters);
			break;
		case Konami:
			KonProc(banks[curBank], parameters);
			break;
		case Kouji_Murata:
			MM3Proc(banks[curBank], parameters);
			break;
		case Kozue_Ishikawa:
			WL2Proc(banks[curBank]);
			break;
		case Kyouhei_Sada:
			SadaProc(banks[curBank], parameters);
			break;
		case Lufia:
			LufiaProc(banks[curBank]);
			break;
		case Make_Software:
			MakeProc(banks[curBank]);
			break;
		case Mark_Cooksey:
			MCProc(banks[curBank]);
			break;
		case Martin_Walker:
			MWProc(banks[curBank]);
			break;
		case MIDI:
			MIDProc();
			break;
		case MPlay:
			MPlayProc(banks[curBank], parameters);
			break;
		case MusyX:
			MXProc(banks[curBank], parameters);
			break;
		case Natsume:
			NatProc(banks[curBank], parameters);
			break;
		case NMK:
			RCProc(banks[curBank]);
			break;
		case Ocean:
			OcnProc(banks[curBank]);
			break;
		case Paragon_5:
			P5Proc(banks[curBank]);
			break;
		case Probe_Software:
			ProbProc(banks[curBank]);
			break;
		case RARE:
			RAREProc(parameters);
			break;
		case Ryohji_Yoshitomi:
			M2Proc(banks[curBank]);
			break;
		case Sheep:
			SheepProc(banks[curBank]);
			break;
		case Square:
			SqrProc(banks[curBank]);
			break;
		case Sunsoft:
			SSProc(banks[curBank]);
			break;
		case Tarantula_Studios:
			TarantulaProc(banks[curBank], parameters);
			break;
		case Technos_Japan:
			TJProc(banks[curBank]);
			break;
		case Tiertex:
			TTProc(banks[curBank], parameters);
			break;
		case Titus_1:
			Tit1Proc(banks[curBank]);
			break;
		case Titus_2:
			Tit2Proc(banks[curBank], parameters);
			break;
		case TOSE:
			TOSEProc(parameters);
			break;
		default:
			MIDProc();
			break;
		}
	}

	if (rom != NULL)
	{
		fclose(rom);
	}
}