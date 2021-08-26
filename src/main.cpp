/*! @mainpage
//-------------------------------------------------------------------------------------------------------
*/
#include <Arduino.h>
#include <EEPROM.h>
// https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/WString.cpp
// #include <string>
#include "strtod.h"

//-- DEBUG ---------------------------------------------------------------------
#define DEBUG_KEYPAD            false
#define DEBUG_FLOAT             false
#define DEBUG_STACK             false

#if DEBUG_FLOAT
#define PR(msg, value)          { Serial.print(msg); Serial.print(value); Serial.println('*'); }
#else
#define PR(msg, value)          {}           
#endif

#if DEBUG_STACK
#define PR_STACK(msg)           { Serial.print("STACK:"); \
                                  Serial.println(msg); \
                                  Serial.print("T:"); \
                                  Serial.println(t); \
                                  Serial.print("Z:"); \
                                  Serial.println(z); \
                                  Serial.print("Y:"); \
                                  Serial.println(y); \
                                  Serial.print("X:"); \
                                  Serial.println(x); \
                                  Serial.print("D:"); \
                                  Serial.println(display); \
                                  Serial.print("U:"); \
                                  Serial.println(userOPcompleted); } 
#else   
#define PR_STACK(msg)              {}         
#endif

//-- CONFIGURATIONS -------------------------------------------------------------
#define UART_ECHO               (0)
#define UART_BAUDRATE           (115200)
#define FLOAT_DECIMALS          (15)
#define DISPLAY_TRUNC_LEN       (12)
#define DISPLAY_USER_LEN        (TM_DISPLAY_SIZE * 2)
#define DISPLAY_MAX_USER_LEN    (DISPLAY_USER_LEN)
#define DISPLAY_BUFFER_LEN      (TM_DISPLAY_SIZE * 3)
#define SINGLE_PRECISION_32        (9)
#define SINGLE_PRECISION_32_FORMAT "%.9f"

//--- TM 1638 leds & keys -------------------------------------------------------------------------------
/*
  Project Name: TM1638plus (arduino library)
  File: TM1638plus_HELLOWORLD_TEST_Model1.ino
  Description: A Basic test file for model 2, TM1638 module(16 KEY 16 pushbuttons).
  Author: Gavin Lyons.
  Created Feb 2020
  URL: https://github.com/gavinlyonsrepo/TM1638plus
*/

// Model No 	Module Name 	    LEDS 	      Push buttons
//    1 	    TM1638 LED & KEY 	8 red only 	8
#include <TM1638plus.h>

// GPIO I/O pins on the Arduino connected to strobe, clock, data, pick on any I/O pin you want.
// attenzione: i pin per il framework arduino iniziano per D
#define STROBE_TM       D5   // strobe = GPIO connected to strobe line of module
#define CLOCK_TM        D6   // clock = GPIO connected to clock line of module
#define DIO_TM          D7   // data = GPIO connected to data line of module

// Constructor object Init the module
TM1638plus tm(STROBE_TM, CLOCK_TM , DIO_TM, false);
char    display[DISPLAY_BUFFER_LEN + 1];    // display and format buffers 33 bytes 
char    *displayBaseP = display;            // index per il scroll display
char    *displayCursor = display;

uint8_t functionBtn = 0;
uint8_t lastFunctionBtn = 0;

#define FUNCT_LED(number)    (number)

const char keymap[] = {'0', '1', '4', '7', '.', '2', '5', '8', 'D', '3', '6', '9'};

//--- BUZZER --------------------------------------------------------------------------------------------
#define BUZZER_PIN    D0

#include "EasyBuzzer.h"

// bug in library code
// https://github.com/evert-arias/EasyBuzzer/issues/1
void buzzerFinish() {
  pinMode(BUZZER_PIN, INPUT);
}

void buzzerFinishTest() {
  buzzerFinish();
  tm.setLEDs(0);
}

//--- capacitive keypad ---------------------------------------------------------------------------------
/*********************************************************
This is a library for the MPR121 12-channel Capacitive touch sensor

Designed specifically to work with the MPR121 Breakout in the Adafruit shop 
  ----> https://www.adafruit.com/products/

These sensors use I2C communicate, at least 2 pins are required 
to interface

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Limor Fried/Ladyada for Adafruit Industries.  
BSD license, all text above must be included in any redistribution
**********************************************************/

#include <Wire.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

//-------------------------------------------------------------------------------------------------------
#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino

//--- NTP CLIENT ----------------------------------------------------------------------------------------
#include <credentials.h> 
#include <ezTime.h>

#define INTERVAL_HOURS_MS(h)  (1000L * 60 * 60 * h)
#define INTERVAL_HOURS_S(h)   (60 * 60 * h)
#define NTP_OFFSET            (3600 * 2)
#define NTP_REFRESH_HOURS     INTERVAL_HOURS_MS(2)
#define NTP_SERVER_LOCAL      "europe.pool.ntp.org"

Timezone myTZ;

//--- MQTT CLIENT ---------------------------------------------------------------------------------------
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define MQTT_SERVER           "192.168.147.1"
#define MQTT_MSG_BUFFER_SIZE	(50)
#define MQTT_TOPIC_KEYBOARD   "home_bug_keyboard"
#define MQTT_TOPIC_DISPLAY    "home_bug_display"

WiFiClient   mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);
char mqttMsg[MQTT_MSG_BUFFER_SIZE + 1];
StaticJsonDocument<256> doc;
char mqttRXMsg[MQTT_MSG_BUFFER_SIZE + 1];
char *mqttRXMsgP = mqttRXMsg;
int mqttRXSeconds = 0;

void keyPublish(int8_t key) {
  // KEYPAD MAPPINGS:
  //  K3->7          K7->8           K11->9
  //  K2->4          K6->5           K10->6
  //  K1->1          K5->2           K9->3
  //  K0->0          K4->.           K8->del
  if (key >= 0) {
    // MQTT publish
    char bts[] = {keymap[key], 0};
    doc["key"] = bts;
    serializeJson(doc, mqttMsg);
    mqttClient.publish(MQTT_TOPIC_KEYBOARD, mqttMsg);
    //
  }
}

// MQTT client examples:
// mosquitto_sub -h 192.168.147.1 -t home_bug_keyboard
// result: {"key":"B0"} ... {"key":"B7"} ... {"key":"0"} ... 0-->9 . D
// mosquitto_pub -h 192.168.147.1 -t home_bug_display -m '{"msg" : "antonioooo", "sec": 60}'
// mosquitto_pub -h 192.168.147.1 -t home_bug_display -m '{"msg" : "PIOGGIA1234567890123", "sec": 25}'

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("I:MQTT:RX:T:");
  Serial.print(topic);
  Serial.print(":");
  if (!strcmp(topic, MQTT_TOPIC_DISPLAY)) {
    memcpy(mqttMsg, payload, length < MQTT_MSG_BUFFER_SIZE ? length : MQTT_MSG_BUFFER_SIZE);
    *(mqttMsg + length) = 0;
    DeserializationError err = deserializeJson(doc, mqttMsg);
    if (err == DeserializationError::Ok) {
      strcpy(mqttRXMsg, doc["msg"]);
      mqttRXSeconds = doc["sec"];
      mqttRXMsgP = mqttRXMsg;
      Serial.print("I:MQTT:msg:"); Serial.println(mqttRXMsg);
      Serial.print("I:MQTT:sec:"); Serial.println(mqttRXSeconds);
    } else {
      Serial.print("E:JSON:");
      Serial.println(err.f_str());
    }
  }
}

void mqttConnect() {
  if (!mqttClient.connected()) {
    Serial.println("I:MQTT:connection...");
    // Create a random client ID
    String clientId = "home-bug-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("I:MQTT:connected");
      // Once connected, publish an announcement...
      //mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe(MQTT_TOPIC_DISPLAY);
    } else {
      Serial.print("E:MQTT:failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

//-------------------------------------------------------------------------------------------------------
#include <TaskScheduler.h>

void welcome();
Task welcomeTask(3000, 4, &welcome);
void wallClock();
void wallClockEnable();
Task clockTask(1000, TASK_FOREVER, &wallClock);
void ntpRefreshClock();
Task ntpRefreshTask(NTP_REFRESH_HOURS, TASK_FOREVER, &ntpRefreshClock);
void calc();
void calcEnable();
void calcWelcome();
int8_t keypadBtnTouched();
Task calcTask(100, TASK_FOREVER, &calc);
Task calcWelcomeTask(1500, TASK_FOREVER, &calcWelcome);
void calcWelcomeOP();
Task calcWelcomeOPTask(1000, TASK_FOREVER, &calcWelcomeOP);
void mqttConnect();
Task mqttTask(5000, TASK_FOREVER, &mqttConnect);
void wifiConnect();
Task wifiTask(5000, TASK_FOREVER, &wifiConnect);
Scheduler runner;

uint8_t welcomeState = 0;
bool welcomeFirst = true;

uint8_t calcWelcomeState = 0;
uint8_t calcWelcomeOPState = 0;

void welcome() {
  if (welcomeState == 0) {
    welcomeState++;
    tm.displayText("HOME BUG");
  } else if (welcomeState == 1) {
    tm.displayText(" SALVE  ");
    welcomeState++;
    if (welcomeFirst) {
      welcomeFirst = false;
    } else {
      welcomeState = 0;
    }
  } else if (welcomeState == 2) {
    tm.displayText("2021.06.27");
    welcomeState = 0;
  }
  if (welcomeTask.isLastIteration()) {
      welcomeTask.disable();
      wallClockEnable();
  }
};

bool userDigitPoint = false;
bool userOPcompleted = false;

void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) {
    if (timeStatus() != timeSet) {
      Serial.print("I:WIFI:IP:");
      Serial.println(WiFi.localIP());
      // NTP begin
      Serial.println("I:NTP:START");
      myTZ.setLocation("Europe/Rome");
      ezt::setDebug(INFO);
      ezt::setServer(NTP_SERVER_LOCAL);
      ezt::setInterval(INTERVAL_HOURS_S(6));
      ezt::updateNTP();
      ntpRefreshClock();
      // NTP end  
    }
  } else {
    Serial.println("I:WIFI:DISC");
    Serial.println("I:WIFI:START");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// cpp reference
// https://en.cppreference.com/w/c/string/byte/strchr
//
void trimRightDisplay() {
  // trim decimal right zeroes
  PR("C:", display);
  const char* decimalP = strchr(display, '.');
  if (decimalP != 0 ) {
    PR("FOUND:", decimalP);
    char *p = display + strlen(display) - 1;
    while (p >= decimalP) {
      PR("FOUND C:", *p);
      if (*p != '0') {
        if (*p == '.') {
          if (userDigitPoint) {
            *(p + 1) = 0;
            break;
          }
          *p = 0;
        }
        break;
      } else {
        *p = 0;
      }
      p--;
    }
  }
  PR("D:", display);
}

void clearDisplay() {
  tm.displayText("        "); // TM_DISPLAY_SIZE
}

void trimLeftDisplay() {
  // trim decimal left zeroes
  PR("A:", display);
  if (strcmp(display, "0.")) {
    char buf[DISPLAY_USER_LEN + 1];
    const char *p = buf;
    strcpy(buf, display);
    while (!*p) {
      if (*p != '0') {
        strcpy(display, p);
        break;
      }
      p++;
    }
  }
  PR("B:", display);
}

void trimDisplay() {
  trimRightDisplay();
  trimLeftDisplay();
}

void displayFloat() {
  trimDisplay();
  PR("E:", display);
  uint8_t len = 0;
  char *p = display;
  while (*p != 0) {
    if (isDigit(*p++)) len++; // escludere il punto, che è embedded nella cifra
  }
  if (len > TM_DISPLAY_SIZE) {
    tm.setLED(FUNCT_LED(7), 1);
  } else {
    tm.setLED(FUNCT_LED(7), 0);
  }
  clearDisplay();
  tm.displayText(display);
}

void registerRound(double &value, const uint32_t &to) {
  uint32_t places = 1, whole = *(&value);

  for (uint32_t i = 0; i < to; i++) places *= 10;
  value -= whole;           // leave decimals
  value *= places;          // 0.1234 -> 123.4
  value  = round(value);    // 123.4 -> 123
  value /= places;          // 123 -> .123
  value += whole;           // bring the whole value back
}

void registerToDisplay(double value) {
  PR("F1:", value);
  registerRound(value, SINGLE_PRECISION_32);
  snprintf(display, DISPLAY_BUFFER_LEN, SINGLE_PRECISION_32_FORMAT, value);
  display[DISPLAY_TRUNC_LEN + 1] = 0;
  PR("F2:", display);
  trimDisplay();
  PR("F3:", display);
}

double displayToRegister() {
  return p_strtod(display, NULL);
}

int hh = 0;
int mm = 0;
int ss = 0;
bool clockIndicator = false;
bool clockDisplay = false;
bool clockExtended = false;
bool clockBing = false;
bool clockTest = false;
uint8_t clockDate = 0;

const char *monthc[] = { "GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC" };
const char *dayc[]   = { "DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB" };

void ntpRefreshClock() {
  if (WiFi.status() == WL_CONNECTED && ezt::timeStatus() == timeSet) {
    Serial.println("I:NTP REFRESH");
    hh = myTZ.hour();
    mm = myTZ.minute();
    ss = myTZ.second();
  }
}

void wallClock() {
  if (clockDisplay) {
    // MQTT message, request display
    if (--mqttRXSeconds > 0) {
      uint8_t len = strlen(mqttRXMsg);
      clearDisplay();
      tm.displayText(mqttRXMsgP);
      if (len > TM_DISPLAY_SIZE) {
        if (++mqttRXMsgP > mqttRXMsg + len) {
            mqttRXMsgP = mqttRXMsg;
        };
      }
      return;
    }

    if (clockDate > 0) {
      // display calendar
      clockDate--;
      snprintf(display, TM_DISPLAY_SIZE + 1, "%2d", myTZ.day());
      if (clockIndicator) strcat(display, ".");
      strcat(display, monthc[myTZ.month()-1]);
      if (clockIndicator) strcat(display, ".");
      strcat(display, dayc[myTZ.weekday()-1]);
    } else if (clockExtended) {
      snprintf(display, TM_DISPLAY_SIZE + 1, "%2d%1s%02d%1s%02d", \
                hh, clockIndicator ? ":" : "-", mm, \
                clockIndicator ? ":" : "-", ss);
    } else {
      snprintf(display, TM_DISPLAY_SIZE + 1, "%2d%1s%02d   ", \
                hh, clockIndicator ? ":" : "-", mm);
    }
    tm.displayText(display);
    if (hh >= 8 && hh <= 22 && clockBing) {
      if (mm == 0 && ss == 0) {
        Serial.println("B2:");
        EasyBuzzer.singleBeep(432, 500, &buzzerFinish);
      } else {
        Serial.println("B3:");
        EasyBuzzer.singleBeep(432, 250, &buzzerFinish);
      }
    }
  }

  if (clockTest) {
    clockTest = false;
    EasyBuzzer.singleBeep(432, 500, &buzzerFinishTest);
    tm.displayText("8.8.8.8.8.8.8.8.");
    tm.setLEDs(0xFFFF);
  }

  clockIndicator = !clockIndicator;
  if (++ss > 59) {
    ss = 0;
    if (++mm > 59) {
      mm = 0;
      if (++hh > 23) {
        hh = 0;
      }
    }
  }

  if (((mm == 59 || mm == 29) && ss >= 55) ||
      ((mm == 0 || mm == 30) && ss <= 5)) {
    clockExtended = true;
  } else {
    clockExtended = false;
  }
  if ((mm == 59 && ss >= 56) ||
      (mm == 0 && ss == 0)) {
    clockBing = true;
    Serial.println("B1:");
  } else {
    clockBing = false;
  }
};

void wallClockEnable() {
  clockDisplay = true;
  tm.setLEDs(0);
  tm.displayText("        ");
}

void wallClockDisable() {
  clockDisplay = false;
}

// stack registers
double t = 0;
double z = 0;
double y = 0;
double x = 0; // on display
char op = ' ';

void displayResetZero() {
  PR("displayResetZero", "");
  display[0] = '0';
  display[1] = 0;
  displayCursor = display;
  userDigitPoint = false;
  // reset display overflow indicator + display at home position
  tm.setLED(FUNCT_LED(7), 0);
  displayBaseP = display;
}

void pushStackRegister() {
  t = z; z = y; y = x;
}

void popStackRegister() {
   y = z; z = t;
}

void calc() {
  switch (op) {
    case '+':
      if (userOPcompleted) {
        popStackRegister();
      } else {
        x = displayToRegister();
      }
      PR_STACK("E");
      x = y + x;
      op = 0;
      registerToDisplay(x);
      displayFloat();
      userOPcompleted = true;
      popStackRegister();
      pushStackRegister();
      PR_STACK("U");
      return;
    case '-':
      if (userOPcompleted) {
        popStackRegister();
      } else {
        x = displayToRegister();
      }
      PR_STACK("E");
      x = y - x;
      op = 0;
      registerToDisplay(x);
      displayFloat();
      userOPcompleted = true;
      popStackRegister();
      pushStackRegister();
      PR_STACK("U");
      return;
    case '*':
      if (userOPcompleted) {
        popStackRegister();
      } else {
        x = displayToRegister();
      }
      PR_STACK("E");
      x = y * x;
      op = 0;
      registerToDisplay(x);
      displayFloat();
      userOPcompleted = true;
      popStackRegister();
      pushStackRegister();
      PR_STACK("U");
      return;
    case '/':
      if (userOPcompleted) {
        popStackRegister();
      } else {
        x = displayToRegister();
      }
      PR_STACK("E");
      x = y / x;
      op = 0;
      registerToDisplay(x);
      displayFloat();
      userOPcompleted = true;
      popStackRegister();
      pushStackRegister();
      PR_STACK("U");
      return;
    case '=': // ENTER push button
      x = displayToRegister();
      PR_STACK("E");
      pushStackRegister();
      op = 0;
      userOPcompleted = true;
      PR_STACK("U");
      return;      
  }

  // KEYPAD MAPPINGS:
  //  K3->7          K7->8           K11->9
  //  K2->4          K6->5           K10->6
  //  K1->1          K5->2           K9->3
  //  K0->0          K4->.           K8->del
  int8_t key = keypadBtnTouched();
  if (key >= 0) {
    PR("KEY:", keymap[key]);
    keyPublish(key);
    if (userOPcompleted) {
      userOPcompleted = false;
      clearDisplay();
      displayResetZero();
    }
    switch (keymap[key]) {
      case 'D':
        if (displayCursor > display) {
          clearDisplay();           // il TM1638 non cancella la cifra precedente, va impostato a blank
          if (*(--displayCursor) == '.') {
            userDigitPoint = false;
          }
          *displayCursor = ' ';     // il TM1638 non cancella la cifra precedente, va impostato a blank
          *(displayCursor + 1) = 0;
          if (userDigitPoint && *displayCursor == ' ') {
            *displayCursor = 0;
          }
          if (displayCursor > display + TM_DISPLAY_SIZE) {
            if (displayBaseP > display) {
                displayBaseP--;
                if (displayBaseP > display && *displayBaseP == '.') {
                  displayBaseP--;
                }
            }
            // reset display overflow indicator ? 
            uint8_t len = 0;
            char *p = display;
            while (*p != 0) {
              if (isDigit(*p++)) len++; // escludere il punto, che è embedded nella cifra
            }
            if (len <= TM_DISPLAY_SIZE) {
              tm.setLED(FUNCT_LED(7), 0);
            }
          } else {
            // reset display overflow indicator + display at home position
            tm.setLED(FUNCT_LED(7), 0);
            displayBaseP = display;
          }
          tm.displayText(displayBaseP);
        }
        break;
      case '.':
        if (displayCursor >= display + DISPLAY_MAX_USER_LEN + userDigitPoint ? 1 : 0) break;
        if (!userDigitPoint) {
          userDigitPoint = true;
          if (displayCursor == display) {
            *(displayCursor++) = '0';       // un float che inizia per punto è preceduto dallo zero
          }
          *(displayCursor++) = '.';
          *displayCursor = 0;
          if (displayCursor > display + TM_DISPLAY_SIZE) {
            // set display overflow indicator + display right scroll
            tm.setLED(FUNCT_LED(7), 1);
            displayBaseP++;
          }
        }
        break;
      default:
        if (displayCursor >= display + DISPLAY_MAX_USER_LEN + userDigitPoint ? 1 : 0) break;
        if (!userDigitPoint && keymap[key] == '0' && *display == '0') break;  // una sola cifra zero prima del punto più a sinistra
        *(displayCursor++) = keymap[key];
        *displayCursor = 0;
        if (displayCursor > display + TM_DISPLAY_SIZE + userDigitPoint ? 1 : 0) {
            // set display overflow indicator + display right scroll
            tm.setLED(FUNCT_LED(7), 1);
            displayBaseP++;
            if (*displayBaseP == '.') {
              displayBaseP++;
            }
        } 
        break;     
    }
    if (displayCursor == display) {
      displayResetZero();
    }
    tm.displayText(displayBaseP);
    PR("Y1:", display);
    PR("Y2:", displayBaseP);
  }
}

void calcWelcome() {
  switch (calcWelcomeState) {
    case 0:
      tm.setLEDs(0);
      tm.setLED(FUNCT_LED(0), 1);
      tm.displayText("  CALC  ");
      calcWelcomeState++;
      break;
    case 1:
      clearDisplay();
      displayResetZero();
      calcWelcomeTask.disable();
      runner.deleteTask(calcWelcomeTask);
      calcWelcomeState = 0;
      // calc start
      registerToDisplay(x);
      displayFloat();
      calcTask.enable();
      PR("X:", x);
      break;
  }  
}

void calcWelcomeOP() {
  switch (calcWelcomeOPState) {
    case 0:
      switch (op) {
        case 'S':
          tm.displayText("CHN SIGN");
          break;
        case '+':
          tm.displayText("  ADD   ");
          break;
        case '-':
          tm.displayText("  SUB   ");
          break;
        case '*':
          tm.displayText("  MUL   ");
          break;
        case '/':
          tm.displayText("  DIV   ");
          break;
        case '=':
          tm.displayText(" ENTER  ");
          break;
      }
      calcWelcomeOPState++;
      break;  
    case 1:
      clearDisplay();
      tm.displayText(displayBaseP);
      calcWelcomeOPState = 0;
      calcWelcomeOPTask.disable();
      calcTask.enable();
      break;
  }
}

void calcWelcomeOPEnable() {
  calcTask.disable();
  calcWelcomeOPState = 0;
  runner.addTask(calcWelcomeOPTask);
  calcWelcomeOPTask.enable();
}

void calcEnable() {
  runner.addTask(calcWelcomeTask);
  calcWelcomeTask.enable();
}

void calcDisable() {
  calcTask.disable();
  tm.setLED(FUNCT_LED(0), 0);
}

int8_t keypadBtnTouched() {
  int8_t btn = -1;
  // Get the currently touched pads
  currtouched = cap.touched();
  for (uint8_t i=0; i<12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      btn = i;
      break;
    }
    ESP.wdtFeed();
  }
  // reset our state
  lasttouched = currtouched;

  if (DEBUG_KEYPAD) {
    // debugging info, what
    Serial.print("0x"); 
    Serial.println(cap.touched(), HEX);
    Serial.print("Filt: ");
    for (uint8_t i=0; i<12; i++) {
      Serial.print(cap.filteredData(i)); Serial.print("\t");
    }
    Serial.println();
    Serial.print("Base: ");
    for (uint8_t i=0; i<12; i++) {
      Serial.print(cap.baselineData(i)); Serial.print("\t");
    }
    Serial.println();
    
    // put a delay so it isn't overwhelming
    ESP.wdtFeed();
  }

  return btn;
}

//-------------------------------------------------------------------------------------------------------

int8_t keyBtnPressed() {
  int8_t btn = -1;
  // Get the currently touched pads
  functionBtn = tm.readButtons();
  if (functionBtn) {
    for (uint8_t i=0; i<8; i++) {
      // it if *is* pressed and *wasnt* pressed before, alert!
      if ((functionBtn & _BV(i)) && !(lastFunctionBtn & _BV(i)) ) {
        btn = i;
        break;
      }
      ESP.wdtFeed();
    }
  }
  // reset our state
  lastFunctionBtn = functionBtn;
  return btn;
}

//-------------------------------------------------------------------------------------------------------

void setup() {
  ESP.wdtDisable();

  Serial.begin(UART_BAUDRATE);
  // needed to keep leonardo/micro from starting too fast!
  while (!Serial) { delay(10); }
  
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap.begin(0x5A)) {
    Serial.println("E:MPR121 not found");
    while (1);
  }
  Serial.println("I:START");
  cap.setThresholds(70, 100);
  ESP.wdtEnable(1000);

  // MQTT begin
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setCallback(mqttCallback);
  // MQTT end

  // BUZZER
  EasyBuzzer.setPin(BUZZER_PIN);

  tm.displayBegin();
  displayResetZero();
  
  runner.init();
  runner.addTask(welcomeTask);
  runner.addTask(clockTask);
  runner.addTask(calcTask);
  runner.addTask(ntpRefreshTask);
  runner.addTask(mqttTask);
  runner.addTask(wifiTask);
  ntpRefreshTask.enable();
  welcomeTask.enable();
  clockTask.enable();
  mqttTask.enable();
  wifiTask.enable();
}

//-------------------------------------------------------------------------------------------------------
void loop() {
  ESP.wdtFeed();
  runner.execute();
  ezt::events();
  mqttClient.loop();
  EasyBuzzer.update();
  
  int8_t btn = keyBtnPressed();
  if (btn >= 0) {
    // MQTT publish
    char bts[] = {'B', (char)('0' + btn), 0};
    doc["key"] = bts;
    serializeJson(doc, mqttMsg);
    mqttClient.publish(MQTT_TOPIC_KEYBOARD, mqttMsg);
    //
    switch (btn) {
      case 0:
        if (calcTask.isEnabled()) {
          calcDisable();
          wallClockEnable();       
        } else {
          wallClockDisable();
          calcEnable();
        }
        break;
      case 1:
        if (calcTask.isEnabled()) {
          op = 'S';
          calcWelcomeOPEnable();
        }
        break;
      case 2:
        break;
      case 3:
        if (calcTask.isEnabled()) {
          op = '+';
          calcWelcomeOPEnable();
        }
        break;
      case 4:
        if (calcTask.isEnabled()) {
          op = '-';
          calcWelcomeOPEnable();
        }
        break;
      case 5:
        if (calcTask.isEnabled()) {
          op = '*';
          calcWelcomeOPEnable();
        }
        break;
      case 6:
        if (calcTask.isEnabled()) {
          op = '/';
          calcWelcomeOPEnable();
        } else {
          // test buzzer & leds
          clockTest = true;
        }
        break;
      case 7:
        if (calcTask.isEnabled()) {
          op = '='; // ENTER
          calcWelcomeOPEnable();
        } else {
          clockDate = 4;
        }
        break;
    }
  }
  if (!calcTask.isEnabled()) {
    keyPublish(keypadBtnTouched());
  }
}