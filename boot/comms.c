/*
 * comms.c
 *
 *  Created on: 30 Jul 2013
 *      Author: pjbotma
 */

#include "comms.h"

#define UPLOADBLOCKSIZE 256

uint8_t msgId;

uint8_t debugStr[256], debugLen;

uint8_t uartReceived;

uint8_t uploadIndex;
uint16_t uploadBlockCRC;

void COMMS_Init(void)
{
    BSP_UART_Init(BSP_UART_DEBUG);

    msgId = 0x00;

    uartReceived = 0;
}

#define BASE BSP_EBI_SRAM1_BASE
void check(size_t i, char value, size_t* errors)
{
    *(volatile uint8_t*)(BASE + i) = value;
    char r = *(volatile uint8_t*)(BASE + i);

    if (r != value)
    {
        char buf[80] = {0};
        sprintf(buf, "%.6X (%.2X -> %.2X)\n", i, value, r);
        BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)buf, sizeof(buf), true);
        (*errors)++;
    }
}

void testSram(void)
{
    BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"\ntestSram ", 10, true);

    size_t size = 2 * 1024 * 1024;

    size_t errors = 0;

    for (size_t i = 0; i < size; i++)
    {
        char value = i % 256;
        check(i, 0xFF, &errors);
        check(i, value, &errors);
    }

    if (errors == 0)
    {
        BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"------------OK\n", 16, true);
    }
    else
    {
        BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"Err\n", 4, true);
    }
}

volatile uint8_t* Sram;
void testSram_FF()
{
    BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"\ntestSram_FF ", 13, true);

    Sram = (volatile uint8_t*)BASE;

    size_t size = 1000; // 2 * 1024 * 1024;
    size_t errors = 0;

    for (size_t i = 0; i < size; i++)
    {
        Sram[i] = 0x55;
    }
    Delay(10);
    for (size_t i = 0; i < size; i++)
    {
        uint8_t r = Sram[i];

        if (r != 0x55)
        {
            char buf[80] = {0};
            sprintf(buf, "%.6X (%.2X -> %.2X)\n", i, 0x55, r);
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)buf, sizeof(buf), true);

            errors++;
        }
    }

    if (errors == 0)
    {
        BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"------------OK\n", 16, true);
    }
    else
    {
        BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"------------ER\n", 16, true);
    }
}

void testSram_M()
{
    BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"\ntestSram_M ", 12, true);

    Sram = (volatile uint8_t*)BASE;

    size_t size = 2 * 1024 * 1024;
    size_t errors = 0;

    for (size_t i = 0; i < size; i++)
    {
        uint8_t expected = i % 256;
        i[Sram] = expected;
        //        for (volatile size_t i = 0; i < 100; i++)
        //            ;
        //*(volatile uint8_t*)(BASE + i) = i % 256;
    }

    Delay(10);
    for (size_t i = 0; i < size; i++)
    {
        uint8_t expected = i % 256;
        uint8_t now = Sram[i];

        //        for (volatile size_t i = 0; i < 100; i++)
        //            ;

        if (now != expected)
        {
            char buf[80] = {0};
            sprintf(buf, "%.6X (%.2X -> %.2X)\n", i, expected, now);
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)buf, sizeof(buf), true);

            errors++;
        }
    }

    if (errors == 0)
    {
        BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"------------OK\n", 16, true);
    }
    else
    {
        BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)"------------ER\n", 16, true);
    }
}

void COMMS_processMsg(void)
{
    uint8_t index, entry, *data;
    uint32_t len, i;
    uint8_t *crcStart, *crcEnd;
    uint16_t crcVal;
    uint8_t desc[BOOT_ENTRY_DESCRIPTION_SIZE];

    // Disable UART interrupts
    BSP_UART_DEBUG->IEN &= ~USART_IF_RXDATAV;

    switch (msgId)
    {
        case 'b': //

            uartReceived = 0;

            // reset message id
            msgId = 0x00;

            break;

        case 's': // set boot index

            uartReceived = 1;

            debugLen = sprintf((char*)debugStr, "\n\nNew Boot Index: ");
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            // get boot index
            index = USART_Rx(BSP_UART_DEBUG);

            debugLen = sprintf((char*)debugStr, "%d", index);
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            // test boot index
            if (index > BOOT_TABLE_SIZE)
            {
                debugLen = sprintf((char*)debugStr, "\n\nError: Boot index out of bounds!");
                BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);
                return;
            }

            // upload new program to internal flash
            BOOT_setBootIndex(index);
            BOOT_resetBootCounter();

            debugLen = sprintf((char*)debugStr, "...Done!");
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            // reset message id
            msgId = 0x00;

            break;

        case 'l': // list entries

            uartReceived = 1;

            debugLen = sprintf((char*)debugStr, "\n\nBoot Table Entries:\n");
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            for (entry = 1; entry <= BOOT_TABLE_SIZE; entry++)
            {
                debugLen = sprintf((char*)debugStr, "\n%d. ", entry);
                BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

                // check if entry is valid
                if (*(uint8_t*)(BOOT_TABLE_BASE + BOOT_getOffsetValid(entry)) != BOOT_ENTRY_ISVALID)
                {
                    debugLen = sprintf((char*)debugStr, "Not Valid!");
                    BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

                    continue;
                }

                // display description
                for (i = 0; i < BOOT_ENTRY_DESCRIPTION_SIZE; i++)
                {
                    data = (uint8_t*)(BOOT_TABLE_BASE + BOOT_getOffsetDescription(entry) + i);

                    if (*data == '\r')
                    {
                        break;
                    }

                    USART_Tx(BSP_UART_DEBUG, *data);
                }
            }

            // reset message id
            msgId = 0x00;

            break;

        case 'x': // upload to boot table

            uartReceived = 1;

            debugLen = sprintf((char*)debugStr, "\n\nBoot Index: ");
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            // get boot index
            index = USART_Rx(BSP_UART_DEBUG);

            debugLen = sprintf((char*)debugStr, "%d", index);
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            // test boot index
            if (index == 0x00)
            {
                debugLen = sprintf((char*)debugStr, "\nError: Cant override safe mode program!");
                BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);
                return;
            }
            if (index > BOOT_TABLE_SIZE)
            {
                debugLen = sprintf((char*)debugStr, "\nError: Boot index out of bounds!");
                BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);
                return;
            }

            debugLen = sprintf((char*)debugStr, "\nUpload Binary: ");
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            len = XMODEM_upload(index);

            // update entry length
            BOOT_setLen(index, len);

            // calculate actual CRC of downloaded application
            crcStart = (uint8_t*)(BOOT_TABLE_BASE + BOOT_getOffsetProgram(index));
            crcEnd = crcStart + len;
            crcVal = CRC_calc(crcStart, crcEnd);

            // update entry crc
            BOOT_setCRC(index, crcVal);

            // mark entry as valid
            BOOT_setValid(index);

            debugLen = sprintf((char*)debugStr, "\nBoot Description: ");
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            // get description
            for (i = 0; i < BOOT_ENTRY_DESCRIPTION_SIZE; i++)
            {
                desc[i] = USART_Rx(BSP_UART_DEBUG);

                USART_Tx(BSP_UART_DEBUG, desc[i]);

                if (desc[i] == '\n')
                {
                    break;
                }
            }

            // update entry description
            BOOT_programDescription(index, desc);

            // change boot index
            BOOT_setBootIndex(index);
            BOOT_resetBootCounter();

            debugLen = sprintf((char*)debugStr, "...Done!");
            BSP_UART_txBuffer(BSP_UART_DEBUG, (uint8_t*)debugStr, debugLen, true);

            // reset message id
            msgId = 0x00;

            break;

        case 'r': // Reset MCU

            SCB->AIRCR = 0x05FA0004;

            break;

        case 'z': // upload safe mode

            uartReceived = 1;

            // get boot index
            index = 0;

            // upload safe mode application
            len = XMODEM_upload(index);

            // change boot index
            BOOT_setBootIndex(index);
            BOOT_resetBootCounter();

            // reset message id
            msgId = 0x00;

            break;

        case 'S':
            while (1)
            {
                testSram_M();
                testSram_FF();
                testSram();
            }
            break;

        default:
            // reset message id
            msgId = 0x00;
            break;
    }

    // Enable UART interrupts
    USART_IntClear(BSP_UART_DEBUG, USART_IF_RXDATAV);
    BSP_UART_DEBUG->IEN |= USART_IF_RXDATAV;
}

/**
 * UART interrupt handler
 */
void BSP_UART_DEBUG_IRQHandler(void)
{
    uint8_t temp;

    // disable interrupt
    BSP_UART_DEBUG->IEN &= ~USART_IEN_RXDATAV;

    // only save message if its been processed (i.e. 0x00)
    if (msgId == 0x00)
    {
        // save message id
        msgId = BSP_UART_DEBUG->RXDATA;
    }
    else
    {
        // store in dummy variable to clear flag
        temp = BSP_UART_DEBUG->RXDATA;
    }

    // enable interrupt
    BSP_UART_DEBUG->IEN |= USART_IEN_RXDATAV;
}
