#ifndef __SERIALCOMM_H__
#define __SERIALCOMM_H__

class SerialComm
{
    public:
        SerialComm(const char* device, const int baudrate);
        virtual ~SerialComm(void);

        int Open(void);
        int Close(void);
        int Flush(void);
        int Send(const unsigned char* in, unsigned int inLen);
        int Receive(unsigned char *out, unsigned int outLen);
        int GetReceiveSize(void);

    private:
        char* device;
        int   fd;
        int   baudrate;
};

#endif // __SERIALCOMM_H__
