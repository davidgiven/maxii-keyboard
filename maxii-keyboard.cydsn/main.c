/* maxii-keyboard firmware
 * (C) 2017 David Given
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "project.h"

#define PROBES 16

static uint8 states[PROBES];

static char buffer[16];


int main(void)
{
    UART_Start();
    CyGlobalIntEnable;

    UART_PutString("GO\r");
    
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
}
