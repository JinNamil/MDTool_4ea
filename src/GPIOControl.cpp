#include <cstdio>
#include <unistd.h>

#include <wiringPi.h>

#include "GPIOControl.h"

#include "debug.h"

typedef enum _eGPIOSTATE {
    GPIO_SET   = (1==1),
    GPIO_RESET = (1==2)
} eGPIOSTATE;

/*
 *  2 OUTPUT RESET UART_CTL_S0
 *  3 OUTPUT RESET UART_CTL_S1
 *  4  INPUT LOW   DL_READY
 *  5 OUTPUT RESET DL_STATE
 *  6 OUTPUT SET   UART_SW_ENABLE
 * 10 OUTPUT SET   RST_CH1         => after 0.2
 * 11 OUTPUT SET   RST_CH2         => after 0.2
 * 12 OUTPUT SET   RST_CH3         => after 0.2
 * 13 OUTPUT SET   RST_CH4         => after 0.2
 * 16 OUTPUT SET   CH1_LEDR
 * 17 OUTPUT SET   CH1_LEDG
 * 18 OUTPUT SET   CH2_LEDR
 * 19 OUTPUT SET   CH2_LEDG
 * 20 OUTPUT SET   CH3_LEDR
 * 21 OUTPUT SET   CH3_LEDG
 * 22 OUTPUT SET   CH4_LEDR
 * 23 OUTPUT SET   CH4_LEDG
 * 24 OUTPUT SET   UART_SW1
 * 25 OUTPUT SET   UART_SW2
 * 26 OUTPUT SET   UART_SW3
 * 27 OUTPUT SET   UART_SW4
 * 30 OUTPUT SET   RST             => 0.1 only
 * 31 INPUT  HIGH  DL_START
 * 32 OUTPUT RESET UART_TX1
 * 33 OUTPUT RESET UART_RX1
 */

static const int pinNumber[GPIOPINNAME_MAX] = {  
    2,
    3,
    4,
    5,
    6,
    10,
    11,
    12,
    13,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
//    30,
    31
//    32,
//    33
};

static const int pinDir[GPIOPINNAME_MAX] = {
    OUTPUT,
    OUTPUT,
    INPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
    OUTPUT,
//    OUTPUT,
    INPUT
//    OUTPUT,
//    OUTPUT
};

static const int pinDefaultValue[GPIOPINNAME_MAX] = {
    GPIO_RESET,
    GPIO_RESET,
    PUD_DOWN,
    GPIO_RESET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
    GPIO_SET,
//    GPIO_SET,
    PUD_UP
//    GPIO_RESET,
//    GPIO_RESET
};

GPIOControl::GPIOControl(void)
{
    int ret = -1;

    enabledSocket = SOCKET_MAX;

    wiringPiSetupGpio();

    ret = gpioInit();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
    }
}

GPIOControl::~GPIOControl(void)
{
    int ret = -1;

    ret = gpioInit();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
    }
}

int GPIOControl::gpioDump(void)
{
#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[GPIO Dump]");
    for ( int i = 0; i < GPIOPINNAME_MAX; i++ )
    {
        DBG_LOG("%02d ", pinNumber[i]);
    }
    fprintf(stdout, "\n");
    for ( int i = 0; i < GPIOPINNAME_MAX; i++ )
    {
        DBG_LOG("%02d ", digitalRead(pinNumber[i]));
    }
    fprintf(stdout, "\n");
#endif
    return 0;
}

int GPIOControl::gpioInit(void)
{
    int ret = -1;

#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("[GPIO]");
    DBG_LOG("-PIN-+-MODE-+-VALUE-----");
#endif
    for ( int i = 0; i < GPIOPINNAME_MAX; i++ )
    {
        pinMode(pinNumber[i], pinDir[i]);

        if ( pinDir[i] == OUTPUT )
        {
            if ( pinDefaultValue[i] == GPIO_SET )
            {
                ret = gpioSet((eGPIOPINNAME)i);
                if ( ret < 0 )
                {
                    DBG_ERR("error!!!");
                    return -1;
                }
            }
            else
            {
                ret = gpioReset((eGPIOPINNAME)i);
                if ( ret < 0 )
                {
                    DBG_ERR("error!!!");
                    return -1;
                }
            }
#ifdef __MP_DEBUG_BUILD__
    fprintf(stdout, "[Log %s3%d] %4d |%5d | OUT %6d\n", __FUNCTION__, __LINE__, pinNumber[i], pinDir[i], pinDefaultValue[i]);
#endif
        }
        else if ( pinDir[i] == INPUT )
        {
            pullUpDnControl(pinNumber[i], pinDefaultValue[i]) ;
#ifdef __MP_DEBUG_BUILD__
    fprintf(stdout, "[Log %s3%d] %4d |%5d | IN  %6d\n", __FUNCTION__, __LINE__, pinNumber[i], pinDir[i], pinDefaultValue[i]);
#endif
        }

    }
#ifdef __MP_DEBUG_BUILD__
    DBG_LOG("-----+------+-----------");
#endif

    return 0;
}

int GPIOControl::gpioSet(eGPIOPINNAME pin)
{
    if ( (pin >= GPIOPINNAME_MAX) || (pin < GPIOPINNAME_MS500_UART_CTL_S0) )
    {
        return -1;
    }

    if ( pinDir[pin] != OUTPUT )
    {
        return -1;
    }

    digitalWrite(pinNumber[pin], GPIO_SET);

    return 0;
}

int GPIOControl::gpioReset(eGPIOPINNAME pin)
{
    if ( (pin >= GPIOPINNAME_MAX) || (pin < GPIOPINNAME_MS500_UART_CTL_S0) )
    {
        return -1;
    }

    if ( pinDir[pin] != OUTPUT )
    {
        return -1;
    }

    digitalWrite(pinNumber[pin], GPIO_RESET);

    return 0;
}

int GPIOControl::GPIOTest(void)
{
    int ret = -1;

    for ( int i = 0; i < GPIOPINNAME_MAX; i++ )
    {
        if ( pinDir[i] != OUTPUT )
        {
            continue;
        }

        DBG_LOG("GPIO#%02d Set", pinNumber[i]);
        ret = gpioSet((eGPIOPINNAME)i);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
        gpioDump();
        usleep(100*1000);
    }

    for ( int i = 0; i < GPIOPINNAME_MAX; i++ )
    {
        if ( pinDir[i] != OUTPUT )
        {
            continue;
        }

        DBG_LOG("GPIO#%02d Reset", pinNumber[i]);
        ret = gpioReset((eGPIOPINNAME)i);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
        gpioDump();
        usleep(100*1000);
    }

    while ( 1 )
    {
        gpioDump();
        usleep(100*1000);
    }

    return 0;
}

int GPIOControl::WaitDownloadReadySet(void)
{
    int ret = -1;

    ret = gpioReset(GPIOPINNAME_MS500_DL_STATE);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    do
    {
        usleep(100);
        ret = digitalRead(pinNumber[GPIOPINNAME_MS500_DL_READY]);
    } while ( ret != 1 );
    DBG_LOG("Power: %d", ret);

    ret = gpioSet(GPIOPINNAME_MS500_DL_STATE);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int GPIOControl::WaitDownloadReadyReset(void)
{
    int ret = -1;

    ret = gpioReset(GPIOPINNAME_MS500_DL_STATE);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    do
    {
        usleep(100);
        ret = digitalRead(pinNumber[GPIOPINNAME_MS500_DL_READY]);
    } while ( ret != 0 );
    DBG_LOG("Power: %d", ret);

    ret = gpioSet(GPIOPINNAME_MS500_DL_STATE);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int GPIOControl::WaitDownloadStart(void)
{
    int ret = -1;

    ret = gpioReset(GPIOPINNAME_MS500_DL_STATE);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    do
    {
        usleep(100);
        ret = digitalRead(pinNumber[GPIOPINNAME_MS500_DL_START]);
    } while ( ret != 0 );
    DBG_LOG("Start button value: %d", ret);

    ret = gpioSet(GPIOPINNAME_MS500_DL_STATE);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int GPIOControl::ResetAllSocket(void)
{
/* 0.1 only */
#if 0
    int ret = -1;

    ret = gpioSet(GPIOPINNAME_MS500_RESET);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* sleep 0.5 sec */
    usleep(50*1000);

    ret = gpioReset(GPIOPINNAME_MS500_RESET);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* sleep 0.5 sec */
    usleep(50*1000);

    ret = gpioSet(GPIOPINNAME_MS500_RESET);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
/* after 0.2 */
#else
    int ret = -1;
    
    for ( int i = SOCKET_CH1; i < SOCKET_MAX; i++ )
    {
        ret = gpioSet((eGPIOPINNAME)(GPIOPINNAME_MS500_RST_CH1+i));
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        /* sleep 0.5 sec */
        usleep(50*1000);

        ret = gpioReset((eGPIOPINNAME)(GPIOPINNAME_MS500_RST_CH1+i));
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }

        /* sleep 0.5 sec */
        usleep(50*1000);

        ret = gpioSet((eGPIOPINNAME)(GPIOPINNAME_MS500_RST_CH1+i));
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    return 0;
#endif
}

int GPIOControl::ResetSocket(void)
{
    int ret = -1;
    int rst = -1;
    
    if ( (enabledSocket < SOCKET_CH1) || (enabledSocket >= SOCKET_MAX) )
    {
        DBG_ERR("error!!!");
        return -1;
    }
    
    rst = (GPIOPINNAME_MS500_RST_CH1+enabledSocket);
    ret = gpioSet((eGPIOPINNAME)rst);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* sleep 0.5 sec */
    usleep(50*1000);

    ret = gpioReset((eGPIOPINNAME)rst);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* sleep 0.5 sec */
    usleep(50*1000);

    ret = gpioSet((eGPIOPINNAME)rst);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int GPIOControl::resetResultLED(void)
{
    int ret = -1;

    if ( (enabledSocket < SOCKET_CH1) || (enabledSocket >= SOCKET_MAX) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    if ( pinDefaultValue[(GPIOPINNAME_MS500_CH1_LEDR+(enabledSocket*2))] == GPIO_SET )
    {
        ret = gpioSet((eGPIOPINNAME)(GPIOPINNAME_MS500_CH1_LEDR+(enabledSocket*2)));
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }
    else
    {
        ret = gpioReset((eGPIOPINNAME)(GPIOPINNAME_MS500_CH1_LEDR+(enabledSocket*2)));
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }


    if ( pinDefaultValue[(GPIOPINNAME_MS500_CH1_LEDR+(enabledSocket*2)+1)] == GPIO_SET )
    {
        ret = gpioSet((eGPIOPINNAME)(GPIOPINNAME_MS500_CH1_LEDR+(enabledSocket*2)+1));
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }
    else
    {
        ret = gpioReset((eGPIOPINNAME)(GPIOPINNAME_MS500_CH1_LEDR+(enabledSocket*2)+1));
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }
}

int GPIOControl::EnableUARTSW(void)
{
    int ret = -1;

    ret = gpioReset((eGPIOPINNAME)(GPIOPINNAME_MS500_UART_SW_ENABLE));
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int GPIOControl::DisableUARTSW(void)
{
    int ret = -1;

    ret = gpioSet((eGPIOPINNAME)(GPIOPINNAME_MS500_UART_SW_ENABLE));
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int GPIOControl::enableUARTCH(void)
{
    int ret = -1;

    if ( (enabledSocket < SOCKET_CH1) || (enabledSocket >= SOCKET_MAX) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = gpioSet((eGPIOPINNAME)(GPIOPINNAME_MS500_UART_SW1+enabledSocket));
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}

int GPIOControl::setUARTExtension(void)
{
    int ret = -1;
    int s[2] = {0};

    if ( (enabledSocket < SOCKET_CH1) || (enabledSocket >= SOCKET_MAX) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ( (enabledSocket&0x1) ? (s[0] = GPIO_SET) : (s[0] = GPIO_RESET) );
    ( (enabledSocket&0x2) ? (s[1] = GPIO_SET) : (s[1] = GPIO_RESET) );

    if ( s[0] == GPIO_SET )
    {
        ret = gpioSet(GPIOPINNAME_MS500_UART_CTL_S0);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }
    else
    {
        ret = gpioReset(GPIOPINNAME_MS500_UART_CTL_S0);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    if ( s[1] == GPIO_SET )
    {
        ret = gpioSet(GPIOPINNAME_MS500_UART_CTL_S1);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }
    else
    {
        ret = gpioReset(GPIOPINNAME_MS500_UART_CTL_S1);
        if ( ret < 0 )
        {
            DBG_ERR("error!!!");
            return -1;
        }
    }

    return 0;
}

int GPIOControl::SelectSocket(eSOCKETCHANNEL ch)
{
    int ret = -1;

    if ( (ch < SOCKET_CH1) || (ch >= SOCKET_MAX) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    enabledSocket = ch;

    ret = resetResultLED();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = enableUARTCH();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    ret = setUARTExtension();
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    /* sleep 0.5 sec */
    usleep(500*1000);

    return 0;
}

int GPIOControl::SetResultLED(eRESULTLED res)
{
    int ret = -1;
    int led = 0;

    if ( (enabledSocket < SOCKET_CH1) || (enabledSocket >= SOCKET_MAX) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    if ( (led < LED_R) || (led > LED_G) )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    led =  GPIOPINNAME_MS500_CH1_LEDR;
    led += enabledSocket*2;
    led += res;

    if ( (led < GPIOPINNAME_MS500_CH1_LEDR) || (led > GPIOPINNAME_MS500_CH4_LEDG) )
    {
        DBG_ERR("error!!!");
        return -1;
    }
    ret = gpioReset((eGPIOPINNAME)led);
    if ( ret < 0 )
    {
        DBG_ERR("error!!!");
        return -1;
    }

    return 0;
}