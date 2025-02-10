/*General MIDI (Loriciel - Best of the Best)*/
#include <stdio.h>
#include "SHARED.H"
#include "MIDI.H"

const char midHeader[4] = { 0x4D, 0x54, 0x68, 0x64 };
const char trackEnd[3] = { 0xFF, 0x2F, 0x00 };

unsigned char* romData;
unsigned char* midData;
long fileSize;
long filePos;
int songNum = 0;


void MIDProc()
{
	/*Get size of file*/
	fseek(rom, 0, SEEK_END);
	fileSize = ftell(rom);
	romData = (unsigned char*)malloc(fileSize);
	fseek(rom, 0, SEEK_SET);

	fread(romData, 1, fileSize, rom);

	/*Look for MIDI files*/
	filePos = 0;
	for (i = 0; i < fileSize; i++)
	{
		if (!memcmp(&romData[i], midHeader, 4))
		{
			songNum++;
			filePos = i;
			printf("Song %i: 0x%04X\n", songNum, filePos);
			i = extMID(songNum, filePos);
		}

		if (i == fileSize)
		{
			break;
		}
	}
}

/*Extract the MIDI file*/
long extMID(int songNum, long ptr)
{
	int romPos = 0;
	int midPos = 0;
	int numTrack = 1;
	int curTrack = 0;
	long ofsStart = 0;
	long ofsEnd = 0;
	int k = 0;
	midPos = 0;

	romPos = ptr;
	ofsStart = ptr;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	for (k = 0; k < midLength; k++)
	{
		midData[k] = 0;
	}

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
		numTrack = romData[romPos + 0x0B];

		while (curTrack < numTrack)
		{
			for (romPos = ptr; romPos < fileSize; romPos++)
			{
				if (!memcmp(&romData[romPos], trackEnd, 3))
				{
					curTrack++;
					if (curTrack == numTrack)
					{
						break;
					}
				}
			}
		}

		ofsEnd = romPos + 2;

		romPos = ofsStart;

		while (romPos <= ofsEnd)
		{
			midData[midPos] = romData[romPos];
			romPos++;
			midPos++;
		}

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(midData, midPos, 1, mid);
		fclose(mid);

	}
	return ofsEnd;
}