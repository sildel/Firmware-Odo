/////////////////////////////////////////////////////////////////
#include "usb.h"
#include "usb_function_cdc.h"
#include "HardwareProfile.h"
#include "GenericTypeDefs.h"
#include "Compiler.h"
#include "usb_config.h"
#include "usb_device.h"
/////////////////////////////////////////////////////////////////
#pragma config PLLDIV   = 3         // (12 MHz crystal on PICDEM FS USB board)
#pragma config CPUDIV   = OSC1_PLL2   
#pragma config USBDIV   = 2         // Clock source from 96MHz PLL/2
#pragma config FOSC     = HSPLL_HS
#pragma config FCMEN    = OFF
#pragma config IESO     = OFF
#pragma config PWRT     = OFF
#pragma config BOR      = ON
#pragma config BORV     = 3
#pragma config VREGEN   = ON      //USB Voltage Regulator
#pragma config WDT      = OFF
#pragma config WDTPS    = 32768
#pragma config MCLRE    = ON
#pragma config LPT1OSC  = OFF
#pragma config PBADEN   = OFF
//#pragma config CCP2MX   = ON
#pragma config STVREN   = ON
#pragma config LVP      = OFF
//#pragma config ICPRT    = OFF       // Dedicated In-Circuit Debug/Programming
#pragma config XINST    = OFF       // Extended Instruction Set
#pragma config CP0      = OFF
#pragma config CP1      = OFF
//#pragma config CP2      = OFF
//#pragma config CP3      = OFF
#pragma config CPB      = OFF
//#pragma config CPD      = OFF
#pragma config WRT0     = OFF
#pragma config WRT1     = OFF
//#pragma config WRT2     = OFF
//#pragma config WRT3     = OFF
#pragma config WRTB     = OFF       // Boot Block Write Protection
#pragma config WRTC     = OFF
//#pragma config WRTD     = OFF
#pragma config EBTR0    = OFF
#pragma config EBTR1    = OFF
//#pragma config EBTR2    = OFF
//#pragma config EBTR3    = OFF
#pragma config EBTRB    = OFF
/////////////////////////////////////////////////////////////////
#pragma udata
/////////////////////////////////////////////////////////////////
char USB_In_Buffer[64];
char USB_Out_Buffer[64];
/////////////////////////////////////////////////////////////////
BOOL stringPrinted;
volatile BOOL buttonPressed;
volatile BYTE buttonCount;
/////////////////////////////////////////////////////////////////
unsigned int odoCounter;
unsigned char saveKm;
unsigned int delta;
union{
    unsigned int value;
    unsigned char bytes[2];
}diff;
union {
    unsigned long value;
    unsigned char bytes[4];
} km;
/////////////////////////////////////////////////////////////////
static void InitializeSystem(void);
void ProcessIO(void);
void USBDeviceTasks(void);
void YourHighPriorityISRCode();
void YourLowPriorityISRCode();
void BlinkUSBStatus(void);
void UserInit(void);
void InitHardware(void);
void InitSoftware(void);
/////////////////////////////////////////////////////////////////
#define REMAPPED_RESET_VECTOR_ADDRESS		0x1000
#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x1008
#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x1018
/////////////////////////////////////////////////////////////////
extern void _startup(void); // See c018i.c in your C18 compiler dir
/////////////////////////////////////////////////////////////////
#pragma code REMAPPED_RESET_VECTOR = REMAPPED_RESET_VECTOR_ADDRESS

void _reset(void) {
    _asm goto _startup _endasm
}
/////////////////////////////////////////////////////////////////
#pragma code REMAPPED_HIGH_INTERRUPT_VECTOR = REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS

void Remapped_High_ISR(void) {
    _asm goto YourHighPriorityISRCode _endasm
}
/////////////////////////////////////////////////////////////////
#pragma code REMAPPED_LOW_INTERRUPT_VECTOR = REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS

void Remapped_Low_ISR(void) {
    _asm goto YourLowPriorityISRCode _endasm
}
/////////////////////////////////////////////////////////////////	
#pragma code HIGH_INTERRUPT_VECTOR = 0x08

void High_ISR(void) {
    _asm goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS _endasm
}
/////////////////////////////////////////////////////////////////
#pragma code LOW_INTERRUPT_VECTOR = 0x18

void Low_ISR(void) {
    _asm goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS _endasm
}
#pragma code
/////////////////////////////////////////////////////////////////	
#pragma interrupt YourHighPriorityISRCode

void YourHighPriorityISRCode() {
    USBDeviceTasks();

    if (INTCONbits.TMR0IF) {
        TMR0H = 0x48;
        TMR0L = 0xE5;

        delta = odoCounter;
        diff.value = delta;
        odoCounter = 0;
        if(delta!=0){
            km.value += delta;
            saveKm = 1;
        }

        LATEbits.LATE1 = !LATEbits.LATE1;

        INTCONbits.TMR0IF = 0;
    }

    if (INTCONbits.INT0IF) {
        odoCounter++;

        INTCONbits.INT0IF = 0;
    }
}
/////////////////////////////////////////////////////////////////
#pragma interruptlow YourLowPriorityISRCode

void YourLowPriorityISRCode() {

}
/////////////////////////////////////////////////////////////////
#pragma code
/////////////////////////////////////////////////////////////////

void main(void) {
    InitializeSystem();

    USBDeviceAttach();

    while (1) {
        ProcessIO();
        if (saveKm && delta>2) {
            LATE |= 0x01;
            saveKm = 0;

            EECON1bits.EEPGD = 0; // access to eeprom
            EECON1bits.WREN = 1; // write enable

            EEADR = 0; // address to write
            EEDATA = km.bytes[0]; // data to write
            INTCONbits.GIE = 0; // disable interrupts
            EECON2 = 0x55; // sequence 1
            EECON2 = 0xAA; // sequence 2
            EECON1bits.WR = 1; // trigger write
            INTCONbits.GIE = 1; // enable interrupts

            while (!PIR2bits.EEIF); // wait until write ends
            PIR2bits.EEIF = 0; // clear write flag

            EEADR = 1; // address to write
            EEDATA = km.bytes[1]; // data to write
            INTCONbits.GIE = 0; // disable interrupts
            EECON2 = 0x55; // sequence 1
            EECON2 = 0xAA; // sequence 2
            EECON1bits.WR = 1; // trigger write
            INTCONbits.GIE = 1; // enable interrupts

            while (!PIR2bits.EEIF); // wait until write ends
            PIR2bits.EEIF = 0; // clear write flag

            EEADR = 2; // address to write
            EEDATA = km.bytes[2]; // data to write
            INTCONbits.GIE = 0; // disable interrupts
            EECON2 = 0x55; // sequence 1
            EECON2 = 0xAA; // sequence 2
            EECON1bits.WR = 1; // trigger write
            INTCONbits.GIE = 1; // enable interrupts

            while (!PIR2bits.EEIF); // wait until write ends
            PIR2bits.EEIF = 0; // clear write flag

            EEADR = 3; // address to write
            EEDATA = km.bytes[3]; // data to write
            INTCONbits.GIE = 0; // disable interrupts
            EECON2 = 0x55; // sequence 1
            EECON2 = 0xAA; // sequence 2
            EECON1bits.WR = 1; // trigger write
            INTCONbits.GIE = 1; // enable interrupts

            while (!PIR2bits.EEIF); // wait until write ends
            PIR2bits.EEIF = 0; // clear write flag

            LATE &= 0xFE;
        }
    }
}
/////////////////////////////////////////////////////////////////

static void InitializeSystem(void) {
    ADCON1 |= 0x0F;

    UserInit();

    USBDeviceInit();
}
/////////////////////////////////////////////////////////////////

void InitHardware() {
    TRISD = 0xFF;
    PORTEbits.RDPU = 1;

    TRISB = 0x01;
    PORTB = 0x00;

    RCONbits.IPEN = 0;

    TRISE = 0x0C;
    PORTE = PORTE | 0x01;
    PORTE = PORTE & 0x7D;

    T0CON = 0x06;
    TMR0H = 0x48;
    TMR0L = 0xE5;

    INTCON = 0xF0;
    INTCON2 = 0x04;
    RCON = 0x80;
    T0CON |= 0x80;
}
/////////////////////////////////////////////////////////////////

void InitSoftware() {
    odoCounter = 0;
    delta = 0;

    saveKm = 0;

    EECON1bits.EEPGD = 0; // access to eeprom

    EEADR = 0; // address to read
    EECON1bits.RD = 1; // trigger read
    km.bytes[0] = EEDATA; // save readed data

    EEADR = 1; // address to read
    EECON1bits.RD = 1; // trigger read
    km.bytes[1] = EEDATA; // save readed data

    EEADR = 2; // address to read
    EECON1bits.RD = 1; // trigger read
    km.bytes[2] = EEDATA; // save readed data

    EEADR = 3; // address to read
    EECON1bits.RD = 1; // trigger read
    km.bytes[3] = EEDATA; // save readed data
}
/////////////////////////////////////////////////////////////////

void UserInit(void) {
    InitSoftware();

    InitHardware();
}
/////////////////////////////////////////////////////////////////

void ProcessIO(void) {
    BYTE numBytesRead;
    unsigned char tosend[10], mil, cen, dec, uni;

    if ((USBDeviceState < CONFIGURED_STATE) || (USBSuspendControl == 1)) return;

    if (mUSBUSARTIsTxTrfReady()) {
        numBytesRead = getsUSBUSART(USB_In_Buffer, 64);
        if (numBytesRead > 0) {
            BYTE i;

            for (i = 0; i < numBytesRead; i++) {
                switch (USB_In_Buffer[i]) {
                    case 'a':
                    case 'A':
                        if (T0CONbits.TMR0ON) {
                            T0CON &= 0x7F;
                        } else {
                            T0CON |= 0x80;
                        }
                        LATEbits.LATE0 = !LATEbits.LATE0;

                        tosend[0] = 'S';

                        if (PORTEbits.RE0) {
                            tosend[1] = '1';
                        } else {
                            tosend[1] = '0';
                        }

                        if (PORTEbits.RE1) {
                            tosend[2] = '1';
                        } else {
                            tosend[2] = '0';
                        }

                        if (PORTEbits.RE2) {
                            tosend[3] = '1';
                        } else {
                            tosend[3] = '0';
                        }

                        tosend[4] = '\0';

                        putsUSBUSART((char*) tosend);

                        break;

                    case 'b':
                    case 'B':
                        LATEbits.LATE1 = !LATEbits.LATE1;

                        tosend[0] = 'S';

                        if (PORTEbits.RE0) {
                            tosend[1] = '1';
                        } else {
                            tosend[1] = '0';
                        }

                        if (PORTEbits.RE1) {
                            tosend[2] = '1';
                        } else {
                            tosend[2] = '0';
                        }

                        if (PORTEbits.RE2) {
                            tosend[3] = '1';
                        } else {
                            tosend[3] = '0';
                        }

                        tosend[4] = '\0';

                        putsUSBUSART((char*) tosend);

                        break;

                    case 'c':
                    case 'C':
                        km.value = 0;
                        break;

                    case 'e':
                    case 'E':
                        tosend[0] = km.bytes[0];
                        tosend[1] = km.bytes[1];
                        tosend[2] = km.bytes[2];
                        tosend[3] = km.bytes[3];
                        tosend[4] = diff.bytes[0];
                        tosend[5] = diff.bytes[1];

                        putUSBUSART((char*) tosend, 6);
                        break;

                    case 'd':
                    case 'D':

                        mil = km.value / 1000;
                        cen = (km.value % 1000) / 100;
                        dec = (km.value % 100) / 10;
                        uni = km.value % 10;

                        tosend[0] = mil + '0';
                        tosend[1] = cen + '0';
                        tosend[2] = dec + '0';
                        tosend[3] = uni + '0';
                        tosend[4] = '\r';
                        tosend[5] = '\0';

                        putsUSBUSART((char*) tosend);
                        break;
                }
            }
        }
    }
    CDCTxService();
}
/////////////////////////////////////////////////////////////////

void USBCBSuspend(void) {

}
/////////////////////////////////////////////////////////////////

void USBCBWakeFromSuspend(void) {
    // If clock switching or other power savings measures were taken when
    // executing the USBCBSuspend() function, now would be a good time to
    // switch back to normal full power run mode conditions.  The host allows
    // a few milliseconds of wakeup time, after which the device must be
    // fully back to normal, and capable of receiving and processing USB
    // packets.  In order to do this, the USB module must receive proper
    // clocking (IE: 48MHz clock must be available to SIE for full speed USB
    // operation).
}
/////////////////////////////////////////////////////////////////***/

void USBCB_SOF_Handler(void) {
    // No need to clear UIRbits.SOFIF to 0 here.
    // Callback caller is already doing that.

    //This is reverse logic since the pushbutton is active low
    if (buttonPressed == sw2) {
        if (buttonCount != 0) {
            buttonCount--;
        } else {
            //This is reverse logic since the pushbutton is active low
            buttonPressed = !sw2;

            //Wait 100ms before the next press can be generated
            buttonCount = 100;
        }
    } else {
        if (buttonCount != 0) {
            buttonCount--;
        }
    }
}
/////////////////////////////////////////////////////////////////

void USBCBErrorHandler(void) {

}
/////////////////////////////////////////////////////////////////

void USBCBCheckOtherReq(void) {
    USBCheckCDCRequest();
}
/////////////////////////////////////////////////////////////////

void USBCBStdSetDscHandler(void) {

}
/////////////////////////////////////////////////////////////////

void USBCBInitEP(void) {
    CDCInitEP();
}
/////////////////////////////////////////////////////////////////

void USBCBSendResume(void) {
    static WORD delay_count;

    USBResumeControl = 1; // Start RESUME signaling

    delay_count = 1800U; // Set RESUME line for 1-13 ms
    do {
        delay_count--;
    }    while (delay_count);
    USBResumeControl = 0;
}
/////////////////////////////////////////////////////////////////

BOOL USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, WORD size) {
    switch (event) {
        case EVENT_CONFIGURED:
            USBCBInitEP();
            break;
        case EVENT_SET_DESCRIPTOR:
            USBCBStdSetDscHandler();
            break;
        case EVENT_EP0_REQUEST:
            USBCBCheckOtherReq();
            break;
        case EVENT_SOF:
            USBCB_SOF_Handler();
            break;
        case EVENT_SUSPEND:
            USBCBSuspend();
            break;
        case EVENT_RESUME:
            USBCBWakeFromSuspend();
            break;
        case EVENT_BUS_ERROR:
            USBCBErrorHandler();
            break;
        case EVENT_TRANSFER:
            Nop();
            break;
        default:
            break;
    }
    return TRUE;
}
/////////////////////////////////////////////////////////////////
