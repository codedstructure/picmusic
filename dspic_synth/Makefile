# MPLAB IDE generated this makefile for use with GNU make.
# Project: 802_DacTest.mcp
# Date: Sat Dec 13 19:47:44 2008

AS = pic30-as.exe
CC = pic30-gcc.exe
LD = pic30-ld.exe
AR = pic30-ar.exe
HX = pic30-bin2hex.exe
RM = rm

802_DacTest.hex : 802_DacTest.cof
	$(HX) "802_DacTest.cof"

802_DacTest.cof : DacTest.o test.o traps.o
	$(CC) -mcpu=33FJ64GP802 "DacTest.o" "test.o" "traps.o" -o"802_DacTest.cof" -Wl,-L"C:\Program Files\Microchip\MPLAB C30\lib",-Tp33FJ64GP802.gld,--defsym=__MPLAB_BUILD=1,--defsym=__MPLAB_DEBUG=1,--defsym=__MPLAB_DEBUGGER_PICKIT2=1,--defsym=__ICD2RAM=1,-Map="802_DacTest.map",--report-mem

DacTest.o : ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../include/math.h ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../include/yvals.h ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../include/stdlib.h ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../support/generic/h/dsp.h ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../support/dsPIC33F/h/p33FJ64GP802.h ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../support/dsPIC33F/h/p33fxxxx.h DacTest.c
	$(CC) -mcpu=33FJ64GP802 -x c -c "DacTest.c" -o"DacTest.o" -D__DEBUG -g -Wall

test.o : fake
	$(CC) -mcpu=33FJ64GP802 -c -D__DEBUG "C:\Program Files\MDS\dsPICfdLite\Output\test.s" -o"test.o" -Wa,--defsym=__DEBUG=1,-g

traps.o : ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../support/dsPIC33F/h/p33FJ64GP802.h ../../../../program\ files/microchip/mplab\ c30/bin/bin/../../support/dsPIC33F/h/p33Fxxxx.h ../traps.c
	$(CC) -mcpu=33FJ64GP802 -x c -c "C:\Dev\PIC\Projects\traps.c" -o"traps.o" -D__DEBUG -g -Wall

clean : 
	$(RM) "DacTest.o" "test.o" "traps.o" "802_DacTest.cof" "802_DacTest.hex"

