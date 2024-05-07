#include <avr/sleep.h>
#include <avr/power.h>

byte pattern = 0b00001010; //A0,A1 and A2,A3 in push-pull

void setup()
{

 DDRC = 0b00001111; //set pins A0 to A3 as outputs
 PORTC = 0b00000000; //output low in all of them
 
  // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 199;            // compare match register 16MHz / 200 = 80kHz -> 40kHz square
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS10);    // 1 prescaler, no prescaling
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts

  // disable ADC
  //ADCSRA = 0;  
  
  // turn off everything we can
  power_adc_disable ();
  power_spi_disable();
  power_twi_disable();
  power_timer0_disable();
  
  power_usart0_disable();
  //Serial.begin(230400);
}

ISR(TIMER1_COMPA_vect){ // timer compare interrupt service routine
  PORTC = pattern;
  pattern = ~pattern;
}

void loop(){}
