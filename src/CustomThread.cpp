#include <cstdio>
#include <cstdlib>

#include <pthread.h>

#include "CustomThread.h"

void *CustomThread::GetThreadParam(void)
{
    return threadParam;
}

int CustomThread::ThreadStart(void* threadParam)
{
    int ret;
    pthread_t threads;
    this->threadParam = threadParam;
    ret = pthread_create(&threads, NULL, &(CustomThread::threadRun), (void*)this);
    if ( ret < 0 )
    {
        fprintf(stderr, "pthread_create error(0x%02X)\r\n", ret);
        ret = -1;
        return ret;
    }
    ret = pthread_detach(threads);
    if ( ret < 0 )
    {
        fprintf(stderr, "pthread_detach error(0x%02X)\r\n", ret);
        ret = -1;
        return ret;
    }
}
