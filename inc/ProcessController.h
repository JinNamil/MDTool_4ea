#ifndef __PROCESSCONTROLLER_H__
#define __PROCESSCONTROLLER_H__

#include <limits.h>

#include "SerialComm.h"
#include "GPIOControl.h"

#pragma pack(push, 1)
typedef struct _cmdPacketHeader_t
{
    unsigned char sync;         /* 0x57 */
    unsigned char type;         /* packet type */
    unsigned char reserved[2];  /* reserved */
    unsigned int  param;        /* data address or eFuse type */
    unsigned int  size[2];      /* [0]: data size, [1]: option size*/
    unsigned int  crc;          /* verify */
} cmdPacketHeader_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _response_t
{
    unsigned char ack;
    unsigned char nak;
    unsigned char reserved[2];
} response_t;
#pragma pack(pop)

typedef enum _eFILETYPE
{
    FILE_NAME_CONF = 0,
    FILE_NAME_UPLOADER,
    FILE_NAME_APPIMAGE,
    FILE_NAME_SDBINFO
} eFILETYPE;

typedef enum _eEFUSETYPE {
    EFUSE_BOOT_SRC = 0,
    EFUSE_SB_EN,
    EFUSE_TYPE_UKEY_LOCK,
    EFUSE_TYPE_PKF_LOCK,
    EFUSE_TYPE_DUK_LOCK,
	EFUSE_WRITE_PKF,
    EFUSE_TYPE_UKEY,
    EFUSE_TYPE_PKF,
    EFUSE_TYPE_MAX
} eEFUSETYPE;

typedef enum _ePACKETTYPE {
    PACKET_TYPE_SRAM        = 0x33,
    PACKET_TYPE_EFUSE_WRITE = 0x22,
    PACKET_TYPE_EFUSE_READ  = 0x11,
    PACKET_TYPE_FLASH       = 0x55,
    PACKET_TYPE_FLASH_SDB   = 0x66
} ePACKETTYPE;

class ProcessController
{
    public:
        ProcessController(const char* device, const int baudrate);
        virtual ~ProcessController(void);
        int SetName(eFILETYPE type, const char* in);
        int ProcessInit(void);
        int ProcessStart(void);

    private:
        SerialComm*  comm = NULL;
        GPIOControl* gpio = NULL;

        char* configFileName;
        char* uploaderFileName;
        char* appImageFileName;
        char* sdbInfoFileName;
        char* sdbImageFileName;

        /* uploader file binary */
        unsigned int   uploaderBinarySize;
        unsigned char* uploaderBinary;

        /* ini file binary */
        unsigned char* sdbCodeBinary;
        unsigned char* sdbCodePacket;
        unsigned int   sdbCodeBinarySize;
        unsigned int   sdbCodePacketSize;
        unsigned char  sdbPath;

        /* app image file binary */
        int secureBootEnabled;
        unsigned char* pkaBinary;
        unsigned int   pkaBinarySize;
        unsigned char* signatureBinary;
        unsigned int   signatureBinarySize;
        unsigned char* appCodeBinary;
        unsigned int   appCodeBinarySize;
        unsigned int   appImageTotalSize;

        unsigned char  eFuseBootSource;
        unsigned char  eFuseSecureBootEnable;
		unsigned char  eFusePKfWrite;
        unsigned char  eFuseUKeyLock;
        unsigned char  eFusePKfLock;
        unsigned char  eFuseDUKLock;
        unsigned char  eFuseUKey[32];
        unsigned char  eFusePKf[32];

        int makeCmdHeader(ePACKETTYPE type, unsigned int param, unsigned char* in, unsigned int inSize, unsigned int optionSize, cmdPacketHeader_t* out);

		void swapPkf(unsigned char* arr, int first, int second);
        int keyStringTohexArray(eEFUSETYPE type, const char* keyValue);
        int parseValue(eEFUSETYPE type, const char* keyValue);
        int parseLine(const char* line);
        int parseConfigFile(void);
        int parseSdb(int index);

        int openUploaderFile(void);

        int checkImgFilePrefix(unsigned char* in);
        int readSizeFromImgFile(unsigned int in, unsigned int* out);
        int checkAppCodeBinarySize(unsigned int in);
        int checkSdbCodeBinarySize(unsigned int in);
        int checkAppImageTotalSize(unsigned int in, unsigned int in2);
        int openAppImageFile(void);

        int sendUploaderFile(void);

        int sendNVMWrite(eEFUSETYPE type);
        int sendNVMRead(eEFUSETYPE type, unsigned char* out, unsigned int* outLen);

        int sendDataToFlash(const unsigned char* in, unsigned int inLen, unsigned int addr);
        int sendSDBDataToFlash(const unsigned char* in, unsigned int inLen);
        int sendAppImageFirmware(void);
        int sendSdbInfo(void);

        int downloadProcess(eSOCKETCHANNEL ch);
};

#endif //__PROCESSCONTROLLER_H__