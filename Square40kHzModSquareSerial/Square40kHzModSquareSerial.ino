#include <avr/sleep.h>
#include <avr/power.h>

byte pattern = 0b00001010; //(A0,A1) and (A2,A3) in push-pull

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
  
  //power_usart0_disable();
  Serial.begin(115200);
  Serial.println("40kHz square + square generation");
  Serial.println("Type a frequency to set the modulation");  
}

int currentTicks = 0, aTicks = 199, bTicks = 199; //no modulation by default
ISR(TIMER1_COMPA_vect){ 
  PORTC = pattern;

  if (currentTicks > bTicks){
    currentTicks = 0;
    pattern = 0b00001010;
  }else if (currentTicks > aTicks){
    pattern = 0;
  }else{
    pattern = ~pattern;
  }
  
  currentTicks += 1;
}



void loop(){
  if (Serial.available() > 0){
    const float fr = Serial.parseFloat();
    while (Serial.read() != '\n');
    if (fr == 0){
      Serial.println("No modulation");
      aTicks = bTicks = 199;
    }else{
      Serial.print("Setting frequency at ");
      Serial.println( fr );
      bTicks = 80000 / fr;
      aTicks = bTicks / 2; //always 50% duty cycle
      
      Serial.print(aTicks);
      Serial.print(" ");
      Serial.println(bTicks);
    }
  }
}
