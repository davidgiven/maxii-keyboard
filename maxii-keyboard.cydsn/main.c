/* maxii-keyboard firmware
 * (C) 2017 David Given
 */

#include <stdio.h>
#include "project.h"

int main(void)
{
    UART_Start();
    CyGlobalIntEnable; /* Enable global interrupts. */

    UART_PutString("Hello, world!\n");
    
    for (;;)
    {
        uint8 c = UART_GetChar();
        switch (c)
        {
            case 0: /* nothing to read */
                break;
            
            default:
            {
                char buffer[16];
                sprintf(buffer, "%d ", c);
                UART_PutString(buffer);
            }
        }
    }
}
