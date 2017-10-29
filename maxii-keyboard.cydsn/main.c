/* maxii-keyboard firmware
 * (C) 2017 David Given
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "project.h"
#include "usbkeycodes.h"

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
        KEY_LeftControl, 
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

static void read_keypresses(void)
{
    static const uint8 keycodes[9][8] = {
        { KEY_9, KEY_0,              KEY_LeftBracket,  KEY_Quote,     0,          KEY_P, KEY_Semicolon, KEY_Slash },
        { KEY_8, KEY_Minus,          KEY_RightBracket, KEY_NonUSHash, 0,          KEY_O, KEY_L,         KEY_Period },
        { KEY_7, KEY_Equals,         KEY_Delete,       KEY_Grave,     KEY_4,      KEY_I, KEY_K,         KEY_Comma },
        { KEY_6, KEY_NonUSBackslash, KEY_Enter,        KEY_LeftGUI,   KEY_5,      KEY_U, KEY_J,         KEY_M },
        { 0,     0,                  0,                KEY_Up,        KEY_Escape, KEY_Q, KEY_A,         KEY_Z },
        { KEY_G, KEY_H,              0,                KEY_Right,     KEY_1,      KEY_W, KEY_S,         KEY_X },
        { KEY_T, KEY_B,              0,                KEY_Space,     KEY_2,      KEY_E, KEY_D,         KEY_C },
        { KEY_Y, KEY_N,              0,                KEY_Left,      KEY_3,      KEY_R, KEY_F,         KEY_V },
        { 0,     0,                  0,                KEY_Down,      0,          0,     0,             0 }
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

static void usbwait(void)
{
    while (!USBFS_GetEPAckState(1))
        ;
}

int main(void)
{
    CyGlobalIntEnable;
    LedReg_Write(1);
    //UART_Start();
    ProbeCounter_Start();
    ProbeInterrupt_StartEx(&ProbeInterrupt);
    USBFS_Start(0, USBFS_DWR_VDDD_OPERATION);

    //UART_PutString("GO\r");
    LedReg_Write(0);

    for (;;)
    {
        while (readptr == writeptr)
        {
            if (!USBFS_GetConfiguration() || USBFS_IsConfigurationChanged())
            {
                //UART_PutString("Waiting for USB configuration\r");
                while (!USBFS_GetConfiguration())
                    ;
                USBFS_EnableOutEP(2);
                //UART_PutString("USB configuration done\r");
            }
        }

        char buffer[32];
        struct queue_entry* entry = &queue[readptr];
        sprintf(buffer, "k=%d o=%d m=%02x\r", entry->keycode, entry->pressed, (uint8)~ModifierReg_Read());
        //UART_PutString(buffer);
        LedReg_Write(0);
        
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
        usbwait();
       
        readptr = (readptr+1) & (QUEUE_SIZE-1);
    }
}
