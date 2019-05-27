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

static void print(const char* s)
{
    while (!USBFS_CDCIsReady())
        ;
    USBFS_PutString(s);
}

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */
    USBFS_Start(0, USBFS_DWR_POWER_OPERATION);

    LCD_Init();
    
    struct kbdstate
    {
        uint8_t modifiers;
        uint8_t keys[8];
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
    
        /* Probe the modifier keys. */
        
        KBDPROBE_Write(0xff);
        CyDelayUs(150); /* Time for the capacitors to charge */
        newstate.modifiers = MODIFIERS_Read();

        /* Probe the keyboard matrix. */
        
        for (unsigned y=0; y<8; y++)
        {
            KBDPROBE_Write(1 << y);
            CyDelayUs(100);
            newstate.keys[y] = KBDSENSE_Read();
        }
        
        /* Detect changes. */
        
        if (memcmp(&newstate, &oldstate, sizeof(newstate)) != 0)
        {
            static char buffer[80];
            
            print("changed");
            
            snprintf(buffer, sizeof(buffer), "%x\r\n", newstate.modifiers);
            print(buffer);
            
            for (unsigned y=0; y<8; y++)
            {
                snprintf(buffer, sizeof(buffer), "%d: %x\r\n", y, newstate.keys[y]);
                print(buffer);
            }
            
            oldstate = newstate;
        }
    }
}
