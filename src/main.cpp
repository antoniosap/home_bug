/*! @mainpage
//-------------------------------------------------------------------------------------------------------
*/
#include <Arduino.h>

//-- DEBUG ---------------------------------------------------------------------
#define DEBUG_KEYPAD        false
#define UART_ECHO           (0)
#define UART_BAUDRATE       (115200)

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

#define FUNCT_BTN_NULL  0
#define FUNCT_BTN_0     1
#define FUNCT_BTN_1     2
#define FUNCT_BTN_2     4
#define FUNCT_BTN_3     8
#define FUNCT_BTN_4     16
#define FUNCT_BTN_5     32
#define FUNCT_BTN_6     64
#define FUNCT_BTN_7     128
uint8_t functionBtn = FUNCT_BTN_NULL;


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
Task welcomeTask(3000, 3, &welcome);
void wallClock();
void wallClockEnable();
Task clockTask(1000, TASK_FOREVER, &wallClock);
void calc();
void calcEnable();
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

void displayFloat(double value) {
  snprintf(display, TM_DISPLAY_SIZE + 1, "%f", value);
  tm.displayText(display);
}

uint8_t hour = 0;
uint8_t minutes = 0;
uint8_t seconds = 0;
bool clockIndicator = false;


void wallClock() {
  snprintf(display, TM_DISPLAY_SIZE + 1, "%2d%1s%02d", hour, clockIndicator ? ":" : "-", minutes); 
  tm.displayText(display);

  clockIndicator = !clockIndicator;
  if (++seconds > 59) {
    seconds = 0;
    if (++minutes > 59) {
      minutes = 0;
      if (++hour > 24) {
        hour = 0;
      }
    }
  }
};

void wallClockEnable() {
  clockTask.enable();
  tm.setLED(2, 1);
  tm.displayText("        ");
}

void wallClockDisable() {
  clockTask.disable();
  tm.setLED(2, 0);
}

void calc() {

}

void calcEnable() {
  calcTask.enable();
  tm.setLED(2, 1);
  displayFloat(0);
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
  welcomeTask.enable();
}

//-------------------------------------------------------------------------------------------------------
void loop() {
  ESP.wdtFeed();
  runner.execute();

  // Get the currently touched pads
  currtouched = cap.touched();
  for (uint8_t i=0; i<12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" touched");
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" released");
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

  functionBtn = tm.readButtons();
  if (functionBtn != FUNCT_BTN_NULL) {
    Serial.print("KEY:");
    Serial.println(functionBtn, 2);

    if (functionBtn == FUNCT_BTN_0) {

    }
  }
}