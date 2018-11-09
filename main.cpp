#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/types.h>

#include "ProcessController.h"
#include "GPIOControl.h"

#include "debug.h"

static int  baudrate              = 230400;
static char serialDeviceName[128] = "/dev/serial1";
static char configFileName[128]   = "/home/pi/setting.cfg";
static char uploaderFileName[128] = "/home/pi/uploader.bin";
static char appImageFileName[128] = "/home/pi/test.img";
static char sdbInfoFileName[128]  = "/home/pi/sdbinfo.ini";
static void gpio_test(void)
{
    GPIOControl gpio;

    gpio.GPIOTest();

    exit(1);
}

static void print_usage(const char *prog)
{
    fprintf(stdout, "Usage: %s [-bdcuag]\n", prog);
    fprintf(stdout, "  -b --baudrate uart baudrate       (default %d)\n", baudrate);
    fprintf(stdout, "  -d --device   serial device name  (default %s)\n", serialDeviceName);
    fprintf(stdout, "  -c --config   config file name    (default %s)\n", configFileName);
    fprintf(stdout, "  -u --uploader uploader file name  (default %s)\n", uploaderFileName);
    fprintf(stdout, "  -a --appimage app image file name (default %s)\n", appImageFileName);
    fprintf(stdout, "  -g --gpiotest\n");
    exit(1);
}
static void parse_opts(int argc, char *argv[])
{
    int c;
    while ( 1 )
    {
        static const struct option lopts[] = {
            { "baudrate", required_argument, 0, 'b' },
            { "device",   required_argument, 0, 'd' },
            { "config",   required_argument, 0, 'c' },
            { "uploader", required_argument, 0, 'u' },
            { "appimage", required_argument, 0, 'a' },
            { "gpiotest", no_argument,       0, 'g' },
            { 0, 0, 0, 0 },
        };

        c = getopt_long(argc, argv, "d:b:c:u:a:g", lopts, NULL);

        if ( c == -1 )
        {
            break;
        }

        switch ( c )
        {
            case 'b':
                {
                    baudrate = atoi(optarg);
                }
                break;

            case 'd':
                {
                    memset(serialDeviceName, 0x00, sizeof(serialDeviceName));
                    strcpy(serialDeviceName, optarg);
                }
                break;

            case 'c':
                {
                    memset(configFileName, 0x00, sizeof(configFileName));
                    strcpy(configFileName, optarg);
                }
                break;

            case 'u':
                {
                    memset(uploaderFileName, 0x00, sizeof(uploaderFileName));
                    strcpy(uploaderFileName, optarg);
                }
                break;

            case 'a':
                {
                    memset(appImageFileName, 0x00, sizeof(appImageFileName));
                    strcpy(appImageFileName, optarg);
                }
                break;

            case 'g':
                {
                    gpio_test();
                }
                break;

            default:
                {
                    print_usage(argv[0]);
                }
                break;
        }
    }
}

time_t     currentTime;
struct tm* currentTimeStruct;
void timestamping(FILE* file)
{
    if ( file == NULL )
    {
        file = stdout;
    }

    currentTime       = time(NULL);
    currentTimeStruct = localtime(&currentTime);
    fprintf(file,
            "[%04d %02d %02d %02d:%02d:%02d] ",
            currentTimeStruct->tm_year+1900,
            currentTimeStruct->tm_mon+1,
            currentTimeStruct->tm_mday,
            currentTimeStruct->tm_hour,
            currentTimeStruct->tm_min,
            currentTimeStruct->tm_sec);
}

int main(int argc, char* argv[])
{
    int ret = -1;

    parse_opts(argc, argv);

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("%s %s", __DATE__, __TIME__);
    DBG_LOG("Start upload");
    DBG_LOG("[TEST MODE]");
    DBG_LOG("-PARAMS---+-VALUES----------");
    DBG_LOG(" baudrate | %d", baudrate);
    DBG_LOG("   device | %s", serialDeviceName);
    DBG_LOG("   config | %s", configFileName);
    DBG_LOG(" uploader | %s", uploaderFileName);
    DBG_LOG(" appimage | %s", appImageFileName);
	DBG_LOG("  sdbinfo | %s", sdbInfoFileName);
    DBG_LOG("----------+-----------------");
#endif

    ProcessController* processController = new ProcessController(serialDeviceName, baudrate);


    ret = processController->SetName(FILE_NAME_CONF, configFileName);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = processController->SetName(FILE_NAME_UPLOADER, uploaderFileName);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = processController->SetName(FILE_NAME_APPIMAGE, appImageFileName);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }
	
	ret = processController->SetName(FILE_NAME_SDBINFO, sdbInfoFileName);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = processController->ProcessInit();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }


#ifdef __TEST10000__
    char  logFileName[128] = {0,};
    FILE* logFile = NULL;

    currentTime       = time(NULL);
    currentTimeStruct = localtime(&currentTime);
    sprintf(logFileName,
            "log%04d_%02d_%02d_%02d:%02d:%02d.txt",
            currentTimeStruct->tm_year+1900,
            currentTimeStruct->tm_mon+1,
            currentTimeStruct->tm_mday,
            currentTimeStruct->tm_hour,
            currentTimeStruct->tm_min,
            currentTimeStruct->tm_sec);

    logFile = fopen(logFileName, "w");
    if ( logFile == NULL )
    {
        delete processController;
        processController = NULL;
        return -1;
    }

    timestamping(logFile);
    fprintf(logFile, "start test\n");
    for ( int i = 0; i < 20; i++)
    {
        timestamping(logFile);
        fprintf(logFile, "#%d start\n", i);
        ret = processController->ProcessStart();
        timestamping(logFile);
        fprintf(logFile, "#%d done, OK: %d / Fail: %d\n", i, ret, (SOCKET_MAX - ret));
    }
    timestamping(logFile);
    fprintf(logFile, "test done\n");

    fclose(logFile);
#else /* __TEST10000__ */
    do
    {
        ret = processController->ProcessStart();
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    } while ( 1 );
#endif /* __TEST10000__ */
    delete processController;
    processController = NULL;

    DBG_LOG("upload done.");

    return 1;
}
