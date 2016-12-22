//*****************************************************************************
//
// CC3200 HVAC Demo - Demo showing how to use the CC3200 with Exosite's Murano.
//
// Copyright (c) 2013-2016 Exosite LLC.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of Exosite LLC nor the names of its contributors may
//    be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
//
//*****************************************************************************

#include <SPI.h>
#include <WiFi.h>
#include "Exosite.h"
#include <Adafruit_TMP006.h>

/*==============================================================================
* Configuration Variables
*
* Change these variables to your own settings.
*=============================================================================*/
char ssid[] = ""; // <-- Fill in your network SSID (name)
char pass[] = ""; // <-- Fill in your network password
char productID[] = ""; // <-- Fill in your product.id

// Number of Errors before we try a reprovision.
const unsigned char reprovisionAfter = 3;

// If you connected another LED for the ac, which pin is it on?
//#define BLUE_LED 31

/*==============================================================================
* End of Configuration Variables
*=============================================================================*/
unsigned char errorCount = reprovisionAfter;  // Force Provision On First Loop
char macString[18];  // Used to store a formatted version of the MAC Address
byte macData[WL_MAC_ADDR_LENGTH];

int status = WL_IDLE_STATUS;
WiFiClient client;
Exosite exosite(&client);
Adafruit_TMP006 tmp006(0x41);

/*==============================================================================
* setup
*
* Arduino setup function.
*=============================================================================*/
void setup(){
  Serial.begin(115200);
  Serial.println("Boot");

  pinMode(RED_LED, OUTPUT);
#ifdef BLUE_LED
  pinMode(BLUE_LED, OUTPUT);
#endif

  if (! tmp006.begin()) {
    Serial.println("No TMP006 sensor found");
    while (1);
  }

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while(true);
  }

  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to Network named: ");
  // print the network name (SSID);
  Serial.println(ssid);
  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  WiFi.begin(ssid, pass);
  while ( WiFi.status() != WL_CONNECTED) {
    // print dots while we wait to connect
    Serial.print(".");
    delay(300);
  }

  Serial.println("\nYou're connected to the network");
  Serial.println("Waiting for an ip address");

  while (WiFi.localIP() == INADDR_NONE) {
    // print dots while we wait for an ip addresss
    Serial.print(".");
    delay(300);
  }

  Serial.println("\nIP Address obtained");

  // Create MAC Address String in the format FF:FF:FF:FF:FF:FF
  WiFi.macAddress(macData);
  snprintf(macString, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           macData[5], macData[4], macData[3], macData[2], macData[1], macData[0]);

  // Print Some Useful Info
  Serial.print(F("MAC Address: "));
  Serial.println(macString);

  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());

    // print the received signal strength:
  Serial.print("signal strength (RSSI):");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  exosite.begin();
}

/*==============================================================================
* loop
*
* Arduino loop function.
*=============================================================================*/
void loop(){
  // Check if we should reprovision.
  if(errorCount >= reprovisionAfter){
    // This will return an error if the device is already provisioned.
    if(exosite.provision(productID, productID, macString)){
      errorCount = 0;
    } else {
      Serial.println("Device Provisioned");
    }
  }

  // Grab temperature measurements and print them.
  float objt = tmp006.readObjTempC();
  Serial.print("Temperature: Obj: "); Serial.print(objt); Serial.print("ºC ");
  float diet = tmp006.readDieTempC();
  Serial.print("Die: "); Serial.print(diet); Serial.println("ºC");

  // Write Temp&Humi
  // Read heat_on&ac_on
  String returnString;
  String ws = String("temperature=")+String(objt)+String("&ambient_temperature=")+String(diet);
  if ( exosite.writeRead(ws, String("heat_on&ac_on"), returnString)){
    //Serial.print("OK: ");
    //Serial.println(returnString);
    errorCount = 0;

    String ac_value = ValueForAlias(returnString, String("ac_on"));
    String heat_value = ValueForAlias(returnString, String("heat_on"));
    Serial.print("AC: "); Serial.println(ac_value);
    Serial.print("HEAT: "); Serial.println(heat_value);

    // Eventhough there are three LEDs, they stuck two of them on the I2C lines. *really* now?
    // So, one LED.
    if (heat_value.toInt() == 1) {
      digitalWrite(RED_LED, HIGH);
    } else {
      digitalWrite(RED_LED, LOW);
    }
#ifdef BLUE_LED
    if (ac_value.toInt() == 1) {
      digitalWrite(BLUE_LED, HIGH);
    } else {
      digitalWrite(BLUE_LED, LOW);
    }
#endif
  }else{
    Serial.println("Error");
    errorCount++;
  }

  delay(5000);
}

/**
 * From a query encoded string, find the value for a given key
 */
String ValueForAlias(const String &buffer, const String &alias) {
  int tokenBegin = 0;

  while( tokenBegin < buffer.length() ) {

    int tokenEnd = buffer.indexOf('=', tokenBegin);
    if (tokenEnd > -1) {
      if (buffer.substring(tokenBegin, tokenEnd) == alias) {
        // ok, skip to value.
        tokenBegin = tokenEnd + 1; // move past the '='
        tokenEnd = buffer.indexOf('&', tokenBegin);
        if (tokenEnd < 0) {
          return buffer.substring(tokenBegin); // to end.
        } else {
          return buffer.substring(tokenBegin, tokenEnd);
        }
      } else {
        // Not it, try next.
        tokenBegin = buffer.indexOf('&', tokenBegin+1);
        if (tokenBegin < 0) {
          // all done.
          return String("");
        }
        tokenBegin ++;
      }
    } else {
      // No '=', move to '&'
      tokenBegin = buffer.indexOf('&', tokenBegin+1);
      if (tokenBegin < 0) {
        // all done.
        return String("");
      }
      tokenBegin ++;
    }
  }
  return String("");
}

// vim: set ai cin et sw=2 ts=2 :