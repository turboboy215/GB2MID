/*GB/GBC to MIDI universal converter shell*/
/*By Will Trowbridge*/
/*Portions based on code by ValleyBell*/
/*Uses ZLIB code by ?*/
/*Uses XML-C code by OOXI*/
#include <zlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "GB2MID.H"
#include "XML.C"
#include "XML.H"
#include "GBMLIB/SHARED.H"

#define bankSize 16384
FILE* rom, * mid, * xm, * mod, * txt, * raw, * wav, * cfg, *xml;
int masterBank;
int bank;
int banks[50];
char outfile[1000000];
int songPtr;
int songPtrs[4];
char gameCode[5];
char parameters[4][50];
unsigned long crc;
unsigned long hash;
unsigned long tempCrc;
int driver = 0;
int romSize = 0;
unsigned char checksum[2];
int checksumVar;
unsigned int tempCS;
long romTitle;
int gameVer = 0;
int tempSize = 0;
int romMatch = 0;
int i, j, z;
int tempVer = 0;
int part = 0;
int numParts = 0;
long numROMs = 0;
long itemNo;
int charGet = 0;
int numBanks = 0;
int curPartNum = 0;
uint8_t* foundROM;
uint8_t* curPart;
uint8_t* curTempPart;
uint8_t* tempPart;
uint8_t* bankString;
uint8_t* paramString;
uint8_t* formatString;

unsigned static char* romData;
unsigned static char* fullRomData;
unsigned static char* exRomData;
unsigned static char* midData;
unsigned static char* ctrlMidData;
unsigned static char* xmData;
unsigned static char* modData;
unsigned static char* headerBank;

long midLength;
long xmLength;

int main(int args, char* argv[])
{

    crc = crc32(0L, Z_NULL, 0);
    printf("GB2MID: GB/GBC to MIDI/tracker/sample converter\n");
    if (args != 2)
    {
        printf("Usage: GB2MID <rom>\n");
        return -1;
    }
    else
    {
        if ((rom = fopen(argv[1], "rb")) == NULL)
        {
            printf("ERROR: Unable to open file %s!\n", argv[1]);
            exit(1);
        }
        else
        {
            if ((xml = fopen("GAMELIST.XML", "r")) == NULL)
            {
                printf("ERROR: Unable to open game list XML file!\n");
                exit(1);
            }

            /*Get size of ROM file*/
            fseek(rom, 0, SEEK_END);
            romSize = ftell(rom);
            fseek(rom, 0, SEEK_SET);

            fullRomData = (unsigned char*)malloc(romSize);
            fread(fullRomData, 1, romSize, rom);

            /*Get hash of the ROM*/
            crc = crc32(hash, fullRomData, romSize);

            /*Now get some information from the ROM header*/
            
            /*Get game code (if present)*/
            for (i = 0; i < 4; i++)
            {
                gameCode[i] = fullRomData[i + 0x013F];
            }
            gameCode[4] = 0;

            /*Get global checksum*/
            for (i = 0; i < 2; i++)
            {
                checksum[i] = fullRomData[i + 0x014E];
            }

            checksumVar = checksum[1] + (checksum[0] * 0x100);

            /*Get revision/version*/
            gameVer = fullRomData[0x014C];

            /*Get XML data*/
            struct xml_document* gameList = xml_open_document(xml);
            struct xml_node* root = xml_document_root(gameList);

            /*Get the number of ROMs in the game list*/
            numROMs = (unsigned long)xml_node_children(root);

            for (i = 0; i < numROMs; i++)
            {
                struct xml_node* rootRom = xml_node_child(root, i);
                struct xml_string* rom = xml_node_name(rootRom);

                /*Get XML ROM name*/
                struct xml_node* gameName = xml_node_child(rootRom, 0);
                struct xml_string* gameNameText = xml_node_content(gameName);
                uint8_t* xmlName = calloc(xml_string_length(gameNameText) + 1, sizeof(uint8_t));
                xml_string_copy(gameNameText, xmlName, xml_string_length(gameNameText));
                tempSize = strlen(xmlName);
                xmlName[tempSize] = 0;

                /*Get XML ROM game code*/
                struct xml_node* gameCodeVar = xml_node_child(rootRom, 1);
                struct xml_string* gameCodeText = xml_node_content(gameCodeVar);
                uint8_t* xmlGameCode = calloc(xml_string_length(gameCodeText) + 1, sizeof(uint8_t));
                xml_string_copy(gameCodeText, xmlGameCode, xml_string_length(gameCodeText));
                tempSize = strlen(xmlGameCode);
                xmlGameCode[tempSize] = 0;

                /*Get XML ROM version/revision*/
                struct xml_node* gameRev = xml_node_child(rootRom, 2);
                struct xml_string* gameRevText = xml_node_content(gameRev);
                uint8_t* xmlRev = calloc(xml_string_length(gameRevText) + 1, sizeof(uint8_t));
                xml_string_copy(gameRevText, xmlRev, xml_string_length(gameRevText));
                tempSize = strlen(xmlRev);
                xmlRev[tempSize] = 0;

                /*Get XML ROM checksum (global)*/
                struct xml_node* gameChecksum = xml_node_child(rootRom, 3);
                struct xml_string* gameCSText = xml_node_content(gameChecksum);
                uint8_t* xmlChecksum = calloc(xml_string_length(gameCSText) + 1, sizeof(uint8_t));
                xml_string_copy(gameCSText, xmlChecksum, xml_string_length(gameCSText));
                tempSize = strlen(xmlChecksum);
                xmlChecksum[tempSize] = 0;

                /*Get XML ROM hash (CRC32)*/
                struct xml_node* gameCrc = xml_node_child(rootRom, 4);
                struct xml_string* gameCrcText = xml_node_content(gameCrc);
                uint8_t* xmlCrc = calloc(xml_string_length(gameCrcText) + 1, sizeof(uint8_t));
                xml_string_copy(gameCrcText, xmlCrc, xml_string_length(gameCrcText));
                tempSize = strlen(xmlCrc);
                xmlCrc[tempSize] = 0;

                /*Get XML ROM sound engine/format*/
                struct xml_node* gameFormat = xml_node_child(rootRom, 5);
                struct xml_string* gameFormatText = xml_node_content(gameFormat);
                uint8_t* xmlFormat = calloc(xml_string_length(gameFormatText) + 1, sizeof(uint8_t));
                xml_string_copy(gameFormatText, xmlFormat, xml_string_length(gameFormatText));
                tempSize = strlen(xmlFormat);
                xmlFormat[tempSize] = 0;

                /*Get XML ROM part*/
                struct xml_node* gamePart = xml_node_child(rootRom, 6);
                struct xml_string* gamePartText = xml_node_content(gamePart);
                uint8_t* xmlPart = calloc(xml_string_length(gamePartText) + 1, sizeof(uint8_t));
                xml_string_copy(gamePartText, xmlPart, xml_string_length(gamePartText));
                tempSize = strlen(xmlPart);
                xmlPart[tempSize] = 0;

                /*Get XML ROM banks*/
                struct xml_node* gameBanks = xml_node_child(rootRom, 7);
                struct xml_string* gameBanksText = xml_node_content(gameBanks);
                uint8_t* xmlBanks = calloc(xml_string_length(gameBanksText) + 1, sizeof(uint8_t));
                xml_string_copy(gameBanksText, xmlBanks, xml_string_length(gameBanksText));
                tempSize = strlen(xmlBanks);
                xmlBanks[tempSize] = 0;

                /*Get XML ROM parameters*/
                struct xml_node* gameParameters = xml_node_child(rootRom, 8);
                struct xml_string* gameParamText = xml_node_content(gameParameters);
                uint8_t* xmlParameters = calloc(xml_string_length(gameParamText) + 1, sizeof(uint8_t));
                xml_string_copy(gameParamText, xmlParameters, xml_string_length(gameParamText));
                tempSize = strlen(xmlParameters);
                xmlParameters[tempSize] = 0;

                /*Check values*/
                tempCrc = strtoul(xmlCrc, NULL, 16);
                tempCS = strtoul(xmlChecksum, NULL, 16);
                tempVer = strtoul(xmlRev, NULL, 16);

                /*Check if the ROM matches the current XML entry*/

                /*First, check the CRC32 hash*/
                if (crc == tempCrc)
                {
                    romMatch = 1;
                    foundROM = xmlName;
                    itemNo = i;
                    curPart = xmlPart;
                    bankString = xmlBanks;
                    paramString = xmlParameters;
                    formatString = xmlFormat;
                    break;
                }
            }

            /*If no match, then check the checksum*/
            if (romMatch < 1)
            {
                for (i = 0; i < numROMs; i++)
                {
                    struct xml_node* rootRom = xml_node_child(root, i);
                    struct xml_string* rom = xml_node_name(rootRom);

                    /*Get XML ROM name*/
                    struct xml_node* gameName = xml_node_child(rootRom, 0);
                    struct xml_string* gameNameText = xml_node_content(gameName);
                    uint8_t* xmlName = calloc(xml_string_length(gameNameText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameNameText, xmlName, xml_string_length(gameNameText));
                    tempSize = strlen(xmlName);
                    xmlName[tempSize] = 0;

                    /*Get XML ROM game code*/
                    struct xml_node* gameCodeVar = xml_node_child(rootRom, 1);
                    struct xml_string* gameCodeText = xml_node_content(gameCodeVar);
                    uint8_t* xmlGameCode = calloc(xml_string_length(gameCodeText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameCodeText, xmlGameCode, xml_string_length(gameCodeText));
                    tempSize = strlen(xmlGameCode);
                    xmlGameCode[tempSize] = 0;

                    /*Get XML ROM version/revision*/
                    struct xml_node* gameRev = xml_node_child(rootRom, 2);
                    struct xml_string* gameRevText = xml_node_content(gameRev);
                    uint8_t* xmlRev = calloc(xml_string_length(gameRevText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameRevText, xmlRev, xml_string_length(gameRevText));
                    tempSize = strlen(xmlRev);
                    xmlRev[tempSize] = 0;

                    /*Get XML ROM checksum (global)*/
                    struct xml_node* gameChecksum = xml_node_child(rootRom, 3);
                    struct xml_string* gameCSText = xml_node_content(gameChecksum);
                    uint8_t* xmlChecksum = calloc(xml_string_length(gameCSText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameCSText, xmlChecksum, xml_string_length(gameCSText));
                    tempSize = strlen(xmlChecksum);
                    xmlChecksum[tempSize] = 0;

                    /*Get XML ROM hash (CRC32)*/
                    struct xml_node* gameCrc = xml_node_child(rootRom, 4);
                    struct xml_string* gameCrcText = xml_node_content(gameCrc);
                    uint8_t* xmlCrc = calloc(xml_string_length(gameCrcText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameCrcText, xmlCrc, xml_string_length(gameCrcText));
                    tempSize = strlen(xmlCrc);
                    xmlCrc[tempSize] = 0;

                    /*Get XML ROM sound engine/format*/
                    struct xml_node* gameFormat = xml_node_child(rootRom, 5);
                    struct xml_string* gameFormatText = xml_node_content(gameFormat);
                    uint8_t* xmlFormat = calloc(xml_string_length(gameFormatText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameFormatText, xmlFormat, xml_string_length(gameFormatText));
                    tempSize = strlen(xmlFormat);
                    xmlFormat[tempSize] = 0;

                    /*Get XML ROM part*/
                    struct xml_node* gamePart = xml_node_child(rootRom, 6);
                    struct xml_string* gamePartText = xml_node_content(gamePart);
                    uint8_t* xmlPart = calloc(xml_string_length(gamePartText) + 1, sizeof(uint8_t));
                    xml_string_copy(gamePartText, xmlPart, xml_string_length(gamePartText));
                    tempSize = strlen(xmlPart);
                    xmlPart[tempSize] = 0;

                    /*Get XML ROM banks*/
                    struct xml_node* gameBanks = xml_node_child(rootRom, 7);
                    struct xml_string* gameBanksText = xml_node_content(gameBanks);
                    uint8_t* xmlBanks = calloc(xml_string_length(gameBanksText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameBanksText, xmlBanks, xml_string_length(gameBanksText));
                    tempSize = strlen(xmlBanks);
                    xmlBanks[tempSize] = 0;

                    /*Get XML ROM parameters*/
                    struct xml_node* gameParameters = xml_node_child(rootRom, 8);
                    struct xml_string* gameParamText = xml_node_content(gameParameters);
                    uint8_t* xmlParameters = calloc(xml_string_length(gameParamText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameParamText, xmlParameters, xml_string_length(gameParamText));
                    tempSize = strlen(xmlParameters);
                    xmlParameters[tempSize] = 0;

                    /*Check values*/
                    tempCrc = strtoul(xmlCrc, NULL, 16);
                    tempCS = strtoul(xmlChecksum, NULL, 16);
                    tempVer = strtoul(xmlRev, NULL, 16);

                    /*Check if the ROM matches the current XML entry*/
                    if (checksumVar == tempCS)
                    {
                        romMatch = 2;
                        foundROM = xmlName;
                        itemNo = i;
                        curPart = xmlPart;
                        bankString = xmlBanks;
                        paramString = xmlParameters;
                        formatString = xmlFormat;
                        break;
                    }
                }
            }

            /*If again no match, then check the game code*/
            if (romMatch < 1)
            {
                for (i = 0; i < numROMs; i++)
                {
                    struct xml_node* rootRom = xml_node_child(root, i);
                    struct xml_string* rom = xml_node_name(rootRom);

                    /*Get XML ROM name*/
                    struct xml_node* gameName = xml_node_child(rootRom, 0);
                    struct xml_string* gameNameText = xml_node_content(gameName);
                    uint8_t* xmlName = calloc(xml_string_length(gameNameText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameNameText, xmlName, xml_string_length(gameNameText));
                    tempSize = strlen(xmlName);
                    xmlName[tempSize] = 0;

                    /*Get XML ROM game code*/
                    struct xml_node* gameCodeVar = xml_node_child(rootRom, 1);
                    struct xml_string* gameCodeText = xml_node_content(gameCodeVar);
                    uint8_t* xmlGameCode = calloc(xml_string_length(gameCodeText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameCodeText, xmlGameCode, xml_string_length(gameCodeText));
                    tempSize = strlen(xmlGameCode);
                    xmlGameCode[tempSize] = 0;

                    /*Get XML ROM version/revision*/
                    struct xml_node* gameRev = xml_node_child(rootRom, 2);
                    struct xml_string* gameRevText = xml_node_content(gameRev);
                    uint8_t* xmlRev = calloc(xml_string_length(gameRevText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameRevText, xmlRev, xml_string_length(gameRevText));
                    tempSize = strlen(xmlRev);
                    xmlRev[tempSize] = 0;

                    /*Get XML ROM checksum (global)*/
                    struct xml_node* gameChecksum = xml_node_child(rootRom, 3);
                    struct xml_string* gameCSText = xml_node_content(gameChecksum);
                    uint8_t* xmlChecksum = calloc(xml_string_length(gameCSText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameCSText, xmlChecksum, xml_string_length(gameCSText));
                    tempSize = strlen(xmlChecksum);
                    xmlChecksum[tempSize] = 0;

                    /*Get XML ROM hash (CRC32)*/
                    struct xml_node* gameCrc = xml_node_child(rootRom, 4);
                    struct xml_string* gameCrcText = xml_node_content(gameCrc);
                    uint8_t* xmlCrc = calloc(xml_string_length(gameCrcText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameCrcText, xmlCrc, xml_string_length(gameCrcText));
                    tempSize = strlen(xmlCrc);
                    xmlCrc[tempSize] = 0;

                    /*Get XML ROM sound engine/format*/
                    struct xml_node* gameFormat = xml_node_child(rootRom, 5);
                    struct xml_string* gameFormatText = xml_node_content(gameFormat);
                    uint8_t* xmlFormat = calloc(xml_string_length(gameFormatText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameFormatText, xmlFormat, xml_string_length(gameFormatText));
                    tempSize = strlen(xmlFormat);
                    xmlFormat[tempSize] = 0;

                    /*Get XML ROM part*/
                    struct xml_node* gamePart = xml_node_child(rootRom, 6);
                    struct xml_string* gamePartText = xml_node_content(gamePart);
                    uint8_t* xmlPart = calloc(xml_string_length(gamePartText) + 1, sizeof(uint8_t));
                    xml_string_copy(gamePartText, xmlPart, xml_string_length(gamePartText));
                    tempSize = strlen(xmlPart);
                    xmlPart[tempSize] = 0;

                    /*Get XML ROM banks*/
                    struct xml_node* gameBanks = xml_node_child(rootRom, 7);
                    struct xml_string* gameBanksText = xml_node_content(gameBanks);
                    uint8_t* xmlBanks = calloc(xml_string_length(gameBanksText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameBanksText, xmlBanks, xml_string_length(gameBanksText));
                    tempSize = strlen(xmlBanks);
                    xmlBanks[tempSize] = 0;

                    /*Get XML ROM parameters*/
                    struct xml_node* gameParameters = xml_node_child(rootRom, 8);
                    struct xml_string* gameParamText = xml_node_content(gameParameters);
                    uint8_t* xmlParameters = calloc(xml_string_length(gameParamText) + 1, sizeof(uint8_t));
                    xml_string_copy(gameParamText, xmlParameters, xml_string_length(gameParamText));
                    tempSize = strlen(xmlParameters);
                    xmlParameters[tempSize] = 0;

                    /*Check values*/
                    tempCrc = strtoul(xmlCrc, NULL, 16);
                    tempCS = strtoul(xmlChecksum, NULL, 16);
                    tempVer = strtoul(xmlRev, NULL, 16);

                    /*Check if the ROM matches the current XML entry*/
                    if (!strcmp(xmlGameCode, gameCode) && gameCode[0] != 0)
                    {
                        romMatch = 2;
                        foundROM = xmlName;
                        itemNo = i;
                        curPart = xmlPart;
                        bankString = xmlBanks;
                        paramString = xmlParameters;
                        formatString = xmlFormat;
                        break;
                    }
                }
            }


            
            if (romMatch == 1)
            {
                printf("Identified ROM: \"%s\"\n", foundROM);
            }

            else if (romMatch == 2)
            {
                printf("Exact match not found. Using close match: \"%s\"\n", foundROM);
            }

            else
            {
                printf("ERROR: Unknown or unsupported ROM!\n");
                exit(-1);
            }

            /*Get the bank information*/
            i = 0;
            j = 0;
            z = 0;

            curTempPart = malloc(sizeof(unsigned int));
            while (bankString[i] != 0)
            {
                if (bankString[i] != ',')
                {
                    curTempPart[j] = bankString[i];
                    j++;
                    i++;
                }
                else if (bankString[i] == ',')
                {
                    curTempPart[j] = 0;
                    banks[z] = strtoul(curTempPart, NULL, 16);
                    numBanks++;
                    j = 0;
                    z++;
                    i++;
                }
            }
            curTempPart[j] = 0;
            banks[z] = strtoul(curTempPart, NULL, 16);
            numBanks++;

            /*If there are multiple "part" options available...*/
            /*First, get the total number of options*/
            if (curPart[0] != 0)
            {
                i = 0;
                curTempPart = malloc(sizeof(unsigned int));
                while (curPart[i + 2] != '=')
                {
                    curTempPart[i] = curPart[i + 2];
                    i++;
                }
                curTempPart[i] = 0;
                numParts = strtoul(curTempPart, NULL, 16);

                /*Prompt the user to select an option*/
                printf("%i part options are available. Please select one of the following choices:\n", numParts);

                curTempPart = malloc(sizeof(unsigned int));
                /*Now extract the information*/
                for (i = 0; i < numParts; i++)
                {
                    struct xml_node* rootRom = xml_node_child(root, itemNo + i);
                    struct xml_string* rom = xml_node_name(rootRom);

                    /*Get XML ROM part*/
                    struct xml_node* gamePart = xml_node_child(rootRom, 6);
                    struct xml_string* gamePartText = xml_node_content(gamePart);
                    uint8_t* xmlPart = calloc(xml_string_length(gamePartText) + 1, sizeof(uint8_t));
                    xml_string_copy(gamePartText, xmlPart, xml_string_length(gamePartText));
                    tempSize = strlen(xmlPart);
                    xmlPart[tempSize] = 0;

                    j = 0;
                    while (xmlPart[j] != '=')
                    {
                        j++;
                    }

                    z = 0;
                    curTempPart = xmlPart + j + 1;
                    printf("%i: %s\n", (i + 1), curTempPart);
                }

                scanf("%d", &charGet);

                if (charGet > numParts || charGet < 1)
                {
                    printf("ERROR: Invalid argument!\n");
                    exit(1);
                }

                /*Get the current part's XML data*/
                struct xml_node* rootRom = xml_node_child(root, itemNo + (charGet - 1));
                struct xml_string* rom = xml_node_name(rootRom);

                /*Get XML ROM name*/
                struct xml_node* gameName = xml_node_child(rootRom, 0);
                struct xml_string* gameNameText = xml_node_content(gameName);
                uint8_t* xmlName = calloc(xml_string_length(gameNameText) + 1, sizeof(uint8_t));
                xml_string_copy(gameNameText, xmlName, xml_string_length(gameNameText));
                tempSize = strlen(xmlName);
                xmlName[tempSize] = 0;

                /*Get XML ROM game code*/
                struct xml_node* gameCodeVar = xml_node_child(rootRom, 1);
                struct xml_string* gameCodeText = xml_node_content(gameCodeVar);
                uint8_t* xmlGameCode = calloc(xml_string_length(gameCodeText) + 1, sizeof(uint8_t));
                xml_string_copy(gameCodeText, xmlGameCode, xml_string_length(gameCodeText));
                tempSize = strlen(xmlGameCode);
                xmlGameCode[tempSize] = 0;

                /*Get XML ROM version/revision*/
                struct xml_node* gameRev = xml_node_child(rootRom, 2);
                struct xml_string* gameRevText = xml_node_content(gameRev);
                uint8_t* xmlRev = calloc(xml_string_length(gameRevText) + 1, sizeof(uint8_t));
                xml_string_copy(gameRevText, xmlRev, xml_string_length(gameRevText));
                tempSize = strlen(xmlRev);
                xmlRev[tempSize] = 0;

                /*Get XML ROM checksum (global)*/
                struct xml_node* gameChecksum = xml_node_child(rootRom, 3);
                struct xml_string* gameCSText = xml_node_content(gameChecksum);
                uint8_t* xmlChecksum = calloc(xml_string_length(gameCSText) + 1, sizeof(uint8_t));
                xml_string_copy(gameCSText, xmlChecksum, xml_string_length(gameCSText));
                tempSize = strlen(xmlChecksum);
                xmlChecksum[tempSize] = 0;

                /*Get XML ROM hash (CRC32)*/
                struct xml_node* gameCrc = xml_node_child(rootRom, 4);
                struct xml_string* gameCrcText = xml_node_content(gameCrc);
                uint8_t* xmlCrc = calloc(xml_string_length(gameCrcText) + 1, sizeof(uint8_t));
                xml_string_copy(gameCrcText, xmlCrc, xml_string_length(gameCrcText));
                tempSize = strlen(xmlCrc);
                xmlCrc[tempSize] = 0;

                /*Get XML ROM sound engine/format*/
                struct xml_node* gameFormat = xml_node_child(rootRom, 5);
                struct xml_string* gameFormatText = xml_node_content(gameFormat);
                uint8_t* xmlFormat = calloc(xml_string_length(gameFormatText) + 1, sizeof(uint8_t));
                xml_string_copy(gameFormatText, xmlFormat, xml_string_length(gameFormatText));
                tempSize = strlen(xmlFormat);
                xmlFormat[tempSize] = 0;

                /*Get XML ROM part*/
                struct xml_node* gamePart = xml_node_child(rootRom, 6);
                struct xml_string* gamePartText = xml_node_content(gamePart);
                uint8_t* xmlPart = calloc(xml_string_length(gamePartText) + 1, sizeof(uint8_t));
                xml_string_copy(gamePartText, xmlPart, xml_string_length(gamePartText));
                tempSize = strlen(xmlPart);
                xmlPart[tempSize] = 0;

                /*Get XML ROM banks*/
                struct xml_node* gameBanks = xml_node_child(rootRom, 7);
                struct xml_string* gameBanksText = xml_node_content(gameBanks);
                uint8_t* xmlBanks = calloc(xml_string_length(gameBanksText) + 1, sizeof(uint8_t));
                xml_string_copy(gameBanksText, xmlBanks, xml_string_length(gameBanksText));
                tempSize = strlen(xmlBanks);
                xmlBanks[tempSize] = 0;

                /*Get XML ROM parameters*/
                struct xml_node* gameParameters = xml_node_child(rootRom, 8);
                struct xml_string* gameParamText = xml_node_content(gameParameters);
                uint8_t* xmlParameters = calloc(xml_string_length(gameParamText) + 1, sizeof(uint8_t));
                xml_string_copy(gameParamText, xmlParameters, xml_string_length(gameParamText));
                tempSize = strlen(xmlParameters);
                xmlParameters[tempSize] = 0;

                bankString = xmlBanks;
                paramString = xmlParameters;
                formatString = xmlFormat;

                /*Get the bank information*/
                numBanks = 0;
                i = 0;
                j = 0;
                z = 0;

                curTempPart = malloc(sizeof(unsigned int));
                while (bankString[i] != 0)
                {
                    if (bankString[i] != ',')
                    {
                        curTempPart[j] = bankString[i];
                        j++;
                        i++;
                    }
                    else if (bankString[i] == ',')
                    {
                        curTempPart[j] = 0;
                        banks[z] = strtoul(curTempPart, NULL, 16);
                        numBanks++;
                        j = 0;
                        z++;
                        i++;
                    }
                }
                curTempPart[j] = 0;
                banks[z] = strtoul(curTempPart, NULL, 16);
                numBanks++;

            }

            /*Now parse the sound engine/format*/
            if (!strcmp(formatString, "AJ_Gonzalez"))
            {
                driver = AJ_Gonzalez;
            }
            else if (!strcmp(formatString, "AudioArts"))
            {
                driver = AudioArts;
            }
            else if (!strcmp(formatString, "Beam_Software"))
            {
                driver = Beam_Software;
            }
            else if (!strcmp(formatString, "Capcom"))
            {
                driver = Capcom;
            }
            else if (!strcmp(formatString, "Carillon_Player"))
            {
                driver = Carillon_Player;
            }
            else if (!strcmp(formatString, "Climax"))
            {
                driver = Climax;
            }
            else if (!strcmp(formatString, "Cosmigo"))
            {
                driver = Cosmigo;
            }
            else if (!strcmp(formatString, "Cube"))
            {
                driver = Cube;
            }
            else if (!strcmp(formatString, "David_Shea"))
            {
                driver = David_Shea;
            }
            else if (!strcmp(formatString, "David_Warhol"))
            {
                driver = David_Warhol;
            }
            else if (!strcmp(formatString, "David_Whittaker"))
            {
                driver = David_Whittaker;
            }
            else if (!strcmp(formatString, "Digital_Eclipse_1"))
            {
                driver = Digital_Eclipse_1;
            }
            else if (!strcmp(formatString, "Ed_Magnin"))
            {
                driver = Ed_Magnin;
            }
            else if (!strcmp(formatString, "Game_Freak"))
            {
                driver = Game_Freak;
            }
            else if (!strcmp(formatString, "GHX"))
            {
                driver = GHX;
            }
            else if (!strcmp(formatString, "HAL_Laboratory"))
            {
                driver = HAL_Laboratory;
            }
            else if (!strcmp(formatString, "Hirokazu_Tanaka"))
            {
                driver = Hirokazu_Tanaka;
            }
            else if (!strcmp(formatString, "Hiroshi_Wada"))
            {
                driver = Hiroshi_Wada;
            }
            else if (!strcmp(formatString, "Hirotomo_Nakamura"))
            {
                driver = Hirotomo_Nakamura;
            }
            else if (!strcmp(formatString, "Hudson_Soft"))
            {
                driver = Hudson_Soft;
            }
            else if (!strcmp(formatString, "Imagineering"))
            {
                driver = Imagineering;
            }
            else if (!strcmp(formatString, "Jeroen_Tel"))
            {
                driver = Jeroen_Tel;
            }
            else if (!strcmp(formatString, "Junichi_Saito"))
            {
                driver = Junichi_Saito;
            }
            else if (!strcmp(formatString, "Konami"))
            {
                driver = Konami;
            }
            else if (!strcmp(formatString, "Kouji_Murata"))
            {
                driver = Kouji_Murata;
            }
            else if (!strcmp(formatString, "Kozue_Ishikawa"))
            {
                driver = Kozue_Ishikawa;
            }
            else if (!strcmp(formatString, "Kyouhei_Sada"))
            {
                driver = Kyouhei_Sada;
            }
            else if (!strcmp(formatString, "Lufia"))
            {
                driver = Lufia;
            }
            else if (!strcmp(formatString, "Make_Software"))
            {
                driver = Make_Software;
            }
            else if (!strcmp(formatString, "Mark_Cooksey"))
            {
                driver = Mark_Cooksey;
            }
            else if (!strcmp(formatString, "Martin_Walker"))
            {
                driver = Martin_Walker;
            }
            else if (!strcmp(formatString, "MIDI"))
            {
                driver = MIDI;
            }
            else if (!strcmp(formatString, "MPlay"))
            {
                driver = MPlay;
            }
            else if (!strcmp(formatString, "MusyX"))
            {
                driver = MusyX;
            }
            else if (!strcmp(formatString, "Natsume"))
            {
                driver = Natsume;
            }
            else if (!strcmp(formatString, "NMK"))
            {
                driver = NMK;
            }
            else if (!strcmp(formatString, "Ocean"))
            {
                driver = Ocean;
            }
            else if (!strcmp(formatString, "Paragon_5"))
            {
                driver = Paragon_5;
            }
            else if (!strcmp(formatString, "Probe_Software"))
            {
                driver = Probe_Software;
            }
            else if (!strcmp(formatString, "RARE"))
            {
                driver = RARE;
            }
            else if (!strcmp(formatString, "Ryohji_Yoshitomi"))
            {
                driver = Ryohji_Yoshitomi;
            }
            else if (!strcmp(formatString, "Sheep"))
            {
                driver = Sheep;
            }
            else if (!strcmp(formatString, "Square"))
            {
                driver = Square;
            }
            else if (!strcmp(formatString, "Sunsoft"))
            {
                driver = Sunsoft;
            }
            else if (!strcmp(formatString, "Tarantula_Studios"))
            {
                driver = Tarantula_Studios;
            }
            else if (!strcmp(formatString, "Technos_Japan"))
            {
                driver = Technos_Japan;
            }
            else if (!strcmp(formatString, "Tiertex"))
            {
                driver = Tiertex;
            }
            else if (!strcmp(formatString, "Titus_1"))
            {
                driver = Titus_1;
            }
            else if (!strcmp(formatString, "Titus_2"))
            {
                driver = Titus_2;
            }
            else if (!strcmp(formatString, "TOSE"))
            {
                driver = TOSE;
            }
            else
            {
                printf("ERROR: Invalid sound engine name!\n");
                exit(1);
            }

            /*Parse the parameters*/
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 50; j++)
                {
                    parameters[i][j] = 0;
                }
            }

            i = 0;
            j = 0;
            z = 0;

            while (paramString[i] != 0)
            {
                if (paramString[i] != '\\')
                {
                    parameters[z][j] = paramString[i];
                    i++;
                    j++;
                }
                else if (paramString[i] == '\\')
                {
                    parameters[z][j] = 0;
                    i++;
                    j = 0;
                    z++;
                }
            }


            gb2MID(rom, banks, numBanks, driver, parameters);
            printf("The operation was successfully completed!\n");
            return 0;
        }
    }
}