/*WAV header template info*/
struct header
{
	char riffID[4];
	unsigned int fileSize;
	char waveID[4];
	char fmtID[4];
	unsigned int blockAlign;
	unsigned short dataFmt;
	unsigned short channels;
	unsigned int sampleRate;
	unsigned int byteRate;
	unsigned short bytesPerSamp;
	unsigned short bits;
	char dataID[4];
	unsigned int dataSize;
};
