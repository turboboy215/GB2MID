/*Titus "SQZ" decompressor*/
/*Based on UnSQZ from OpenTitus (TTF re-implementation) by Erik Stople*/

#define LZW_CLEAR_CODE 0x100
#define LZW_END_CODE   0x101
#define LZW_FIRST      0x102
#define LZW_MAX_TABLE  4096

void UnSQZ(unsigned char* compData, unsigned char* decompData, long compLength);