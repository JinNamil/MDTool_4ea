#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>

#include "CRC32.h"
#include "ProcessController.h"

#include "debug.h"
#define MINIINI_NO_STL
#include "ini.h"

#define CONFIG_LINE_BUFFER_SIZE    (1024)

static const char eFuseKeyParams[EFUSE_TYPE_MAX][64] = {
    "[BOOTSOURCE]",
    "[SECUREBOOTENABLE]",
    "[UKEYLOCK]",
    "[PKFLOCK]",
    "[DUKLOCK]",
	"[PKFWRITE]",
    "[UKEY]",
    "[PKF]"
};

/* addresses and sizes */
static const unsigned int SRAM_BASE_ADDR        = (0x20000000);
static const unsigned int FLASH_BASE_ADDR       = (0x30000000);
static const unsigned int PKA_BASE_ADDR         = (FLASH_BASE_ADDR + 0x10000);
static const unsigned int PKA_FW_IV_ADDR        = (PKA_BASE_ADDR + 0x0000);
static const unsigned int PKA_AKEY_ADDR         = (PKA_BASE_ADDR + 0x0400);
static const unsigned int PKA_CURVE_PARAM_ADDR  = (PKA_BASE_ADDR + 0x0800);
static const unsigned int PKA_CLP300_FW_ADDR    = (PKA_BASE_ADDR + 0x1000);
static const unsigned int APP_BASE_ADDR         = (FLASH_BASE_ADDR + 0x20000);
static const unsigned int APP_SIGNATURE_S_ADDR  = (APP_BASE_ADDR + 0x0000);
static const unsigned int APP_SIGNATURE_R_ADDR  = (APP_BASE_ADDR + 0x0400);
static const unsigned int APP_SIZE_ADDR         = (APP_BASE_ADDR + 0x0800);
static const unsigned int APP_IMAGE_BASE_ADDR   = (APP_BASE_ADDR + 0x2000);
static const unsigned int SDB_INFO_ADDR         = (FLASH_BASE_ADDR + 0x0300000);
static const unsigned int FLASH_SECTOR_SIZE     = (0x1000);
static const unsigned int FLASH_BLOCK_SIZE      = (0x10000);


/* uploader binary prefix */
static const unsigned char UPLOADER_BINARY_PREFIX[4] = {
    (unsigned char)('e'),
    (unsigned char)('W'),
    (unsigned char)('B'),
    (unsigned char)('M')
};

#pragma pack(push, 1)
typedef struct _FirmwareImageFileHeader_t {
    unsigned char  prefix[5];
    unsigned char  sbEnEnabled;
    unsigned int   codeSize;
    unsigned int   totalSize;
} FirmwareImageFileHeader_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct SDBInfoFile_t{
    unsigned char path[128];
    unsigned int  option;
    unsigned int  datasize;
    unsigned char data[1];
} SDBInfoFile_t;
#pragma pack(pop)

/* eFuse read length */
static const unsigned int eFuseLength[EFUSE_TYPE_MAX] = {1, 1, 1, 1, 1, 1, 32, 32};

ProcessController::ProcessController(const char* device, const int baudrate)
{
    comm = NULL;
    gpio = NULL;

    configFileName   = NULL;
    uploaderFileName = NULL;
    appImageFileName = NULL;

    uploaderBinary = NULL;
    uploaderBinarySize = 0;

    secureBootEnabled = 0;

    pkaBinary = NULL;
    pkaBinarySize = 0;

    signatureBinary = NULL;
    signatureBinarySize = 0;

    sdbCodeBinary = NULL;
    sdbCodePacket = NULL;
    sdbCodeBinarySize = 0;
    sdbCodePacketSize = 0;
    appCodeBinary = NULL;
    appCodeBinarySize = 0;
    appImageTotalSize = 0;

    eFuseBootSource = 0x00;
    eFuseSecureBootEnable = 0;
    eFuseUKeyLock = 0;
    eFusePKfLock  = 0;
    eFuseDUKLock  = 0;
    memset(eFuseUKey, 0x00, sizeof(eFuseUKey));
    memset(eFusePKf,  0x00, sizeof(eFusePKf));

    comm = new SerialComm(device, baudrate);
    gpio = new GPIOControl();
}

ProcessController::~ProcessController()
{
    if ( comm != NULL )
    {
        delete comm;
        comm = NULL;
    }

    if ( configFileName != NULL )
    {
        delete[] configFileName;
        configFileName = NULL;
    }

    if ( uploaderFileName != NULL )
    {
        delete[] uploaderFileName;
        uploaderFileName = NULL;
    }

    if ( appImageFileName != NULL )
    {
        delete[] appImageFileName;
        appImageFileName = NULL;
    }

    if ( uploaderBinary != NULL && uploaderBinarySize != 0 )
    {
        delete[] uploaderBinary;
        uploaderBinary     = NULL;
        uploaderBinarySize = 0;
    }

    if ( pkaBinary != NULL && pkaBinarySize != 0 )
    {
        delete[] pkaBinary;
        pkaBinary     = NULL;
        pkaBinarySize = 0;
    }

    if ( signatureBinary != NULL && signatureBinarySize != 0 )
    {
        delete[] signatureBinary;
        signatureBinary     = NULL;
        signatureBinarySize = 0;
    }

    if ( appCodeBinary != NULL && appCodeBinarySize != 0 )
    {
        delete[] appCodeBinary;
        appCodeBinary     = NULL;
        appCodeBinarySize = 0;
    }
    
    if ( sdbCodePacket != NULL && sdbCodePacketSize != 0 )
    {
        delete[] sdbCodePacket;
        sdbCodePacket     = NULL;
        sdbCodePacketSize = 0;
    }
}

int ProcessController::SetName(eFILETYPE type, const char* in)
{
    size_t inLen = 0;

    if ( in == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    inLen = strlen(in);
    if ( inLen <= 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    switch ( type )
    {
        case FILE_NAME_CONF:
            {
                configFileName = new char[(inLen + 1)] {0,};
                strncpy(configFileName, in, inLen);
            }
            break;

        case FILE_NAME_UPLOADER:
            {
                uploaderFileName = new char[(inLen + 1)] {0,};
                strncpy(uploaderFileName, in, inLen);
            }
            break;

        case FILE_NAME_APPIMAGE:
            {
                appImageFileName = new char[(inLen + 1)] {0,};
                strncpy(appImageFileName, in, inLen);
            }
            break;
            
        case FILE_NAME_SDBINFO:
            {
                sdbInfoFileName = new char[(inLen + 1)] {0,};
                strncpy(sdbInfoFileName, in, inLen);
            }
        break;

        default:
            {
                DBG_ERR("error!!!");
            }
            return -1;
    }

    return 0;
}



int ProcessController::parseSdb(int index)
{
    int ret = -1;
    int sdbinfoSize = 0;
    char section[128] = {0,};
    ini_t* sdb = ini_load(sdbInfoFileName);
    if(sdb == NULL)
    {
        DBG_ERR("sdbinfo is NULL");
        return -1;
    }
    
    sprintf(section, "SDB_%d", index);
    const char* path = ini_get(sdb, section, "Path");
    if ( path == NULL )
    {
        ini_free(sdb);
        return 1;
    }
    
    const char* option = ini_get(sdb, section, "Option");
    const char* data = ini_get(sdb, section, "Data");
    FILE* appImageFile = NULL;
    appImageFile = fopen(data, "r");
    if ( appImageFile == NULL )
    {
        DBG_ERR("%s, file(%s) open error", __FUNCTION__);
        return -1;
    }
    
    /* Get Uploader File Size */
    int appImageSize = 0;
    ret = fseek(appImageFile, 0, SEEK_END);
    if ( ret != 0 )
    {
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }
    
    appImageSize = ftell(appImageFile);
    if ( appImageSize < 0 )
    {
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }
    
    rewind(appImageFile);
    ret = ftell(appImageFile);
    if ( ret != SEEK_SET )
    {
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }
    unsigned int sdbDataSize = 0;
    sdbDataSize = appImageSize;
    unsigned char* sdbDataBuffer = NULL;
    
    sdbDataBuffer = new unsigned char[sdbDataSize] {0,};
    
    ret = fread(sdbDataBuffer, 1, sdbDataSize, appImageFile);
    if ( ret != sdbDataSize )
    {
        if ( sdbDataBuffer != NULL )
        {
            delete[] sdbDataBuffer;
            sdbDataBuffer = NULL;
            sdbDataSize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }
    
    sdbCodePacketSize = ( sdbDataSize + 128 + 4 + 4 );
    
    if ( sdbCodePacket != NULL )
        delete[] sdbCodePacket;
    
    sdbCodePacket = new unsigned char[sdbCodePacketSize] {0,};
    
    SDBInfoFile_t* sdbinfo = (SDBInfoFile_t*)sdbCodePacket;
    
    memcpy(sdbinfo->path, path, strlen(path));
    sdbinfo->option = atoi(option);
    sdbinfo->datasize = sdbDataSize;
    memcpy(sdbinfo->data, sdbDataBuffer, sdbDataSize);
    memcpy(sdbCodePacket, (unsigned char*)sdbinfo, sdbCodePacketSize);
    
    
#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[SDB Data]");
    DBG_LOG("-PARAMS--------------+-VALUES-----");
    if ( sdbinfo->datasize > 0 )
    {
        fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
        fprintf(stdout, "   data              | ");
        for ( int i = 0; i < 16; i++ )
        {
            fprintf(stdout, "%02X", sdbinfo->data[i]);
        }
        fprintf(stdout, "\n");
    }
    DBG_LOG("   path              | %s", sdbCodePacket);
    DBG_LOG("   option            | %d", sdbinfo->option);
    DBG_LOG("   datasize          | %d", sdbinfo->datasize);
    DBG_LOG("---------------------+------------");
#endif

    if(sdbDataBuffer != NULL)
    {
        delete[] sdbDataBuffer;
        sdbDataBuffer = NULL;
    }
    
    if ( appImageFile != NULL )
    {
        fclose(appImageFile);
        appImageFile = NULL;
    }
    
    ini_free(sdb);
    
    return 0;
}

void ProcessController::swapPkf(unsigned char* arr, int first, int second)
{
	unsigned char temp;

	temp = arr[first];
	arr[first] = arr[second];
	arr[second] = temp;
}

int ProcessController::keyStringTohexArray(eEFUSETYPE type, const char* in)
{
    int ret = -1;
    int inLen = 0;	
	int removeCharLen = 0;
	char endChar = 5;
	char removeChar[65] = {0,};
	
	inLen = strlen(in);
	if(inLen == 66)
	{		
		DBG_LOG("PKF Swap Mode");
		endChar = in[inLen-1];
		DBG_LOG("endChar: %c", endChar);
		
		memcpy(removeChar, in, 64);
		removeCharLen = strlen(removeChar);		
		if(removeCharLen != 64)
		{
			DBG_ERR("removecharLen: %d", removeCharLen);
			DBG_ERR("removeChar: %s", removeChar);
			return -1;
		}
	}
	else
	if(inLen == 64)
	{
		DBG_LOG("PKF None Swap Mode");
		
		removeCharLen = 64;
		memcpy(removeChar, in, 64);
	}
	else
	{
		DBG_ERR("Error 111111");
		return -1;
	}
	
	
    /* check type */
    unsigned char* hexArray = NULL;
    switch ( type )
    {
        case EFUSE_TYPE_UKEY:
            {
                hexArray = eFuseUKey;
                ret = 0;
            }
            break;

        case EFUSE_TYPE_PKF:
            {
                hexArray = eFusePKf;
                ret = 0;
            }
            break;

        default:
            {
                DBG_ERR("error!!!");
                ret = -1;
            }
            break;
    }
    if ( ret != 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* convert: string(char array) => hex(unsigned char array) */
    memset(hexArray, 0, 32);
    for ( int i = 0; i < removeCharLen; i++ )
    {
        /* a(10)~f(15) => A(10)~F(15)  */
        if ( removeChar[i] >= 'a' && removeChar[i] <= 'f' )
        {
            removeChar[i] -= ('a' - 'A');
        }

        /* case #1: A ~ F */
        if ( removeChar[i] >= 'A' && removeChar[i] <= 'F' )
        {
            /* Even, 0xA0 ~ 0xF0 */
            if ( i % 2 == 0 )
            {
                hexArray[i/2] = (removeChar[i]-('A'-0xA))<<4;
            }
            /* Odd, 0xA ~ 0xF */
            else
            {
                hexArray[i/2] |= (removeChar[i]-('A'-0xA));
            }
        }
        /* case #2: 0 ~ 9 */
        else if( removeChar[i] >= '0' && removeChar[i] <= '9' )
        {
            /* Even, 0x00 ~ 0x90 */
            if ( i % 2 == 0 )
            {
                hexArray[i/2] = (removeChar[i]-('0'-0))<<4;
            }
            /* Odd, 0x00 ~ 0x09 */
            else
            {
                hexArray[i/2] |= (removeChar[i]-('0'-0));
            }
        }
        /* invalid key string */
        else
        {

            DBG_ERR("error!!!");
            return -1;
        }
    }
	
	switch (endChar)
	{
		case '0':
			swapPkf(hexArray, 3, 7);
			swapPkf(hexArray, 16, 22);
			swapPkf(hexArray, 25, 29);
			break;
		case '1':
			swapPkf(hexArray, 4, 26);
			swapPkf(hexArray, 6, 18);
			swapPkf(hexArray, 12, 20);
			break;
		case '2':
			swapPkf(hexArray, 5, 14);
			swapPkf(hexArray, 11, 18);
			swapPkf(hexArray, 3, 30);
			break;
		case '3':
			swapPkf(hexArray, 2, 13);
			swapPkf(hexArray, 7, 21);
			swapPkf(hexArray, 15, 17);
			break;
		default:
			break;
	}
	
    return 0;
}

int ProcessController::parseValue(eEFUSETYPE type, const char* in)
{
    int ret = -1;

    /* check key type */
    if ( (type < EFUSE_BOOT_SRC) || (type >= EFUSE_TYPE_MAX) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    switch ( type )
    {
        case EFUSE_BOOT_SRC:
            {
                /* param is empty */
                if ( in == NULL )
                {
                    eFuseBootSource = 0;
                    ret = 0;
                }

                /* set boot source value */
                const char bootSrcString[6][64] = {
                    "ExternalPins",    /* 0b000, 0 */
                    "SPIDirect1",      /* 0b100, 4 */
                    "SPIDirect2",      /* 0b101, 5 */
                    "SPIDirect3",      /* 0b110, 6 */
                    "SRAM",            /* 0b111, 7 */
                    "ROM"              /* 0b001, 0 */
                };
                const unsigned char bootSrcValue[6] = {0, 4, 5, 6, 7, 0};

                /* check param, set boot source value */
                for ( unsigned int i = 0; i < 6; i++ )
                {
                    if ( !strcmp(in, bootSrcString[i]) )
                    {
                        eFuseBootSource = (bootSrcValue[i]&0x07);
                        ret = 0;
                        break;
                    }
                }
            }
            break;

        case EFUSE_SB_EN:
            {
                /* param is 'n' or empty: disable */
                if ( (in == NULL) || !strcmp(in, "n") )
                {
                    eFuseSecureBootEnable = 0;
                    ret = 0;
                }
                /* param is 'y': enable */
                else if ( !strcmp(in, "y") )
                {
                    eFuseSecureBootEnable = 1;
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;

        case EFUSE_TYPE_UKEY_LOCK:
            {
                /* param is 'n' or empty: disable */
                if ( (in == NULL) || !strcmp(in, "n") )
                {
                    DBG_LOG("UKEY_N");
                    eFuseUKeyLock = 0;
                    ret = 0;
                }
                /* param is 'y': enable */
                else if ( !strcmp(in, "y") )
                {
                    DBG_LOG("UKEY_Y");
                    eFuseUKeyLock = 1;
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;

        case EFUSE_TYPE_PKF_LOCK:
            {
                /* param is 'n' or empty: disable */
                if ( (in == NULL) || !strcmp(in, "n") )
                {
                    DBG_LOG("PKF_N");
                    eFusePKfLock = 0;
                    ret = 0;
                }
                /* param is 'y': enable */
                else if ( !strcmp(in, "y") )
                {
                    DBG_LOG("PKF_Y");
                    eFusePKfLock = 1;
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;

        case EFUSE_TYPE_DUK_LOCK:
            {
                /* param is 'n' or empty: disable */
                if ( (in == NULL) || !strcmp(in, "n") )
                {
                    DBG_LOG("DUK_N");
                    eFuseDUKLock = 0;
                    ret = 0;
                }
                /* param is 'y': enable */
                else if ( !strcmp(in, "y") )
                {
                    DBG_LOG("DUK_Y");
                    eFuseDUKLock = 1;
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;

        case EFUSE_TYPE_UKEY:
			{
                if ( in == NULL )
                {
                    ret = 0;
                    break;
                }
                ret = keyStringTohexArray(type, in);
                if ( ret < 0 )
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
        case EFUSE_TYPE_PKF:
            {
                if ( in == NULL )
                {
                    ret = 0;
                    break;
                }
                ret = keyStringTohexArray(type, in);
                if ( ret < 0 )
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;
			
		case EFUSE_WRITE_PKF:
			{
				/* param is 'n' or empty: disable */
                if ( (in == NULL) || !strcmp(in, "n") )
                {
                    DBG_LOG("PKF_WRITE_N");
                    eFusePKfWrite = 0;
                    ret = 0;
                }
                /* param is 'y': enable */
                else if ( !strcmp(in, "y") )
                {
                    DBG_LOG("PKF_WRITE_Y");
                    eFusePKfWrite = 1;
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
			}
			break;
    }

    if ( ret != 0 )
    {
        DBG_ERR("error!!!");
        DBG_ERR("type: 0x%04X, in: %s", type, in);
        return ret;
    }

    return ret;
}

int ProcessController::parseLine(const char* in)
{
    int ret = -1;
    int inLen = 0;

    if ( in == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* key value length check */
    inLen = strlen(in);
    if ( (inLen <= 0) || (inLen >= (CONFIG_LINE_BUFFER_SIZE-1)) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* get key type and value */
    char* param = new char[(inLen+1)] {0,};
    char* value = new char[(inLen+1)] {0,};
    sscanf(in, "%s %s", param, value);

    /* value is empty */
    if ( strlen(value) == 0 )
    {
        return 0;
    }

    /* key value length check */
    if ( strlen(value) < 0 || strlen(value) > 66 )
    {
        DBG_ERR("param(%4d): %s", strlen(param), param);
        DBG_ERR("value(%4d): %s", strlen(value), value);
        DBG_ERR("error!!!");
        return -1;
    }

    /* check value, set key */
    for ( unsigned int i = 0; i < EFUSE_TYPE_MAX; i++ )
    {
        if ( strcmp((char*)param, eFuseKeyParams[i]) == 0 )
        {
            ret = parseValue((eEFUSETYPE)i, value);
            if ( ret < 0 )
            {
                DBG_ERR("error!!!");
                return -1;
            }
            return 0;
        }
    }

    /* invalid value */
    DBG_ERR("error!!!");
    return -1;
}

int ProcessController::parseConfigFile(void)
{
    int ret = -1;

    ret = access(configFileName, R_OK);
    if ( ret != 0 )
    {
        DBG_ERR("configFileName: %s", configFileName);
        DBG_ERR("error!!!");
        return -1;
    }

    FILE* confFile = NULL;
    confFile = fopen(configFileName, "r");
    if ( confFile == NULL )
    {
        DBG_ERR("%s, file(%s) open error", __FUNCTION__, confFile);
        return -1;
    }

    char line[CONFIG_LINE_BUFFER_SIZE] = {0,};
    while ( !feof(confFile) )
    {
        /* get line */
        fgets(line, CONFIG_LINE_BUFFER_SIZE, confFile);

        /* Skip Comment or Blank */
        if ( line[0] == '#' || strlen(line) < 4 )
        {
            continue;
        }
        else
        {
            ret = parseLine(line);
            if ( ret < 0 )
            {
                DBG_ERR("error!!!");
                return -1;
            }
        }
    }

    return 0;
}

int ProcessController::openUploaderFile(void)
{
    int ret = -1;

    /* check uploader file */
    ret = access(uploaderFileName, R_OK);
    if ( ret != 0 )
    {
        DBG_ERR("uploaderFileName: %s", uploaderFileName);
        DBG_ERR("error!!!");
        return -1;
    }

    /* Open Uploader File */
    FILE* uploaderFile = NULL;
    uploaderFile = fopen(uploaderFileName, "r");
    if ( uploaderFile == NULL )
    {
        DBG_ERR("%s, file(%s) open error", __FUNCTION__);
        return -1;
    }

    /* Get Uploader File Size */
    int uploaderSize = 0;
    ret = fseek(uploaderFile, 0, SEEK_END);
    if ( ret != 0 )
    {
        if ( uploaderFile != NULL )
        {
            fclose(uploaderFile);
            uploaderFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }
    uploaderSize = ftell(uploaderFile);
    if ( uploaderSize < 0 )
    {
        if ( uploaderFile != NULL )
        {
            fclose(uploaderFile);
            uploaderFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* Rewind the file pointer */
    rewind(uploaderFile);
    ret = ftell(uploaderFile);
    if ( ret != SEEK_SET )
    {
        if ( uploaderFile != NULL )
        {
            fclose(uploaderFile);
            uploaderFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* Memory allocate and read uploader */
    if ( uploaderBinary != NULL )
    {
        delete[] uploaderBinary;
        uploaderBinary = NULL;
        uploaderBinarySize = 0;
    }

    uploaderBinarySize = uploaderSize;
    uploaderBinary = new unsigned char[uploaderBinarySize] {0,};
    if ( uploaderBinary == NULL )
    {
        if ( uploaderFile != NULL )
        {
            fclose(uploaderFile);
            uploaderFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* read from file */
    ret = fread(uploaderBinary, 1, uploaderBinarySize, uploaderFile);
    if ( ret != uploaderBinarySize )
    {
        if ( uploaderBinary != NULL )
        {
            delete[] uploaderBinary;
            uploaderBinary = NULL;
            uploaderBinarySize = 0;
        }
        if ( uploaderFile != NULL )
        {
            fclose(uploaderFile);
            uploaderFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    fclose(uploaderFile);
    uploaderFile = NULL;

    return 0;
}

int ProcessController::checkImgFilePrefix(unsigned char* in)
{
    const unsigned char prefix[5] = {(unsigned char)'e', (unsigned char)'W', (unsigned char)'B', (unsigned char)'M', 0x66 };

    for ( int i = 0; i < sizeof(prefix); i++ )
    {
        if ( in[i] != prefix[i] )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    return 0;
}

int ProcessController::readSizeFromImgFile(unsigned int in, unsigned int* out)
{
    if ( out == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    *out = (((in & 0xFF)       << 24)
          | ((in & 0xFF00)     <<  8)
          | ((in & 0xFF0000)   >>  8)
          | ((in & 0xFF000000) >> 24));

    return 0;
}

int ProcessController::checkAppCodeBinarySize(unsigned int in)
{
    int ret = -1;

    if ( (appImageTotalSize <= 0) || ((secureBootEnabled != 1) && (secureBootEnabled != 0)) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = readSizeFromImgFile(in, &appCodeBinarySize);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    if ( secureBootEnabled == 1 )
    {
        if ( appCodeBinarySize != appImageTotalSize - 0x4000 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }
    else
    {
        if ( appCodeBinarySize != appImageTotalSize )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    return 0;
}

int ProcessController::checkSdbCodeBinarySize(unsigned int in)
{
    int ret = -1;

    if ( (appImageTotalSize <= 0) || ((secureBootEnabled != 1) && (secureBootEnabled != 0)) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = readSizeFromImgFile(in, &sdbCodeBinarySize);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    if ( secureBootEnabled == 1 )
    {
        if ( appCodeBinarySize != appImageTotalSize - 0x4000 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }
    else
    {
        if ( appCodeBinarySize != appImageTotalSize )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    return 0;
}

int ProcessController::checkAppImageTotalSize(unsigned int in, unsigned int in2)
{
    int ret = -1;

    if ( in2 <= 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = readSizeFromImgFile(in, &appImageTotalSize);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    if ( appImageTotalSize != (in2 - sizeof(FirmwareImageFileHeader_t)) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int ProcessController::openAppImageFile(void)
{
    int ret = -1;

    /* check uploader file */
    ret = access(appImageFileName, R_OK);
    if ( ret != 0 )
    {
        DBG_ERR("appImageFileName: %s", appImageFileName);
        DBG_ERR("error!!!");
        return -1;
    }

    /* Open Uploader File */
    FILE* appImageFile = NULL;
    appImageFile = fopen(appImageFileName, "r");
    if ( appImageFile == NULL )
    {
        DBG_ERR("%s, file(%s) open error", __FUNCTION__);
        return -1;
    }

    /* Get Uploader File Size */
    int appImageSize = 0;
    ret = fseek(appImageFile, 0, SEEK_END);
    if ( ret != 0 )
    {
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }
    appImageSize = ftell(appImageFile);
    if ( appImageSize < 0 )
    {
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* Rewind the file pointer */
    rewind(appImageFile);
    ret = ftell(appImageFile);
    if ( ret != SEEK_SET )
    {
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* Memory allocate and read uploader */
    unsigned int   appImageFileBinarySize = appImageSize;
    unsigned char* appImageFileBinary = new unsigned char[appImageFileBinarySize] {0,};
    if ( appImageFileBinary == NULL )
    {
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* read from file */
    ret = fread(appImageFileBinary, 1, appImageFileBinarySize, appImageFile);
    if ( ret != appImageFileBinarySize )
    {
        if ( appImageFileBinary != NULL )
        {
            delete[] appImageFileBinary;
            appImageFileBinary = NULL;
            appImageFileBinarySize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    FirmwareImageFileHeader_t* appImageFileBinaryHeader = (FirmwareImageFileHeader_t*)appImageFileBinary;
    if ( appImageFileBinaryHeader == NULL )
    {
        if ( appImageFileBinary != NULL )
        {
            delete[] appImageFileBinary;
            appImageFileBinary = NULL;
            appImageFileBinarySize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* check .img prefix */
    ret = checkImgFilePrefix(appImageFileBinaryHeader->prefix);
    if ( ret < 0 )
    {
        if ( appImageFileBinary != NULL )
        {
            delete[] appImageFileBinary;
            appImageFileBinary = NULL;
            appImageFileBinarySize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* check secure boot */
    secureBootEnabled = appImageFileBinaryHeader->sbEnEnabled;
    if ( (secureBootEnabled != 0) && (secureBootEnabled != 1) )
    {
        if ( appImageFileBinary != NULL )
        {
            delete[] appImageFileBinary;
            appImageFileBinary = NULL;
            appImageFileBinarySize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* check app image total size (image size = image file size - header size) */
    ret = checkAppImageTotalSize(appImageFileBinaryHeader->totalSize, appImageFileBinarySize);
    if ( ret < 0 )
    {
        if ( appImageFileBinary != NULL )
        {
            delete[] appImageFileBinary;
            appImageFileBinary = NULL;
            appImageFileBinarySize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* check app code binary size */
    ret = checkAppCodeBinarySize(appImageFileBinaryHeader->codeSize);
    if ( ret < 0 )
    {
        if ( appImageFileBinary != NULL )
        {
            delete[] appImageFileBinary;
            appImageFileBinary = NULL;
            appImageFileBinarySize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    /* import pka, signature, code */
    if ( appCodeBinary != NULL )
    {
        delete[] appCodeBinary;
        appCodeBinary = NULL;
    }
    appCodeBinary = new unsigned char[appCodeBinarySize] {0,};
    if ( appCodeBinary == NULL )
    {
        if ( appImageFileBinary != NULL )
        {
            delete[] appImageFileBinary;
            appImageFileBinary = NULL;
            appImageFileBinarySize = 0;
        }
        if ( appImageFile != NULL )
        {
            fclose(appImageFile);
            appImageFile = NULL;
        }
        DBG_ERR("error!!!");
        return -1;
    }

    unsigned int base = sizeof(FirmwareImageFileHeader_t);
    if ( secureBootEnabled == 1 )
    {
        if ( pkaBinary != NULL )
        {
            delete[] pkaBinary;
            pkaBinary = NULL;
        }
        pkaBinarySize = 0x2000;
        pkaBinary = new unsigned char[pkaBinarySize]{0,};

        if ( signatureBinary != NULL )
        {
            delete[] signatureBinary;
            signatureBinary = NULL;
        }
        signatureBinarySize = 0x2000;
        signatureBinary = new unsigned char[signatureBinarySize]{0,};
    }
    else
    {
        if ( pkaBinary != NULL )
        {
            delete[] pkaBinary;
            pkaBinary = NULL;
        }
        pkaBinarySize = 0;
        if ( signatureBinary != NULL )
        {
            delete[] signatureBinary;
            signatureBinary = NULL;
        }
        signatureBinary = 0;
    }
    memcpy(pkaBinary,       appImageFileBinary + base, pkaBinarySize);
    base += pkaBinarySize;
    memcpy(signatureBinary, appImageFileBinary + base, signatureBinarySize);
    base += signatureBinarySize;
    memcpy(appCodeBinary,   appImageFileBinary + base, appCodeBinarySize);
    base += appCodeBinarySize;

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[IMG FILE INFO]");
    DBG_LOG("-PARAMS--------------+-VALUES--------------------------");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "              header | ");
    for ( int i = 0; i < sizeof(appImageFileBinaryHeader->prefix); i++ )
    {
        fprintf(stdout, "%02X", appImageFileBinaryHeader->prefix[i]);
    }
	
    fprintf(stdout, " %02X %08X %08X\n", appImageFileBinaryHeader->sbEnEnabled, appImageFileBinaryHeader->codeSize, appImageFileBinaryHeader->totalSize);
    DBG_LOG("   secureBootEnabled | 0x%02X",      secureBootEnabled, secureBootEnabled);
    DBG_LOG("   appCodeBinarySize | 0x%08X(%d)", appCodeBinarySize, appCodeBinarySize);
    DBG_LOG("   appImageTotalSize | 0x%08X(%d)", appImageTotalSize, appImageTotalSize);
    DBG_LOG("       pkaBinarySize | 0x%08X(%d)", pkaBinarySize, pkaBinarySize);
    DBG_LOG(" signatureBinarySize | 0x%08X(%d)", signatureBinarySize, signatureBinarySize);
    if ( appCodeBinarySize > 0 )
    {
        fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
        fprintf(stdout, "       appCodeBinary | ");
        for ( int i = 0; i < 16; i++ )
        {
            fprintf(stdout, "%02X", appCodeBinary[i]);
        }
        fprintf(stdout, "\n");
    }
    if ( pkaBinarySize > 0 )
    {
        fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
        fprintf(stdout, "           pkaBinary | ");
        for ( int i = 0; i < 16; i++ )
        {
            fprintf(stdout, "%02X", pkaBinary[i]);
        }
        fprintf(stdout, "\n");
    }
    if ( signatureBinarySize > 0 )
    {
        fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
        fprintf(stdout, "     signatureBinary | ");
        for ( int i = 0; i < 16; i++ )
        {
            fprintf(stdout, "%02X", signatureBinary[i]);
        }
        fprintf(stdout, "\n");
    }
    DBG_LOG("---------------------+----------------------------------\n");
#endif

    if ( appImageFileBinary != NULL )
    {
        delete[] appImageFileBinary;
        appImageFileBinary = NULL;
        appImageFileBinarySize = 0;
    }
    if ( appImageFile != NULL )
    {
        fclose(appImageFile);
        appImageFile = NULL;
    }

    return 0;
}

int ProcessController::makeCmdHeader(ePACKETTYPE type, unsigned int param, unsigned char* in, unsigned int inSize, unsigned int optionSize, cmdPacketHeader_t* out)
{
    int ret = -1;

    if ( out == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }
    memset(out, 0x00, sizeof(cmdPacketHeader_t));

    /* check type and size */
    switch ( type )
    {
        case PACKET_TYPE_SRAM:
            {
                if ( (inSize != 0) && (optionSize != 0)
                  && (inSize == optionSize + sizeof(UPLOADER_BINARY_PREFIX) + sizeof(uploaderBinarySize))
                  && (optionSize == uploaderBinarySize) )
                {
                    out->crc = CRC32::CalcCRC32(in, optionSize);
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;

        case PACKET_TYPE_EFUSE_WRITE:
            {
                if ( (inSize != 0) && (optionSize == 0) )
                {
                    out->crc = CRC32::CalcCRC32(in, inSize);
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;

        case PACKET_TYPE_EFUSE_READ:
            {
                if ( (inSize == 0) && (optionSize == 0) )
                {
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;

        case PACKET_TYPE_FLASH:
            {
                if ( (inSize != 0) && (optionSize != 0)
                  && (inSize <= FLASH_SECTOR_SIZE) )
                {
                    out->crc = CRC32::CalcCRC32(in, inSize);
                    ret = 0;
                }
                else
                {
                    DBG_ERR("error!!!");
                    ret = -1;
                }
            }
            break;
            
        case PACKET_TYPE_FLASH_SDB:
            {
                ret = 0;
//                if ( (inSize != 0) && (optionSize != 0) 
//					&& (inSize <= FLASH_SECTOR_SIZE) )
//                {
//                    out->crc = CRC32::CalcCRC32(in, inSize);
//                    ret = 0;
//                }
//                else
//                {
//                    DBG_ERR("error!!!");
//                    ret = -1;
//                }
            }
            break;

        default:
            {
                DBG_ERR("error!!!");
                ret = -1;
            }
            break;
    }

    if ( ret < 0 )
    {
        return -1;
    }

    /* make header */
    out->sync = 0x57;
    out->type = (unsigned char)(0xFF&type);
    out->param = param;
    if ( inSize != 0 )
    {
        out->size[0] = inSize;
    }
    if ( optionSize != 0 )
    {
        out->size[1] = optionSize;
    }

    return 0;
}

int ProcessController::sendUploaderFile(void)
{
    int ret = -1;

    /* make send packet */
    unsigned int sendPacketSize = 0;
    sendPacketSize = uploaderBinarySize + sizeof(UPLOADER_BINARY_PREFIX) + sizeof(uploaderBinarySize);

    unsigned char* sendPacket = NULL;
    sendPacket = new unsigned char[sendPacketSize] {0,};
    if ( sendPacket == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }
    memcpy(sendPacket, uploaderBinary, uploaderBinarySize);
    memcpy(sendPacket + uploaderBinarySize, UPLOADER_BINARY_PREFIX, sizeof(UPLOADER_BINARY_PREFIX));
    memcpy(sendPacket + uploaderBinarySize + sizeof(UPLOADER_BINARY_PREFIX), &uploaderBinarySize, sizeof(uploaderBinarySize));

    /* make SEND PACKET header */
    cmdPacketHeader_t sendPacketHeader = {0,};
    ret = makeCmdHeader(PACKET_TYPE_SRAM, SRAM_BASE_ADDR, uploaderBinary, sendPacketSize, uploaderBinarySize, &sendPacketHeader);
    if ( ret != 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[UART2SRAM packet]");
    DBG_LOG("-PARAMS-----+-VALUES-----");
    DBG_LOG("       sync | 0x%02X", sendPacketHeader.sync);
    DBG_LOG("       type | 0x%02X", sendPacketHeader.type);
    DBG_LOG("       addr | 0x%08X", sendPacketHeader.param);
    DBG_LOG(" dwn length | 0x%08X", sendPacketHeader.size[0]);
    DBG_LOG(" app length | 0x%08X", sendPacketHeader.size[1]);
    DBG_LOG("        crc | 0x%08X", sendPacketHeader.crc);
    DBG_LOG("------------+------------\n");
#endif

    int sentBytes = 0;
    int readBytes = 0;
    unsigned char responseBuffer[128] = {0,};
    response_t* response = (response_t*)responseBuffer;

    /* send header */
    sentBytes = comm->Send((const unsigned char *)&sendPacketHeader, sizeof(cmdPacketHeader_t));
    if ( sentBytes < 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    /* receive response */
    memset(responseBuffer, 0x00, sizeof(responseBuffer));
    readBytes = comm->Receive(responseBuffer, 4);
    if ( readBytes < 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    if ( readBytes == 0 || response->ack != true || response->nak != false )
    {
        DBG_ERR("error!!!");
        DBG_ERR("readBytes %d", readBytes);
        DBG_ERR("%02X %02X %02X %02X", responseBuffer[0], responseBuffer[1], responseBuffer[2], responseBuffer[3]);
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    /* send uploader image */
    sentBytes = comm->Send(sendPacket, sendPacketSize);
    if ( sentBytes < 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    /* receive response */
    memset(responseBuffer, 0x00, sizeof(responseBuffer));
    readBytes = comm->Receive(responseBuffer, 4);
    if ( readBytes < 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    if ( readBytes == 0 || response->ack != true || response->nak != false )
    {
        DBG_ERR("error!!!");
        DBG_ERR("readBytes %d", readBytes);
        DBG_ERR("%02X %02X %02X %02X", responseBuffer[0], responseBuffer[1], responseBuffer[2], responseBuffer[3]);
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    /* make DONE header */
    ret = makeCmdHeader(PACKET_TYPE_SRAM, SRAM_BASE_ADDR, uploaderBinary, sendPacketSize, uploaderBinarySize, &sendPacketHeader);
    if ( ret != 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    /* send Done */
    sentBytes = comm->Send((const unsigned char *)&sendPacketHeader, sizeof(cmdPacketHeader_t));
    if ( sentBytes < 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }

    /* receive start message */
    memset(responseBuffer, 0x00, sizeof(responseBuffer));
    readBytes = comm->Receive(responseBuffer, 10);
    if ( readBytes <= 0 )
    {
        DBG_ERR("error!!!");
        if ( sendPacket != NULL )
        {
            delete[] sendPacket;
            sendPacket = NULL;
        }
        return -1;
    }
#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[Message from MS500]");
    for ( int i = 0; i < readBytes; i++ )
    {
        DBG_LOG("%02X ", responseBuffer[i]);
    }
    fprintf(stdout, "\n");
#endif /* __MP_DEBUG_BUILD__ */
    if ( sendPacket != NULL )
    {
        delete[] sendPacket;
        sendPacket = NULL;
    }

    return 0;
}

int ProcessController::sendDataToFlash(const unsigned char* in, unsigned int inLen, unsigned int addr)
{
    int ret = -1;

    if ( in == NULL || inLen <= 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    int sentBytes = 0;
    int readBytes = 0;
    unsigned char responseBuffer[128] = {0,};
    response_t* response = (response_t*)responseBuffer;
    if ( response == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    unsigned int base      = 0;
    unsigned int loopCount = 0;
    unsigned int sendSize  = 0;

    cmdPacketHeader_t sendPacketHeader;

    loopCount = ((inLen + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE);
    if ( loopCount <= 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    for ( unsigned int i = 0; i < loopCount; i++ )
    {
        base = i * FLASH_SECTOR_SIZE;
        if ( FLASH_SECTOR_SIZE > inLen - base )
        {
            sendSize = inLen - base;
        }
        else
        {
            sendSize = FLASH_SECTOR_SIZE;
        }

        ret = makeCmdHeader(PACKET_TYPE_FLASH, addr + base, (unsigned char*)(in + base), sendSize, inLen, &sendPacketHeader);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

#ifdef __MP_DEBUG_BUILD__
        DBG_LOG("[SEND BLOCK#%d]", i);
        DBG_LOG("-PARAMS-----+-VALUES-----");
        DBG_LOG("       sync | 0x%02X", sendPacketHeader.sync);
        DBG_LOG("       type | 0x%02X", sendPacketHeader.type);
        DBG_LOG("       addr | 0x%08X", sendPacketHeader.param);
        DBG_LOG(" dwn length | 0x%08X", sendPacketHeader.size[0]);
        DBG_LOG(" app length | 0x%08X", sendPacketHeader.size[1]);
        DBG_LOG("        crc | 0x%08X", sendPacketHeader.crc);
        DBG_LOG("------------+------------\n");
#endif

        /* send header */
        sentBytes = comm->Send((const unsigned char *)&sendPacketHeader, sizeof(cmdPacketHeader_t));
        if ( sentBytes < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        /* send uploader image */
        sentBytes = comm->Send(in + base, sendSize);
        if ( sentBytes < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        /* receive response */
        memset(responseBuffer, 0x00, sizeof(responseBuffer));
        readBytes = comm->Receive(responseBuffer, 4);
        if ( readBytes < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        if ( readBytes == 0 || response->ack != true || response->nak != false )
        {
            DBG_ERR("error!!!");
            DBG_ERR("readBytes %d", readBytes);
            DBG_ERR("%02X %02X %02X %02X", responseBuffer[0], responseBuffer[1], responseBuffer[2], responseBuffer[3]);
            return -1;
        }
    }

    return 0;
}

int ProcessController::sendSDBDataToFlash(const unsigned char* in, unsigned int inLen)
{
    int ret = -1;

    if ( in == NULL || inLen <= 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    int sentBytes = 0;
    int readBytes = 0;
    unsigned char responseBuffer[128] = {0,};
    response_t* response = (response_t*)responseBuffer;
    if ( response == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    unsigned int base      = 0;
    unsigned int loopCount = 0;
    unsigned int sendSize  = 0;

    cmdPacketHeader_t sendPacketHeader;

    ret = makeCmdHeader(PACKET_TYPE_FLASH_SDB, 0, (unsigned char*)(in), inLen, inLen, &sendPacketHeader);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[SEND SDB]");
    DBG_LOG("-PARAMS-----+-VALUES-----");
    DBG_LOG("       sync | 0x%02X", sendPacketHeader.sync);
    DBG_LOG("       type | 0x%02X", sendPacketHeader.type);
    DBG_LOG("       addr | 0x%08X", sendPacketHeader.param);
    DBG_LOG(" dwn length | 0x%08X", sendPacketHeader.size[0]);
    DBG_LOG(" app length | 0x%08X", sendPacketHeader.size[1]);
    DBG_LOG("        crc | 0x%08X", sendPacketHeader.crc);
    DBG_LOG("------------+------------\n");
#endif

        /* send header */
        sentBytes = comm->Send((const unsigned char *)&sendPacketHeader, sizeof(cmdPacketHeader_t));
        if ( sentBytes < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        /* send uploader image */
        sentBytes = comm->Send(in, inLen);
        if ( sentBytes < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        /* receive response */
        memset(responseBuffer, 0x00, sizeof(responseBuffer));
        readBytes = comm->Receive(responseBuffer, 4);
        if ( readBytes < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        if ( readBytes == 0 || response->ack != true || response->nak != false )
        {
            DBG_ERR("error!!!");
            DBG_ERR("readBytes %d", readBytes);
            DBG_ERR("%02X %02X %02X %02X", responseBuffer[0], responseBuffer[1], responseBuffer[2], responseBuffer[3]);
            return -1;
        }

    return 0;
}
int ProcessController::sendSdbInfo(void)
{
    int index = 0;
    int ret = -1;
    
    while ( 1 )
    {
        ret = parseSdb(index++);
        {
            if ( ret < 0 )
            {
                DBG_ERR("parseSdb error");
                return -1;
            }
            else
            if ( ret == 1 )
            {
                return 0;
            }
        }
        
        ret = sendSDBDataToFlash(sdbCodePacket, sdbCodePacketSize);
        if ( sdbCodePacket != NULL )
        {
            delete[] sdbCodePacket;
            sdbCodePacket     = NULL;
            sdbCodePacketSize = 0;
        }
        
        if ( ret < 0 )
        {
            DBG_ERR(".ini error");
            return -1;
        }
    }
    
    return ret;
}
int ProcessController::sendAppImageFirmware(void)
{
    int ret = -1;

    /* Send App and Erase Flash */
    ret = sendDataToFlash(appCodeBinary, appCodeBinarySize, APP_IMAGE_BASE_ADDR);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    if ( secureBootEnabled == 1 )
    {
        /* Send PKA */
        ret = sendDataToFlash(pkaBinary, pkaBinarySize, PKA_BASE_ADDR);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        /* Send Signature */
        ret = sendDataToFlash(signatureBinary, signatureBinarySize, APP_BASE_ADDR);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    return ret;
}

int ProcessController::sendNVMWrite(eEFUSETYPE type)
{
    int ret = -1;

    if ( type > EFUSE_TYPE_MAX || type < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    int sentBytes = 0;
    int readBytes = 0;
    unsigned char responseBuffer[128] = {0,};
    response_t* response = (response_t*)responseBuffer;
    if ( response == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    unsigned int   writeLength = eFuseLength[type];
    unsigned char* writeData = new unsigned char[writeLength] {0,};
    if ( writeData == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = -1;
    switch ( type )
    {
        case EFUSE_BOOT_SRC:
            {
                memcpy(writeData, &eFuseBootSource, writeLength);
                ret = 0;
            }
            break;

        case EFUSE_SB_EN:
            {
                memcpy(writeData, &eFuseSecureBootEnable, writeLength);
                ret = 0;
            }
            break;

        case EFUSE_TYPE_UKEY_LOCK:
            {
                memcpy(writeData, &eFuseUKeyLock, writeLength);
                ret = 0;
            }
            break;

        case EFUSE_TYPE_PKF_LOCK:
            {
                memcpy(writeData, &eFusePKfLock, writeLength);
                ret = 0;
            }
            break;

        case EFUSE_TYPE_DUK_LOCK:
            {
                memcpy(writeData, &eFuseDUKLock, writeLength);
                ret = 0;
                
            }
            break;
			
		case EFUSE_WRITE_PKF:
			break;
			
        case EFUSE_TYPE_UKEY:
            {
                memcpy(writeData, eFuseUKey, writeLength);
                ret = 0;
            }
            break;

        case EFUSE_TYPE_PKF:
            {
                memcpy(writeData, eFusePKf, writeLength);
                ret = 0;
            }
            break;

        default:
            {
                DBG_ERR("done");
                ret = -1;
            }
            break;
    }

    if ( ret < 0 )
    {
        return -1;
    }

    /* default check, if all data is default(=0), do nothing and return success */
    ret = 1;
    
    for ( int i = 0; i < writeLength; i++ )
    {
        if ( writeData[i] != 0 )
        {
            ret = 0;
            break;
        }
    }
    if ( ret == 1 )
    {
        return 0;
    }

    /* swap the writeData */
    for ( int i = 0; (i < (writeLength/2)) && (writeLength > 1); i++ )
    {
        if ( writeData[i] == writeData[writeLength - i - 1] )
        {
            continue;
        }
        writeData[i] ^= writeData[writeLength - i - 1];
        writeData[writeLength - i - 1] ^= writeData[i];
        writeData[i] ^= writeData[writeLength - i - 1];
    }

    cmdPacketHeader_t sendPacketHeader;

    ret = makeCmdHeader(PACKET_TYPE_EFUSE_WRITE, type, writeData, writeLength, 0, &sendPacketHeader);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[eFuse Write]");
    DBG_LOG("-PARAMS-----+-VALUES-----");
    DBG_LOG("       sync | 0x%02X", sendPacketHeader.sync);
    DBG_LOG("       type | 0x%02X", sendPacketHeader.type);
    DBG_LOG("       addr | 0x%08X", sendPacketHeader.param);
    DBG_LOG(" dwn length | 0x%08X", sendPacketHeader.size[0]);
    DBG_LOG(" app length | 0x%08X", sendPacketHeader.size[1]);
    DBG_LOG("        crc | 0x%08X", sendPacketHeader.crc);
    fprintf(stdout, "[Log %s#%d]        data | ", __FUNCTION__, __LINE__);
    for ( int i = 0; i < writeLength; i++ )
    {
        fprintf(stdout, "%02X", writeData[i]);
    }
    fprintf(stdout, "\n");
    DBG_LOG("------------+------------\n");
#endif

    /* send header */
    sentBytes = comm->Send((const unsigned char *)&sendPacketHeader, sizeof(cmdPacketHeader_t));
    if ( sentBytes < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* send data */
    sentBytes = comm->Send(writeData, writeLength);
    if ( sentBytes < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* receive response */
    memset(responseBuffer, 0x00, sizeof(responseBuffer));
    readBytes = comm->Receive(responseBuffer, 4);
    if ( readBytes < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    if ( readBytes == 0 || response->ack != true || response->nak != false )
    {
        DBG_ERR("error!!!");
        DBG_ERR("readBytes %d", readBytes);
        DBG_ERR("%02X %02X %02X %02X", responseBuffer[0], responseBuffer[1], responseBuffer[2], responseBuffer[3]);
        return -1;
    }

    return 0;
}

int ProcessController::sendNVMRead(eEFUSETYPE type, unsigned char* out, unsigned int* outLen)
{
    int ret = -1;

    if ( type > EFUSE_TYPE_MAX || type < 0 || out == NULL || outLen == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    int sentBytes = 0;
    int readBytes = 0;
    unsigned char responseBuffer[128] = {0,};
    response_t* response = (response_t*)responseBuffer;
    if ( response == NULL )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    unsigned int readLength = eFuseLength[type];

    cmdPacketHeader_t sendPacketHeader;

    ret = makeCmdHeader(PACKET_TYPE_EFUSE_READ, type, NULL, 0, 0, &sendPacketHeader);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[eFuse Read Head]");
    DBG_LOG("-PARAMS-----+-VALUES-----");
    DBG_LOG("       sync | 0x%02X", sendPacketHeader.sync);
    DBG_LOG("       type | 0x%02X", sendPacketHeader.type);
    DBG_LOG("       addr | 0x%08X", sendPacketHeader.param);
    DBG_LOG(" dwn length | 0x%08X", sendPacketHeader.size[0]);
    DBG_LOG(" app length | 0x%08X", sendPacketHeader.size[1]);
    DBG_LOG("        crc | 0x%08X", sendPacketHeader.crc);
    DBG_LOG("------------+------------\n");
#endif

    /* send header */
    sentBytes = comm->Send((const unsigned char *)&sendPacketHeader, sizeof(cmdPacketHeader_t));
    if ( sentBytes < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* receive response */
    memset(responseBuffer, 0x00, sizeof(responseBuffer));
    readBytes = comm->Receive(responseBuffer, readLength);
    if ( readBytes < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* swap and copy */
    *outLen = readLength;
	memcpy(out, responseBuffer, readLength);

    return 0;
}

int ProcessController::ProcessInit(void)
{
    int ret = -1;

    ret = parseConfigFile();
    if ( ret != 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[%s]", __FUNCTION__);
    DBG_LOG("-PARAMS--------------+-VALUES-----------------------------------------------------------");
    DBG_LOG("    Config File Name | %s", configFileName);
    DBG_LOG("  Uploader File Name | %s", uploaderFileName);
    DBG_LOG(" App Image File Name | %s", appImageFileName);
    DBG_LOG(" sdbinfo   File Name | %s", sdbInfoFileName);
    DBG_LOG("         Boot Source | 0x%02X", eFuseBootSource);
    DBG_LOG("  Secure Boot Enable | %d", eFuseSecureBootEnable);
    DBG_LOG("           UKey Lock | %d", eFuseUKeyLock);
    DBG_LOG("            PKf Lock | %d", eFusePKfLock);
    DBG_LOG("            DUK Lock | %d", eFuseDUKLock);
	DBG_LOG("            PKF Skip | %d", eFusePKfWrite);
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "                UKey | ");
    for ( int i = 0; i < sizeof(eFuseUKey); i++ )
    {
        fprintf(stdout, "%02X", eFuseUKey[i]);
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "                 PKf | ");
    for ( int i = 0; i < sizeof(eFusePKf); i++ )
    {
        fprintf(stdout, "%02X", eFusePKf[i]);
    }
    fprintf(stdout, "\n");
    DBG_LOG("---------------------+------------------------------------------------------------------\n");
#endif

    ret = openUploaderFile();
    if ( ret != 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = openAppImageFile();
    if ( ret != 0 )
    {
        DBG_ERR("appImageFileName: %s", appImageFileName);
        DBG_ERR("error!!!");
        return -1;
    }
    return 0;
}

int ProcessController::downloadProcess(eSOCKETCHANNEL ch)
{
    int ret = -1;

    unsigned int  eFuseLen = 0;
    unsigned char eFuseBuffer[128] = {0,};

    /* Upload the uploader firmware to SRAM */
    ret = sendUploaderFile();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* eFuse the UKey */
    ret = sendNVMWrite(EFUSE_TYPE_UKEY);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* read the UKey */
    eFuseLen = 0;
    memset(eFuseBuffer, 0x00, sizeof(eFuseBuffer));
    ret = sendNVMRead(EFUSE_TYPE_UKEY, eFuseBuffer, &eFuseLen);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* check a value */
    ret = 0;
#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[UKey]");
    DBG_LOG("-PARAMS-+-VALUES-----------------------------------------------------------");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "   data | ");
#endif
    for ( int i = 0; i < eFuseLen; i++ )
    {
#ifdef __MP_DEBUG_BUILD__
        fprintf(stdout, "%02X", eFuseBuffer[i]);
#endif
        if ( eFuseBuffer[i] != eFuseUKey[i] )
        {
            ret = -1;
        }
    }
#ifdef __MP_DEBUG_BUILD__
    fprintf(stdout, "\n");
    DBG_LOG("--------+------------------------------------------------------------------\n");
#endif
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }
	
	if(eFusePKfWrite == 1)
	{
		/* eFuse the PKf */
		ret = sendNVMWrite(EFUSE_TYPE_PKF);
		DBG_LOG("[PKf WRITE]");
		if ( ret < 0 )
		{
			DBG_ERR("error!!!");
			return -1;
		}
	}
	
    /* read the PKf */
    eFuseLen = 0;
    memset(eFuseBuffer, 0x00, sizeof(eFuseBuffer));
    ret = sendNVMRead(EFUSE_TYPE_PKF, eFuseBuffer, &eFuseLen);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* check a value */
    ret = 0;
#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[PKf]");

	if(eFusePKfWrite == 0)
	DBG_LOG("[PKf Write Skip]");
	
    DBG_LOG("-PARAMS-+-VALUES-----------------------------------------------------------");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "    data | ");
#endif
    for ( int i = 0; i < eFuseLen; i++ )
    {
#ifdef __MP_DEBUG_BUILD__
        fprintf(stdout, "%02X", eFuseBuffer[i]);
#endif
		/*PKF Write Skip*/
		if(eFusePKfWrite == 1)
		{
			if ( eFuseBuffer[i] != eFusePKf[i] )
			{
				ret = -1;
			}
		}
    }
#ifdef __MP_DEBUG_BUILD__
    fprintf(stdout, "\n");
    DBG_LOG("--------+------------------------------------------------------------------\n");
#endif
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* Upload the sdbInfo data to sdb */
    ret = sendSdbInfo();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }
    
    /* Upload the app firmware to flash */
    ret = sendAppImageFirmware();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* eFuse the UKey Lock */
    ret = sendNVMWrite(EFUSE_TYPE_UKEY_LOCK);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* read the UKey Lock */
    eFuseLen = 0;
    memset(eFuseBuffer, 0x00, sizeof(eFuseBuffer));
    ret = sendNVMRead(EFUSE_TYPE_UKEY_LOCK, eFuseBuffer, &eFuseLen);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[UKey Lock]");
    DBG_LOG("-PARAMS-+-VALUES-");
    DBG_LOG("   data | ");
#endif
#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("%02X", eFuseBuffer[0]);
    DBG_LOG("--------+--------\n");
#endif

    /* check a value */
    if ( eFuseBuffer[0] != eFuseUKeyLock )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* eFuse the PKf Lock */
    ret = sendNVMWrite(EFUSE_TYPE_PKF_LOCK);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* read the PKf Lock */
    eFuseLen = 0;
    memset(eFuseBuffer, 0x00, sizeof(eFuseBuffer));
    ret = sendNVMRead(EFUSE_TYPE_PKF_LOCK, eFuseBuffer, &eFuseLen);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[PKf Lock]");
    DBG_LOG("-PARAMS-+-VALUES-");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "    data | ");
    for ( int i = 0; i < eFuseLen; i++ )
    {
        fprintf(stdout, "%02X", eFuseBuffer[i]);
    }
    fprintf(stdout, "\n");
    DBG_LOG("--------+--------\n");
#endif

    /* check a value */
    if ( eFuseBuffer[0] != eFusePKfLock )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* eFuse the DUK Lock */
    ret = sendNVMWrite(EFUSE_TYPE_DUK_LOCK);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* read the DUK Lock */
    eFuseLen = 0;
    memset(eFuseBuffer, 0x00, sizeof(eFuseBuffer));
    ret = sendNVMRead(EFUSE_TYPE_DUK_LOCK, eFuseBuffer, &eFuseLen);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[DUK Lock]");
    DBG_LOG("-PARAMS-+-VALUES-");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "   data | ");
    for ( int i = 0; i < eFuseLen; i++ )
    {
        fprintf(stdout, "%02X", eFuseBuffer[i]);
    }
    fprintf(stdout, "\n");
    DBG_LOG("--------+--------\n");
#endif

    /* check a value */
    if ( eFuseBuffer[0] != eFuseDUKLock )
    {
        DBG_LOG("eFuseBuffer:  %02X", eFuseBuffer[0]);
        DBG_LOG("eFuseDUKLock: %02X", eFuseDUKLock);
        DBG_ERR("error!!!");
        return -1;
    }

    /* eFuse the Secure boot enable */
    ret = sendNVMWrite(EFUSE_SB_EN);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* read the Secure boot enable */
    eFuseLen = 0;
    memset(eFuseBuffer, 0x00, sizeof(eFuseBuffer));
    ret = sendNVMRead(EFUSE_SB_EN, eFuseBuffer, &eFuseLen);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[Secure Boot Enable]");
    DBG_LOG("-PARAMS-+-VALUES-");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "   data | ");
    for ( int i = 0; i < eFuseLen; i++ )
    {
        fprintf(stdout, "%02X", eFuseBuffer[i]);
    }
    fprintf(stdout, "\n");
    DBG_LOG("--------+--------\n");
#endif

    /* check a value */
    if ( eFuseBuffer[0] != eFuseSecureBootEnable )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* eFuse the Boot source */
    ret = sendNVMWrite(EFUSE_BOOT_SRC);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* read the Boot source */
    eFuseLen = 0;
    memset(eFuseBuffer, 0x00, sizeof(eFuseBuffer));
    ret = sendNVMRead(EFUSE_BOOT_SRC, eFuseBuffer, &eFuseLen);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[Boot Source]");
    DBG_LOG("-PARAMS-+-VALUES-");
    fprintf(stdout, "[Log %s#%d] ", __FUNCTION__, __LINE__);
    fprintf(stdout, "   data | ");
    for ( int i = 0; i < eFuseLen; i++ )
    {
        fprintf(stdout, "%02X", eFuseBuffer[i]);
    }
    fprintf(stdout, "\n");
    DBG_LOG("--------+--------\n");
#endif

    /* check a value */
    if ( eFuseBuffer[0] != eFuseBootSource )
    {
        DBG_ERR("error!!!");
        return -1;
    }
}

int ProcessController::ProcessStart(void)
{
    int ret = -1;

    DBG_LOG("GPIO Init...");
    ret = gpio->gpioInit();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    DBG_LOG("Reset All Socket");
    ret = gpio->ResetAllSocket();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifndef __TEST10000__
    DBG_LOG("Wait Socket Power On...");
    ret = gpio->WaitDownloadReadySet();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    DBG_LOG("Wait DL Start SW...");
    ret = gpio->WaitDownloadStart();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }
#endif /* __TEST10000__ */

    DBG_LOG("UART Open");
    ret = comm->Open();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifdef __TEST10000__
    int downloadResult = SOCKET_MAX;
#endif /* __TEST10000__ */

    for ( int i = SOCKET_CH1; i < SOCKET_MAX; i++ )
    {
        DBG_LOG("Select Socket#%d", i);
        ret = gpio->SelectSocket((eSOCKETCHANNEL)i);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        DBG_LOG("EnableUARTSW...");
        ret = gpio->EnableUARTSW();
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        DBG_LOG("Reset Socket#%d", i);
        ret = gpio->ResetSocket();
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        DBG_LOG("UART Flush");
        comm->Flush();

        DBG_LOG("Start Download Process");
        ret = downloadProcess((eSOCKETCHANNEL)i);
        sleep( 3 );
        if ( ret < 0 )
        {
            DBG_LOG("LED: R");
            ret = gpio->SetResultLED(LED_R);
            if ( ret < 0 )
            {
                DBG_ERR("error!!!");
                return -1;
            }
            DBG_ERR("error!!!");
        }
        else
        {
            DBG_LOG("LED: G");
            ret = gpio->SetResultLED(LED_G);
            if ( ret < 0 )
            {
                DBG_ERR("error!!!");
                return -1;
            }
        }

        DBG_LOG("DisableUARTSW...");
        ret = gpio->DisableUARTSW();
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    DBG_LOG("UART close");
    ret = comm->Close();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

#ifndef __TEST10000__
    DBG_LOG("Wait Socket Power Off...");
    ret = gpio->WaitDownloadReadyReset();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }
#endif /* __TEST10000__ */

    return 0;
}