/*Nintendo Software Technology (Bionic Commando: Elite Forces/Crystalis)*/
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "SHARED.H"
#include "BIONIC.H"

#define bankSize 16384

FILE* rom, * mid;
long bank;
long offset;
long tablePtrLoc;
long tableOffset;
int i, j;
char outfile[1000000];
int songNum;
long songPtrs[4];
long bankAmt;
int curInst;
int songBank;
int gameNum;

unsigned int BCPtrs[19][5] =
{
	0x4182, 0x4000, 0x45B9, 0x4AF3, 0x76,
	0x420F, 0x4000, 0x467C, 0x4BE0, 0x78,
	0x54F9, 0x51DF, 0x5846, 0x5BED, 0x78,
	0x64C3, 0x6216, 0x6860, 0x6C37, 0x78,
	0x74E2, 0x7196, 0x78A0, 0x7C41, 0x78,
	0x4406, 0x4000, 0x47AC, 0x4B3B, 0x79,
	0x5C56, 0x53D0, 0x64FC, 0x6E5A, 0x79,
	0x7A23, 0x7A17, 0x7A2F, 0x7A3B, 0x79,
	0x4D4A, 0x48B3, 0x514E, 0x5275, 0x7A,
	0x41BE, 0x4000, 0x478A, 0x4CCF, 0x7B,
	0x53E4, 0x5385, 0x542B, 0x543F, 0x7B,
	0x4455, 0x4000, 0x514D, 0x5DCD, 0x7C,
	0x6D6D, 0x6D5B, 0x6D7F, 0x6D93, 0x7C,
	0x451A, 0x4000, 0x47B5, 0x499A, 0x50,
	0x4F5B, 0x4F05, 0x4FB4, 0x4FEF, 0x50,
	0x44E7, 0x4000, 0x4B7F, 0x51D8, 0x51,
	0x5169, 0x5095, 0x5231, 0x52DE, 0x50,
	0x61C8, 0x5CD1, 0x61DA, 0x645E, 0x51,
	0x72A4, 0x6A29, 0x72B6, 0x72CA, 0x51
};

unsigned int CrysPtrs[21][5] =
{
	0x4000, 0x42A1, 0x4596, 0x4909, 0x76,
	0x4FDD, 0x5714, 0x5A2B, 0x5E73, 0x76,
	0x4000, 0x476D, 0x4C91, 0x54A6, 0x78,
	0x5BED, 0x5DA0, 0x6186, 0x6445, 0x78,
	0x4000, 0x4395, 0x487A, 0x4C6C, 0x79,
	0x5388, 0x568D, 0x5912, 0x5BDC, 0x79,
	0x4000, 0x4491, 0x491C, 0x4A2F, 0x73,
	0x4EC8, 0x5238, 0x56B6, 0x5A23, 0x73,
	0x4000, 0x452D, 0x4ECE, 0x56C2, 0x72,
	0x5DBF, 0x5FE5, 0x6515, 0x66E8, 0x72,
	0x4000, 0x420B, 0x43EC, 0x4543, 0x55,
	0x4AAC, 0x4FD8, 0x5510, 0x592E, 0x55,
	0x5FC9, 0x5FDB, 0x6199, 0x6387, 0x55,
	0x4000, 0x4012, 0x4101, 0x41ED, 0x56,
	0x4458, 0x460C, 0x47C3, 0x4893, 0x56,
	0x48AD, 0x54D6, 0x57DB, 0x5957, 0x56,
	0x5B9E, 0x6073, 0x65DB, 0x6991, 0x56,
	0x4000, 0x4C12, 0x56B0, 0x6499, 0x57,
	0x4000, 0x4656, 0x4F30, 0x527D, 0x58,
	0x582A, 0x5EB0, 0x658C, 0x6B70, 0x58,
	0x4E27, 0x53B5, 0x5A4A, 0x60B5, 0x6F
};

unsigned char* romData;
unsigned char* exRomData;
unsigned char* midData;
unsigned char* ctrlMidData;

long midLength;

char* argv3;

/*Function prototypes*/
unsigned short ReadLE16(unsigned char* Data);
unsigned short ReadBE16(unsigned char* Data);
void Write8B(unsigned char* buffer, unsigned int value);
void WriteBE32(unsigned char* buffer, unsigned long value);
void WriteBE24(unsigned char* buffer, unsigned long value);
void WriteBE16(unsigned char* buffer, unsigned int value);
unsigned int WriteNoteEvent(unsigned char* buffer, unsigned int pos, unsigned int note, int length, int delay, int firstNote, int curChan, int inst);
int WriteDeltaTime(unsigned char* buffer, unsigned int pos, unsigned int value);
void Bionicsong2mid(int songNum, long songPtrs[4]);

void BionicProc(int bank)
{
	curInst = 0;
	if (bank == 0x14)
	{
		/*Bionic Commando: Elite Forces*/
		gameNum = 1;
	}
	else
	{
		/*Crystalis*/
		gameNum = 2;
	}

	songNum = 1;

	if (gameNum == 1)
	{
		while (songNum <= 19)
		{
			songPtrs[0] = BCPtrs[songNum - 1][0];
			songPtrs[1] = BCPtrs[songNum - 1][1];
			songPtrs[2] = BCPtrs[songNum - 1][2];
			songPtrs[3] = BCPtrs[songNum - 1][3];
			songBank = BCPtrs[songNum - 1][4];

			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			printf("Song %i bank: %02X\n", songNum, songBank);

			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);
			Bionicsong2mid(songNum, songPtrs);
			free(romData);

			songNum++;
			;
		}
	}

	else
	{
		while (songNum <= 21)
		{
			songPtrs[0] = CrysPtrs[songNum - 1][0];
			songPtrs[1] = CrysPtrs[songNum - 1][1];
			songPtrs[2] = CrysPtrs[songNum - 1][2];
			songPtrs[3] = CrysPtrs[songNum - 1][3];
			songBank = CrysPtrs[songNum - 1][4];

			printf("Song %i channel 1: 0x%04X\n", songNum, songPtrs[0]);
			printf("Song %i channel 2: 0x%04X\n", songNum, songPtrs[1]);
			printf("Song %i channel 3: 0x%04X\n", songNum, songPtrs[2]);
			printf("Song %i channel 4: 0x%04X\n", songNum, songPtrs[3]);
			printf("Song %i bank: %02X\n", songNum, songBank);
			fseek(rom, 0, SEEK_SET);
			romData = (unsigned char*)malloc(bankSize * 2);
			fread(romData, 1, bankSize, rom);
			fseek(rom, ((songBank - 1) * bankSize), SEEK_SET);
			fread(romData + bankSize, 1, bankSize, rom);
			Bionicsong2mid(songNum, songPtrs);
			free(romData);

			songNum++;
		}
	}
}

void Bionicsong2mid(int songNum, long songPtrs[4])
{
	static const char* TRK_NAMES[4] = { "Square 1", "Square 2", "Wave", "Noise" };
	long romPos = 0;
	long seqPos = 0;
	int curTrack = 0;
	int trackCnt = 4;
	int ticks = 120;
	int tempo = 120;
	int k = 0;
	int seqEnd = 0;
	int curNote = 0;
	int curNoteLen = 0;
	int repeat = 0;
	long repeatStart;
	unsigned char command[4];
	int firstNote = 1;
	unsigned int midPos = 0;
	unsigned int ctrlMidPos = 0;
	long midTrackBase = 0;
	long ctrlMidTrackBase = 0;
	int valSize = 0;
	long trackSize = 0;
	int tempByte = 0;
	int curDelay = 0;
	int ctrlDelay = 0;
	long tempPos = 0;
	int inMacro = 0;
	int macroBank = 0;
	long macroPos = 0;
	long macroEnd = 0;

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

	sprintf(outfile, "song%d.mid", songNum);
	if ((mid = fopen(outfile, "wb")) == NULL)
	{
		printf("ERROR: Unable to write to file song%d.mid!\n", songNum);
		exit(2);
	}
	else
	{
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

		for (curTrack = 0; curTrack < trackCnt; curTrack++)
		{
			seqPos = songPtrs[curTrack];
			firstNote = 1;
			/*Write MIDI chunk header with "MTrk"*/
			WriteBE32(&midData[midPos], 0x4D54726B);
			midPos += 8;
			midTrackBase = midPos;

			curDelay = 0;
			ctrlDelay = 0;
			seqEnd = 0;

			curNote = 0;
			curNoteLen = 0;
			repeat = -1;
			inMacro = 0;

			/*Add track header*/
			valSize = WriteDeltaTime(midData, midPos, 0);
			midPos += valSize;
			WriteBE16(&midData[midPos], 0xFF03);
			midPos += 2;
			Write8B(&midData[midPos], strlen(TRK_NAMES[curTrack]));
			midPos++;
			sprintf((char*)&midData[midPos], TRK_NAMES[curTrack]);
			midPos += strlen(TRK_NAMES[curTrack]);

			/*Calculate MIDI channel size*/
			trackSize = midPos - midTrackBase;
			WriteBE16(&midData[midTrackBase - 2], trackSize);

			while (seqEnd == 0 && midPos < 48000 && ctrlDelay < 110000)
			{
				if (inMacro == 0)
				{
					command[0] = romData[seqPos];
					command[1] = romData[seqPos + 1];
					command[2] = romData[seqPos + 2];
					command[3] = romData[seqPos + 3];
				}
				else
				{
					command[0] = exRomData[seqPos];
					command[1] = exRomData[seqPos + 1];
					command[2] = exRomData[seqPos + 2];
					command[3] = exRomData[seqPos + 3];
				}


				/*Play note*/
				if (command[0] <= 0x6F)
				{
					curNote = command[0] + 36;
					if (curTrack == 2 && curNote >= 12)
					{
						curNote -= 12;
					}
					if (inMacro == 0)
					{
						curNoteLen = ReadBE16(&romData[seqPos + 1]);
					}
					else
					{
						curNoteLen = ReadBE16(&exRomData[seqPos + 1]);
					}

					if (gameNum == 1)
					{
						if (songNum == 12 || songNum == 15 || songNum == 16 || songNum == 17 || songNum == 18)
						{
							curNoteLen *= 4;
						}
					}
					else
					{
						if (songNum == 18)
						{
							curNoteLen *= 4;
						}
					}

					tempPos = WriteNoteEvent(midData, midPos, curNote, curNoteLen, curDelay, firstNote, curTrack, curInst);
					firstNote = 0;
					midPos = tempPos;
					curDelay = 0;
					ctrlDelay += curNoteLen;
					seqPos += 3;
				}

				/*Set delay*/
				else if (command[0] == 0x70)
				{
					if (inMacro == 0)
					{
						curNoteLen = ReadBE16(&romData[seqPos + 1]);
					}
					else
					{
						curNoteLen = ReadBE16(&exRomData[seqPos + 1]);
					}

					if (gameNum == 1)
					{
						if (songNum == 12 || songNum == 15 || songNum == 16 || songNum == 17 || songNum == 18)
						{
							curNoteLen *= 4;
						}
					}
					else
					{
						if (songNum == 18)
						{
							curNoteLen *= 4;
						}
					}

					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					seqPos += 3;
				}

				/*Set volume/envelope and delay*/
				else if (command[0] == 0x71)
				{
					if (inMacro == 0)
					{
						curNoteLen = ReadBE16(&romData[seqPos + 2]);
					}
					else
					{
						curNoteLen = ReadBE16(&exRomData[seqPos + 2]);
					}

					if (gameNum == 1)
					{
						if (songNum == 12 || songNum == 15 || songNum == 16 || songNum == 17 || songNum == 18)
						{
							curNoteLen *= 4;
						}
					}
					else
					{
						if (songNum == 18)
						{
							curNoteLen *= 4;
						}
					}

					curDelay += curNoteLen;
					ctrlDelay += curNoteLen;
					seqPos += 4;
				}

				/*Set length/duty cycle*/
				else if (command[0] == 0x74)
				{
					seqPos += 2;
				}

				/*Repeat section*/
				else if (command[0] == 0x75)
				{
					if (command[1] == 0x00)
					{
						seqEnd = 1;
					}
					else
					{
						if (repeat == -1)
						{
							repeat = command[1];
							if (inMacro == 0)
							{
								repeatStart = ReadLE16(&romData[seqPos + 2]);
							}
							else
							{
								repeatStart = ReadLE16(&exRomData[seqPos + 2]);
							}
						}
						else if (repeat >= 1)
						{
							repeat--;
							seqPos = repeatStart;
						}
						else
						{
							repeat = -1;
							seqPos += 4;
						}
					}
				}

				/*Go to macro*/
				else if (command[0] == 0x77)
				{
					if (inMacro == 0)
					{
						macroBank = command[1] + 1;
						macroPos = ReadLE16(&romData[seqPos + 2]);
						macroEnd = seqPos + 4;

						fseek(rom, 0, SEEK_SET);
						exRomData = (unsigned char*)malloc(bankSize * 2);
						fread(exRomData, 1, bankSize, rom);
						fseek(rom, ((macroBank - 1) * bankSize), SEEK_SET);
						fread(exRomData + bankSize, 1, bankSize, rom);

						inMacro = 1;
						seqPos = macroPos;
					}
					else
					{
						seqEnd = 1;
					}

				}

				/*Return from macro*/
				else if (command[0] == 0x7E)
				{
					if (inMacro == 1)
					{
						inMacro = 0;
						free(exRomData);
						seqPos = macroEnd;
					}
					else
					{
						seqPos++;
					}
				}

				/*Unknown command*/
				else
				{
					seqPos++;
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