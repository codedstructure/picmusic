/* Compile options:  -ml (Large code model) */

//#include <stdio.h>
#include <stdlib.h>
#include <timers.h>
#include <usart.h>
#include <p18f252.h>

#pragma config WDT=OFF, OSC=ECIO, PWRT=ON, LVP=OFF

#define NOTES 8
unsigned long tune[NOTES] = { 43893, 65765, 58591, 49269, 55302, 43893, 87786, 52198} ;

#pragma udata mbuf=0x100
char midibuffer[16];
#pragma udata
short tail = 0x100;
short head = 0x100;

unsigned long osc1;
unsigned long incr;
char j = 0;
char k = 0;
unsigned long i = 0;
unsigned char volume = 100;
unsigned char value =0;
long pos = 0;

void timer_isr(void);
void usart_isr(void);

#pragma code low_vector=0x18
void low_interrupt(void)
{
  _asm GOTO usart_isr _endasm
}
#pragma code

#pragma interrupt timer_isr
void timer_isr(void)
{
    osc1 += incr;
    if ( osc1 & 0xFF000000 )
    {
        osc1 = 0;
        volume --;
    }

    value = osc1 >> 16;
    LATB = value;

    INTCONbits.TMR0IF = 0;
    TMR0L = 192;
}

#pragma code high_vector=0x8
void high_interrupt(void)
{
  _asm GOTO timer_isr _endasm
}
#pragma code

#pragma interruptlow usart_isr
void usart_isr(void)
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
  char rstat = 0x90;
  char rx;
  char note = 0;
  char vel = 0;
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
    if (rx & 0x80)
    {
        rstat = rx;
        switch (rstat & 0xF0)
        {
            case 0x80:
            case 0x90:
				note = getMidiByte();
                vel  = getMidiByte();
                break;
        }
    }
	else
    {
		switch (rstat & 0xF0) // ignore channel number
        {
            case 0x80:
            case 0x90: note = rx; vel = getMidiByte(); break;
        }
    }

    incr = 1000L*note; // tune[pos];
  }
}
