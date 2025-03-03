/*Based on UnSQZ from OpenTitus (TTF re-implementation) by Erik Stople*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "SQZDEC.H"

#define bankSize 16384

FILE* cmp, * bin;
char outfile[1000000];
int compType;
long cmpSize;
long decmpSize;
int i;
char b1;
char b2;
char b3;
char b4;
int decompPos;

unsigned char* compData;
unsigned char* decompData;

unsigned short ReadLE162(unsigned char* Data);

/*Convert little-endian pointer to big-endian*/
unsigned short ReadLE162(unsigned char* Data)
{
    return (Data[0] << 0) | (Data[1] << 8);
}

void UnSQZ(unsigned char* compData, unsigned char* decompData, long compLength)
{
    compType = ReadLE16(&compData[0]);
    decmpSize = ReadLE16(&compData[2]);

    /*Huffman + RLE compression*/
    if (compType != 1)
    {
        unsigned short treesize = (((unsigned short)compData[5]) << 8) + (unsigned short)compData[4];
        unsigned short bintree;
        unsigned short node = 0;
        unsigned char nodeL;
        int state = 0;
        unsigned short count;
        unsigned int i;
        unsigned short j;
        unsigned char bit;
        unsigned char last;
        unsigned int out_pos = 0;

        compData += 4;

        for (i = 2 + treesize; i < compLength; i++) {
            for (bit = 128; bit >= 1; bit >>= 1) {

                if (compData[i] & bit)
                    node++;
                bintree = (((unsigned short)compData[3 + node * 2]) << 8) + (unsigned short)compData[2 + node * 2];
                if (bintree <= 0x7FFF)
                    node = bintree >> 1;
                else {
                    node = bintree & 0x7FFF;
                    nodeL = (unsigned char)(node & 0x00FF);
                    if (state == 0) {
                        if (node < 0x100) {
                            last = nodeL;
                            decompData[out_pos++] = last;
                        }
                        else if (nodeL == 0) {
                            state = 1;
                        }
                        else if (nodeL == 1) {
                            state = 2;
                        }
                        else {
                            for (j = 1; j <= (unsigned short)nodeL; j++)
                                decompData[out_pos++] = last;
                        }
                    }
                    else if (state == 1) {
                        for (j = 1; j <= node; j++)
                            decompData[out_pos++] = last;
                        state = 0;
                    }
                    else if (state == 2) {
                        count = 256 * (unsigned short)nodeL;
                        state = 3;
                    }
                    else if (state == 3) {
                        count += (unsigned short)nodeL;
                        for (j = 1; j <= count; j++)
                            decompData[out_pos++] = last;
                        state = 0;
                    }
                    node = 0;
                }
            }
        }
    }

    /*LZW*/
    else
    {
        unsigned char nbit = 9;
        unsigned char bitadd = 0;
        unsigned int i = 0;
        unsigned int k_pos = 0;
        unsigned int k;
        unsigned int tmp_k;
        unsigned int w;
        unsigned int out_pos = 0;
        char addtodict = 0;
        unsigned int* dict_prefix;
        unsigned char* dict_character;
        unsigned int dict_length = 0;
        unsigned char dict_stack[LZW_MAX_TABLE];

        decompData += 4;

        dict_prefix = (unsigned int*)malloc(LZW_MAX_TABLE * sizeof(unsigned int));
        dict_character = (unsigned char*)malloc(LZW_MAX_TABLE * sizeof(unsigned char));

        if (dict_prefix == NULL || dict_character == NULL) {
            if (dict_prefix != NULL)
                free(dict_prefix);
            if (dict_character != NULL)
                free(dict_character);
            printf("ERROR: Insufficient memory!\n");
            return (-2);
        }

        while ((k_pos < compLength) && (out_pos < decmpSize)) {

            k = 0;
            i = 0;
            while (i < 4) {
                k <<= 8;
                if ((k_pos + i < compLength) && (i * 8 < bitadd + nbit))
                    k += compData[k_pos + i];
                i++;
            }
            k <<= bitadd;
            k >>= sizeof(int) * 8 - nbit;

            bitadd += nbit;
            while (bitadd > 8) {
                bitadd -= 8;
                k_pos++;
            }
            if (k == LZW_CLEAR_CODE) {
                nbit = 9;
                dict_length = 0;
                addtodict = 0;
            }
            else if (k != LZW_END_CODE) {
                if (k > 255 && k < LZW_FIRST + dict_length) {
                    i = 0;
                    tmp_k = k;
                    while (tmp_k >= LZW_FIRST) {
                        dict_stack[i] = dict_character[tmp_k - LZW_FIRST];
                        tmp_k = dict_prefix[tmp_k - LZW_FIRST];
                        if (i++ >= LZW_MAX_TABLE)
                        {
                            free(dict_prefix);
                            free(dict_character);
                            printf("ERROR: Invalid file!\n");
                            return (-1);
                        }
                    }
                    dict_stack[i++] = tmp_k;
                    tmp_k = i - 1;
                    while (--i > 0)
                        decompData[out_pos++] = dict_stack[i];

                    decompData[out_pos++] = dict_stack[0];

                    dict_stack[0] = dict_stack[tmp_k];

                }
                else if (k > 255 && k >= LZW_FIRST + dict_length) {
                    i = 1;
                    tmp_k = w;
                    while (tmp_k >= LZW_FIRST) {
                        dict_stack[i] = dict_character[tmp_k - LZW_FIRST];
                        tmp_k = dict_prefix[tmp_k - LZW_FIRST];
                        if (i++ >= LZW_MAX_TABLE)
                        {
                            free(dict_prefix);
                            free(dict_character);
                            return (-1);
                        }
                    }
                    dict_stack[i++] = tmp_k;

                    if (dict_length > 0)
                        tmp_k = dict_character[dict_length - 1];
                    else
                        tmp_k = w;

                    dict_stack[0] = tmp_k;
                    tmp_k = i - 1;
                    while (--i > 0)
                        decompData[out_pos++] = dict_stack[i];

                    decompData[out_pos++] = dict_stack[0];
                    dict_stack[0] = dict_stack[tmp_k];
                }
                else {
                    decompData[out_pos++] = k;
                    dict_stack[0] = k;
                }
                if (addtodict && (LZW_FIRST + dict_length < LZW_MAX_TABLE)) {
                    dict_character[dict_length] = dict_stack[0];
                    dict_prefix[dict_length] = w;
                    dict_length++;
                }

                w = k;
                addtodict = 1;
            }
            if ((LZW_FIRST + dict_length == ((unsigned int)(1) << nbit)) && (nbit < 12))
                nbit++;
        }

        free(dict_prefix);
        free(dict_character);
    }
}