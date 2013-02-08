#include <ctype.h>
#include <inttypes.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include <avr/wdt.h> 
#include <avr/interrupt.h>
#include <avr/eeprom.h> 
#include <avr/pgmspace.h>

#include "mstdio.h"
#include "adchelper.h"
#include "lcd.h"

// We don't really care about unhandled interrupts.
EMPTY_INTERRUPT(__vector_default)


// A macro and function to store string constants in flash and only copy them to
// RAM when needed, note the limit on string length.

char stringBuffer[80];

void setContrast(unsigned char value) {
  if (value) {    
    OCR1AL = value;
    TCCR1A |= _BV(COM1A1);
  } else {
    TCCR1A &=~ _BV(COM1A1);
  }
}


const char *getString(PGM_P src) {
    //assert(strlen_P(src) < sizeof(stringBuffer));
    strcpy_P(stringBuffer, src);
    return stringBuffer;
}

#define PROGSTR(s) getString(PSTR(s))

void led(char on) {
  if (on) {
    PORTB |= _BV(PB5);   
  } else {
    PORTB &=~ _BV(PB5);   
  }
}

void lcdInit() {
  lcd_init(LCD_DISP_ON);
  lcd_clrscr();	// initial screen cleanup
  lcd_home();
}


void lcdHello(char frame) {  
  if (frame & 4) {
    lcd_puts_p(PSTR("Henrik Frandsen "));

  } else {
    lcd_puts_p(PSTR("  1 kW Kuffert  "));
  }

  lcd_home();
}

char menu = 0;

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

  char buffy[27];
  memset(buffy, 0, sizeof(buffy));

  if (watt) {
    msprintf(buffy, PSTR("%2d.%02d V %4d.%d W"), vi,vd, wi,wd);
  } else {
    msprintf(buffy, PSTR("%2d.%02d V %3d.%02d A"), vi,vd, ai,ad);
  }

  while (strlen(buffy) < 16) {
    strcat(buffy, " ");
  }

  lcd_puts(buffy);

  if (!menu) {
    mprintf(PSTR("%2d.%02d V %3d.%02d A %4d.%d W\n"), vi,vd, ai,ad, wi,wd);
  }
  
  lcd_home();
}

char cmd[10];
int contrast = 0;

void pollMenu() {
  if (mchready()) {
    char ch = mgetch();
    if (menu) {

      if (menu <= 2) {
	if (ch == '-' || ch == '+') {
	  if (ch == '-') {
	    contrast -= 10;
	    if (contrast < 0) {
	      contrast = 0;
	    }
	  } else {
	    contrast += 10;
	    if (contrast > 255) {
	      contrast = 255;
	    }
	  }

	  menu = 1;

	  setContrast(contrast);

	  // TODO: Store contrast in EEPROM

	} else if (ch == 'q') {
	  menu = 0;
	}
      }

    } else if (ch == '\r') {
      menu = 1;
    }

    if (menu == 1) {
      mprintf(PSTR("Menu:\n +/-: Contrast: %d\n v: Voltage calibration\n a: Current calibration\n q: Quit\n"),
	      contrast);
      menu = 2;
    }
  }
}


int main(void) {
  wdt_enable(WDTO_4S);
  led(1);

  ADCSRA |= 1<<ADEN; // Enable ADC
  DDRB  |= _BV(PB5);  // LED output

  muartInit();
  mprintf(PSTR("#Power up!\n"));

  _delay_ms(100);

  lcdInit();
  _delay_ms(100);

  DDRB  |= _BV(PB1);  // Contrast PWM OC1A
  DDRB  |= _BV(PB3);  // Backlight PWM OC2A

  // Set up timer 1 for fast PWM mode & the highest frequency available
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);
  
  // TODO: Get contrast from EEPROM
  contrast = 0;

  setContrast(contrast);
  
  led(0);

  char frame = 0;
  char lcdState = 0;
  while(1) {

    pollMenu();

    if (!(frame & 15)) {
      //mprintf(PSTR("OK\n"));
      //      lcd_clrscr();
      //lcd_home();
      //      lcd_puts_format(PROGSTR("hest %d"), frame);
    }

    led(frame & 1); 

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
    
    for (char i=0;i<5;i++) {
      wdt_reset();
      _delay_ms(100);
    }

    wdt_reset();
    frame++;
  }	
}
