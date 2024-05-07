#include <avr/sleep.h>
#include <avr/power.h>

#define N_SAMPLES 256
#define HALF_SAMPLES (N_SAMPLES/2)

#define MAX_VAL 80 //this is in uS, it is the period of the PWM freq minus some time for calculations
#define HALF_VAL (MAX_VAL/2)

#define PWM_FR 20000

#define N_SIGNALS 6
//square, sin, triang, sawRise, sawFall, custom
uint8_t signals[N_SIGNALS][N_SAMPLES]; //each sample is: 2 bits port masks | 6 bits microswait (0 to 63 at most)

inline uint8_t packSample(uint8_t val) {
  if (val >= HALF_VAL){
    return 0b01000000 | (val-HALF_VAL);  
  }else{
    return 0b10000000 | (HALF_VAL-val);
  }
}

//wave vlues obtained with wavepainter (the wave looks like this -->  ‾\)
const uint8_t cappedSawtooth[] = {98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,99,99,98,96,95,94,94,93,92,91,91,91,90,90,88,87,86,86,86,85,83,84,83,82,81,81,80,80,79,78,78,78,77,76,76,75,75,74,73,73,71,72,71,70,69,69,68,67,67,67,66,66,65,64,64,63,62,63,61,61,60,60,60,58,57,56,57,55,54,54,53,53,51,51,50,50,50,49,48,48,47,47,46,45,45,44,44,43,42,42,42,41,40,39,39,38,38,37,38,37,36,36,35,35,34,34,33,32,32,31,31,31,29,29,28,29,27,27,26,26,24,24,23,22,22,22,20,20,18,18,18,16,15,15,14,14,13,12,11,12,10,10,9,8,8,7,6,6,5,5,4,3,3,3,2,2,2,2};

  
void fillInSignals() {
  for (int i = 0; i < N_SAMPLES; ++i) {
    signals[0][i] = packSample( i < HALF_SAMPLES ? MAX_VAL : 0 ); //square wave
    const double v = cos( i * 2.0 * PI / N_SAMPLES ); //sinusoidal
    signals[1][i] = packSample( (byte) ((v + 1.0) / 2.0 * MAX_VAL) ); //map from -1,1  to  0,MAX
    signals[2][i] = packSample( abs( i*MAX_VAL/N_SAMPLES*2 - MAX_VAL ) ); //triangular
    //signals[3][i] = packSample( i * MAX_VAL / N_SAMPLES ); //saw-tooth rise
    //signals[4][i] = packSample( (N_SAMPLES-i) * MAX_VAL / N_SAMPLES ); //saw-tooth fall
    signals[3][i] = packSample( cappedSawtooth[i] * MAX_VAL / 100); // ‾\ wave
    signals[4][i] = packSample( (100-cappedSawtooth[i]) * MAX_VAL / 100); //   /‾  wave
    signals[5][i] = packSample( 0 ); //custom starts empty
  }
}

void setup() {
  fillInSignals();
  
  DDRC = 0b00000011; //set pins A0 to A1 as outputs
  PORTC = 0b00000000; //output low in all of them

  // initialize timer1 to interrupt at PWM_FR (usually 20kHz)
  noInterrupts();           // disable all interrupts
  TCCR1A = TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = (F_CPU / PWM_FR); 
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS10);    // 1 prescaler, no prescaling
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts

  // disable ADC
  ADCSRA = 0;

  // turn off everything we can
  power_adc_disable ();
  power_spi_disable();
  power_twi_disable();
  power_timer0_disable();

  //power_usart0_disable();
  Serial.begin( 57600 );

  //buttons
  pinMode( 2, INPUT_PULLUP );
  pinMode( 3, INPUT_PULLUP );
  pinMode( 4, INPUT_PULLUP );
  pinMode( 5, INPUT_PULLUP );
  
  /*for(int i = 0; i < N_SAMPLES; i++){
    Serial.print( signals[0][i] ); Serial.print(',');
    Serial.print( signals[1][i] ); Serial.print(',');
    Serial.print( signals[2][i] ); Serial.print(',');
    Serial.print( signals[3][i] ); Serial.print(',');
    Serial.print( signals[4][i] ); Serial.println();
  }*/
}

uint32_t indexShift24 = 0;
uint32_t indexIncShift24 = 4294967;
int currentSignal = 1; 
ISR(TIMER1_COMPA_vect) { // timer compare interrupt service routine
  static uint8_t portMask = 0b00000001;
  static uint8_t microsHigh = HALF_VAL;
  PORTC = portMask; //switch on the corresponding pins 
  delayMicroseconds(microsHigh);
  PORTC = 0b00000000; //all off

  //increase the index. overflow makes it cycle
  indexShift24 += indexIncShift24;
  
  //get sample
  const uint8_t sample = signals[currentSignal][indexShift24 >> 24];
  //extract pin mask
  portMask = sample >> 6;
  //extract wait micros
  microsHigh = sample & 0b00111111;
}

inline void stopSignalGen(){
  TIMSK1 &= ~(1 << OCIE1A); //disable the timer interrupt
}
inline void signalGen(int signalType, float fr){
  currentSignal = signalType % N_SIGNALS;
  TIMSK1 |= (1 << OCIE1A); //enable timer interrupt
  indexIncShift24 = (uint32_t) ( (float)((uint32_t)1<<24) * (float)N_SAMPLES * fr / (float)PWM_FR);
}

bool prevButtonPressed = false;
void loop() {
  //0 -> off
  //1 SIGNAL_TYPE FR -> start emitting SIGNAL_TYPE[0,1,2,3,4,5] with FR
  //2 val1 val2 ... val256 -> send a custom signal. 256 numbers from 0 to 100 are expected after the number 2
  if (Serial.available()) {
    const int command = Serial.parseInt();
    Serial.println( command );
    if (command == 0){
      stopSignalGen();
    }else if (command == 1){
      const int signalType = Serial.parseInt();
      const float fr = Serial.parseFloat();
      signalGen( signalType, fr );
    }else if (command == 2){
      stopSignalGen();
      for( int i = 0; i < N_SAMPLES; i++){
        const int val = Serial.parseInt();
        signals[5][i] = packSample( val * MAX_VAL / 100);
      }
    }else if (command == 3){
      stopSignalGen();
      Serial.read();
      for( int i = 0; i < N_SAMPLES; i++){
        const int val = Serial.read();
        signals[5][i] = packSample( val );
      }
    }
    
    while ( Serial.read() != '\n'); //skipt until finding the new line character
    Serial.println("9");
  }
  
  if (digitalRead(2) == LOW && prevButtonPressed == false){
    signalGen(3, 36.4);
    prevButtonPressed = true;
  }else if (digitalRead(3) == LOW && prevButtonPressed == false){
    signalGen(4, 36.4);
    prevButtonPressed = true;
  }else if (digitalRead(4) == LOW && prevButtonPressed == false){
    if (random(2) == 0){
      signalGen(3, 36.4);
    }else{
      signalGen(4, 36.4);
    }
    prevButtonPressed = true;
  }else if (digitalRead(5) == LOW && prevButtonPressed == false){
    stopSignalGen();
    prevButtonPressed = true;
  }else{
    prevButtonPressed = false;
  }
}
