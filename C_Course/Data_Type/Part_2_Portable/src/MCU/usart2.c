/////////////////////////////////////////////////////////////////////////
///	\file usart2.c
///	\brief STM32 serial2 MCU hardware interface layer. to maintain
///	code portability, the hardware related code is split from the main logic.
///
///	Author: Ronald Sousa (Opticalworm)
/////////////////////////////////////////////////////////////////////////
#include "MCU/usart2.h"

/////////////////////////////////////////////////////////////////////////
/// \brief enable 16x oversampling. Used to reduce the baudrate calculation
/// error.
/////////////////////////////////////////////////////////////////////////
//#define USART_OVER_SAMPLE_16

///////////////////////////////////////////////////////////////////////////////
/// \brief Keeps track if the serial port is configure and opend
///////////////////////////////////////////////////////////////////////////////
static uint_fast8_t IsOpenFlag = FALSE;

/////////////////////////////////////////////////////////////////////////
///	\brief	Set usart baudrate. can be called at any time.
///
///	\param baud the desire baudrate
///
///	\note setting baudrate will effect any data currently been sent.susb
///		make sure that you check that the write buffer is empty
/////////////////////////////////////////////////////////////////////////
static void Setbaudrate(uint32_t baud)
{
    uint_fast8_t WasUsartEnable = FALSE;
    uint16_t BaudrateTemp;

    // make sure that we preserve the usart enable state.
    if(USART_CR1_UE & USART2->CR1)
    {
        WasUsartEnable = TRUE;
        USART2->CR1 &= ~(USART_CR1_UE);         // turn off usart.
    }

#ifdef USART_OVER_SAMPLE_16
    // oversample 16x
    BaudrateTemp = (SystemCoreClock) /(baud);
#else
    // oversample is 8x

    //calculate our baudrate
    BaudrateTemp = (2 * SystemCoreClock) /(baud);
    // right shift once bits BaudrateTemp[2:0] and set BaudrateTemp[3] to zero. leave the rest of the bits as if
    BaudrateTemp = ((BaudrateTemp & 0xFFF0) | ((BaudrateTemp >> 1) & 0x0007));

#endif

    USART2->BRR =  BaudrateTemp;


    if(WasUsartEnable)
    {
        USART2->CR1 |= USART_CR1_UE;
    }
}

/////////////////////////////////////////////////////////////////////////
///	\brief	you can use this function to check if the write buffer is
///	empty and ready for new data
///
///	\param destination pointer to return the read byte
///	\return TRUE = Busy else ready. else false
/////////////////////////////////////////////////////////////////////////
static uint_fast8_t IsWriteBusy(void)
{
	uint_fast8_t ReturnState = TRUE;

     if(USART2->ISR & USART_ISR_TXE)
     {
         ReturnState = FALSE;
     }

	return ReturnState;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief return the serial open state
///
/// \return true = the serial port is open else false
///////////////////////////////////////////////////////////////////////////////
static uint_fast8_t IsSerialOpen(void)
{
    return IsOpenFlag;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Open the serial port.
///
/// \param baudrate set the serial port baud rate
///
/// \return true = success else port is already open
///////////////////////////////////////////////////////////////////////////////
static uint_fast8_t Open(uint32_t baudrate)
{
    if (!IsOpenFlag)
    {
        USART2->CR1 = 0;

        // make sure that the IO clock is enabled
        RCC->AHBENR |= ((uint32_t)1<<17);

        // Change PA2 and PA3 port function to their alternate mode
        GPIOA->MODER &= ~((uint32_t)0x3 << 4) | ~((uint32_t)0x3 << 6);    // clear the related bits before we set them. Comment the next call will set all bits.
        GPIOA->MODER |= ((uint32_t)2 << 4) | ((uint32_t)2 << 6);            // Set both PA2 and PA3 to there alternate functions. PA2 = default TX. PA3 = default RX

        GPIOA->AFR[0] &= ~((uint32_t)0xF << 12) | ~((uint32_t)0xF << 8);    // clear the related bits before we set them
        GPIOA->AFR[0] |= ((uint32_t)1 << 12) | ((uint32_t)1 << 8);          // Select TX and RX alternate functions

        RCC->APB1ENR |= ((uint32_t)1<<17);                                  // USART2 peripheral clock

        Setbaudrate(baudrate);                                                // set baudrate

        USART2->CR1 = USART_CR1_UE | USART_CR1_OVER8 | USART_CR1_RE;
        USART2->CR1  |= USART_CR1_TE ;
        IsOpenFlag = TRUE;
        return TRUE;
    }

    // error. Port is already open
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Open the serial port
///
/// \return true = success else port is already open
///////////////////////////////////////////////////////////////////////////////
static void Close(void)
{
    USART2->CR1 &= ~(USART_CR1_UE);         // turn off usart.
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Send a single byte
///
/// \return true = success else port is not open
///////////////////////////////////////////////////////////////////////////////
static uint_fast8_t SendByte(uint8_t source)
{
    if (IsOpenFlag)
    {
        while( IsWriteBusy() );

        USART2->TDR = (uint32_t)source;

        return TRUE;
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Write a string
///
/// \param source pointer to the string to write. must end with null
///
/// \return true = success else either the port is not open or the pointer
/// to the array is invalid.
///////////////////////////////////////////////////////////////////////////////
static uint_fast8_t SendString(const uint8_t *source)
{
    if (IsOpenFlag && source)
    {
        while(*source)
        {
            if (!SendByte(*source) )
            {
                return FALSE;
            }
            source++;
        }
        return TRUE;
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief Write an array of bytes
///
/// \param source pointer to the array to transmit.
/// \param length is the size of the array
///
/// \return true = success else either the port is not open or the pointer
/// to the array is invalid.
///////////////////////////////////////////////////////////////////////////////
static uint_fast8_t SendArray(const uint8_t *source, uint32_t length)
{
    if (IsOpenFlag && source)
    {
        for ( ; length ; length--)
        {
            if ( !SendByte(*source) )
            {
                return FALSE;
            }
            source++;
        }
        return TRUE;
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief return the serial receive byte buffer state
///
/// \return      1 = we have data
///              0 = no data to read
///             -1 = Port is not open
///////////////////////////////////////////////////////////////////////////////
static int_fast8_t DoesReceiveBufferHaveData(void)
{
    if (IsOpenFlag)
    {

        if( USART2->ISR & USART_ISR_RXNE)
        {
            return TRUE;
        }

        return FALSE;
    }
    return -1;
}

///////////////////////////////////////////////////////////////////////////////
/// \brief serial port for reading serial byte
///
/// \param destination pointer to return the newly read byte.
///
/// \return      1 = success on reading a byte
///              0 = no data to read or
///             -1 = Port is not open or the destination pointer is invalid
///////////////////////////////////////////////////////////////////////////////
static int_fast8_t GetByte(uint8_t *destination)
{
    int_fast8_t Result = -1; // set it to port no connected state

    /// \todo need handle rx error flags

    if (IsOpenFlag && destination)
    {
        if( USART2->ISR & USART_ISR_RXNE)
        {
            *destination = (uint8_t) USART2->RDR;
            Result = 1;
        }
        else
        {
            Result = 0;
        }

    }

    return Result;
}


///////////////////////////////////////////////////////////////////////////////
/// \brief Defines the standard serial functions for usart 2
///
/// /sa SerialInterface
///////////////////////////////////////////////////////////////////////////////
SerialInterface SerialPort2 = {
                                    IsSerialOpen,
                                    Open,
                                    Close,
                                    SendByte,
                                    SendString,
                                    SendArray,
                                    DoesReceiveBufferHaveData,
                                    GetByte
                                };
