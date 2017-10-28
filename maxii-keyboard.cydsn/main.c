/* maxii-keyboard firmware
 * (C) 2017 David Given
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "project.h"

#define QUEUE_SIZE 32

struct queue_entry
{
    uint8 x;
    uint8 y;
    uint8 status;
};

static struct queue_entry queue[QUEUE_SIZE];
static volatile int readptr = 0;
static volatile int writeptr = 0;
static uint8 senses[16] = {};

static uint8 Keyboard_Data[8] = {};

static CY_ISR(ProbeInterrupt)
{
    uint8 probe = ProbeReg_Read();
    uint8 sense = SenseReg_Read();
    uint8 changed = senses[probe] ^ sense;
            
    for (int y=0; y<8; y++)
    {
        if (changed & (1<<y))
        {
            if (writeptr != ((readptr-1) & (QUEUE_SIZE-1)))
            {
                struct queue_entry* entry = &queue[writeptr];
                entry->x = probe;
                entry->y = y;
                entry->status = !!(sense & (1<<y));
                
                writeptr = (writeptr+1) & (QUEUE_SIZE-1);
            }
            else
                LedReg_Write(true);
        }
    }

    senses[probe] = sense;
}

static void usbwait(void)
{
    while (!USBFS_bGetEPAckState(1))
        ;
}

int main(void)
{
    LedReg_Write(1);
    UART_Start();
    ProbeCounter_Start();
    ProbeInterrupt_StartEx(&ProbeInterrupt);
    USBFS_Start(0, USBFS_DWR_VDDD_OPERATION);
    USBFS_EnableOutEP(2);
    CyGlobalIntEnable;

    UART_PutString("Waiting for USB\r");
    while (!USBFS_bGetConfiguration())
        ;
    USBFS_LoadInEP(1, Keyboard_Data, 8);
    UART_PutString("GO\r");

    for (;;)
    {
        while (readptr == writeptr)
            ;

        char buffer[32];
        struct queue_entry* entry = &queue[readptr];
        sprintf(buffer, "x=%d y=%d o=%d\r", entry->x, entry->y, entry->status);
        
        usbwait();
        Keyboard_Data[2] = 20;
        USBFS_LoadInEP(1, Keyboard_Data, 8);

        usbwait();
        Keyboard_Data[2] = 0;
        USBFS_LoadInEP(1, Keyboard_Data, 8);
        
        UART_PutString(buffer);
        LedReg_Write(0);
        
        readptr = (readptr+1) & (QUEUE_SIZE-1);
    }
}
