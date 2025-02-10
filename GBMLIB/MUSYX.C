/*MusyX (Factor 5)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "MUSYX.H"
#include "WAV.H"

#define bankSize 16384

struct header wavHeader;

FILE* rom, * mid, * wav;
long bank1;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
char outfile2[10000000];
int songNum;
int numSongs;
int sampNum;
int numSamp;
int mode;
long bankAmt;
int foundTable;
int curMask;
long masterBank;

long asdrTab;
long sfxTab;
int numSfx;
long samTab;
int sampMap;
long numSampMap;
long songTab;
int curInst;
int curVol;
long sampPtr;
long sampSize;
int sampQ;

char* argv3;

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

const char MXMagicBytes[6] = { 0xAF, 0xC9, 0xFC, 0xF3, 0xCF, 0x3F };

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEventGen(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOn(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
unsigned int WriteNoteEventOff(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
int compare(const void* a, const void* b);
void MXsong2mid(int songNum, int bank, long ptr);
void sam2wav(int sampNum, long ptr, long size, int bank, int quality);

int compare(const void* a, const void* b) {
	/*Cast the void pointers to the appropriate type*/
	const int (*rowA)[3] = a;
	const int (*rowB)[3] = b;

	/*Compare the third elements (index 2) of the rows*/
	return (*rowA)[1] - (*rowB)[1];
}

void MXProc(int bank, char parameters[4][50])
{
	bankAmt = bankSize;
	mode = 1;
	foundTable = 0;
	curMask = 0;
	masterBank = 0;

	curInst = 0;
	curVol = 100;
	sampPtr = 0;
	sampSize = 0;
	sampQ = 0;

	argv3 = parameters[0];

	if (!strcmp(argv3, "m") || !strcmp(argv3, "M"))
	{
		mode = 1;
	}
	else if (!strcmp(argv3, "s") || !strcmp(argv3, "S"))
	{
		mode = 2;
	}
	else
	{
		mode = 1;
	}

	masterBank = bank;

	fseek(rom, ((masterBank - 1) * bankSize), SEEK_SET);
	romData = (unsigned char*)malloc(bankSize);
	fread(romData, 1, bankSize, rom);

	/*Try to search the bank for song table loader*/
	for (i = 0; i < bankSize; i++)
	{
		if ((!memcmp(&romData[i], MXMagicBytes, 6)) && foundTable != 1)
		{
			tableOffset = i + 6 + bankAmt;
			printf("Found project data at address 0x%04x!\n", tableOffset);
			foundTable = 1;
		}
	}

	if (foundTable == 1)
	{

		/*Get the MusyX project data parameters*/
		i = tableOffset - bankAmt;
		asdrTab = ReadLE16(&romData[i]) + tableOffset;
		printf("ASDR table address: 0x%04X\n", asdrTab);
		i += 2;
		sfxTab = ReadLE16(&romData[i]) + tableOffset;
		printf("Sound effect table address: 0x%04X\n", sfxTab);
		i += 2;
		numSfx = romData[i];
		printf("Number of sound effects: %i\n", numSfx);
		i++;
		samTab = ReadLE16(&romData[i]) + tableOffset;
		printf("Sample table address: 0x%04X\n", samTab);
		i += 2;
		numSamp = romData[i];
		printf("Number of samples: %i\n", numSamp);
		/*Additionally skip an "empty" 1-byte parameter*/
		i += 2;
		sampMap = ReadLE16(&romData[i]) + tableOffset;
		printf("Sample map address: 0x%04X\n", sampMap);
		i += 2;
		printf("Number of entires in sample map: %i\n", numSampMap);
		i++;
		songTab = ReadLE16(&romData[i]) + tableOffset;
		printf("Song table address: 0x%04X\n", songTab);
		i += 2;
		numSongs = romData[i];
		printf("Number of songs: %i\n", numSongs);

		if (mode == 1)
		{
			/*Now convert all the songs*/
			i = songTab - bankAmt;
			for (songNum = 1; songNum <= numSongs; songNum++)
			{
				bank1 = romData[i] + masterBank;
				offset = ReadLE16(&romData[i + 1]);
				printf("Song %i: bank %01X, address 0x%04X\n", songNum, bank1, offset);
				MXsong2mid(songNum, bank1, offset);
				i += 3;
			}
		}

		else if (mode == 2)
		{
			/*Extract all the samples*/
			i = samTab - bankAmt;
			for (sampNum = 1; sampNum <= numSamp; sampNum++)
			{
				sampPtr = ReadLE16(&romData[i]);
				sampSize = ReadLE16(&romData[i + 2]) * 0x10;
				sampQ = romData[i + 4];
				bank1 = romData[i + 5] + masterBank;
				printf("Sample %i: offset: 0x%04X, size: %01X, quality: %01X, bank: %01X\n", sampNum, sampPtr, sampSize, sampQ, bank);
				sam2wav(sampNum, sampPtr, sampSize, bank1, sampQ);
				i += 6;

			}
		}

		free(romData);
	}
	else
	{
		printf("ERROR: Magic bytes not found!\n");
		exit(-1);
	}
}

void MXsong2mid(int songNum, int bank, long ptr)
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	int seqPos = 0;
	int patPos = 0;
	int numPat = 0;
	int numTracks = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int seqEnd = 0;
	int chanEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	long curDelay = 0;
	long ctrlDelay = 0;
	int curVel = 0;
	int tempo = 150;
	int k = 0;
	int l = 0;
	int activeChan[4] = { 0, 0, 0, 0 };
	unsigned char command[4];
	int ticks = 24;
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	long patBase = 0;
	long trackTab = 0;
	long patTab = 0;
	long patterns[4];

	long tempPos = 0;
	int holdNote = 0;
	long startPos = 0;
	long ptrPos = 0;
	int progC = 0;
	int noteL = 0;

	int prevDelay = 0;
	int prevLen = 0;
	int lastTime = 0;
	int tempTime = 0;
	int nextDelay = 0;
	int nextLen = 0;
	int delayVal = 0;
	int patDelay = 0;
	int finalNote = 0;
	int tempDelay = 0;
	int curPatDelay = 0;
	int tempLen = 0;
	int totalTDelay = 0;

	int noteOffs[15][3];
	int noteOffPos = 0;
	long tempCtrlDelay = 0;
	int firstFlag = 0;


	midPos = 0;
	ctrlMidPos = 0;

	midLength = 0x10000;
	midData = (unsigned char*)malloc(midLength);

	ctrlMidData = (unsigned char*)malloc(midLength);

	for (j = 0; j < midLength; j++)
	{
		midData[j] = 0;
		ctrlMidData[j] = 0;
	}

	sprintf(outfile, "song%i.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%i.mid!\n", songNum);
		exit(2);
	}
	else
	{
		/*Copy the song data's bank*/
		fseek(rom, ((bank1 - 1) * bankSize), SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize);
		fread(exRomData, 1, bankSize, rom);

		/*Go to the song's position*/
		patPos = ptr - bankAmt;

		/*Get header information*/
		command[0] = exRomData[patPos];
		command[1] = exRomData[patPos + 1];
		command[2] = exRomData[patPos + 2];
		command[3] = exRomData[patPos + 3];

		/*Skip header bytes (channel mapping)*/
		patPos += 0x84;
		patBase = patPos;
		trackTab = ReadBE16(&exRomData[patPos]);
		patPos += 2;
		patTab = ReadBE16(&exRomData[patPos]);
		patPos += 2;
		tempo = ReadBE16(&exRomData[patPos]);
		patPos += 2;

		/*Write MIDI header with "MThd"*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D546864);
		WriteBE32(&ctrlMidData[ctrlMidPos + 4], 0x00000006);
		ctrlMidPos += 8;

		WriteBE16(&ctrlMidData[ctrlMidPos], 0x0001);
		WriteBE16(&ctrlMidData[ctrlMidPos + 2], trackCnt + 1);
		WriteBE16(&ctrlMidData[ctrlMidPos + 4], ticks);
		ctrlMidPos += 6;
		/*Write initial MIDI information for "control" track*/
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x4D54726B);
		ctrlMidPos += 8;
		ctrlMidTrackBase = ctrlMidPos;

		/*Set channel name (blank)*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE16(&ctrlMidData[ctrlMidPos], 0xFF03);
		Write8B(&ctrlMidData[ctrlMidPos + 2], 0);
		ctrlMidPos += 2;


		/*Set initial tempo*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF5103);
		ctrlMidPos += 4;

		WriteBE24(&ctrlMidData[ctrlMidPos], 60000000 / tempo);
		ctrlMidPos += 3;

		/*Set time signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5804);
		ctrlMidPos += 3;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0x04021808);
		ctrlMidPos += 4;

		/*Set key signature*/
		WriteDeltaTime(ctrlMidData, ctrlMidPos, 0);
		ctrlMidPos++;
		WriteBE24(&ctrlMidData[ctrlMidPos], 0xFF5902);
		ctrlMidPos += 4;

		/*Get the track pattern pointers*/
		for (curTrack = 0; curTrack < 4; curTrack++)
		{
			patPos = patBase + trackTab + (2 * curTrack);
			patterns[curTrack] = ReadBE16(&exRomData[patPos]);
			patPos += 2;
		}

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			firstNote = 1;
			curDelay = 0;
			ctrlDelay = 0;
			curNoteLen = 0;
			patDelay = 0;
			prevDelay = 0;
			prevLen = 0;
			noteOffPos = 0;
			curPatDelay = 0;
			tempLen = 0;
			firstFlag = 0;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			/*Add track header*/
			/*
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;


			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);
			*/

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			if (patterns[curTrack] == 0x0000)
			{
				chanEnd = 1;
			}

			else
			{

				for (l = 0; l < 15; l++)
				{
					noteOffs[l][0] = -1;
					noteOffs[l][1] = -1;
					noteOffs[l][2] = -1;
				}
				chanEnd = 0;
				patPos = patBase + patterns[curTrack];
				while (chanEnd == 0)
				{
					if (exRomData[patPos + 2] < 0xFE)
					{
						curNoteLen = 0;

						patDelay = ReadBE16(&exRomData[patPos]) - curPatDelay;
						curDelay = ReadBE16(&exRomData[patPos]) - curPatDelay;

						numPat = exRomData[patPos + 2];
						ptrPos = patBase + patTab + (numPat * 2);
						seqPos = ReadBE16(&exRomData[ptrPos]) + patBase;
						curPatDelay = 0;
						seqEnd = 0;
						nextDelay = 0;
					}
					else
					{
						chanEnd = 1;
					}

					while (seqEnd == 0)
					{
						command[0] = exRomData[seqPos];
						command[1] = exRomData[seqPos + 1];
						command[2] = exRomData[seqPos + 2];
						command[3] = exRomData[seqPos + 3];

						/*Rest*/
						if (command[0] == 0x00)
						{
							delayVal = command[1];
							curPatDelay += command[1];
							seqPos += 2;

							/*Is there a program change?*/
							command[0] = exRomData[seqPos];
							progC = command[0] & 0x80;

							if (progC == 0x80)
							{
								curInst = command[0] & 0x7F;
								firstNote = 1;
								seqPos++;
							}
						}

						/*End of pattern/sequence*/
						else if (command[0] == 0xF0 && command[1] == 0x00 && command[2] == 0xFF)
						{
							qsort(noteOffs, 15, sizeof(noteOffs[0]), compare);
							firstFlag = 0;
							for (l = 0; l < 15; l++)
							{
								if (noteOffs[l][1] != -1)
								{
									tempLen = noteOffs[l][1] - ctrlDelay;
									if (firstFlag == 1)
									{
										tempLen = noteOffs[l][1] - totalTDelay;
									}
									totalTDelay = noteOffs[l][1];
									tempPos = WriteNoteEventOff(midData, midPos, noteOffs[l][0], curNoteLen, tempLen, firstNote, curTrack, curInst);
									midPos = tempPos;
									ctrlDelay = noteOffs[l][1];
									curPatDelay += tempLen;
									noteOffs[l][0] = -1;
									noteOffs[l][1] = -1;
									noteOffs[l][2] = -1;
									firstFlag = 1;
								}
							}
							seqEnd = 1;
							patPos += 3;
						}

						/*Play note*/
						else
						{
							progC = 0;
							noteL = 0;

							/*Get the velocity/volume of the note*/
							curVel = command[0] & 0xF0;
							curVol = curVel * 0.4;

							/*Get the note's timing/delay*/
							delayVal = command[1] + ((command[0] & 0x0F) * 0x100);

							seqPos += 2;

							command[0] = exRomData[seqPos];
							/*Get the current note*/
							progC = command[0] & 0x80;
							curNote = command[0] & 0x7F;

							/*Is there a program change?*/
							if (progC == 0x80)
							{
								seqPos++;
								command[0] = exRomData[seqPos];
								curInst = command[0];
								firstNote = 1;
							}

							seqPos++;
							command[0] = exRomData[seqPos];
							command[1] = exRomData[seqPos + 1];
							/*Get the current note's length*/
							noteL = command[0] & 0x80;
							if (noteL == 0x80)
							{
								/*Go to loop pattern*/
								if (command[0] == 0xFE)
								{
									curNoteLen = command[1] & 0x7F;
									seqPos++;
								}
								else
								{
									curNoteLen = command[1] + ((command[0] & 0x15) * 0x100);
									seqPos++;
								}
							}

							else
							{
								curNoteLen = command[0];
							}

							nextDelay = ReadBE16(&exRomData[seqPos + 1]) & 0x7F;
							if (exRomData[seqPos + 3] >= 0x80)
							{
								if (exRomData[seqPos + 5] >= 0x80)
								{
									nextLen = ReadBE16(&exRomData[seqPos + 5]) & 0x7F;
								}
								else
								{
									nextLen = exRomData[seqPos + 5];
								}
							}
							else
							{
								if (exRomData[seqPos + 4] >= 0x80)
								{
									nextLen = ReadBE16(&exRomData[seqPos + 4]) & 0x7F;
								}
								else
								{
									nextLen = exRomData[seqPos + 4];
								}
							}

							if (exRomData[seqPos + 1] == 0xF0 && exRomData[seqPos + 2] == 0x00 && exRomData[seqPos + 3] == 0xF0)
							{
								nextDelay = 0;
							}


							curDelay = delayVal + patDelay;
							ctrlDelay += delayVal + patDelay;

							curPatDelay += delayVal;

							tempCtrlDelay = ctrlDelay;
							totalTDelay = 0;

							qsort(noteOffs, 15, sizeof(noteOffs[0]), compare);

							for (l = 0; l < 15; l++)
							{

								if (tempCtrlDelay >= noteOffs[l][1] && noteOffs[l][1] > 0)
								{
									tempDelay = ctrlDelay - noteOffs[l][1];
									tempLen = curDelay - tempDelay;

									if (firstFlag == 1)
									{
										tempLen = noteOffs[l][1] - totalTDelay;
									}

									tempPos = WriteNoteEventOff(midData, midPos, noteOffs[l][0], curNoteLen, tempLen, firstNote, curTrack, curInst);
									totalTDelay = noteOffs[l][1];
									midPos = tempPos;
									ctrlDelay = noteOffs[l][1];
									curDelay -= tempLen;
									firstFlag = 1;

									noteOffs[l][0] = -1;
									noteOffs[l][1] = -1;
									noteOffs[l][2] = -1;
									if (noteOffPos > 0)
									{
										noteOffPos--;
									}
								}
							}

							if (firstFlag != 0)
							{
								curDelay = tempCtrlDelay - ctrlDelay + patDelay;
								ctrlDelay = tempCtrlDelay;
							}
							firstFlag = 0;



							patDelay = 0;


							if (curTrack == 3 && ctrlDelay >= 200)
							{
								curTrack = 3;
							}

							tempPos = WriteNoteEventOn(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							firstNote = 0;
							midPos = tempPos;

							noteOffs[noteOffPos][0] = curNote;
							noteOffs[noteOffPos][1] = curNoteLen + ctrlDelay;
							noteOffs[noteOffPos][2] = curNoteLen;
							noteOffs[noteOffPos][1] = curNoteLen + ctrlDelay;


							noteOffPos++;

							/*
							tempPos = WriteNoteEventOff(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
							midPos = tempPos;
							ctrlDelay += curNoteLen;
							curDelay = 0;
							lastTime = tempTime;
							*/

							seqPos++;

						}
					}
				}
			}
			/*End of track*/
			WriteBE32(&midData[midPos], 0xFF2F00);
			midPos += 4;

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);
		}
		/*End of control track*/
		ctrlMidPos++;
		WriteBE32(&ctrlMidData[ctrlMidPos], 0xFF2F00);
		ctrlMidPos += 4;

		/*Calculate MIDI channel size*/
		trackSize = ctrlMidPos - ctrlMidTrackBase;
		WriteBE16(&ctrlMidData[ctrlMidTrackBase - 2], trackSize);

		sprintf(outfile, "song%d.mid", songNum);
		fwrite(ctrlMidData, ctrlMidPos, 1, mid);
		fwrite(midData, midPos, 1, mid);
		free(midData);
		free(ctrlMidData);
		fclose(mid);
	}
}

void sam2wav(int sampNum, long ptr, long size, int bank, int quality)
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
	unsigned static char* rawData;
	int rawLength = 0x100000;

	sprintf(outfile2, "sample%i.wav", sampNum);
	if ((wav = fopen(outfile2, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file sample%i.wav!\n", sampNum);
		exit(2);
	}
	else
	{
		/*Copy the sample data's bank (x 2)*/
		fseek(rom, ((bank1 - 1) * bankSize), SEEK_SET);
		exRomData = (unsigned char*)malloc(bankSize * 8);
		fread(exRomData, 1, (bankSize * 8), rom);

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

		if (quality == 0)
		{
			sampleRate = 1920;
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