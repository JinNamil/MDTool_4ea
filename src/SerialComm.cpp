#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "debug.h"
#include "SerialComm.h"

SerialComm::SerialComm(const char* inDevice, const int inBaudrate): fd(-1), baudrate(inBaudrate)
{
    device = new char[strlen(inDevice)+1]{0,};
    strcpy(device, inDevice);
}

SerialComm::~SerialComm(void)
{
    Flush();
    Close();
}

static inline speed_t baudrate2speed(int baudrate)
{
    switch ( baudrate )
    {
        case 50:
            return B50;
        case 75:
            return B75;
        case 110:
            return B110;
        case 134:
            return B134;
        case 150:
            return B150;
        case 200:
            return B200;
        case 300:
            return B300;
        case 600:
            return B600;
        case 1200:
            return B1200;
        case 1800:
            return B1800;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        case 460800:
            return B460800;
        case 500000:
            return B500000;
        case 576000:
            return B576000;
        case 921600:
            return B921600;
        case 1000000:
            return B1000000;
        case 1152000:
            return B1152000;
        case 1500000:
            return B1500000;
        case 2000000:
            return B2000000;
        case 2500000:
            return B2500000;
        case 3000000:
            return B3000000;
        case 3500000:
            return B3500000;
        case 4000000:
            return B4000000;
        default:
            {
                DBG_ERR("error");
            }
            return 0;
    }
}

int SerialComm::Open(void)
{
    int ret = 0;

    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    if ( fd < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    ret = fcntl(fd, F_SETFL, O_RDWR);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    struct termios tio;
    tcgetattr(fd, &tio);

    cfmakeraw(&tio);

    speed_t speed = baudrate2speed(baudrate);
    if ( speed == 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    ret = cfsetispeed(&tio, speed);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }
    ret = cfsetospeed(&tio, speed);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    tio.c_cflag |= (CLOCAL | CREAD) ;
    tio.c_cflag &= ~PARENB ;
    tio.c_cflag &= ~CSTOPB ;
    tio.c_cflag &= ~CSIZE ;
    tio.c_cflag |= CS8 ;
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG) ;
    tio.c_oflag &= ~OPOST ;

    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 100;  /* timeout: c_cc[VTIME] * 0.1 sec */

    ret = tcsetattr(fd, TCSANOW, &tio);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    int status = 0;
    ret = ioctl(fd, TIOCMGET, &status);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    status |= TIOCM_DTR;
    status |= TIOCM_RTS;

    usleep(10*1000);

    ret = ioctl(fd, TIOCMSET, &status);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }


    usleep(10*1000);

    ret = tcflush(fd, TCIOFLUSH);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    usleep(10*1000);

    ret = 0;
    return ret;
}

int SerialComm::Close(void)
{
    int ret = 0;

    ret = close(fd);
    if ( ret < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    usleep(10*1000);
    
    ret = 0;
    return ret;
}

int SerialComm::Flush(void)
{
    int ret = 0;

    ret = tcflush(fd, TCIOFLUSH);
    if ( ret != 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }
    
    usleep(10*1000);

    ret = 0;
    return ret;
}

int SerialComm::Send(const unsigned char* in, unsigned int inLen)
{
    int ret = 0;
    int writenBytes = 0;

    if ( fd < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    writenBytes = 0;
    do
    {
        ret = write(fd, in + writenBytes, inLen - writenBytes);
        if ( ret <= 0 )
        {
            DBG_ERR("error");
            DBG_ERR("%s(%d/%d)\n", __FUNCTION__, writenBytes, inLen);
            ret = -1;
            return ret;
        }
        writenBytes += ret;
    } while ( writenBytes < inLen );
//    fprintf(stdout, "%s done\n", __FUNCTION__);

    return writenBytes;
}

int SerialComm::Receive(unsigned char *out, unsigned int outLen)
{
    int ret = 0;
    int readBytes = 0;

    if ( fd < 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    readBytes = 0;

    do
    {
        ret = read(fd, out + readBytes, outLen - readBytes);
        if ( ret <= 0 )
        {
            DBG_ERR("error");
            DBG_ERR("%s(%d/%d)\n", __FUNCTION__, readBytes, outLen);
            ret = -1;
            return ret;
        }
        readBytes += ret;
    } while ( readBytes < outLen );
//    fprintf(stdout, "%s done\n", __FUNCTION__);

    return readBytes;
}

int SerialComm::GetReceiveSize(void)
{
    int ret = 0;
    int receiveSize = 0;

    ret = ioctl (fd, FIONREAD, &receiveSize);
    if ( ret != 0 )
    {
        DBG_ERR("error");
        ret = -1;
        return ret;
    }

    return receiveSize;
}