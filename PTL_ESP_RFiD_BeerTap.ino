#include <SPI.h>
#include <ArduinoJson.h>
#include <MFRC522.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager


#include "Clock.h"
#include "HttpBuyTransaction.h"
#include "HttpClient.h"
#include "HttpSyncTransaction.h"
#include "RfidReader.h"
#include "Sound.h"

static RfidReader rfid;
static Clock clock;
static Sound sound;

static HttpClient http;

char* lastBadge = "";

#define OPEN_PERIOD    30000UL  // 30 seconds to pour a drink
unsigned long lastBuyTime = millis() + OPEN_PERIOD*2; //so when we start we already pass the minimum wait time

#define RESTART_RFID 30000UL
unsigned long lastRestartTime = millis();

void setup() {
    Serial.begin(115200);
    Serial.println("Starting wifi...");
    sound.play("a1");    
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //reset settings - for testing
    //wifiManager.resetSettings();
    wifiManager.setAPCallback(configModeCallback);
    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if(!wifiManager.autoConnect()) {
      Serial.println("failed to connect and hit timeout");
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
    } 
    Serial.println("Wifi connected...yay :)");

    Serial.println("Starting RFID...");
    SPI.begin();           // Init SPI bus
    //mfrc522.PCD_Init();    // Init MFRC522
    rfid.begin();
    sound.play("b1");
    sound.play("a1");

    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW); // turn off


}

void loop() {
  // put your main code here, to run repeatedly:

  if((lastBuyTime + OPEN_PERIOD) < millis())
  {  
    digitalWrite(PIN_RELAY, LOW); // turn off the relay
    lastBadge = "";
  }
  else if((lastBuyTime + OPEN_PERIOD - 5000) < millis()) //beep the last 5 seconds
  {
    sound.play("a1");
    delay(500);      
  }

  if((lastRestartTime + RESTART_RFID) < millis())   //we're having troubles with the reader,,maybe restarting would help.
  {
    lastRestartTime = millis();
    rfid.restart();
  }

  
  char* badge = rfid.tryRead();
  if (badge)
  {
    if(digitalRead(PIN_RELAY)==LOW) //we only want to read an rfid card if the relay is not open
    {
      sound.play("b1");
      lastBadge = badge;
      Serial.print("badge found ");
      Serial.println(badge);
      Serial.print("Last badge changed to: ");
      Serial.println(lastBadge);
      getBeer(badge);
      // ignore all waiting badge to avoid unintended double buy
      while (rfid.tryRead())
      {
        Serial.println("rfid.tryRead");
        delay(1000);
      }
    } //if relay is off
    else
      sound.play("b2c2");
    
  } //end if badge
    
  

}

bool getBeer(char* badge)
{
  HttpBuyTransaction buyBeerTransaction(http);

  int BEER_ID = 42;

  Serial.println("Starting to get beer...");
  if (!buyBeerTransaction.perform(badge, BEER_ID, clock.getUnixTime()))
  {
    Serial.println("Error getting beer...");
    return false;
  }

  Serial.println("End get beer...");
  
  if (strcmp(buyBeerTransaction.getMessage(0), "ERROR") == 0)
  {
    lastBadge = "";
    Serial.print("Unknown badge or not enough credit");

    //sound.play(buyBeerTransaction.getMelody());
    return false;
  }
  else
  {
    Serial.println("OK beer");
    digitalWrite(PIN_RELAY, HIGH); // turn on relay with voltage HIGH
    lastBuyTime = millis();
    sound.play(buyBeerTransaction.getMelody());
    //PLAY SOUND
    
    return true;
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}
