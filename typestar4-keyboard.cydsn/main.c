#include <stdbool.h>
#include <stdio.h>
#include "project.h"
#include "usbkeycodes.h"

enum
{
    ENDPOINT_KEYBOARD_IN = 4,
    ENDPOINT_KEYBOARD_OUT = 5
};

enum
{
    MODIFIER_SHIFT = 1<<0,
    MODIFIER_CONTROL = 1<<1,
    MODIFIER_SPECIAL = 1<<2,
    MODIFIER_ALT = 1<<3
};

static const uint8_t keycodes[8][8] = {
    { KEY_J,              KEY_L,      KEY_N,           KEY_P,             KEY_I,     KEY_K,            KEY_M,     KEY_O },
    { KEY_Z,              KEY_1,      KEY_3,           KEY_5,             KEY_Y,     KEY_0,            KEY_2,     KEY_4 },
    { KEY_Period,         KEY_Escape, KEY_Enter,       KEY_Delete,        KEY_Comma, KEY_Slash,        KEY_Space, 0 },
    { KEY_B,              KEY_D,      KEY_F,           KEY_H,             KEY_A,     KEY_C,            KEY_E,     KEY_G },
    { KEY_R,              KEY_T,      KEY_V,           KEY_X,             KEY_Q,     KEY_S,            KEY_U,     KEY_W },
    { KEY_7,              KEY_9,      KEY_LeftBracket, KEY_Quote,         KEY_6,     KEY_8,            KEY_Minus, KEY_Semicolon },
    { KEY_Equals,         KEY_Menu,   KEY_F7,          KEY_F3,            KEY_Tab,   KEY_RightBracket, KEY_F6,    KEY_F1 },
    { KEY_F5,             0,          0,               0,                 KEY_F4,    KEY_F2,           0,         0 },
};

static const uint8_t special_keycodes[8][8] = {
    { 0,                  0,          0,               0,                 0,         0,                0,         0 },
    { KEY_Insert,         0,          0,               0,                 0,         0,                0,         0 },
    { 0,                  0,          0,               KEY_DeleteForward, 0,         0,                0,         0 },
    { 0,                  KEY_Right,  KEY_PageDown,    0,                 KEY_Left,  0,                KEY_End,   0 },
    { KEY_PageUp,         0,          0,               KEY_Delete,        KEY_Home,  KEY_Down,         0,         KEY_Up },
    { 0,                  0,          KEY_F14,         0,                 0,         0,                0,         0 },
    { KEY_NonUSBackslash, 0,          0,               KEY_F10,           0,         KEY_NonUSHash,    KEY_F13,   KEY_F8, },
    { KEY_F12,            0,          0,               0,                 KEY_F11,   KEY_F9,           0,         0 },
};

struct usb_keyboard_data
{
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
};

static struct usb_keyboard_data keyboard_state = {};
static char screen[16];
static int cursor;

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
        char c = *s++;
        if (!c)
            return;
        if (pos == 8)
            LCD_Seek(0x3f);
        if (pos == 16)
            return;
        LCD_WriteChar(c);
        pos++;
    }
}

static void SCR_Clear(void)
{
    memset(screen, ' ', sizeof(screen));
    cursor = 0;
}

static void SCR_PutC(char c)
{
    switch (c)
    {
        case '\n':
            SCR_Clear();
            break;
            
        case '\r':
            cursor = 0;
            break;
            
        default:
            if (cursor == 15)
                SCR_Clear();
            screen[cursor++] = c;
            break;
    }
}

static void SCR_Flush(void)
{
    LCD_Clear();
    
    LCD_Seek(0x01);
    for (const char* p = screen; p != (screen+7); p++)
        LCD_WriteChar(*p);
    
    LCD_Seek(0x3f);
    for (const char* p = (screen+7); p != (screen+15); p++)
        LCD_WriteChar(*p);
    
    if (cursor < 7)
        LCD_Seek(0x01 + cursor);
    else
        LCD_Seek(0x3f - 6 + cursor);
}

static void SCR_Print(const char* s)
{
    for (;;)
    {
        char c = *s++;
        if (!c)
            break;
        SCR_PutC(c);
    }
}

static void SCR_PrintN(const char* s, uint32_t n)
{
    while (n--)
        SCR_PutC(*s++);
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
            
    for (;;)
    {
        if (!USBFS_GetConfiguration() || USBFS_IsConfigurationChanged())
        {
            LCD_Write("Waiting for USB");
            LED_Write(1);
            while (!USBFS_GetConfiguration())
                ;
            
            LCD_Write("Setting up USB");
            USBFS_CDC_Init();
            
            USBFS_EnableOutEP(ENDPOINT_KEYBOARD_OUT);
            USBFS_LoadInEP(ENDPOINT_KEYBOARD_IN, (uint8_t*) &keyboard_state, sizeof(keyboard_state));
            SCR_Clear();
            SCR_Print("Ready");
            SCR_Flush();
            LED_Write(0);
        }
    
        /* Only scan the keyboard if the master is ready. */
        
        if (USBFS_GetEPAckState(ENDPOINT_KEYBOARD_IN))
        {            
            /* Probe the modifier keys. */

            static struct usb_keyboard_data newstate;
            
            KBDPROBE_Write(0xff);
            CyDelayUs(150); /* Time for the capacitors to charge */
            uint8_t mods = MODIFIERS_Read();
            newstate.modifiers = 0;
            if (mods & MODIFIER_SHIFT)
                newstate.modifiers |= MOD_LeftShift;
            if (mods & MODIFIER_CONTROL)
                newstate.modifiers |= MOD_LeftControl;
            if (mods & MODIFIER_ALT)
                newstate.modifiers |= MOD_LeftGui;
            bool special = mods & MODIFIER_SPECIAL;
            
            /* Probe the keyboard matrix. */
        
            memcpy(&keyboard_state.keys, &newstate.keys, sizeof(newstate.keys));
            uint8_t allkeys = 0;
            for (unsigned row=0; row<8; row++)
            {
                KBDPROBE_Write(1 << row);
                CyDelayUs(100);
                uint8_t keys = KBDSENSE_Read();
                allkeys |= keys;

                for (unsigned column=0; column<8; column++)
                {
                    uint8_t usbkeycode = (special ? special_keycodes : keycodes)[row][column];
                    unsigned i = 0;
                    
                    /* Search for the key in the set. */
                    
                    while (i<sizeof(newstate.keys))
                    {
                        if (newstate.keys[i] == usbkeycode)
                            break;
                        i++;
                    }
                    
                    if (keys & (1<<column))
                    {
                        /* Pressed --- add it to the keyboard set. */
                    
                        if (i != sizeof(newstate.keys))
                        {
                            /* Already in the set, so do nothing. */
                        }
                        else
                        {
                            /* Add it. */
                            
                            for (unsigned i=0; i<sizeof(newstate.keys); i++)
                            {
                                if (newstate.keys[i] == 0)
                                {
                                    newstate.keys[i] = usbkeycode;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        /* Released --- remove it from the keyboard set. */
                        
                        if (i != sizeof(newstate.keys))
                            newstate.keys[i] = 0;
                    }
                }
            }
            
            /* In case of bugs, check to make sure that if no keys are pressed, the set is reset. */
            
            if (allkeys == 0)
                memset(newstate.keys, 0, sizeof(newstate.keys));
            
            /* Detect changes. */
            
            if (memcmp(&newstate, &keyboard_state, sizeof(newstate)) != 0)
            {
                USBFS_LoadInEP(ENDPOINT_KEYBOARD_IN, (uint8_t*) &newstate, sizeof(newstate));
                keyboard_state = newstate;
                
                static int led = 0;
                led = !led;
                //LED_Write(led);

                CyDelay(20); // to prevent keybounce
            }
        }
        
        /* Handle the serial input. */

        if (USBFS_DataIsReady())
        {
            while (USBFS_DataIsReady())
            {
                char inputbuffer[64];
                int count = USBFS_GetAll((uint8_t*) inputbuffer);
                SCR_PrintN(inputbuffer, count);
            }
            SCR_Flush();
        }
    }
}
