/* Compile options:  -ml (Large code model) */

//#include <stdio.h>
#include <stdlib.h>
#include <p18f252.h>

#pragma config WDT=OFF, OSC=ECIO, PWRT=ON

#define NOTES 4
unsigned long tune[NOTES] = { 200000, 250000, 235000, 200000} ;

void main (void)
{
  char j = 0;
  char k = 0;

  unsigned long i = 0;
  unsigned char volume = 100;
  unsigned char value =0;
  long pos = 0;

  TRISC = 0;
  while (1)
  {
    i = (i+tune[pos]);
    if ( i > (1L << 24) )
    {
        i = 0;
        volume -= 1;
    }
    value = (i * volume) >> 24;
    LATC = value;
    if (volume == 0)
    {
       long x = 0;
       while (x++ < 0xFFFF);
       volume = 100;
       pos = (pos + 1) % NOTES;
    }
  }
}
