
#include "p33fxxxx.h"
#include "dsp.h"
//#include "test.h"

// Sampling Control
#define Fosc		8000000
#define Fcy			(Fosc/2)
#define Fs   		39062 //.5
#define SAMPPRD    (Fcy/Fs)-1
#define NUMSAMP     128

#define EMPTY(X) (X ## head == X ## tail)
#define NOT_EMPTY(X) (X ## head != X ## tail)
#define FULL(X) (((X ## head+1) & 63) == (X ## tail & 63))
#define NOT_FULL(X) ((((X ## head)+1) & 63) != ((X ## tail) & 63))
#define HI_WORD_OF_32(X) (*(((unsigned short*)&(X))+1))
#define LO_WORD_OF_32(X) (*((unsigned short*)&(X))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define MIDI_BUF_SIZE 64
#define MIDI_BUF_MASK 0xFFBF

typedef unsigned char bool;
#define false 0
#define true 1

// Functions
void initDac(void);
void initDma0(void);
void __attribute__((interrupt, no_auto_psv)) _DMA0Interrupt(void);
void __attribute__((interrupt, no_auto_psv)) _DMA1Interrupt(void);
void __attribute__((interrupt, no_auto_psv)) _DAC1LInterrupt(void);
void __attribute__((interrupt, no_auto_psv)) _DAC1RInterrupt(void);

fractional DacLBufferA[NUMSAMP] __attribute__((space(dma), aligned(2*NUMSAMP)));  // Ping-pong buffer A
fractional DacLBufferB[NUMSAMP] __attribute__((space(dma), aligned(2*NUMSAMP)));  // Ping-pong buffer B
fractional DacRBufferA[NUMSAMP] __attribute__((space(dma), aligned(2*NUMSAMP)));  // Ping-pong buffer A
fractional DacRBufferB[NUMSAMP] __attribute__((space(dma), aligned(2*NUMSAMP)));  // Ping-pong buffer B
//fractional DciBufferA[NUMSAMP] __attribute__((space(dma)));  // Ping-pong buffer A
//fractional DciBufferB[NUMSAMP] __attribute__((space(dma)));  // Ping-pong buffer B
unsigned char MidiBufferA[MIDI_BUF_SIZE] __attribute__((space(dma), aligned(MIDI_BUF_SIZE)));	// Ping-pong buffer A
unsigned char MidiBufferB[MIDI_BUF_SIZE] __attribute__((space(dma), aligned(MIDI_BUF_SIZE)));	// Ping-pong buffer B

unsigned char rxmsg[3];
unsigned char txmsg[3];
unsigned long osc1, modincr1;

unsigned char midibuffer[MIDI_BUF_SIZE] __attribute__((aligned(MIDI_BUF_SIZE*2))); // alignment must be double size...
unsigned char rstat =0;
unsigned short miditail = (unsigned short)&midibuffer;
unsigned short midihead = (unsigned short)&midibuffer;
unsigned char note[6] = {0,0,0,0,0,0}; // 6 note buffer

struct Oscillator
{
    unsigned long phase;
    unsigned long incr;
    unsigned short level;
    unsigned short spare;
};

struct Voice
{
    short envelope;
    unsigned char note;
    unsigned char priority; // starts at 8, 0=off
};

struct Oscillator O[16];
struct Voice V[8];

unsigned short  sample; // index to the most recent sample
fractional* DacBufferL; // points to either DacBufferA or DacBufferB as appropriate
fractional* DacBufferR; // points to either DacBufferA or DacBufferB as appropriate

// NCO increment table for MIDI notes 0..127 for a 32bit accumulator at sample rate of 39062.5
unsigned long noteincr[128]/* __attribute__((space(psv)))*/ = {
    898939,     952392,    1009024,    1069024,    1132592,    1199939,    1271291,    1346886,
   1426976,    1511828,    1601726,    1696970,    1797877,    1904785,    2018049,    2138048,
   2265183,    2399878,    2542582,    2693772,    2853952,    3023657,    3203453,    3393940,
   3595754,    3809569,    4036098,    4276097,    4530367,    4799756,    5085165,    5387544,
   5707904,    6047314,    6406906,    6787880,    7191509,    7619138,    8072196,    8552193,
   9060733,    9599513,   10170329,   10775088,   11415809,   12094628,   12813812,   13575761,
  14383018,   15238276,   16144391,   17104387,   18121467,   19199025,   20340658,   21550177,
  22831617,   24189256,   25627624,   27151522,   28766035,   30476553,   32288783,   34208774,
  36242933,   38398050,   40681317,   43100354,   45663234,   48378512,   51255248,   54303043,
  57532070,   60953105,   64577565,   68417547,   72485866,   76796100,   81362634,   86200708,
  91326469,   96757023,  102510495,  108606086,  115064140,  121906210,  129155131,  136835095,
 144971733,  153592200,  162725268,  172401416,  182652938,  193514046,  205020990,  217212173,
 230128281,  243812421,  258310262,  273670189,  289943465,  307184401,  325450536,  344802832,
 365305875,  387028093,  410041981,  434424346,  460256562,  487624841,  516620523,  547340378,
 579886931,  614368802,  650901072,  689605664,  730611750,  774056186,  820083962,  868848692,
 920513124,  975249682, 1033241046, 1094680756, 1159773861, 1228737604, 1301802144, 1379211328};

unsigned short sintab[1024] = {
     0,     201,     402,     603,     804,    1005,    1206,    1406,
  1607,    1808,    2009,    2209,    2410,    2610,    2811,    3011,
  3211,    3411,    3611,    3811,    4011,    4210,    4409,    4608,
  4807,    5006,    5205,    5403,    5601,    5799,    5997,    6195,
  6392,    6589,    6786,    6982,    7179,    7375,    7571,    7766,
  7961,    8156,    8351,    8545,    8739,    8932,    9126,    9319,
  9511,    9703,    9895,   10087,   10278,   10469,   10659,   10849,
 11038,   11227,   11416,   11604,   11792,   11980,   12166,   12353,
 12539,   12724,   12909,   13094,   13278,   13462,   13645,   13827,
 14009,   14191,   14372,   14552,   14732,   14911,   15090,   15268,
 15446,   15623,   15799,   15975,   16150,   16325,   16499,   16672,
 16845,   17017,   17189,   17360,   17530,   17699,   17868,   18036,
 18204,   18371,   18537,   18702,   18867,   19031,   19194,   19357,
 19519,   19680,   19840,   20000,   20159,   20317,   20474,   20631,
 20787,   20942,   21096,   21249,   21402,   21554,   21705,   21855,
 22004,   22153,   22301,   22448,   22594,   22739,   22883,   23027,
 23169,   23311,   23452,   23592,   23731,   23869,   24006,   24143,
 24278,   24413,   24546,   24679,   24811,   24942,   25072,   25201,
 25329,   25456,   25582,   25707,   25831,   25954,   26077,   26198,
 26318,   26437,   26556,   26673,   26789,   26905,   27019,   27132,
 27244,   27355,   27466,   27575,   27683,   27790,   27896,   28001,
 28105,   28208,   28309,   28410,   28510,   28608,   28706,   28802,
 28897,   28992,   29085,   29177,   29268,   29358,   29446,   29534,
 29621,   29706,   29790,   29873,   29955,   30036,   30116,   30195,
 30272,   30349,   30424,   30498,   30571,   30643,   30713,   30783,
 30851,   30918,   30984,   31049,   31113,   31175,   31236,   31297,
 31356,   31413,   31470,   31525,   31580,   31633,   31684,   31735,
 31785,   31833,   31880,   31926,   31970,   32014,   32056,   32097,
 32137,   32176,   32213,   32249,   32284,   32318,   32350,   32382,
 32412,   32441,   32468,   32495,   32520,   32544,   32567,   32588,
 32609,   32628,   32646,   32662,   32678,   32692,   32705,   32717,
 32727,   32736,   32744,   32751,   32757,   32761,   32764,   32766,
 32767,   32766,   32764,   32761,   32757,   32751,   32744,   32736,
 32727,   32717,   32705,   32692,   32678,   32662,   32646,   32628,
 32609,   32588,   32567,   32544,   32520,   32495,   32468,   32441,
 32412,   32382,   32350,   32318,   32284,   32249,   32213,   32176,
 32137,   32097,   32056,   32014,   31970,   31926,   31880,   31833,
 31785,   31735,   31684,   31633,   31580,   31525,   31470,   31413,
 31356,   31297,   31236,   31175,   31113,   31049,   30984,   30918,
 30851,   30783,   30713,   30643,   30571,   30498,   30424,   30349,
 30272,   30195,   30116,   30036,   29955,   29873,   29790,   29706,
 29621,   29534,   29446,   29358,   29268,   29177,   29085,   28992,
 28897,   28802,   28706,   28608,   28510,   28410,   28309,   28208,
 28105,   28001,   27896,   27790,   27683,   27575,   27466,   27355,
 27244,   27132,   27019,   26905,   26789,   26673,   26556,   26437,
 26318,   26198,   26077,   25954,   25831,   25707,   25582,   25456,
 25329,   25201,   25072,   24942,   24811,   24679,   24546,   24413,
 24278,   24143,   24006,   23869,   23731,   23592,   23452,   23311,
 23169,   23027,   22883,   22739,   22594,   22448,   22301,   22153,
 22004,   21855,   21705,   21554,   21402,   21249,   21096,   20942,
 20787,   20631,   20474,   20317,   20159,   20000,   19840,   19680,
 19519,   19357,   19194,   19031,   18867,   18702,   18537,   18371,
 18204,   18036,   17868,   17699,   17530,   17360,   17189,   17017,
 16845,   16672,   16499,   16325,   16150,   15975,   15799,   15623,
 15446,   15268,   15090,   14911,   14732,   14552,   14372,   14191,
 14009,   13827,   13645,   13462,   13278,   13094,   12909,   12724,
 12539,   12353,   12166,   11980,   11792,   11604,   11416,   11227,
 11038,   10849,   10659,   10469,   10278,   10087,    9895,    9703,
  9511,    9319,    9126,    8932,    8739,    8545,    8351,    8156,
  7961,    7766,    7571,    7375,    7179,    6982,    6786,    6589,
  6392,    6195,    5997,    5799,    5601,    5403,    5205,    5006,
  4807,    4608,    4409,    4210,    4011,    3811,    3611,    3411,
  3211,    3011,    2811,    2610,    2410,    2209,    2009,    1808,
  1607,    1406,    1206,    1005,     804,     603,     402,     201,
     0,    -201,    -402,    -603,    -804,   -1005,   -1206,   -1406,
 -1607,   -1808,   -2009,   -2209,   -2410,   -2610,   -2811,   -3011,
 -3211,   -3411,   -3611,   -3811,   -4011,   -4210,   -4409,   -4608,
 -4807,   -5006,   -5205,   -5403,   -5601,   -5799,   -5997,   -6195,
 -6392,   -6589,   -6786,   -6982,   -7179,   -7375,   -7571,   -7766,
 -7961,   -8156,   -8351,   -8545,   -8739,   -8932,   -9126,   -9319,
 -9511,   -9703,   -9895,  -10087,  -10278,  -10469,  -10659,  -10849,
-11038,  -11227,  -11416,  -11604,  -11792,  -11980,  -12166,  -12353,
-12539,  -12724,  -12909,  -13094,  -13278,  -13462,  -13645,  -13827,
-14009,  -14191,  -14372,  -14552,  -14732,  -14911,  -15090,  -15268,
-15446,  -15623,  -15799,  -15975,  -16150,  -16325,  -16499,  -16672,
-16845,  -17017,  -17189,  -17360,  -17530,  -17699,  -17868,  -18036,
-18204,  -18371,  -18537,  -18702,  -18867,  -19031,  -19194,  -19357,
-19519,  -19680,  -19840,  -20000,  -20159,  -20317,  -20474,  -20631,
-20787,  -20942,  -21096,  -21249,  -21402,  -21554,  -21705,  -21855,
-22004,  -22153,  -22301,  -22448,  -22594,  -22739,  -22883,  -23027,
-23169,  -23311,  -23452,  -23592,  -23731,  -23869,  -24006,  -24143,
-24278,  -24413,  -24546,  -24679,  -24811,  -24942,  -25072,  -25201,
-25329,  -25456,  -25582,  -25707,  -25831,  -25954,  -26077,  -26198,
-26318,  -26437,  -26556,  -26673,  -26789,  -26905,  -27019,  -27132,
-27244,  -27355,  -27466,  -27575,  -27683,  -27790,  -27896,  -28001,
-28105,  -28208,  -28309,  -28410,  -28510,  -28608,  -28706,  -28802,
-28897,  -28992,  -29085,  -29177,  -29268,  -29358,  -29446,  -29534,
-29621,  -29706,  -29790,  -29873,  -29955,  -30036,  -30116,  -30195,
-30272,  -30349,  -30424,  -30498,  -30571,  -30643,  -30713,  -30783,
-30851,  -30918,  -30984,  -31049,  -31113,  -31175,  -31236,  -31297,
-31356,  -31413,  -31470,  -31525,  -31580,  -31633,  -31684,  -31735,
-31785,  -31833,  -31880,  -31926,  -31970,  -32014,  -32056,  -32097,
-32137,  -32176,  -32213,  -32249,  -32284,  -32318,  -32350,  -32382,
-32412,  -32441,  -32468,  -32495,  -32520,  -32544,  -32567,  -32588,
-32609,  -32628,  -32646,  -32662,  -32678,  -32692,  -32705,  -32717,
-32727,  -32736,  -32744,  -32751,  -32757,  -32761,  -32764,  -32766,
-32767,  -32766,  -32764,  -32761,  -32757,  -32751,  -32744,  -32736,
-32727,  -32717,  -32705,  -32692,  -32678,  -32662,  -32646,  -32628,
-32609,  -32588,  -32567,  -32544,  -32520,  -32495,  -32468,  -32441,
-32412,  -32382,  -32350,  -32318,  -32284,  -32249,  -32213,  -32176,
-32137,  -32097,  -32056,  -32014,  -31970,  -31926,  -31880,  -31833,
-31785,  -31735,  -31684,  -31633,  -31580,  -31525,  -31470,  -31413,
-31356,  -31297,  -31236,  -31175,  -31113,  -31049,  -30984,  -30918,
-30851,  -30783,  -30713,  -30643,  -30571,  -30498,  -30424,  -30349,
-30272,  -30195,  -30116,  -30036,  -29955,  -29873,  -29790,  -29706,
-29621,  -29534,  -29446,  -29358,  -29268,  -29177,  -29085,  -28992,
-28897,  -28802,  -28706,  -28608,  -28510,  -28410,  -28309,  -28208,
-28105,  -28001,  -27896,  -27790,  -27683,  -27575,  -27466,  -27355,
-27244,  -27132,  -27019,  -26905,  -26789,  -26673,  -26556,  -26437,
-26318,  -26198,  -26077,  -25954,  -25831,  -25707,  -25582,  -25456,
-25329,  -25201,  -25072,  -24942,  -24811,  -24679,  -24546,  -24413,
-24278,  -24143,  -24006,  -23869,  -23731,  -23592,  -23452,  -23311,
-23169,  -23027,  -22883,  -22739,  -22594,  -22448,  -22301,  -22153,
-22004,  -21855,  -21705,  -21554,  -21402,  -21249,  -21096,  -20942,
-20787,  -20631,  -20474,  -20317,  -20159,  -20000,  -19840,  -19680,
-19519,  -19357,  -19194,  -19031,  -18867,  -18702,  -18537,  -18371,
-18204,  -18036,  -17868,  -17699,  -17530,  -17360,  -17189,  -17017,
-16845,  -16672,  -16499,  -16325,  -16150,  -15975,  -15799,  -15623,
-15446,  -15268,  -15090,  -14911,  -14732,  -14552,  -14372,  -14191,
-14009,  -13827,  -13645,  -13462,  -13278,  -13094,  -12909,  -12724,
-12539,  -12353,  -12166,  -11980,  -11792,  -11604,  -11416,  -11227,
-11038,  -10849,  -10659,  -10469,  -10278,  -10087,   -9895,   -9703,
 -9511,   -9319,   -9126,   -8932,   -8739,   -8545,   -8351,   -8156,
 -7961,   -7766,   -7571,   -7375,   -7179,   -6982,   -6786,   -6589,
 -6392,   -6195,   -5997,   -5799,   -5601,   -5403,   -5205,   -5006,
 -4807,   -4608,   -4409,   -4210,   -4011,   -3811,   -3611,   -3411,
 -3211,   -3011,   -2811,   -2610,   -2410,   -2209,   -2009,   -1808,
 -1607,   -1406,   -1206,   -1005,    -804,    -603,    -402,    -201
};


unsigned int didx = 0;
unsigned char d[2241] = {0x00, 0x90, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x39,
0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20,
0x41, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50,
0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x41, 0x64, 0x81,
0x20, 0x41, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00,
0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x41, 0x64,
0x81, 0x20, 0x41, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32,
0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x41,
0x64, 0x81, 0x20, 0x41, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20,
0x32, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50,
0x41, 0x64, 0x81, 0x20, 0x41, 0x00, 0x50, 0x39, 0x64, 0x81,
0x20, 0x39, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00,
0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43, 0x64,
0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32,
0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43,
0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20,
0x32, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50,
0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81,
0x20, 0x32, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00,
0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64,
0x81, 0x20, 0x32, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39,
0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x39,
0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20,
0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50,
0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81,
0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00,
0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64,
0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A,
0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32,
0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20,
0x3A, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50,
0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81,
0x20, 0x3A, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00,
0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x32, 0x64,
0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A,
0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32,
0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20,
0x3A, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50,
0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81,
0x20, 0x3A, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40, 0x00,
0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64,
0x81, 0x20, 0x3A, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40,
0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A,
0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20,
0x40, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50,
0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x39, 0x64, 0x81,
0x20, 0x39, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00,
0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x39, 0x64,
0x81, 0x20, 0x39, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41,
0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x39,
0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20,
0x41, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50,
0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x41, 0x64, 0x81,
0x20, 0x41, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00,
0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x41, 0x64,
0x81, 0x20, 0x41, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39,
0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x39,
0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20,
0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50,
0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43, 0x64, 0x81,
0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00,
0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43, 0x64,
0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32,
0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43,
0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20,
0x32, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50,
0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x39, 0x64, 0x81,
0x20, 0x39, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00,
0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x43, 0x64,
0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32,
0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x43,
0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20,
0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50,
0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81,
0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00,
0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64,
0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A,
0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x3A,
0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20,
0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50,
0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64, 0x81,
0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00,
0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x32, 0x64,
0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A,
0x00, 0x50, 0x45, 0x64, 0x81, 0x20, 0x45, 0x00, 0x50, 0x32,
0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3C, 0x64, 0x81, 0x20,
0x3C, 0x00, 0x50, 0x45, 0x64, 0x81, 0x20, 0x45, 0x00, 0x50,
0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3C, 0x64, 0x81,
0x20, 0x3C, 0x00, 0x50, 0x45, 0x64, 0x81, 0x20, 0x45, 0x00,
0x50, 0x3C, 0x64, 0x81, 0x20, 0x3C, 0x00, 0x50, 0x2E, 0x64,
0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A,
0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00, 0x50, 0x2E,
0x64, 0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20,
0x3A, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00, 0x50,
0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81,
0x20, 0x3A, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00,
0x50, 0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64,
0x81, 0x20, 0x3A, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41,
0x00, 0x50, 0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A,
0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20,
0x41, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50,
0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x37, 0x64, 0x81,
0x20, 0x37, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40, 0x00,
0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x37, 0x64,
0x81, 0x20, 0x37, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40,
0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x37,
0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20,
0x41, 0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50,
0x37, 0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x41, 0x64, 0x81,
0x20, 0x41, 0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00,
0x50, 0x37, 0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x41, 0x64,
0x81, 0x20, 0x41, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43,
0x00, 0x50, 0x2D, 0x64, 0x81, 0x20, 0x2D, 0x00, 0x50, 0x39,
0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20,
0x43, 0x00, 0x50, 0x2D, 0x64, 0x81, 0x20, 0x2D, 0x00, 0x50,
0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43, 0x64, 0x81,
0x20, 0x43, 0x00, 0x50, 0x2D, 0x64, 0x81, 0x20, 0x2D, 0x00,
0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43, 0x64,
0x81, 0x20, 0x43, 0x00, 0x50, 0x2D, 0x64, 0x81, 0x20, 0x2D,
0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x43,
0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x2D, 0x64, 0x81, 0x20,
0x2D, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50,
0x43, 0x64, 0x81, 0x20, 0x43, 0x00, 0x50, 0x41, 0x64, 0x81,
0x20, 0x41, 0x00, 0x50, 0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00,
0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x41, 0x64,
0x81, 0x20, 0x41, 0x00, 0x50, 0x2E, 0x64, 0x81, 0x20, 0x2E,
0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x41,
0x64, 0x81, 0x20, 0x41, 0x00, 0x50, 0x2E, 0x64, 0x81, 0x20,
0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50,
0x40, 0x64, 0x81, 0x20, 0x40, 0x00, 0x50, 0x2E, 0x64, 0x81,
0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00,
0x50, 0x40, 0x64, 0x81, 0x20, 0x40, 0x00, 0x50, 0x2E, 0x64,
0x81, 0x20, 0x2E, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40,
0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00, 0x50, 0x40,
0x64, 0x81, 0x20, 0x40, 0x00, 0x50, 0x2E, 0x64, 0x81, 0x20,
0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00, 0x50,
0x3E, 0x64, 0x81, 0x20, 0x3E, 0x00, 0x50, 0x2E, 0x64, 0x81,
0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A, 0x00,
0x50, 0x3E, 0x64, 0x81, 0x20, 0x3E, 0x00, 0x50, 0x2E, 0x64,
0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20, 0x3A,
0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40, 0x00, 0x50, 0x2E,
0x64, 0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81, 0x20,
0x3A, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40, 0x00, 0x50,
0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00, 0x50, 0x3A, 0x64, 0x81,
0x20, 0x3A, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20, 0x40, 0x00,
0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00, 0x50, 0x30, 0x64,
0x81, 0x20, 0x30, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20, 0x37,
0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00, 0x50, 0x30,
0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20,
0x37, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41, 0x00, 0x50,
0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x37, 0x64, 0x81,
0x20, 0x37, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43, 0x00,
0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x37, 0x64,
0x81, 0x20, 0x37, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20, 0x43,
0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x37,
0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x43, 0x64, 0x81, 0x20,
0x43, 0x00, 0x50, 0x45, 0x64, 0x81, 0x20, 0x45, 0x00, 0x50,
0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x39, 0x64, 0x81,
0x20, 0x39, 0x00, 0x50, 0x45, 0x64, 0x81, 0x20, 0x45, 0x00,
0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x39, 0x64,
0x81, 0x20, 0x39, 0x00, 0x50, 0x45, 0x64, 0x81, 0x20, 0x45,
0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50, 0x39,
0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x40, 0x64, 0x81, 0x20,
0x40, 0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00, 0x50,
0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x40, 0x64, 0x81,
0x20, 0x40, 0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00,
0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x40, 0x64,
0x81, 0x20, 0x40, 0x00, 0x50, 0x41, 0x64, 0x81, 0x20, 0x41,
0x00, 0x50, 0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00, 0x50, 0x35,
0x64, 0x81, 0x20, 0x35, 0x00, 0x50, 0x3E, 0x64, 0x81, 0x20,
0x3E, 0x00, 0x50, 0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00, 0x50,
0x35, 0x64, 0x81, 0x20, 0x35, 0x00, 0x50, 0x3E, 0x64, 0x81,
0x20, 0x3E, 0x00, 0x50, 0x2E, 0x64, 0x81, 0x20, 0x2E, 0x00,
0x50, 0x35, 0x64, 0x81, 0x20, 0x35, 0x00, 0x50, 0x3C, 0x64,
0x81, 0x20, 0x3C, 0x00, 0x50, 0x2D, 0x64, 0x81, 0x20, 0x2D,
0x00, 0x50, 0x35, 0x64, 0x81, 0x20, 0x35, 0x00, 0x50, 0x3C,
0x64, 0x81, 0x20, 0x3C, 0x00, 0x50, 0x2D, 0x64, 0x81, 0x20,
0x2D, 0x00, 0x50, 0x35, 0x64, 0x81, 0x20, 0x35, 0x00, 0x50,
0x3C, 0x64, 0x81, 0x20, 0x3C, 0x00, 0x50, 0x3E, 0x64, 0x81,
0x20, 0x3E, 0x00, 0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00,
0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A, 0x64,
0x81, 0x20, 0x3A, 0x00, 0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B,
0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x3A,
0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x2B, 0x64, 0x81, 0x20,
0x2B, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50,
0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x29, 0x64, 0x81,
0x20, 0x29, 0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30, 0x00,
0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x29, 0x64,
0x81, 0x20, 0x29, 0x00, 0x50, 0x30, 0x64, 0x81, 0x20, 0x30,
0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x3A,
0x64, 0x81, 0x20, 0x3A, 0x00, 0x50, 0x27, 0x64, 0x81, 0x20,
0x27, 0x00, 0x50, 0x33, 0x64, 0x81, 0x20, 0x33, 0x00, 0x50,
0x37, 0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x27, 0x64, 0x81,
0x20, 0x27, 0x00, 0x50, 0x33, 0x64, 0x81, 0x20, 0x33, 0x00,
0x50, 0x37, 0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x27, 0x64,
0x81, 0x20, 0x27, 0x00, 0x50, 0x33, 0x64, 0x81, 0x20, 0x33,
0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50, 0x29,
0x64, 0x81, 0x20, 0x29, 0x00, 0x50, 0x35, 0x64, 0x81, 0x20,
0x35, 0x00, 0x50, 0x39, 0x64, 0x81, 0x20, 0x39, 0x00, 0x50,
0x29, 0x64, 0x81, 0x20, 0x29, 0x00, 0x50, 0x37, 0x64, 0x81,
0x20, 0x37, 0x00, 0x50, 0x35, 0x64, 0x81, 0x20, 0x35, 0x00,
0x50, 0x37, 0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x2B, 0x64,
0x81, 0x20, 0x2B, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32,
0x00, 0x50, 0x37, 0x64, 0x81, 0x20, 0x37, 0x00, 0x50, 0x2B,
0x64, 0x81, 0x20, 0x2B, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20,
0x32, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20, 0x37, 0x00, 0x50,
0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00, 0x50, 0x32, 0x64, 0x81,
0x20, 0x32, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20, 0x37, 0x00,
0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00, 0x50, 0x32, 0x64,
0x81, 0x20, 0x32, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20, 0x37,
0x00, 0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00, 0x50, 0x32,
0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20,
0x37, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50,
0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00, 0x50, 0x32, 0x64, 0x81,
0x20, 0x32, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20, 0x37, 0x00,
0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00, 0x50, 0x32, 0x64,
0x81, 0x20, 0x32, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20, 0x37,
0x00, 0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00, 0x50, 0x32,
0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x37, 0x64, 0x81, 0x20,
0x37, 0x00, 0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00, 0x50,
0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x37, 0x64, 0x81,
0x20, 0x37, 0x00, 0x50, 0x2B, 0x64, 0x81, 0x20, 0x2B, 0x00,
0x50, 0x32, 0x64, 0x81, 0x20, 0x32, 0x00, 0x50, 0x37, 0x64,
0x81, 0x20, 0x37, 0x00, 0x50, 0x32, 0x64, 0x81, 0x20, 0x32,
0x00};




//Macros for Configuration Fuse Registers:
//Invoke macros to set up  device configuration fuse registers.
//The fuses will select the oscillator source, power-up timers, watch-dog
//timers etc. The macros are defined within the device
//header files. The configuration fuse registers reside in Flash memory.


// Internal FRC Oscillator
//_FOSCSEL(FNOSC_FRC);							// FRC Oscillator
_FOSCSEL(FNOSC_PRI);							// FRC Oscillator
//_FOSC(FCKSM_CSECMD & OSCIOFNC_ON  & POSCMD_NONE); 
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF & POSCMD_EC); 
												// Clock Switching is enabled and Fail Safe Clock Monitor is disabled
												// OSC2 Pin Function: OSC2 is Clock Output
												// Primary Oscillator Mode: Disabled

_FWDT(FWDTEN_OFF);              				// Watchdog Timer Enabled/disabled by user software
												// (LPRC can be disabled by clearing SWDTEN bit in RCON register

/*=============================================================================
initDac() is used to configure D/A. 
=============================================================================*/
void initDac(void)
{
	/* Initiate DAC Clock */
	ACLKCONbits.SELACLK = 0;		// PLLCLK as Clock Source 
	ACLKCONbits.AOSCMD = 0;			// Auxiliary Oscillator Disabled
	//ACLKCONbits.ASRCSEL = 1;		// Primary Oscillator is the Clock Source
	ACLKCONbits.APSTSCLR = 6;		// divide by 2 - note main osc also has postpll /2

	DAC1DFLT = 0x8000;				// DAC Default value is the midpoint 
	
	DAC1CONbits.DACFDIV = 7;        // 39.0625kHz
	
	DAC1CONbits.FORM = 1;			// Data Format is signed integer
	DAC1CONbits.AMPON = 0;			// Analog Output Amplifier is enabled during Sleep Mode/Stop-in Idle mode

	DAC1STATbits.LOEN = 1;			// Left Channel DAC Output Enabled  
	DAC1STATbits.LITYPE = 1;        // interrupt when FIFO empty
	DAC1STATbits.ROEN = 1;			// Right Channel DAC Output Enabled  
	DAC1STATbits.RITYPE = 1;        // interrupt when FIFO empty

	DAC1CONbits.DACEN = 1;			// DAC1 Module Enabled 
}

/*void initDci(void)
{
	// Initiate DAC Clock 
	ACLKCONbits.SELACLK = 0;		// FRC w/ Pll as Clock Source 
	ACLKCONbits.AOSCMD = 0;			// Auxiliary Oscillator Disabled
	ACLKCONbits.ASRCSEL = 0;		// Auxiliary Oscillator is the Clock Source
	ACLKCONbits.APSTSCLR = 7;		// FRC divide by 1 

	DAC1STATbits.LOEN = 1;			// Left Channel DAC Output Enabled  
	DAC1STATbits.LITYPE = 1;        // interrupt when FIFO empty

	DAC1DFLT = 0x8000;				// DAC Default value is the midpoint 
	
				    	  // 103.16KHz  	// 8.038KHz		// 44.211KHz	// 25KHz
	DAC1CONbits.DACFDIV = 5; 		  		//76; 			//13; 		    // 23; //		// Divide High Speed Clock by DACFDIV+1 
	
	DAC1CONbits.FORM = 1;			// Data Format is signed integer
	DAC1CONbits.AMPON = 0;			// Analog Output Amplifier is enabled during Sleep Mode/Stop-in Idle mode

	DAC1CONbits.DACEN = 1;			// DAC1 Module Enabled 
}*/

void initTimer()
{
    T3CONbits.TON = 0;
    T2CONbits.TON = 0;
    T2CONbits.T32 = 1;
    T2CONbits.TCS = 0;
    T2CONbits.TGATE = 0;
    T2CONbits.TCKPS = 0; // 0 - 1:1, 1 - 1:8, 2 - 1:64, 3 - 1:256
    TMR3 = 0;
    TMR2 = 0;
    PR3 = 0x001F;
    PR2 = 0xFFFF;
    IPC2bits.T3IP = 0x01; // priority level
    IFS0bits.T3IF = 0;
    IEC0bits.T3IE = 1; // enable interrupt

    T2CONbits.TON = 1; // start timer...
}

void __attribute__((__interrupt__, __shadow__, __no_auto_psv__)) _T3Interrupt()
{
    IFS0bits.T3IF = 0; // clear timer 3 interrupt flag
}    

/*=============================================================================  
DMA0 configuration
 Direction: Read from peripheral address 0-x300 (ADC1BUF0) and write to DMA RAM 
 AMODE: Register indirect with post increment
 MODE: Continuous, Ping-Pong Mode
 IRQ: ADC Interrupt
 ADC stores results stored alternatively between BufferA[] and BufferB[]
=============================================================================*/
void initDma0(void) // Buffer -> DAC
{
	DMA0CONbits.AMODE = 0;			// Configure DMA for Register indirect with post increment
	DMA0CONbits.MODE = 2;			// Configure DMA for Continuous Ping-Pong mode
	DMA0CONbits.DIR = 1;			// RAM -> Peripheral

	DMA0PAD = (int)&DAC1LDAT;		// Peripheral Address Register: Left DAC FIFO
	DMA0CNT = (NUMSAMP-1);			// DMA Transfer Count is (NUMSAMP-1)	
	
	DMA0REQ = 0x4F;					// DAC1 Left Data Transfer interrupt selected for DMA channel IRQ
	
	DMA0STA = __builtin_dmaoffset(DacLBufferA);	// DMA RAM Start Address A	
	DMA0STB = __builtin_dmaoffset(DacLBufferB); // DMA RAM Start Address B

	IFS0bits.DMA0IF = 0;			// Clear the DMA interrupt flag bit
    IEC0bits.DMA0IE = 1;			// Set the DMA interrupt enable bit

	DMA0CONbits.CHEN = 1;			// Enable DMA channel
}

void initDma1(void) // Buffer -> DAC
{
	DMA1CONbits.AMODE = 0;			// Configure DMA for Register indirect with post increment
	DMA1CONbits.MODE = 2;			// Configure DMA for Continuous Ping-Pong mode
	DMA1CONbits.DIR = 1;			// RAM -> Peripheral

	DMA1PAD = (int)&DAC1RDAT;		// Peripheral Address Register: Right DAC FIFO
	DMA1CNT = (NUMSAMP-1);			// DMA Transfer Count is (NUMSAMP-1)	
	
	DMA1REQ = 0x4E;					// DAC1 Right Data Transfer interrupt selected for DMA channel IRQ
	
	DMA1STA = __builtin_dmaoffset(DacRBufferA);	// DMA RAM Start Address A	
	DMA1STB = __builtin_dmaoffset(DacRBufferB); // DMA RAM Start Address B

	IFS0bits.DMA1IF = 0;			// Clear the DMA interrupt flag bit
    IEC0bits.DMA1IE = 1;			// Set the DMA interrupt enable bit

	DMA1CONbits.CHEN = 1;			// Enable DMA channel
}

void initDma2(void) // DCI -> Buffer
{
	DMA2CONbits.AMODE = 0;			// Configure DMA for Register indirect with post increment
	DMA2CONbits.MODE = 2;			// Configure DMA for Continuous Ping-Pong mode
	DMA2CONbits.DIR = 0;			// Peripheral -> RAM

	DMA2PAD = (int)&RXBUF0;	        // Peripheral Address Register: DCI RX
	DMA2CNT = (NUMSAMP-1);			// DMA Transfer Count is (NUMSAMP-1)	

	DMA2REQ = 0x3C;					// DCI Data Transfer interrupt selected for DMA channel IRQ

//	DMA2STA = __builtin_dmaoffset(DacBufferA);	// DMA RAM Start Address A	
//	DMA2STB = __builtin_dmaoffset(DacBufferB); // DMA RAM Start Address B

	//IFS1bits.DMA2IF = 0;			// Clear the DMA interrupt flag bit
    //IEC1bits.DMA2IE = 1;			// Set the DMA interrupt enable bit
	
	DMA2CONbits.CHEN = 1;			// Enable DMA channel
	//DMA2REQbits.FORCE = 1;  // Kick it off!
}

void initDma7(void) // UART -> Buffer
{
	DMA7CONbits.AMODE = 0;			// Configure DMA for Register indirect with post increment
	DMA7CONbits.MODE = 2;			// Configure DMA for Continuous Ping-Pong mode
	DMA7CONbits.DIR = 0;			// Peripheral -> RAM
	DMA7CONbits.SIZE = 1;           // byte access

	DMA7PAD = (int)&U1RXREG;        // Peripheral Address Register: UART1 RX
	DMA7CNT = (MIDI_BUF_SIZE-1);			// DMA Transfer Count is (NUMSAMP-1)	

	DMA7REQ = 0x0B;					// DCI Data Transfer interrupt selected for DMA channel IRQ

	DMA7STA = __builtin_dmaoffset(MidiBufferA);	// DMA RAM Start Address A	
	DMA7STB = __builtin_dmaoffset(MidiBufferB); // DMA RAM Start Address B

	//IFS1bits.DMA2IF = 0;			// Clear the DMA interrupt flag bit
    //IEC1bits.DMA2IE = 1;			// Set the DMA interrupt enable bit
	
	DMA7CONbits.CHEN = 1;			// Enable DMA channel
	//DMA2REQbits.FORCE = 1;  // Kick it off!
}



/*=============================================================================
_DMA0Interrupt(): ISR name is chosen from the device linker script.
=============================================================================*/

void __attribute__((interrupt, no_auto_psv)) _DMA0Interrupt(void)
{
 	IFS0bits.DMA0IF = 0;			// Clear the DMA0 Interrupt Flag
    DacBufferL = DMACS1bits.PPST0 ? &DacLBufferA[0] : &DacLBufferB[0];
    sample = 0;
}

void __attribute__((interrupt, no_auto_psv)) _DMA1Interrupt(void)
{
 	IFS0bits.DMA1IF = 0;			// Clear the DMA1 Interrupt Flag
    DacBufferR = DMACS1bits.PPST1 ? &DacRBufferA[0] : &DacRBufferB[0];
    sample = 0;
}

void __attribute__((interrupt, no_auto_psv)) _DMA2Interrupt(void)
{
 	IFS1bits.DMA2IF = 0;			// Clear the DMA2 Interrupt Flag
}

void __attribute__((interrupt, no_auto_psv)) _DMA7Interrupt(void)
{
 	IFS4bits.DMA7IF = 0;			// Clear the DMA7 Interrupt Flag
}

/*=============================================================================
_DAC1LInterrupt(): ISR name is chosen from the device linker script.
=============================================================================*/
void __attribute__((interrupt, no_auto_psv)) _DAC1LInterrupt(void)
{
    IFS4bits.DAC1LIF = 0;     // Clear Left Channel Interrupt Flag
}
/*=============================================================================
_DAC1RInterrupt(): ISR name is chosen from the device linker script.
=============================================================================*/
void __attribute__((interrupt, no_auto_psv)) _DAC1RInterrupt(void)
{
    IFS4bits.DAC1RIF = 0;     // Clear Left Channel Interrupt Flag
}

void __attribute__((interrupt, no_auto_psv)) _DCIInterrupt(void)
{
    IFS3bits.DCIIF = 0;       // Clear DCI Interrupt Flag 	 
}

////////////////////
//
// MIDI in from UART
//
////////////////////
static int ferror = 0;
static int oerror = 0;
static int rxok   = 0;
void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void)
{
    if ( U1STAbits.OERR )
    {
        U1STAbits.OERR = 0;
        oerror ++;
        return;
    }   
    if ( U1STAbits.FERR )
    {
        volatile unsigned char rx = U1RXREG;
        ferror ++;
        return;
    }    
    while ( U1STAbits.URXDA )
    {
        volatile unsigned char rx = U1RXREG;
        IFS0bits.U1RXIF = 0;       // Clear UART RX Interrupt Flag 	 
        rxok++;

        if ( NOT_FULL(midi) )
        {
            *((unsigned char*)midihead) = rx;
            midihead++;
            midihead &= MIDI_BUF_MASK;
        }
        else
        {
            // MIDI Rx overflow - indicate
    //        LATC |= 4;
            // drop oldest byte. Will undoubtedly cause corruption...
            miditail++;
            miditail &= MIDI_BUF_MASK;
        }
    }
}

bool getMidiByte(unsigned char* result)
{
    unsigned char temp;
    if ( EMPTY(midi) ) return false;
    temp = *((unsigned char*)miditail);
    miditail++;
    miditail &= MIDI_BUF_MASK;
    *result = temp;
    return true;
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

void shiftNoteOff(unsigned char n)
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

void noteOn(unsigned char n)
{
//    in_release = false;

    // should make it configurable whether a new note re-triggers the envelope or not.
    // or have two envelopes, one of each type...
/*    if (note[0] == 0)
    {
        envpos = 0;
    }
*/
    shiftNoteIn(n);

    O[0].incr = noteincr[n];O[0].level = 0x7fff;
//    O[1].incr = noteincr[n+12];O[1].level = 0x3fff;
//    O[2].incr = noteincr[n+24];O[2].level = 0x1fff;
/*
    // pre-compute bend values to reduce time taken in applyBend
    // (on basis that pitch bend data can be much more frequent
    // than note data)
    bendupincr = 1.0 / 8192.0 * (tune[n+1] - tune[n]);
    benddownincr = 1.0 / 8192.0 * (tune[n] - tune[n-1]);
    applyBend();
*/
}

void noteOff(unsigned char n)
{
    shiftNoteOff(n);

    if (note[0] == 0)
    {
        int i;
        for(i=0; i<16; i++)
        {
            O[i].incr = 0;
            O[i].phase = 0;
        }    
    }
    else
    {
        noteOn(note[0]);
    }
}

void processMsg()
{
    unsigned char status = rxmsg[0];
    
    if (( status == 0x90 || status == 0x80 ) &&
        rxmsg[1]>20 && rxmsg[1]<117) // note on/off (in range)
    {
        if (status == 0x90 && rxmsg[2] != 0)
        {
            noteOn(rxmsg[1]);
        }
        else
        {
            noteOff(rxmsg[1]);
        }
    }
    else if ( status == 0xB0 )
    {
/*        switch ( rxmsg[1] )
        {
            case 7: // volume
                mainvolume = rxmsg[2];
                break;
            case 71: // type
                if (rxmsg[2] < WAVE_END) waveform = rxmsg[2];
                break;
            case 77: // shape
                shape = rxmsg[2];
                break;
            case 72: // release
                release = max(1, rxmsg[2]);
                break;
            case 73: // attack
                attack = max(1, rxmsg[2]);
                break;
            case 123: // all notes off
//                LATC &= ~8;
                TRISB = 0xFF;
                break;
            default:
                break;
        }
*/
    }
    else if ( rstat == 0xE0 )
    {
        // pitch bend
/*
        bend = ((short)rxmsg[1] + ((short)rxmsg[2] << 7)) - 8192;
        applyBend();
*/
    }
}    

void processMidi()
{
    unsigned char rx;
    static bool insysex = false;
    static char bytecount = 0;
    static char origbytecount = 0;
    static unsigned char* msg;
    
    
    if (getMidiByte(&rx) == false) return;

    if ( (insysex && (rx != 0xF7)) ||
         rx >= 0xF8 )
    {
        // sysex or system realtime - ignore
        return;
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
        rxmsg[0] = rstat;
        msg = &rxmsg[1];
    }
    else // databyte
    {
        if (rstat)
        {
            *msg++ = rx;
            if ( --bytecount == 0 )
            {
                bytecount = origbytecount; // reset counter for new set of databytes with running status
                processMsg();
                msg = &rxmsg[1];
            }
        } // otherwise no running status - ignore databyte
    }
}

////////////////////////
//
// SMF File output on TX
//
////////////////////////

unsigned long getLength()
{
    unsigned long l = 0;
    int count = 4;
    while (count--)
    {
        unsigned char z = d[didx++];
        l = l*128 + (z & 127);
        if ((z & 128) == 0)
            break;
    }

    return l;
}    

int getEvent()
{
    static unsigned char rstat = 0;
    unsigned char rx;

    rx = d[didx++];
    int msgbytes = 0;
    int len;

    // Meta Event, Sysex, Escaped SysEx, Channel Message
    if ( rx == 0xF0 ||  // SysEx
         rx == 0xF7 )   // escaped sysex
    {
        len = d[didx++];
        didx += len;
        rstat = 0;
        return 0;
    }
    else if ( rx == 0xFF ) // meta event
    {
        didx++; // skip type - don't care
        len = d[didx++];
        didx += len;
        rstat = 0;
        return 0;
    }
    else if ( rx >= 0x80 )
    {
        // get new running status
        rstat = rx;
        rx = d[didx++];
        txmsg[msgbytes++] = rstat;
    }
    else if ( rstat == 0 )
    {
        // bad - got data bytes without any running status active
        return 0;
    }    

    // 1 or 2 databytes of channel message left
    txmsg[msgbytes++] = rx;
    if ( !(rstat >= 0xC0 && rx <= 0xDF) )
    {
        // there's another byte to send
        rx = d[didx++];
        txmsg[msgbytes++] = rx;
    }

    return msgbytes;
}

void outputSmf()
{
    int i;
    unsigned long len;
    didx = 0;
    while (didx < sizeof(d))
    { 			 
        len = getLength();
        for (i=0; i < len*750L; i++);
        len = getEvent();
        for (i = 0; i<len; i++)
        {
            while (U1STAbits.TRMT == 0);
            U1TXREG = txmsg[i];
        }    
	}
}

////////////
//
// Main Prog
//
////////////

int main (void)
{
	// Configure Oscillator to operate the device at 40MIPS
	// Fosc= Fin*M/(N1*N2), Fcy=Fosc/2
	// Fosc= 7.37M*43/(2*2)=79.22Mhz for ~40MIPS input clock
	// Fosc= 8.00M*40/(2*2)=80.00Mhz for ~40MIPS input clock
	PLLFBD=38;									// M=40
	CLKDIVbits.PLLPOST=0;						// N1=2
	CLKDIVbits.PLLPRE=0;						// N2=2
	OSCTUN=0;									// Tune FRC oscillator, if FRC is used

	// Disable Watch Dog Timer
	RCONbits.SWDTEN=0;

	// Clock switch to incorporate PLL
	__builtin_write_OSCCONH(0x03);				// Initiate Clock Switch to
												// EC with PLL (NOSC=0b011)
	__builtin_write_OSCCONL(0x01);				// Start clock switching
    while (OSCCONbits.COSC != 0x03);			// Wait for Clock switch to occur

    // Wait for PLL to lock
    while(OSCCONbits.LOCK!=1);

    // Configure PPS: UART RX on RP9 (pin 18)
	__builtin_write_OSCCONL(OSCCON & ~(1<<6));
	RPINR18bits.U1RXR = 9;
	__builtin_write_OSCCONL(OSCCON | (1<<6));

	U1BRG  = 79;                    // 79 = 31250 @ 40MHz Fcy
	U1MODE = 0x8000; 				// Reset UART to 8-n-1, and enable 
	U1STA  = 0x0400; 				// Reset status register and enable TX & RX
	U1STAbits.URXISEL = 0;         // interrupt after one character rxd
	IEC0bits.U1RXIE = 1;            // enable interrupt

    //initTimer();
	initDma0();									// Initialize the DMA controller to buffer ADC data in conversion order
    initDma1();									// Initialize the DMA controller to buffer ADC data in conversion order
	//initDma7();									// Initialize the DMA controller to buffer UART RX data
	initDac(); 									// Initialize the D/A converter 

    DacBufferL = &DacLBufferA[0];
    DacBufferR = &DacRBufferA[0];

 	long i, j, k, l;

    while (true)
    {
        if ( sample < NUMSAMP )
        {
            int i;
            long dacdword;
            short wave;
            unsigned long tempL, tempR;

            // update oscillators (all 16 of them...)
            // This is a candidate for MAC stuff to accumulate all these
            // a 40bit register...
            register unsigned long* p;
            register unsigned long* q;
            #define offsetIncr sizeof(struct Oscillator)/sizeof(unsigned long);
            p = &O[0].phase;
            q = &O[0].incr;
            // unrolled for speed
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            /*
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            *p += *q; p+=offsetIncr;q+=offsetIncr;
            */
            tempL=0;
            int z;
            //for (z=0; z<8; z++)
            //{
            //    long temp;
            //    temp = (long)(O[z].level>>2) * (long)sintab[HI_WORD_OF_32(O[z].phase) >> 6];
            //    tempL += HI_WORD_OF_32(temp);
            //}

            tempL  = sintab[HI_WORD_OF_32(O[0].phase) >> 6]; // >>= 3;
            tempR = tempL;
            //temp += sintab[(HI_WORD_OF_32(O[2].phase)/5) >> 6];

/*            switch (waveform)
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
*/
            DacBufferL[sample] = tempL;
            DacBufferR[sample] = tempR;
            sample++;
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
//        else if (sample == NUMSAMP)
//        {
//            // buffer is full. Send green
//            //LATC = 1;
//            O[0].incr += 100;
//            O[1].incr += 100;
//        }

        processMidi();
    }

    return 0;
}
