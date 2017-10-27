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
    bool onoff;
};

static struct queue_entry queue[QUEUE_SIZE];
static volatile int readptr = 0;
static volatile int writeptr = 0;

void SenseInterrupt_Interrupt_InterruptCallback(void)
{
    static bool phase = 0;
    static uint8 probe = 1;
    static uint8 senses[10] = {};
    
    switch (phase)
    {
        case false:
            probe++;
            if (probe == 11)
                probe = 1;
            ProbeReg_Write(probe);
            break;
            
        case true:
        {
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
                        entry->onoff = !!(sense & (1<<y));
                        
                        writeptr = (writeptr+1) & (QUEUE_SIZE-1);
                    }
                    else
                        LedReg_Write(1);
                }
            }
            
            senses[probe] = sense;
            break;
        }
    }
    
    phase = !phase;
}

int main(void)
{
    UART_Start();
    SenseInterrupt_Start();
    CyGlobalIntEnable;

    UART_PutString("GO\r");

    for (;;)
    {
        while (readptr == writeptr)
            ;

        char buffer[32];
        struct queue_entry* entry = &queue[readptr];
        sprintf(buffer, "x=%d y=%d o=%d\r", entry->x, entry->y, entry->onoff);
        UART_PutString(buffer);
        LedReg_Write(0);
        
        readptr = (readptr+1) & (QUEUE_SIZE-1);
    }
    
#if 0
    uint8 oldprobe = 0;
    for (;;)
    {
        uint8 probe = ProbeReg_Read();
        if (probe == oldprobe)
            continue;
        oldprobe = probe;

        if (probe == 0)
            UART_PutString("\r");
            
        uint8 sense = SenseReg_Read();
        sprintf(buffer, "%02x ", sense);
        UART_PutString(buffer);
    }
#endif
}
