/////////////////////////////////////////////////////////////////////////
///	\file main.c
///	\brief This is the main program code.
///
///	Author: Ronald Sousa (Opticalworm)
/////////////////////////////////////////////////////////////////////////
#include "common.h"
#include "MCU/led.h"
#include "MCU/usart2.h"
#include "MCU/usart1.h"
#include "MCU/tick.h"
#include "Terminal/DebugInterface.h"

/////////////////////////////////////////////////////////////////////////
///	\brief the first user code function to be called after the ARM M0
///	has initial.
/////////////////////////////////////////////////////////////////////////
void main(void)
{
    uint8_t TempData;

    Led_Init();
    Tick_init();

    SDebug_Init(&SerialPort2);
    SerialPort1.Open(9600);

    SerialPort1.SendString("Hello World\r\n");

    for ( ;; )
    {
        SDebug_Monitor();

        if(SerialPort1.GetByte(&TempData))
        {
            SerialPort1.SendByte(TempData);
        }
    }
}