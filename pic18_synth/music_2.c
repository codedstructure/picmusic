/* Compile options:  -ml (Large code model) */

//#include <stdio.h>
#include <stdlib.h>
#include <timers.h>
#include <p18f252.h>

#pragma config WDT=OFF, OSC=ECIO, PWRT=ON

#define NOTES 8
unsigned long tune[NOTES] = { 43893, 65765, 58591, 49269, 55302, 43893, 87786, 52198} ;

unsigned long osc1;
unsigned long incr;

void timer_isr(void);

#pragma code low_vector=0x18
void low_interrupt(void)
{
  _asm GOTO timer_isr _endasm
}

#pragma code
#pragma interruptlow timer_isr
void timer_isr(void)
{
    osc1 += incr;
    INTCONbits.TMR0IF = 0;
    TMR0L = 192;
}

void main (void)
{
  char j = 0;
  char k = 0;
  unsigned long i = 0;
  unsigned char volume = 100;
  unsigned char value =0;
  long pos = 0;
  incr = tune[0];

  OpenTimer0(TIMER_INT_ON & T0_SOURCE_INT & T0_8BIT);
  INTCONbits.GIE = 1;

  TRISC = 0;
  while (1)
  {
    if ( osc1 > (1L << 24) )
    {
        osc1 = 0;
        volume -= 1;
    }
    value = (osc1 * volume) >> 24;
    LATC = value;
    if (volume == 0)
    {
       long x = 0;
       while (x++ < 0xFFFF);
       volume = 100;
       pos = (pos + 1) & 0x7;
       incr = tune[pos];
    }
  }
}
