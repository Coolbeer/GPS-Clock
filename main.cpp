#define F_CPU 10000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "t_ks0108.h"

t_ks0108 lcd;

volatile char buffer[128];
volatile uint8_t bufferHead = 0;
volatile uint8_t bufferTail = 0;
volatile char time[7] = {'0','0','0','0','0','0','\0'};
volatile char date[7] = {'0','0','0','0','0','0','\0'};
volatile char debug = 'a';
volatile bool fin = false;

inline void tunedDelay(uint16_t delay) 
{ 
  uint8_t tmp=0;

  asm volatile("sbiw    %0, 0x01 \n\t"
    "ldi %1, 0xFF \n\t"
    "cpi %A0, 0xFF \n\t"
    "cpc %B0, %1 \n\t"
    "brne .-10 \n\t"
    : "+w" (delay), "+a" (tmp)
    : "0" (delay)
    );
}

void init(void)
{
    DDRB &= (1 << PB0);
    DDRB |= (1 << PB1);                //Output
    SETPIN(B, 1);
    CLEARPIN(B, 0);
    PCICR |= (1 << PCIE0);             //Pin change interrupt 0 enable
    PCMSK0 |= (1 << PCINT0);         //Port B0 active
    sei();                            //Enable global interrupts
}

void tossByte(volatile uint8_t eb)
{
    _delay_ms(1);
    CLEARPIN(B, 1);
    tunedDelay(150);
    for(volatile uint8_t i = 0x01; i; i <<= 1)
    {
        if(eb & i)
            SETPIN(B, 1);
        else
            CLEARPIN(B, 1);
        tunedDelay(147);
    }
    SETPIN(B, 1);
    tunedDelay(147);
}

void transmit(volatile char *text)
{
    cli();
    while(*text != 0)
    {
        tossByte(*text);
        ++text;
    }
    tossByte(0x0D);
    tossByte(0x0A);
    sei();
}

void getTimeandDate()
{
    volatile uint8_t desmo = 0;
    volatile bool haveTime = false;
    if(buffer[0] != '$' && buffer[1] != 'G' && buffer[2] != 'P' && buffer[3] != 'R' && buffer[4] != 'M' && buffer[5] != 'C')
    {
        debug = 'b';
        return;
    }
    debug = 'c';
    for(volatile uint8_t i = 0; i != bufferTail-bufferHead; ++i)
    {
        if(buffer[i] == ',')
        {
            debug = 'd';
            desmo += 1;
        }
        else if(desmo == 1 && !haveTime)
        {
            debug = 'e';
            time[0] = buffer[i];
            time[1] = buffer[i+1];
            time[2] = buffer[i+2];
            time[3] = buffer[i+3];
            time[4] = buffer[i+4];
            time[5] = buffer[i+5];
            i += 6;
            //lcd.writeString((char*)buffer);
            haveTime = true;
        }
        else if(desmo == 9)
        {
            debug = 'f';
            date[0] = buffer[i];
            date[1] = buffer[i+1];
            date[2] = buffer[i+2];
            date[3] = buffer[i+3];
            date[4] = buffer[i+4];
            date[5] = buffer[i+5];
            i += 6;
            break;
        }
    }
    return;
}

int main(void)
{
    lcd.init();
    lcd.drawRectangle(2,1,125,6);
    
    init();
    transmit((volatile char*)"$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29");
    transmit((volatile char*)"$PMTK220,1000*1F");
    while(true)
    {
        if(fin)
        {
            cli();
            getTimeandDate();
            lcd.gotoXY(40,3);
            lcd.putChar(time[0]);
            lcd.putChar(time[1]);
            lcd.putChar(':');
            lcd.putChar(time[2]);
            lcd.putChar(time[3]);
            lcd.putChar(':');
            lcd.putChar(time[4]);
            lcd.putChar(time[5]);
            lcd.gotoXY(40, 4);            
            lcd.putChar(date[0]);
            lcd.putChar(date[1]);
            lcd.putChar('.');
            lcd.putChar(date[2]);
            lcd.putChar(date[3]);
            lcd.putChar('.');
            lcd.putChar(date[4]);
            lcd.putChar(date[5]);
            bufferHead = bufferTail = 0;
            fin = false;
            sei();
        }    
    }
    return 0;
}

ISR(PCINT0_vect)
{
    volatile uint8_t d = 0;
    if(!(PINB &(1 << PB0)))        //PB0 went low
    {
        tunedDelay(73);
        for(volatile uint8_t i=0x1; i; i <<= 1)
        {
            tunedDelay(148);
            if (PINB &(1 << PB0))
                d |= i;
        }
        tunedDelay(148); //Skip the stop bit
        if(bufferTail < 127)
        {
            buffer[bufferTail] = d;
            ++bufferTail;
            if(d == 10)
                fin = true;
        }
    }
}
