#ifndef __CUSTOMTHREAD_H__
#define __CUSTOMTHREAD_H__

class CustomThread
{
    public:
        CustomThread(void);
        virtual ~CustomThread(void);

        virtual void customThread(void* param) = 0;
        static void* threadRun(void* classPointer)
        {
            CustomThread* customThread = (CustomThread*)classPointer;
            customThread->customThread(customThread->GetThreadParam());
        }
        void *GetThreadParam(void);
        int ThreadStart(void* threadParam);

    private:
        void* threadParam;

};

#endif // __CUSTOMTHREAD_H__
