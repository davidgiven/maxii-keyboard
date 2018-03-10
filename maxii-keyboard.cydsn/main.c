/* maxii-keyboard firmware
 * (C) 2017 David Given
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "project.h"
#include "usbkeycodes.h"

#define KEY_Magic 255
#define QUEUE_SIZE 32

struct queue_entry
{
    uint8 keycode;
    bool pressed;
};

static struct queue_entry queue[QUEUE_SIZE];
static volatile int readptr = 0;
static volatile int writeptr = 0;
static uint8 modifiers;

static uint8 Keyboard_Data[8] = {};

static void post_keyevent(uint8 keycode, bool pressed)
{
    if (writeptr != ((readptr-1) & (QUEUE_SIZE-1)))
    {
        struct queue_entry* entry = &queue[writeptr];
        entry->keycode = keycode;
        entry->pressed = pressed;
        
        writeptr = (writeptr+1) & (QUEUE_SIZE-1);
    }
    else
        LedReg_Write(true);
}

static void read_modifiers(void)
{
    static const uint8 keycodes[8] = {
        KEY_LeftShift,
        KEY_Tab, 
        KEY_LeftAlt, 
        KEY_CapsLock, 
        0, 
        0, 
        0, 
        0
    };
    
    static uint8 oldsense = 0;
    uint8 sense = ~ModifierReg_Read(); /* active high */
    uint8 changed = sense ^ oldsense;
    
    for (int y=0; y<8; y++)
    {
        if (changed & (1<<y))
        {
            uint8 keycode = keycodes[y];
            if (keycode)
                post_keyevent(keycode, sense & (1<<y));
        }
    }
    
    oldsense = sense;
}

static uint8 alt_keycode(uint8 normal_keycode)
{
    switch (normal_keycode)
    {
        case KEY_W: return KEY_Up;
        case KEY_A: return KEY_Left;
        case KEY_S: return KEY_Down;
        case KEY_D: return KEY_Right;
        case KEY_R: return KEY_PageUp;
        case KEY_F: return KEY_PageDown;
        case KEY_Q: return KEY_Home;
        case KEY_E: return KEY_End;
        case KEY_1: return KEY_F1;
        case KEY_2: return KEY_F2;
        case KEY_3: return KEY_F3;
        case KEY_4: return KEY_F4;
        case KEY_5: return KEY_F5;
        case KEY_6: return KEY_F6;
        case KEY_7: return KEY_F7;
        case KEY_8: return KEY_F8;
        case KEY_9: return KEY_F9;
        case KEY_0: return KEY_F10;
        case KEY_Minus: return KEY_F11;
        case KEY_Equals: return KEY_F12;
        
        case KEY_LeftShift:
        case KEY_RightShift:
        case KEY_LeftAlt:
        case KEY_RightAlt:
        case KEY_LeftGUI:
        case KEY_RightGUI:
        case KEY_Menu:
            return normal_keycode;
    }
    return 0;
}
        
static void read_keypresses(void)
{
    static const uint8 keycodes[9][8] = {
        { KEY_9, KEY_0,              KEY_LeftBracket,  KEY_Quote,     0,          KEY_P, KEY_Semicolon, KEY_Slash },
        { KEY_8, KEY_Minus,          KEY_RightBracket, KEY_NonUSHash, 0,          KEY_O, KEY_L,         KEY_Period },
        { KEY_7, KEY_Equals,         KEY_Insert,       KEY_Grave,     KEY_4,      KEY_I, KEY_K,         KEY_Comma },
        { KEY_6, KEY_NonUSBackslash, KEY_Enter,        KEY_Magic,     KEY_5,      KEY_U, KEY_J,         KEY_M },
        { 0,     0,                  0,                KEY_LeftAlt,   KEY_Escape, KEY_Q, KEY_A,         KEY_Z },
        { KEY_G, KEY_H,              0,                KEY_Menu,      KEY_1,      KEY_W, KEY_S,         KEY_X },
        { KEY_T, KEY_B,              0,                KEY_Space,     KEY_2,      KEY_E, KEY_D,         KEY_C },
        { KEY_Y, KEY_N,              0,                KEY_LeftGUI,   KEY_3,      KEY_R, KEY_F,         KEY_V },
        { 0,     KEY_Delete,         KEY_Enter,        KEY_RightAlt,  0,          0,     0,             0 }
    };

    static uint8 senses[16] = {};
    uint8 probe = ProbeReg_Read();
    uint8 sense = SenseReg_Read();
    uint8 changed = senses[probe] ^ sense;

    for (int y=0; y<8; y++)
    {
        if (changed & (1<<y))
        {
            uint8 keycode = keycodes[probe][y];
            if (keycode)
                post_keyevent(keycode, sense & (1<<y));
        }
    }

    senses[probe] = sense;
}

static CY_ISR(ProbeInterrupt)
{
    read_modifiers();
    read_keypresses();
}

int main(void)
{
    CyGlobalIntEnable;
    LedReg_Write(1);
    UART_Start();
    ProbeCounter_Start();
    ProbeInterrupt_StartEx(&ProbeInterrupt);
    USBFS_Start(0, USBFS_DWR_VDDD_OPERATION);

    UART_PutString("GO\r");
    LedReg_Write(0);

    for (;;)
    {
        if (!USBFS_GetConfiguration() || USBFS_IsConfigurationChanged())
        {
            UART_PutString("Waiting for USB configuration\r");
            while (!USBFS_GetConfiguration())
                ;
            USBFS_EnableOutEP(2);
            USBFS_LoadInEP(1, Keyboard_Data, 8);
            UART_PutString("USB configuration done\r");
        }

        if ((readptr != writeptr) && USBFS_GetEPAckState(1))
        {
            char buffer[32];
            struct queue_entry* entry = &queue[readptr];
            //sprintf(buffer, "k=%d o=%d m=%02x\r", entry->keycode, entry->pressed, (uint8)~ModifierReg_Read());
            UART_PutString(buffer);
            LedReg_Write(0);
            
            static bool special_modifier = 0;
            if (entry->keycode == KEY_Magic)
                special_modifier = entry->pressed;
            else
            {
                if (special_modifier)
                    entry->keycode = alt_keycode(entry->keycode);
                if (entry->keycode)
                {
                    if (entry->pressed)
                    {
                        switch (entry->keycode)
                        {
                            case KEY_LeftControl: modifiers |= MOD_LeftControl; break;
                            case KEY_LeftShift:   modifiers |= MOD_LeftShift;   break;
                            case KEY_LeftAlt:     modifiers |= MOD_LeftAlt;     break;
                            case KEY_LeftGUI:     modifiers |= MOD_LeftGui;     break;
                        }
                        
                        for (int i=2; i<8; i++)
                            if (Keyboard_Data[i] == 0)
                            {
                                Keyboard_Data[i] = entry->keycode;
                                break;
                            }
                    }
                    else
                    {
                        switch (entry->keycode)
                        {
                            case KEY_LeftControl: modifiers &= ~MOD_LeftControl; break;
                            case KEY_LeftShift:   modifiers &= ~MOD_LeftShift;   break;
                            case KEY_LeftAlt:     modifiers &= ~MOD_LeftAlt;     break;
                            case KEY_LeftGUI:     modifiers &= ~MOD_LeftGui;     break;
                        }
                        
                        for (int i=2; i<8; i++)
                            if (Keyboard_Data[i] == entry->keycode)
                            {
                                Keyboard_Data[i] = 0;
                                break;
                            }            
                    }
                    Keyboard_Data[0] = modifiers;
                    USBFS_LoadInEP(1, Keyboard_Data, 8);
                }
            }
           
            readptr = (readptr+1) & (QUEUE_SIZE-1);
        }
    }
}
