/*! @mainpage
//-------------------------------------------------------------------------------------------------------
*/
#include <Arduino.h>
#include <string>
#include "strtod.h"

//-- DEBUG ---------------------------------------------------------------------
#define DEBUG_KEYPAD        false
#define UART_ECHO           (0)
#define UART_BAUDRATE       (115200)
#define FLOAT_DECIMALS      (15)

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
char display[TM_DISPLAY_SIZE + 1];

uint8_t functionBtn = 0;
uint8_t lastFunctionBtn = 0;

#define FUNCT_LED(x)    (x)

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

//-------------------------------------------------------------------------------------------------------
// #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

// //needed for library
// #include <DNSServer.h>
// #include <ESP8266WebServer.h>
// #include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager

// void configModeCallback (WiFiManager *myWiFiManager) {
//   Serial.println("Entered config mode");
//   Serial.println(WiFi.softAPIP());
//   //if you used auto generated SSID, print it
//   Serial.println(myWiFiManager->getConfigPortalSSID());
// }

//-------------------------------------------------------------------------------------------------------
#include <TaskScheduler.h>

void welcome();
Task welcomeTask(3000, 4, &welcome);
void wallClock();
void wallClockEnable();
Task clockTask(1000, TASK_FOREVER, &wallClock);
void calc();
void calcEnable();
int8_t keypadBtnTouched();
Task calcTask(100, TASK_FOREVER, &calc);
Scheduler runner;

uint8_t welcomeState = 0;
bool welcomeFirst = true;

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
    tm.displayText("20210627");
    welcomeState = 0;
  }
  if (welcomeTask.isLastIteration()) {
      welcomeTask.disable();
      wallClockEnable();
  }
};

String trimRightZeroesFloat(String buf) {
  // trim decimal right zeroes
  int8 decimalIndex = buf.indexOf('.');
  if (decimalIndex > 0 ) {
    for (uint8_t i = buf.length() - 1; i >= decimalIndex; i--) {
      char c = buf.charAt(i);
      if (c != '0') {
        if (c == '.') buf.remove(i, 1);
        break;
      }
      buf.remove(i, 1);
    }
  }
  return buf;
}

String trimLeftZeroesFloat(String buf) {
  // trim decimal left zeroes
  Serial.println("A:" + buf + '*');
  for (uint8_t i = 0; i < buf.length(); i++) {
    if (buf.charAt(i) != '0') {
      if (i > 0) buf.remove(0, i);
      break;
    }
  }
  Serial.println("B:" + buf + '*');
  return buf;
}

String trimZeroesFloat(String buf) {
  buf = trimRightZeroesFloat(buf);
  buf = trimLeftZeroesFloat(buf);
  return buf;
}

void displayFloat(String buf) {
  buf = trimZeroesFloat(buf);
  Serial.println("7:" + buf + '*');
  uint8_t len = buf.length();
  buf += "        "; // TM_DISPLAY_SIZE
  if (len > TM_DISPLAY_SIZE) {
    tm.setLED(FUNCT_LED(7), 1);
  } else {
    tm.setLED(FUNCT_LED(7), 0);
  }
  buf.toCharArray(display, TM_DISPLAY_SIZE);
  tm.displayText(display);
}

String doubleTrimRightZeroes(double value) {
  String buf = String(value, FLOAT_DECIMALS);
  return trimZeroesFloat(buf);
}

double stringToDouble(String buf) {
  char cbuf[33];

  buf.toCharArray(cbuf, 33);
  return p_strtod(cbuf, NULL);
}

uint8_t hour = 0;
uint8_t minutes = 0;
uint8_t seconds = 0;
bool clockIndicator = false;
bool clockDisplay = false;

void wallClock() {
  if (clockDisplay) {
    snprintf(display, TM_DISPLAY_SIZE + 1, "%2d%1s%02d", hour, clockIndicator ? ":" : "-", minutes); 
    tm.displayText(display);
  }

  clockIndicator = !clockIndicator;
  if (++seconds > 59) {
    seconds = 0;
    if (++minutes > 59) {
      minutes = 0;
      if (++hour > 23) {
        hour = 0;
      }
    }
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

// void displayXB() {
//   String buf = "        " + xb;
//   buf = buf.substring(buf.length() - TM_DISPLAY_SIZE + 1);
//   buf.toCharArray(display, TM_DISPLAY_SIZE + 1);
//   tm.displayText(display);
// }

void calc() {
  switch (op) {
    case ' ':
      break;
    case '+':
      x += y; y = z; z = t; t = 0;
      break;
    case '-':
      x -= y; y = z; z = t; t = 0;
      break;
    case '*':
      x *= y; y = z; z = t; t = 0;
      break;
    case '/':
      x /= y; y = z; z = t; t = 0;
      break;
    case '=':
      t = z; z = y; y = x;
      break;      
  }
  op = ' ';

// KEYPAD MAPPINGS:
//  K03->7          K07->8           K11->9
//  K02->4          K06->5           K10->6
//  K01->1          K05->2           K09->3
//  K00->0          K04->.           K08->del
  int8_t key = keypadBtnTouched();
  if (key >= 0) {
    String buf = doubleTrimRightZeroes(x);
    Serial.println("1:" + buf);
    char keymap[] = {'0', '1', '4', '7', '.', '2', '5', '8', 'D', '3', '6', '9'};
    switch (keymap[key]) {
      case 'D':
        if (buf.length() > 0) {
          buf.remove(buf.length() - 1, 1);
        }
        break;
      case '.':
        if (buf.indexOf('.') < 0) {
          buf += keymap[key];
        }
        break;
      default:
        buf += keymap[key];
        break;     
    }
    // write to register
    x = stringToDouble(buf);
    Serial.println("3:" + buf);
    if (errno == ERANGE) {
      Serial.println("2:" + errno);
    };
    displayFloat(buf);
  }
}

void calcEnable() {
  calcTask.enable();
  tm.setLEDs(0);
  tm.setLED(FUNCT_LED(0), 1);
  String buf = String(x, FLOAT_DECIMALS);
  displayFloat(buf);
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
  Serial.println("I:MPR121 found!");
  cap.setThresholds(70, 100);
  ESP.wdtEnable(1000);

  tm.displayBegin();
  
  runner.init();
  runner.addTask(welcomeTask);
  runner.addTask(clockTask);
  runner.addTask(calcTask);
  welcomeTask.enable();
  clockTask.enable();
}

//-------------------------------------------------------------------------------------------------------
void loop() {
  ESP.wdtFeed();
  runner.execute();
  
  int8_t btn = keyBtnPressed();
  if (btn >= 0) {
    switch (btn) {
      case 0:
        wallClockDisable();
        calcEnable();
        break;
      case 1:
        break;
      case 2:
        calcDisable();
        wallClockEnable();
        break;
      case 3:
        if (calcTask.isEnabled()) {
          op = '+';
        }
        break;
      case 4:
        if (calcTask.isEnabled()) {
          op = '-';
        }
        break;
      case 5:
        if (calcTask.isEnabled()) {
          op = '*';
        }
        break;
      case 6:
        if (calcTask.isEnabled()) {
          op = '/';
        }
        break;
      case 7:
        if (calcTask.isEnabled()) {
          op = '='; // ENTER
        }
        break;
    }
  }
}