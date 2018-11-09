#ifndef __GPIOCONTROL_H__
#define __GPIOCONTROL_H__

typedef enum _eSOCKETCHANNEL {
    SOCKET_CH1 = 0,
    SOCKET_CH2 = 1,
    SOCKET_CH3 = 2,
    SOCKET_CH4 = 3,
    SOCKET_MAX = 4
} eSOCKETCHANNEL;

typedef enum _eRESULTLED {
    LED_R = 0,
    LED_G = 1,
} eRESULTLED;

typedef enum _eGPIOPINNAME {
    GPIOPINNAME_MS500_UART_CTL_S0 = 0,
    GPIOPINNAME_MS500_UART_CTL_S1,
    GPIOPINNAME_MS500_DL_READY,
    GPIOPINNAME_MS500_DL_STATE,
    GPIOPINNAME_MS500_UART_SW_ENABLE,
    GPIOPINNAME_MS500_RST_CH1,
    GPIOPINNAME_MS500_RST_CH2,
    GPIOPINNAME_MS500_RST_CH3,
    GPIOPINNAME_MS500_RST_CH4,
    GPIOPINNAME_MS500_CH1_LEDR,
    GPIOPINNAME_MS500_CH1_LEDG,
    GPIOPINNAME_MS500_CH2_LEDR,
    GPIOPINNAME_MS500_CH2_LEDG,
    GPIOPINNAME_MS500_CH3_LEDR,
    GPIOPINNAME_MS500_CH3_LEDG,
    GPIOPINNAME_MS500_CH4_LEDR,
    GPIOPINNAME_MS500_CH4_LEDG,
    GPIOPINNAME_MS500_UART_SW1,
    GPIOPINNAME_MS500_UART_SW2,
    GPIOPINNAME_MS500_UART_SW3,
    GPIOPINNAME_MS500_UART_SW4,
//    GPIOPINNAME_MS500_RESET,
    GPIOPINNAME_MS500_DL_START,
//    GPIOPINNAME_MS500_UART_TX1,
//    GPIOPINNAME_MS500_UART_RX1,
    GPIOPINNAME_MAX
} eGPIOPINNAME;

class GPIOControl
{
    public:
        GPIOControl(void);
        virtual ~GPIOControl(void);

        int GPIOTest(void);
        int gpioInit(void);
        int WaitDownloadReadySet(void);
        int WaitDownloadReadyReset(void);
        int WaitDownloadStart(void);
        int ResetAllSocket(void);
        int ResetSocket(void);
        int SetResultLED(eRESULTLED res);
        int SelectSocket(eSOCKETCHANNEL ch);
        int EnableUARTSW(void);
        int DisableUARTSW(void);

    private:
        eSOCKETCHANNEL enabledSocket;
        int gpioSet(eGPIOPINNAME pin);
        int gpioReset(eGPIOPINNAME pin);
        int resetResultLED(void);
        int enableUARTCH(void);
        int setUARTExtension(void);
        int gpioDump(void);
        
};



#endif // __GPIOCONTROL_H__
