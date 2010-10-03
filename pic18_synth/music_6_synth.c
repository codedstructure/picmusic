/* Compile options:  -ml (Large code model) */

//#include <stdio.h>
#include <stdlib.h>
#include <timers.h>
#include <usart.h>
#include <p18f252.h>

void timer_isr();
void usart_isr();
void processMsg();
char getMidiByte();
void applyBend();

#pragma config WDT=OFF, OSC=ECIO, PWRT=ON, LVP=OFF

typedef unsigned char bool;
#define false 0
#define true 1

#define NOTES 128
rom unsigned long tune[NOTES] = {
   432,     458,     485,     514,
   544,     577,     611,     647,
   686,     727,     770,     816,
   864,     915,     970,    1028,
  1089,    1153,    1222,    1295,
  1372,    1453,    1540,    1631,
  1728,    1831,    1940,    2055,
  2177,    2307,    2444,    2589,
  2743,    2906,    3079,    3262,
  3456,    3662,    3880,    4110,
  4355,    4614,    4888,    5179,
  5487,    5813,    6159,    6525,
  6913,    7324,    7759,    8221,
  8710,    9227,    9776,   10357,
 10973,   11626,   12317,   13050,
 13826,   14648,   15519,   16441,
 17419,   18455,   19552,   20715,
 21947,   23252,   24634,   26099,
 27651,   29295,   31037,   32883,
 34838,   36910,   39105,   41430,
 43893,   46504,   49269,   52198,
 55302,   58591,   62075,   65766,
 69677,   73820,   78209,   82860,
 87787,   93007,   98538,  104397,
110605,  117182,  124150,  131532,
139353,  147640,  156419,  165720,
175574,  186014,  197075,  208794,
221209,  234363,  248299,  263064,
278706,  295279,  312837,  331439,
351148,  372028,  394150,  417588,
442419,  468726,  496598,  526127} ;

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
char midibuffer[64];
char msgbuffer[2];
#pragma udata
short tail = 0x100;
short head = 0x100;
char* const MSG_ADDR = &msgbuffer[0];
char* msg = &msgbuffer[0];

// running status
char rstat = 0;

unsigned long osc1 = 0;
unsigned long incr1 = 0;
unsigned long newincr1 = 0;
unsigned long osc2 = 0;
unsigned long incr2 = 0;
unsigned long newincr2 = 0;
char j = 0;
char k = 0;
unsigned long i = 0;
unsigned char volume = 100;
unsigned char newvolume = 100;
short bend = 0;
float bendupincr = 0;
float benddownincr = 0;
unsigned char value =0;
char note = 0;
bool sawtooth = false;
bool twoosc = true;

#pragma code low_vector=0x18
void low_interrupt()
{
    _asm GOTO usart_isr _endasm
}
#pragma code

#pragma interrupt timer_isr
void timer_isr()
{
    osc1 += incr1;
    if ( osc1 & 0xFFF00000 )
    {
        osc1 = 0;
    }
    // can't just be in above or would never start.
    if ( osc1 == 0 )
    {
        incr1 = newincr1;
        volume = newvolume;
    }

    if ( twoosc )
    {
        osc2 += incr2;
        if ( osc2 & 0xFFF00000 )
        {
            incr2 = newincr2;
            osc2 = 0;
        }
    }    

    if ( sawtooth )
    {
        // max value is less: value here goes up to FF,
        // but max(sintab) is only 120, hence >> 9 rather
        // than 8.
        if ( twoosc )
        {
            value = ((osc1 >> 12)/2 + (osc2 >> 12)/2);
        }
        else
        {
            value = osc1 >> 12;
        }
    }    
    else
    {
        if ( twoosc )
        {
            value = (sintab[osc1 >> 12]/2) + (sintab[osc2 >> 12]/2);
        }
        else
        {
            value = sintab[osc1 >> 12];
        }
    }

    LATB = ((short)value * (short)volume) >> 8;

    INTCONbits.TMR0IF = 0;
    TMR0L = 100;
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
    char rx = RCREG;
    PIR1bits.RCIF = 0;
    if ( RCSTAbits.OERR ) RCSTAbits.OERR = 0;

    LATC ^= 1;
    if ( ((head+1) & 63) != (tail & 63) )
    {
        *((char*)head) = rx;
        head++;
        head &= 0xFF3F;
    }
    else
    {
        // MIDI Rx overflow - indicate
        LATC |= 2;
        // drop oldest byte. Will undoubtedly cause corruption...
        tail++;
        tail &= 0xFF3F;
    }    
}

char getMidiByte()
{
    char temp;
    while ( tail == head );
    temp = *((char*)tail);
    tail++;
    tail &= 0xFF3F;
    return temp;
}

void main (void)
{
    bool insysex = false;
    unsigned char rx;
    unsigned char bytecount, origbytecount;
    incr1 = tune[0];

    INTCON2bits.TMR0IP = 1;
    IPR1bits.RCIP = 0;
    RCONbits.IPEN = 1;
  
    TRISB = 0xFF;  // start high impedance
    TRISC = 0x80;  // RX input, others output
    LATC = 0;

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
    // status is in rstat, msg bytes are in msgbuffer[0], msgbuffer[1]
    if ( rstat == 0x90 || rstat == 0x80 ) // note on/off
    {
        if (rstat == 0x90 && msg[1] != 0)
        {
            note = msg[0];
            LATC |= 4;
            TRISB = 0;
            newincr1 = tune[note];
            // pre-compute bend values to reduce time taken in applyBend
            // (on basis that pitch bend data can be much more frequent
            // than note data)
            bendupincr = 1.0 / 8192.0 * (tune[note+1] - tune[note]);
            benddownincr = 1.0 / 8192.0 * (tune[note] - tune[note-1]);
            if ( twoosc )
            {
                newincr2 = tune[note] - 100;
            }
            applyBend();
        }
        else if ( msg[0] == note )
        {
            LATC &= ~4;
            TRISB = 0xFF;
        }
    }
    else if ( rstat == 0xB0 )
    {
        switch ( msg[0] )
        {
            case 7: // volume
                newvolume = msg[1];
                break;
            case 71: // type
                sawtooth = msg[1];
                break;
            case 123: // all notes off
                LATC &= ~4;
                TRISB = 0xFF;
                break;
            default:
                break;
        }        
    }
    else if ( rstat == 0xE0 )
    {
        // pitch bend
        bend = ((short)msg[0] + ((short)msg[1] << 7)) - 8192;
        applyBend();
    }
    msg[0] = 0;
    msg[1] = 0;
}


void applyBend()
{
    // work out proportion bend is of note difference and apply
    if (bend > 0)
    {
        newincr1 = tune[note] + (long)((float)bend * bendupincr);
    }
    else if (bend < 0)
    {
        newincr1 = tune[note] + (long)((float)bend * benddownincr);
    }    
    else
    {
        newincr1 = tune[note];
    }    
}    