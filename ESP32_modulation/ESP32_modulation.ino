/*
 * Ultrasonic Bowl v01 - ESP32 DoIT ESP32 DEVKIT
 */
#include <EEPROM.h>
#include "WiFi.h"

hw_timer_t *my_timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//pins (13 12 14 27) output: 0101, 1010
#define SET_BITS_PATTERN 0b00001000000000000001000000000000
#define CLEAR_BITS_PATTERN 0b00000000000000000110000000000000
int  SET_BITS =   SET_BITS_PATTERN;
int  CLEAR_BITS = CLEAR_BITS_PATTERN;

typedef struct {
  int aTicks, bTicks;
  float hz;
  int duty;
} DataForEProm;

DataForEProm de = {
    .aTicks = 200,
    .bTicks = 400,
    .hz = 200,
    .duty = 1
};

int currentTicks = 0;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux); //change pins 13 12 14 27 according to the masks
  (*((volatile uint32_t *) GPIO_OUT_W1TS_REG)) = SET_BITS;
  (*((volatile uint32_t *) GPIO_OUT_W1TC_REG)) = CLEAR_BITS;
  portEXIT_CRITICAL_ISR(&timerMux);

	//the output should be swaping for aTicks, and then off for bTicks
  if (currentTicks > de.bTicks) {  //restart
    currentTicks = 0;
    SET_BITS =   SET_BITS_PATTERN;
    CLEAR_BITS = CLEAR_BITS_PATTERN;
  } else if (currentTicks > de.aTicks) { //we do not emitt during the bticks
    SET_BITS = CLEAR_BITS = 0;
  } else {   //during aTicks we toogle the outputs
    uint32_t tmp = SET_BITS;
    SET_BITS = CLEAR_BITS;
    CLEAR_BITS = tmp;
  }

  currentTicks += 1;
}

void saveSettings(){
  EEPROM.put(0, de); 
  EEPROM.commit();
}

void loadSettings(){
  EEPROM.begin( sizeof(de) );
  EEPROM.get(0, de);
}

void showSettings(){
  Serial.printf("Read %d %d %.2f %d\n", de.aTicks, de.bTicks, de.hz, de.duty);
}

void setHzAndDuty(float hz, int duty){
    int bTicks = 80000 / hz; //the interrupt happens at 80kHz
    if (bTicks < 0) { bTicks = 1; }
    else if (bTicks > (1 << 24) ) { bTicks = 1 << 24; }
    
    int aTicks = bTicks * duty / 100;
    if (aTicks < 1){ aTicks = 1;}
    else if(aTicks > bTicks){ aTicks = bTicks; }

    de.aTicks = aTicks;
    de.bTicks = bTicks;
    de.hz = hz;
    de.duty = duty;
}


/* Network functions*/
const char* ssid = "Ultrasonic Bowl";
const char* password = "12345678";
WiFiServer server(80);

void configWifiAsAccessPoint(const char* ssidName) {
  Serial.println("Configuring as access point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP( ssidName, password );
  Serial.println("Wait 100 ms for AP_START...");
  delay(100);

  Serial.println("Set softAPConfig");
  IPAddress Ip(192, 168, 1, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}

void setup() {
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(27, OUTPUT);

  configWifiAsAccessPoint( ssid ); //start as access point with regular ssid
  server.begin();
  delay(200);
  
  my_timer = timerBegin(0, 40, true);
  timerAttachInterrupt(my_timer, &onTimer, true);
  timerAlarmWrite(my_timer, 25, true);
  timerAlarmEnable(my_timer);

  loadSettings();

  Serial.begin(115200);
  showSettings();
}

int parseFloats(const char* s, int startIndex, float target[], int maxNumbers);

void processCommand(String command) {
  if ( command.startsWith("mod") ) {
    float values[2];
    const int n = parseFloats( command.c_str(), command.indexOf('=') + 1 , values, 2);
    if (n == 2){
      setHzAndDuty(values[0], values[1]);
      showSettings();
    }
  } else if ( command.startsWith("save") ) {
    saveSettings();
    Serial.println("Settings Saved");
  }

}


void loop() {
   if (Serial.available() > 0){
    const float hz = Serial.parseFloat();
    const int duty = Serial.parseInt();
    while (Serial.read() != '\n');

    if (duty == -1){
      saveSettings();
    }else{
      setHzAndDuty(hz, duty);
      Serial.print(hz);
      Serial.print(" ");
      Serial.println(duty);
    }
    showSettings();
  }

  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    if (request.startsWith("GET /favicon.ico")){
      client.println("HTTP/1.1 200 OK\nContent-type:text/html\nConnection: close\n");
      client.println("<!DOCTYPE html><html />");
    }else{
      client.println("HTTP/1.1 200 OK\nContent-type:text/html\nConnection: close\n");
      
      const int sep1 = request.indexOf('/');
      if (sep1 != -1) {
        String command = request.substring(sep1 + 1);
        processCommand( command );
      }
      
      client.print("<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/></head><body><form action=\"/modulation\" method=\"get\"> Modulation (Hz 1 to 4000):<input type=\"text\" name=\"mod\" value=\""); 
      client.print(de.hz);
      client.print("\" /> Duty cycle(0 to 100): <input type=\"text\" name=\"duty\" value=\"");
      client.print(de.duty);
      client.println("\" /> <br/> <input type=\"submit\" value=\"Modulate\" /></form><br/><form action=\"/save\" method=\"get\"><input type=\"submit\" value=\"Save\" /></form> </body></html>");

    }
    client.println();
    client.stop();
  }
  
}


int parseFloats(const char* s, int startIndex, float target[], int maxNumbers) {
  int index = 0;

  float number = 0;
  bool haveSeenDecPoint = false;
  bool haveSeenMinus = false;
  float divider = 10;
  for (int i = startIndex; s[i] != '\0'; ++i) {
    char c = s[i];
    if ( c >= '0' && c <= '9') {
      if (! haveSeenDecPoint) {
        number = number * 10 + (c - '0');
      } else {
        number += divider * (c - '0');
        divider /= 10;
      }
    } else if ( c == '.') {
      haveSeenDecPoint = true;
      divider = 0.1;
    } else if (c == '-') {
      haveSeenMinus = true;
    }  else if (c==' ' || c== ',' || c== '\n' || c== '&'){
      if (haveSeenMinus) {
        number = -number;
      }
      target[index] = number;
      number = 0;
      haveSeenDecPoint = false;
      haveSeenMinus = false;
      divider = 0.1;
      index += 1;
      if (index == maxNumbers)
        break;
    }else{
      //just skip the other chars
    }
  }
  return index;
}
