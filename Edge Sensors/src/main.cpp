

/**
 * Description:
 *   The sensor will have 2 operating modes.
 * 
 *   MAIN Mode:
 *   The sensor will transmit via ESP-NOW a message that contains three parts
 *      Status (on/off), Pitch (note), Velocity (loudness)
 * 
 *   CONFIGURATION Mode:
 *   By long pressing (5 seconds) the button at boot the device will be put into
 *   configuration mode. It will turn the device into a captive portal 
 *   where you can change the devices values.
 *   This means, they will be loaded from/saved to EEPROM,
 *   and will appear in the config portal.
 * 
 */

#include <IotWebConf.h>
#include <EasyButton.h>
#include <EEPROM.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "touchThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "makkaraperuna";

#define STRING_LEN 128
#define NUMBER_LEN 32

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "original"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN 35

// -- AP SETUP_PIN is pulled to ground on startup for 5 seconds, the Thing will start
//    a captive portal where you can configure the device.
#define SETUP_PIN 0

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN 17

// -- Callback method declarations.
void configSaved();
boolean formValidator();

DNSServer dnsServer;
WebServer server(80);

char pitchValue[NUMBER_LEN];
char velocityValue[NUMBER_LEN];
char intParamValue[NUMBER_LEN];


IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfSeparator pitchSeperator = IotWebConfSeparator("Note Number (21 - 108)");
IotWebConfParameter pitchParam = IotWebConfParameter("Pitch", "pitchParam", pitchValue, NUMBER_LEN, "number", "1..127", NULL, "min='1' max='127' step='1'");

IotWebConfSeparator velocitySeperator = IotWebConfSeparator("Velocity or Loudness (0-127)");
IotWebConfParameter velocityParam = IotWebConfParameter("Velocity", "velocityParam", velocityValue, NUMBER_LEN, "number", "1..127", NULL, "min='1' max='127' step='1'");

EasyButton button(SETUP_PIN);


void configServerCallback();

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");
  

  iotWebConf.setStatusPin(STATUS_PIN);
  //iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addParameter(&pitchSeperator);
  iotWebConf.addParameter(&pitchParam);
  iotWebConf.addParameter(&velocitySeperator);
  iotWebConf.addParameter(&velocityParam);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.getApTimeoutParameter()->visible = true;

  // -- Initializing the configuration.
  iotWebConf.init();

  // -- Initizlizing setup button
  button.onPressedFor(5000, configServerCallback);

  Serial.println("Ready.");
}

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();
  button.read();
}


void configSaved()
{
  Serial.println("Configuration was updated.");
}

void configServerCallback() 
{
  Serial.println("Starting Config Server.");
  // -- Set up required URL handlers on the web server.
  iotWebConf.forceApMode();
  
  server.on("/", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
}
