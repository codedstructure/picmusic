/* Compile options:  -ml (Large code model) */

//#include <stdio.h>
#include <stdlib.h>
#include <timers.h>
#include <usart.h>
#include <p18f252.h>

#pragma config WDT=OFF, OSC=ECIO, PWRT=ON, LVP=OFF

typedef unsigned char bool;
#define false 0
#define true 1

#define NOTES 8
unsigned long tune[NOTES] = { 43893, 65765, 58591, 49269, 55302, 43893, 87786, 52198} ;

/*
>>> z = []
>>> for i in range(256):
...   z.append(int(sin(i*2*math.pi / 256)*80.0+80))
...
>>> for i in range(32):
...  for j in range(8):
...    print "%3d, "%z[i*8+j],
...  print
...
*/

rom unsigned char sintab[256] = {
 60,   61,   62,   64,   65,   67,   68,   70,
 71,   73,   74,   76,   77,   78,   80,   81,
 82,   84,   85,   86,   88,   89,   90,   92,
 93,   94,   95,   96,   98,   99,  100,  101,
102,  103,  104,  105,  106,  107,  108,  109,
109,  110,  111,  112,  112,  113,  114,  114,
115,  115,  116,  116,  117,  117,  118,  118,
118,  119,  119,  119,  119,  119,  119,  119,
120,  119,  119,  119,  119,  119,  119,  119,
118,  118,  118,  117,  117,  116,  116,  115,
115,  114,  114,  113,  112,  112,  111,  110,
109,  109,  108,  107,  106,  105,  104,  103,
102,  101,  100,   99,   98,   96,   95,   94,
 93,   92,   90,   89,   88,   86,   85,   84,
 82,   81,   80,   78,   77,   76,   74,   73,
 71,   70,   68,   67,   65,   64,   62,   61,
 60,   58,   57,   55,   54,   52,   51,   49,
 48,   46,   45,   43,   42,   41,   39,   38,
 37,   35,   34,   33,   31,   30,   29,   27,
 26,   25,   24,   23,   21,   20,   19,   18,
 17,   16,   15,   14,   13,   12,   11,   10,
 10,    9,    8,    7,    7,    6,    5,    5,
  4,    4,    3,    3,    2,    2,    1,    1,
  1,    0,    0,    0,    0,    0,    0,    0,
  0,    0,    0,    0,    0,    0,    0,    0,
  1,    1,    1,    2,    2,    3,    3,    4,
  4,    5,    5,    6,    7,    7,    8,    9,
 10,   10,   11,   12,   13,   14,   15,   16,
 17,   18,   19,   20,   21,   23,   24,   25,
 26,   27,   29,   30,   31,   33,   34,   35,
 37,   38,   39,   41,   42,   43,   45,   46,
 48,   49,   51,   52,   54,   55,   57,   58};

#pragma udata mbuf=0x100
char midibuffer[16];
char msgbuffer[2];
#pragma udata
short tail = 0x100;
short head = 0x100;
char* const MSG_ADDR = &msgbuffer[0];
char* msg = &msgbuffer[0];

// running status
char rstat = 0;

unsigned long osc1;
unsigned long incr;
char j = 0;
char k = 0;
unsigned long i = 0;
unsigned char volume = 100;
unsigned char value =0;
long pos = 0;

void timer_isr();
void usart_isr();
void processMsg();
char getMidiByte();

#pragma code low_vector=0x18
void low_interrupt()
{
    _asm GOTO usart_isr _endasm
}
#pragma code

#pragma interrupt timer_isr
void timer_isr()
{
    osc1 += incr;
    if ( osc1 & 0xFF000000 )
    {
        osc1 = 0;
        volume --;
    }

    value = osc1 >> 16;
    LATB = sintab[value];

    INTCONbits.TMR0IF = 0;
    TMR0L = 192;
}

#pragma code high_vector=0x8
void high_interrupt()
{
    _asm GOTO timer_isr _endasm
}
#pragma code

#pragma interruptlow usart_isr
void usart_isr()
{
    char rx = ReadUSART();
    PIR1bits.RCIF = 0;
    LATC ^= 1;
    if ( ((head+1) & 0xF) != (tail & 0xF) )
    {
        *((char*)head) = rx;
        head++;
        head &= 0xFF0F;
    }
}

char getMidiByte()
{
    char temp;
    while ( tail == head );
    LATC ^= 2;
    temp = *((char*)tail);
    tail++;
    tail &= 0xFF0F;
    return temp;
}

void main (void)
{
    bool insysex = false;
    unsigned char rx;
    unsigned char bytecount, origbytecount;
    incr = tune[0];

    INTCON2bits.TMR0IP = 1;
    IPR1bits.RCIP = 0;
    RCONbits.IPEN = 1;
  
    TRISB = 0;
    TRISC = 0x80;  //RX input, others output
    LATB = 0x55;
    LATC = 1;
  
    OpenTimer0(TIMER_INT_ON & T0_SOURCE_INT & T0_8BIT);
    OpenUSART(USART_TX_INT_OFF & USART_RX_INT_ON &
              USART_ASYNCH_MODE & USART_EIGHT_BIT &
              USART_CONT_RX & USART_BRGH_HIGH,
              39);
  
    INTCONbits.GIEH = 1;  
    INTCONbits.GIEL = 1;
  
    while (1)
    {
        rx = getMidiByte();
        if ( (insysex && rx != 0xF7) ||
             rx >= 0xF8 )
        {
            // sysex or system realtime - ignore
            continue;
        }
        else if ( rx == 0xF7 )
        {
            // end of sysex
            insysex = false;
        }
        else if ( rx == 0xF0 )
        {
            // start of sysex
            insysex = true;
        }
        else if ( rx >= 0xF0 )
        {
            // system common - clear running status
            rstat = 0;
        }
        else if ( rx >= 0x80 )
        {
            rstat = rx;
            bytecount = 2;
            if ( rx >= 0xC0 && rx <= 0xDF )
                bytecount = 1;
            origbytecount = bytecount;
            msg = MSG_ADDR;
        }
        else // databyte
        {
            if (rstat)
            {
                *msg++ = rx;
                if ( --bytecount == 0 )
                {
                    bytecount = origbytecount; // reset counter for new set of databytes with running status
                    msg = MSG_ADDR;
                    processMsg();
                }
            } // otherwise no running status - ignore databyte
        }
    }
}

void processMsg()
{
    static char note = 0;

    // status is in rstat, msg bytes are in msgbuffer[0], msgbuffer[1]
    if ( rstat == 0x90 || rstat == 0x80 ) // note on/off
    {
        if (rstat == 0x90 && msg[1] != 0)
        {
            note = msg[0];
            LATC |= 4;
            incr = 3000L*note; // tune[pos];
        }
        else if ( msg[0] == note )
        {
            LATC &= ~4;
        }
    }
}
