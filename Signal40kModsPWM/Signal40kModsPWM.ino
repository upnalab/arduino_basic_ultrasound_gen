#include <avr/sleep.h>
#include <avr/power.h>

#define PERIODS_MOD 200
#define BUTTON_PIN 2

#define MAX_MICROS 12 //(12us is 50% duty on a 40kHz Wave)

#define HALF_PERIODS (PERIODS_MOD/2)
#define N_MODS 5
//square, sin, triang, sawRise, sawFall
byte microOnMod[N_MODS][PERIODS_MOD];


void fillInMods() {
  for (int i = 0; i < PERIODS_MOD; ++i) {
    microOnMod[0][i] = i < HALF_PERIODS ? MAX_MICROS : 0; //square wave

    const double v = cos( i * 2.0 * PI / PERIODS_MOD ); //sinusoidal
    microOnMod[1][i] = (byte) ((v + 1.0) / 2.0 * MAX_MICROS); //map from -1,1  to  0,MAX

    microOnMod[2][i] = abs( i*2*MAX_MICROS/PERIODS_MOD - MAX_MICROS ); //triangular

    microOnMod[3][i] = i * MAX_MICROS / PERIODS_MOD; //saw-tooth rise

    microOnMod[4][i] = MAX_MICROS - microOnMod[3][i]; //saw-tooth fall
  }
}

unsigned int microsHigh = 0;
int modIndex = 0;
int currentMod = 1;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  fillInMods();
  microsHigh = microOnMod[0];

  DDRC = 0b00001111; //set pins A0 to A3 as outputs
  PORTC = 0b00000000; //output low in all of them

  // initialize timer1
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 399;            // compare match register 16MHz / 400 = 40kHz
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
  Serial.begin(9600);

  noInterrupts(); 
  PORTC = 0b00000000;
}

ISR(TIMER1_COMPA_vect) { // timer compare interrupt service routine
  static volatile byte ciclesWait = MAX_MICROS;
  PORTC = 0b00001010;
  //while (--ciclesWait);
  delayMicroseconds(microsHigh);
  PORTC = 0b00000101;

  if (modIndex < PERIODS_MOD) {
    modIndex += 1;
  } else {
    modIndex = 0;
  }

  microsHigh = microOnMod[currentMod][modIndex];
  ciclesWait = microsHigh;
}


int previousButton = HIGH;
void loop() {
  if (Serial.available()) {
    const int nMod = Serial.parseInt();
    while ( Serial.read() != '\n');
    /*for (int i = 0; i < PERIODS_MOD; ++i) {
      Serial.println( microOnMod[currentMod][i] );
    }*/
    currentMod = nMod % N_MODS;
  }

  const int button = digitalRead(BUTTON_PIN);
  if (button == HIGH && previousButton == LOW){
    noInterrupts(); 
    PORTC = 0b00000000;
  }else if (button == LOW && previousButton == HIGH){
    interrupts();  
  }
  previousButton = button;
}
