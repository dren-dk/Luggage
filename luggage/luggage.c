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
unsigned int adc2mv;
unsigned int adc2ma;
unsigned int currentCalibration;

void lcdReadout(char watt) {
  unsigned long aadc = getOsADC(0);
  unsigned int ma = (aadc*adc2ma) >> 8;
  int ai =  ma / 1000;
  int ad = (ma % 1000) / 10;

  unsigned long vadc = getOsADC(1);
  unsigned int mv = (vadc*adc2mv) >> 8;
  int vi =  mv / 1000;
  int vd = (mv % 1000) / 10;

  unsigned long uw = ma;
  uw *= mv;
  unsigned int mw = uw / 1000;
  
  int wi =  mw/1000;
  int wd = (mw%1000)/100;

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
    mprintf(PSTR("%2d.%02d V %3d.%02d A %4d.%d W %l vadc %d mV %l aadc %d mA\n"), vi,vd, ai,ad, wi,wd, 
	    vadc, mv, aadc, ma);
  }
  
  lcd_home();
}

char cmd[10];
int contrast;

void pollMenuOrDelay() {
  for (char i=0;i<5;i++) {
    wdt_reset();
    char ch = mchready() ? mgetch() : 0;

    if (!ch) {
      _delay_ms(100);
    }

    if (menu == 0) {
      if (ch == '\r') {
	menu = 1;
      }      

    } else if (menu == 1) {
      mprintf(PSTR("Menu:\n +/-: Contrast: %d\n v: Voltage calibration %d\n a: Current calibration %d\n q: Quit\n"),
	      contrast, adc2mv, adc2ma);
      menu = 2;

    } else if (menu == 2) {
      if (ch == '-' || ch == '+') {
	if (ch == '-') {
	  contrast -= 1;
	  if (contrast < 0) {
	    contrast = 0;
	  }
	} else {
	  contrast += 1;
	  if (contrast > 60) {
	    contrast = 60;
	  }
	}

	menu = 1;

	setContrast(contrast);

	// TODO: Store contrast in EEPROM

      } else if (ch == 'v') {
	menu = 3;

      } else if (ch == 'a') {
	menu = 5;

      } else if (ch == 'q') {
	menu = 0;
      }

    } else if (menu == 3 || menu == 5) {
      if (menu == 3) {
	mprintf(PSTR("Enter voltage in mV: "));
	menu = 4;
      } else {
	mprintf(PSTR("Enter current in mA: "));
	menu = 6;
      }
      currentCalibration = 0;
      
    } else if (menu == 4 || menu == 6) {
	
      if (ch >= '0' && ch <= '9') {
	currentCalibration *= 10;
	currentCalibration += ch - '0';
	mputchar(ch);
	
      } else if (ch == '\r') {

	if (currentCalibration < 1000) {
	  mputs("\r\nError: Value entered is too low for calibration, ignoring.\r\n");
	  menu = 1;

	} else if (menu == 4) {
	  unsigned adc = getOsADC(1);

	  if (adc < 100) {
	    mputs("\r\nError: Voltage too low for calibration\r\n");
	    menu = 3;

	  } else {
	    mputs("\r\nStored voltage calibration\r\n");
	
	    unsigned long mv = currentCalibration;
	    mv <<= 8;
	    mv /= adc;
	    adc2mv = mv;
	
	    // TODO: Store adc2mv in EEPROM
	  }
	} else {

	  unsigned int adc = getOsADC(0);
	  if (adc < 20) {
	    mputs("\r\nError: Current too low for calibration\r\n");
	    menu = 5;

	  } else {
	    mputs("\r\nStored current calibration\r\n");
	
	    unsigned long ma = currentCalibration;
	    ma <<= 8;
	    ma /= adc;
	    adc2ma = ma;
	
	    // TODO: Store adc2ma in EEPROM
	  }
	}
	
	menu = 1;
      }
      
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
  contrast = 30;
  setContrast(contrast);

  // TODO: Get adc2mv from EEPROM
  adc2mv = 6365;
  
  // TODO: Get adc2ma from EEPROM
  adc2ma = 11988;
   
  
  led(0);

  char frame = 0;
  char lcdState = 0;
  while(1) {


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
    
    pollMenuOrDelay();

    wdt_reset();
    frame++;
  }	
}
