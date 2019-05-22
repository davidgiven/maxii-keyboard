#include <stdbool.h>
#include <stdio.h>
#include "project.h"

static uint8 Keyboard_Data[8] = {};

static void lcd_write_byte(bool rs, uint8_t data)
{
    CyPins_SetPin(LCDCTRL_E);

    if (rs)
        CyPins_SetPin(LCDCTRL_RS);
    else
        CyPins_ClearPin(LCDCTRL_RS);

    LCDDATA_Write(data);
    CyPins_ClearPin(LCDCTRL_RW);

    CyDelayUs(2);
    CyPins_ClearPin(LCDCTRL_E);
    CyDelayUs(2);
}

static void LCD_Clear(void)
{
    lcd_write_byte(false, 0x01); /* clear */
    CyDelay(2);
}

static void LCD_Init(void)
{
    CyDelay(200);

    lcd_write_byte(false, 0x38); /* FUNCTION_SET + 8BIT */
    CyDelay(5);
    lcd_write_byte(false, 0x38); /* FUNCTION_SET + 8BIT */
    CyDelay(1);
    lcd_write_byte(false, 0x38); /* FUNCTION_SET + 8BIT */
    CyDelay(1);
        
    lcd_write_byte(false, 0x0e); /* display control: display on, cursor on, blinking cursor pos off */
    CyDelayUs(40);

    LCD_Clear();
}

static void LCD_WriteChar(char c)
{
    lcd_write_byte(true, c);
    CyDelayUs(40);
}

static void LCD_Seek(uint8_t pos)
{
    lcd_write_byte(false, 0x80 | pos); /* Set DDRAM address */
    CyDelayUs(40);
}

static void LCD_Write(const char* s)
{
    uint8_t pos = 1;
    LCD_Clear();
    LCD_Seek(1);
    for (;;)
    {
        if (pos == 8)
            LCD_Seek(0x3f);
        if (pos == 16)
            return;
        char c = *s++;
        if (!c)
            return;
        LCD_WriteChar(c);
        pos++;
    }
}

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */
    USBFS_Start(0, USBFS_DWR_POWER_OPERATION);

    LCD_Init();
    
    struct key
    {
        int pin;
        char code;
    };
    
    static const struct key keyboard_pins[] =
    {
        { KBD_0, 'a' },
        { KBD_1, 'b' },
        { KBD_2, 'c' }, // return
        { KBD_3, 'd' },
        { KBD_4, 'e' },
        { KBD_5, 'f' },
        { KBD_6, 'g' }, // always on?
        { KBD_7, 'h' }, // shift
        { KBD_8, 'i' },
        { KBD_9, 'j' },
        { KBD_10, 'k' },
        { KBD_11, 'l' }, // repeat
        { KBD_12, 'm' },
        { KBD_13, 'n' },
        { KBD_14, 'o' },
        { KBD_15, 'p' },
        { KBD_16, 'q' },
    };
    #define PINS (sizeof(keyboard_pins)/sizeof(*keyboard_pins))
    
    struct kbdstate
    {
        bool pressed[PINS][PINS];
    };
    
    static struct kbdstate oldstate = {};
    static struct kbdstate newstate = {};
    
    for (;;)
    {
        if (!USBFS_GetConfiguration() || USBFS_IsConfigurationChanged())
        {
            LCD_Write("Waiting for USB");
            while (!USBFS_GetConfiguration())
                ;
            
            LCD_Write("Setting up USB");
            USBFS_CDC_Init();
            USBFS_EnableOutEP(4);
            USBFS_LoadInEP(1, Keyboard_Data, 8);
            LCD_Write("Ready");
        }
        
        for (unsigned y=0; y<PINS; y++)
        {
            CyPins_SetPin(keyboard_pins[y].pin);
            for (unsigned x=0; x<PINS; x++)
            {
                if (x == y)
                    continue;
                
                newstate.pressed[y][x] = CyPins_ReadPin(keyboard_pins[x].pin);
            }
            CyPins_ClearPin(keyboard_pins[y].pin);
        }
        
        if (memcmp(&newstate, &oldstate, sizeof(struct kbdstate)) != 0)
        {
            CyPins_SetPin(LED_0);
            while (!USBFS_CDCIsReady())
                ;
            USBFS_PutString("\r\nchanged\r\n");
            for (unsigned y=0; y<PINS; y++)
            {
                static char buffer[80];
                char* p = buffer;
                
                *p++ = keyboard_pins[y].code;
                *p++ = ':';
                for (unsigned x=0; x<PINS; x++)
                    *p++ = newstate.pressed[y][x] ? keyboard_pins[x].code : '.';
                *p++ = '\n';
                *p++ = '\r';
                *p++ = 0;
                while (!USBFS_CDCIsReady())
                    ;
                USBFS_PutString(buffer);
            }
            
            oldstate = newstate;
            CyPins_ClearPin(LED_0);
        }
    }
}
