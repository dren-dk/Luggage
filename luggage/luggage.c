#include "defines.h"

#include <ctype.h>
#include <inttypes.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include <avr/wdt.h> 
#include <avr/interrupt.h>
#include <avr/eeprom.h> 
#include <avr/pgmspace.h>

#include "uart.h"
#include "sleep.h"

#include "aux_globals.h"
#include "HD44780.h"

// We don't really care about unhandled interrupts.
EMPTY_INTERRUPT(__vector_default)


// A macro and function to store string constants in flash and only copy them to
// RAM when needed, note the limit on string length.

char stringBuffer[80];

const char *getString(PGM_P src) {
    //assert(strlen_P(src) < sizeof(stringBuffer));
    strcpy_P(stringBuffer, src);
    return stringBuffer;
}

#define PROGSTR(s) getString(PSTR(s))

unsigned int getADC(unsigned char input) {
    ADCSRA |= 1<<ADEN;

    _delay_ms(1);
    
    ADMUX = (input & 15) | _BV(REFS0); // AVcc reference + external cap.
    ADCSRA |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0) | _BV(ADIE);

    ADCSRA |= 1<<ADSC;
    while(ADCSRA & 1<<ADSC) {}
    
    unsigned int result = ADCL | ADCH << 8;
    return result;
}

#define ADC_OVERSAMPLES 6
unsigned int getOsADC(unsigned char input) {
    unsigned int sum = 0;

    for (int i=0;i<_BV(ADC_OVERSAMPLES);i++) {
	sum += getADC(input);
    }
    
    return sum >> ADC_OVERSAMPLES;
}


void led(char on) {
  if (on) {
    PORTB |= _BV(PB1);   
  } else {
    PORTB &=~ _BV(PB1);   
  }
}

void lcdInit() {
  lcd_init();   // init the LCD screen
  lcd_clrscr();	// initial screen cleanup
  lcd_home();
  lcd_instr(LCD_DISP_ON);
}


void lcdHello(char frame) {  
  if ((frame & 3) == 0) {
    lcdInit();
  }

  if (frame & 4) {
    lcd_string(PROGSTR("Henrik Frandsen "));

  } else {
    lcd_string(PROGSTR("  1 kW Kuffert  "));
  }

  lcd_home();
}

void lcdReadout(char watt) {
  
  float a = getOsADC(0)*100.0/(1<<10);
  int ai = a;
  int ad = trunc((a-ai)*100);

  float v = getOsADC(1)*30.0/(1<<10);
  int vi = v;
  int vd = trunc((v-vi)*100);

  float w = v*a;
  int wi = w;
  int wd = trunc((w-wi)*10);

  char buffy[17];
  memset(buffy, 0, sizeof(buffy));

  if (watt) {
    sprintf(buffy, PROGSTR("%2d.%02d V %4d.%01d W"), vi,vd, wi, wd);
  } else {
    sprintf(buffy, PROGSTR("%2d.%02d V %3d.%02d A"), vi,vd, ai,ad);
  }

  while (strlen(buffy) < 16) {
    strcat(buffy, " ");
  }

  lcd_string(buffy);
  //  lcd_string_format(PROGSTR("%d   %d  "), getOsADC(0), getOsADC(1));
  lcd_home();
}


int main(void) {
  wdt_enable(WDTO_4S);
  led(1);

  ADCSRA |= 1<<ADEN; // Enable ADC
  DDRB  |= _BV(PB1);  // LED output

  uart_init();
  FILE uart_str = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
  stdout = stdin = &uart_str;
  fprintf(stdout, PROGSTR("#Power up!\n"));

  sleepMs(2500);

  lcdInit();
  sleepMs(100);
  
  led(0);

  char frame = 0;
  char lcdState = 0;
  while(1) {
    if (!(frame & 15)) {
      fprintf(stdout, PROGSTR("OK\n"));
      //      lcd_clrscr();
      //lcd_home();
      //      lcd_string_format(PROGSTR("hest %d"), frame);
    }

    //    led(frame & 1); 

    if (lcdState == 0) {
      lcdHello(frame);      

      if (frame > 8) {
	++lcdState;
      }

    } else if (lcdState == 1 || lcdState == 2) {

      lcdReadout(lcdState == 2);      
      
      if (!(frame & 15)) {
	if (++lcdState > 2) {
	  lcdState = 1;
	}
      }
    }

    sleepMs(500);
    wdt_reset();
    frame++;
  }	
}
