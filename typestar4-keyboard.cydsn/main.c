#include <stdbool.h>
#include <stdio.h>
#include "project.h"

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
    CyDelay(100);

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

    LCD_Init();
    
    char buf[16];
    uint8_t i = 100;
    for (;;)
    {
        snprintf(buf, sizeof(buf), "Hello world!%d", i++);
        LCD_Write(buf);
        CyDelay(100);
    }
}
