/* Compile options:  -ml (Large code model) */

//#include <stdio.h>
#include <stdlib.h>
#include <timers.h>
#include <usart.h>
#include <p18f252.h>

#pragma config WDT=OFF, OSC=HSPLL, PWRT=ON, LVP=OFF

typedef unsigned char bool;
#define false 0
#define true 1

void timer_isr();
void usart_isr();
bool getMidiByte(char* n);
void shiftNoteOn(char n);
void shiftNoteOff(char n);
void noteOn(char n);
void noteOff(char n);
void processMsg();
void processMods();
void applyBend();

#define NOTES 128
rom unsigned short long tune[NOTES] = {
      0,        0,        0,        0,
   5185,     5493,     5820,     6166,
   6532,     6921,     7332,     7768,
   8230,     8719,     9238,     9787,
  10369,    10986,    11639,    12331,
  13064,    13841,    14664,    15536,
  16460,    17439,    18476,    19575,
  20739,    21972,    23278,    24662,
  26129,    27683,    29329,    31073,
  32920,    34878,    36952,    39149,
  41477,    43944,    46557,    49325,
  52258,    55365,    58658,    62146,
  65841,    69756,    73904,    78298,
  82954,    87887,    93113,    98650,
 104516,   110731,   117315,   124291,
 131682,   139512,   147808,   156597,
 165909,   175774,   186226,   197300,
 209032,   221461,   234630,   248582,
 263364,   279024,   295616,   313194,
 331817,   351548,   372452,   394599,
 418064,   442923,   469260,   497164,
 526727,   558048,   591231,   626388,
 663635,   703096,   744905,   789199,
 836127,   885846,   938521,   994328,
1053454,  1116096,  1182462,  1252775,
1327269,  1406193,  1489809,  1578398,
1672254,  1771692,  1877042,  1988657,
2106908,  2232192,  2364925,  2505550,
2654538,  2812385,  2979618,  3156796,
3344509,  3543383,  3754084,  3977313,
4213817,  4464383,  4729849,  5011101,
5309077,  5624771,  5959237,  6313592} ;

/*
>>> z = []
>>> for i in range(256):
...  z.append(int(math.sin(i*2*math.pi/256)*32767))
...
... ## and for sawtooth...
... 
>>> def sawtooth(order = 7):
...   z = []
...   for i in range(256):
...     rad = i*2*math.pi/256.0
...     point = 0.0
...     for t in range(1, order+1):
...       point += (1.0/t)*math.sin(t*rad)
...     z.append(int(0.5+point*16384))
...   return z
...
>>> for i in range(32):
...  for j in range(8):
...   print "%6d, "%z[i*8+j],
...  print
...
*/
rom short sintab[256] = {
     0,     785,    1570,    2354,    3137,    3917,    4695,    5471,
  6243,    7011,    7775,    8535,    9289,   10038,   10780,   11517,
 12246,   12968,   13682,   14388,   15085,   15773,   16451,   17120,
 17778,   18426,   19062,   19687,   20301,   20902,   21490,   22065,
 22627,   23176,   23710,   24231,   24736,   25227,   25703,   26163,
 26607,   27035,   27447,   27843,   28221,   28583,   28928,   29255,
 29564,   29856,   30129,   30385,   30622,   30841,   31041,   31222,
 31385,   31529,   31654,   31759,   31846,   31913,   31961,   31990,
 32000,   31990,   31961,   31913,   31846,   31759,   31654,   31529,
 31385,   31222,   31041,   30841,   30622,   30385,   30129,   29856,
 29564,   29255,   28928,   28583,   28221,   27843,   27447,   27035,
 26607,   26163,   25703,   25227,   24736,   24231,   23710,   23176,
 22627,   22065,   21490,   20902,   20301,   19687,   19062,   18426,
 17778,   17120,   16451,   15773,   15085,   14388,   13682,   12968,
 12246,   11517,   10780,   10038,    9289,    8535,    7775,    7011,
  6243,    5471,    4695,    3917,    3137,    2354,    1570,     785,
     0,    -784,   -1569,   -2353,   -3136,   -3916,   -4694,   -5470,
 -6242,   -7010,   -7774,   -8534,   -9288,  -10037,  -10779,  -11516,
-12245,  -12967,  -13681,  -14387,  -15084,  -15772,  -16450,  -17119,
-17777,  -18425,  -19061,  -19686,  -20300,  -20901,  -21489,  -22064,
-22626,  -23175,  -23709,  -24230,  -24735,  -25226,  -25702,  -26162,
-26606,  -27034,  -27446,  -27842,  -28220,  -28582,  -28927,  -29254,
-29563,  -29855,  -30128,  -30384,  -30621,  -30840,  -31040,  -31221,
-31384,  -31528,  -31653,  -31758,  -31845,  -31912,  -31960,  -31989,
-31999,  -31989,  -31960,  -31912,  -31845,  -31758,  -31653,  -31528,
-31384,  -31221,  -31040,  -30840,  -30621,  -30384,  -30128,  -29855,
-29563,  -29254,  -28927,  -28582,  -28220,  -27842,  -27446,  -27034,
-26606,  -26162,  -25702,  -25226,  -24735,  -24230,  -23709,  -23175,
-22626,  -22064,  -21489,  -20901,  -20300,  -19686,  -19061,  -18425,
-17777,  -17119,  -16450,  -15772,  -15084,  -14387,  -13681,  -12967,
-12245,  -11516,  -10779,  -10037,   -9288,   -8534,   -7774,   -7010,
 -6242,   -5470,   -4694,   -3916,   -3136,   -2353,   -1569,    -784};

rom short sawtab[256] = {
     0,    2809,    5585,    8293,   10903,   13385,   15712,   17858,
 19805,   21534,   23034,   24296,   25317,   26096,   26640,   26956,
 27058,   26962,   26687,   26254,   25686,   25009,   24247,   23427,
 22572,   21707,   20853,   20032,   19261,   18556,   17928,   17386,
 16936,   16582,   16322,   16154,   16071,   16065,   16127,   16243,
 16402,   16589,   16790,   16992,   17181,   17345,   17472,   17554,
 17583,   17553,   17460,   17303,   17084,   16803,   16467,   16081,
 15654,   15192,   14708,   14209,   13707,   13212,   12733,   12280,
 11859,   11477,   11140,   10851,   10612,   10422,   10281,   10185,
 10131,   10111,   10120,   10150,   10193,   10241,   10284,   10316,
 10328,   10314,   10267,   10184,   10061,    9896,    9687,    9437,
  9148,    8822,    8464,    8081,    7678,    7262,    6841,    6423,
  6014,    5621,    5250,    4908,    4598,    4324,    4088,    3891,
  3732,    3610,    3522,    3463,    3429,    3415,    3412,    3416,
  3419,    3414,    3395,    3355,    3289,    3194,    3064,    2899,
  2697,    2457,    2183,    1875,    1538,    1176,     795,     401,
     0,    -400,    -794,   -1175,   -1537,   -1874,   -2182,   -2456,
 -2696,   -2898,   -3063,   -3193,   -3288,   -3354,   -3394,   -3413,
 -3418,   -3415,   -3411,   -3414,   -3428,   -3462,   -3521,   -3609,
 -3731,   -3890,   -4087,   -4323,   -4597,   -4907,   -5249,   -5620,
 -6013,   -6422,   -6840,   -7261,   -7677,   -8080,   -8463,   -8821,
 -9147,   -9436,   -9686,   -9895,  -10060,  -10183,  -10266,  -10313,
-10327,  -10315,  -10283,  -10240,  -10192,  -10149,  -10119,  -10110,
-10130,  -10184,  -10280,  -10421,  -10611,  -10850,  -11139,  -11476,
-11858,  -12279,  -12732,  -13211,  -13706,  -14208,  -14707,  -15191,
-15653,  -16080,  -16466,  -16802,  -17083,  -17302,  -17459,  -17552,
-17582,  -17553,  -17471,  -17344,  -17180,  -16991,  -16789,  -16588,
-16401,  -16242,  -16126,  -16064,  -16070,  -16153,  -16321,  -16581,
-16935,  -17385,  -17927,  -18555,  -19260,  -20031,  -20852,  -21706,
-22571,  -23426,  -24246,  -25008,  -25685,  -26253,  -26686,  -26961,
-27057,  -26955,  -26639,  -26095,  -25316,  -24295,  -23033,  -21533,
-19804,  -17857,  -15711,  -13384,  -10902,   -8292,   -5584,   -2808};

#define EMPTY(X) (X ## head == X ## tail)
#define NOT_EMPTY(X) (X ## head != X ## tail)
#define FULL(X) (((X ## head+1) & 63) == (X ## tail & 63))
#define NOT_FULL(X) ((((X ## head)+1) & 63) != ((X ## tail) & 63))
#define LO_BYTE(X) (*((unsigned char*)&(X)))
#define HI_BYTE_OF_32(X) (*(((unsigned char*)&(X))+3))
#define HI_BYTE_OF_24(X) (*(((unsigned char*)&(X))+2))
#define HI_BYTE_OF_16(X) (*(((unsigned char*)&(X))+1))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#pragma udata mbuf=0x100
char midibuffer[64];
char msgbuffer[2];
#pragma udata dbuf = 0x200
char dacbuffer[128];
#pragma udata
short miditail = 0x100;
short midihead = 0x100;
short dachead  = 0x200;
short dactail  = 0x200;
char* const MSG_ADDR = &msgbuffer[0];
char* msg = &msgbuffer[0];

// running status
char rstat = 0;

unsigned long sampleCount = 0;
unsigned long envpos = 0;
unsigned short long osc1 = 0;
unsigned short long incr1 = 0;
unsigned short long newincr1 = 0;
unsigned short long osc2 = 0;
unsigned short long incr2 = 0;
unsigned short long newincr2 = 0;
unsigned short long modincr = 0;
short envvolume, modvolume, release_volume, env_delta;
char j = 0;
char k = 0;
char volume = 100;
char mainvolume = 100;
char attack = 10;
char release = 10;
short bend = 0;
float bendupincr = 0;
float benddownincr = 0;
unsigned char value =0;
char note[6] = {0,0,0,0,0,0}; // 6 note buffer
char shape = 0;
bool twoosc = false;
bool in_release = true;

enum Waveform
{
    WAVE_SAWTOOTH,
    WAVE_PULSE,
    WAVE_TRIANGLE,
    WAVE_SINE,
    WAVE_END
};

enum Waveform waveform = WAVE_SAWTOOTH;

#pragma code low_vector=0x18
void low_interrupt()
{
    _asm GOTO usart_isr _endasm
}
#pragma code

#pragma interrupt timer_isr
void timer_isr()
{
    char temp;
    // This takes approx 40 cycles including ISR overhead.
    // Could probably be written in asm...
    if ( NOT_EMPTY(dac) )
    {
        LATB = *((char*)dactail);
        dactail++;
        dactail &= 0xFF7F;
    }
    else
    {
        // underrun condition
        LATC = 2;
    }        

    PIR1bits.TMR2IF = 0;
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

    if ( NOT_FULL(midi) )
    {
        *((char*)midihead) = rx;
        midihead++;
        midihead &= 0xFF3F;
    }
    else
    {
        // MIDI Rx overflow - indicate
//        LATC |= 4;
        // drop oldest byte. Will undoubtedly cause corruption...
        miditail++;
        miditail &= 0xFF3F;
    }
}

bool getMidiByte(char* result)
{
    char temp;
    if ( EMPTY(midi) ) return false;
    temp = *((char*)miditail);
    miditail++;
    miditail &= 0xFF3F;
    *result = temp;
    return true;
}

bool getDacByte(char* result)
{
    char temp;
    if ( EMPTY(dac) ) return false;
    temp = *((char*)dactail);
    dactail++;
    dactail &= 0xFF3F;
    *result = temp;
    return true;
}

void main (void)
{
    bool insysex = false;
    unsigned char rx;
    unsigned char bytecount, origbytecount;
    char dacbyte;
    long i, j;

    incr1 = tune[0];

    INTCON2bits.TMR0IP = 1;
    IPR1bits.RCIP = 0;
    RCONbits.IPEN = 1;
  
    TRISB = 0; //0xFF;  // start high impedance
    TRISC = 0x80;  // RX input, others output
//    LATC = 1;


    LATC = 1; // RED
    for (i=0; i<100000; i++);
    LATC = 2; // RED
    for (i=0; i<100000; i++);
    
    for (i=0; i<500; i++)
    {
        for (j=0; j<100; j++) LATC = 1;
        for (j=0; j<100; j++) LATC = 2;
    }

    // 100kHz before postscaler.
    // set to 33.3kHz  (300 cycles per sample, of which approx 40 are timer ISR)
    PR2 = 99;
    OpenTimer2(TIMER_INT_ON & T2_PS_1_1 & T2_POST_1_3);
    
    // 39 @ 20MHz, 79 @ 40MHz
    OpenUSART(USART_TX_INT_OFF & USART_RX_INT_ON &
              USART_ASYNCH_MODE & USART_EIGHT_BIT &
              USART_CONT_RX & USART_BRGH_HIGH,
              79);
  
    INTCONbits.GIEH = 1;  
    INTCONbits.GIEL = 1;
  
    while (1)
    {
        if ( NOT_FULL(dac) )
        {
            long dacdword;
            short wave;
            unsigned char temp;
            sampleCount++;
            if ((LO_BYTE(sampleCount)) == 0)
            {
                processMods();
            }

            // modincr 24 bits
            // wave & modvolume 16 bits

            osc1 += modincr;
            temp = HI_BYTE_OF_24(osc1);
            switch (waveform)
            {
                case WAVE_SINE:
                    wave = sintab[temp];
                    break;
                case WAVE_SAWTOOTH:
                    if (shape == 0)
                        wave = sawtab[temp];
                    else
                        wave = ((signed short)temp-128) * 255;
                    break;
                case WAVE_PULSE:
                    wave = (temp > shape) ? 32000 : -32000;
                    break;
                default:
                    wave = 0;
                    break;
            }
            dacdword = (long)wave * modvolume;
            dacbyte = HI_BYTE_OF_32(dacdword);
            dacbyte += 0x80; // offset to remove large transitions

            // TODO: wait for zero crossing if not currently sending.

            *((char*)dachead) = dacbyte;
            dachead++;
            dachead &= 0xFF7F;
/*
            if (LATC != 2)
            {
                if (dacbyte == 0x80)
                {
                    LATC = 0;
                }
                else
                {
                    LATC = 1;
                }
            }
*/
        }
        else
        {
            // buffer is full. Send green
            //LATC = 1;
        }    

        if (getMidiByte(&rx) == false) continue;

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
//    LATC ^= 3; // toggle low two bits

    // status is in rstat, msg bytes are in msgbuffer[0], msgbuffer[1]
    if (( rstat == 0x90 || rstat == 0x80 ) &&
        msg[0]>20 && msg[0]<117) // note on/off (in range)
    {
        if (rstat == 0x90 && msg[1] != 0)
        {
            noteOn(msg[0]);
        }
        else
        {
            noteOff(msg[0]);
        }
    }
    else if ( rstat == 0xB0 )
    {
        switch ( msg[0] )
        {
            case 7: // volume
                mainvolume = msg[1];
                break;
            case 71: // type
                if (msg[1] < WAVE_END) waveform = msg[1];
                break;
            case 77: // shape
                shape = msg[1];
                break;
            case 72: // release
                release = max(1, msg[1]);
                break;
            case 73: // attack
                attack = max(1, msg[1]);
                break;
            case 123: // all notes off
//                LATC &= ~8;
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

void processMods()
{
   
    LATC = 0; // clear any overflow indicator
    if (envpos++ == 0)
    {
        envvolume = modvolume;
        if (in_release)
        {
            env_delta = (float)envvolume/release;
        }
        else
        {
            env_delta = (32767.0-envvolume)/attack;
        }    
    }

    if (in_release)
    {
        if (envvolume > env_delta)
        {
            envvolume -= env_delta;
        }
        else
        {
            envvolume = 0;
        }
    }    
    else if (envpos <= attack)
    {
        if (envvolume < 32767-env_delta)
        {
            envvolume += env_delta;
        }
        else
        {
            envvolume = 32767;
        }
    }

    modvolume = (int)((float)envvolume * ((float)mainvolume)/127.0);

    modincr = newincr1;
}    

void applyBend()
{
    // work out proportion bend is of note difference and apply
    if (bend > 0)
    {
        newincr1 = tune[note[0]] + (long)((float)bend * bendupincr);
    }
    else if (bend < 0)
    {
        newincr1 = tune[note[0]] + (long)((float)bend * benddownincr);
    }    
    else
    {
        newincr1 = tune[note[0]];
    }    
}

void shiftNoteIn(char n)
{
    if (note[0] != n)
    {
        note[5] = note[4];
        note[4] = note[3];
        note[3] = note[2];
        note[2] = note[1];
        note[1] = note[0];
        note[0] = n;
    }
}

void shiftNoteOff(char n)
{
    if (note[0] == n)
    {
        note[0] = note[1];
        note[1] = note[2];
        note[2] = note[3];
        note[3] = note[4];
        note[4] = note[5];
        note[5] = 0;
    }
    else if (note[1] == n)
    {
        note[1] = note[2];
        note[2] = note[3];
        note[3] = note[4];
        note[4] = note[5];
        note[5] = 0;
    }
    else if (note[2] == n)
    {
        note[2] = note[3];
        note[3] = note[4];
        note[4] = note[5];
        note[5] = 0;
    }
    else if (note[3] == n)
    {
        note[3] = note[4];
        note[4] = note[5];
        note[5] = 0;
    }
    else if (note[2] == n)
    {
        note[4] = note[5];
        note[5] = 0;
    }
    else if (note[3] == n)
    {
        note[5] = 0;
    }
}

void noteOn(char n)
{
    in_release = false;

    // should make it configurable whether a new note re-triggers the envelope or not.
    // or have two envelopes, one of each type...
    if (note[0] == 0)
    {
        envpos = 0;
    }

    shiftNoteIn(n);

//    LATC |= 8;
    TRISB = 0;
    newincr1 = tune[n];
    // pre-compute bend values to reduce time taken in applyBend
    // (on basis that pitch bend data can be much more frequent
    // than note data)
    bendupincr = 1.0 / 8192.0 * (tune[n+1] - tune[n]);
    benddownincr = 1.0 / 8192.0 * (tune[n] - tune[n-1]);
    if ( twoosc )
    {
        newincr2 = tune[n] - 100;
    }
    applyBend();
}

void noteOff(char n)
{
    shiftNoteOff(n);

    if (note[0] == 0)
    {
//        LATC &= ~8;
        //newincr1 = 0;
        //TRISB = 0xFF;
        in_release = true;
        envpos = 0;
        release_volume = envvolume;
    }
    else
    {
        noteOn(note[0]);
    }
}
