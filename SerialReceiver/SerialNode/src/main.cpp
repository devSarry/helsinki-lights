/**
   ESPNOW - Basic communication - Slave
   Date: 26th September 2017
   Author: Arvind Ravulavaru <https://github.com/arvindr21>
   Purpose: ESPNow Communication between a Master ESP32 and a Slave ESP32
   Description: This sketch consists of the code for the Slave module.
   Resources: (A bit outdated)
   a. https://espressif.com/sites/default/files/documentation/esp-now_user_guide_en.pdf
   b. http://www.esploradores.com/practica-6-conexion-esp-now/
   << This Device Slave >>
   Flow: Master
   Step 1 : ESPNow Init on Master and set it in STA mode
   Step 2 : Start scanning for Slave ESP32 (we have added a prefix of `slave` to the SSID of slave for an easy setup)
   Step 3 : Once found, add Slave as peer
   Step 4 : Register for send callback
   Step 5 : Start Transmitting data from Master to Slave
   Flow: Slave
   Step 1 : ESPNow Init on Slave
   Step 2 : Update the SSID of Slave with a prefix of `slave`
   Step 3 : Set Slave in AP mode
   Step 4 : Register for receive callback and wait for data
   Step 5 : Once data arrives, print it in the serial monitor
   Note: Master and Slave have been defined to easily understand the setup.
         Based on the ESPNOW API, there is no concept of Master and Slave.
         Any devices can act as master or salve.
*/

#include <esp_now.h>
#include <WiFi.h>
#include <WifiEspNow.h>
#include <MIDI.h>

#define CHANNEL 1

#define SERIALMIDI_BAUD_RATE  115200

struct SerialMIDISettings : public midi::DefaultSettings
{
  static const long BaudRate = SERIALMIDI_BAUD_RATE;
};

HardwareSerial                SerialMIDI(0);


MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, SerialMIDI, MIDI, SerialMIDISettings);

void InitESPNow();
void configDeviceAP();
void printReceivedMessage(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg);


void printReceivedMessage(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg) {
  int delimiterCount = 0;

  int status = 0;
  int pitch = 0;
  int velocity = 0;

  char messageBuffer[40];
  for (int i = 0; i < count; ++i) {
    messageBuffer[i] = static_cast<char>(buf[i]);
    //Serial.print(static_cast<char>(buf[i]));
  }

  char * pch;
  pch = strtok (messageBuffer," ");
  while (pch != NULL)
  {

    if(delimiterCount == 0) {
      status = atoi(pch);
    }
    if(delimiterCount == 1) {
      pitch = atoi(pch);
    }
    if(delimiterCount == 2) {
      velocity = atoi(pch);
    }
    pch = strtok (NULL, " ");
    delimiterCount++;
  }

  Serial.println();
  if(status == 128) {
      MIDI.sendNoteOff(pitch, velocity,1);
  }
  if(status == 144) {
      MIDI.sendNoteOn(pitch, velocity, 1);
  }

}

// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  bool ok = WifiEspNow.begin();

  if (ok) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  const char *SSID = "Slave_1";
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPdisconnect(false);
  bool result = WiFi.softAP(SSID, "Slave_1_Password", CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
  }
}

void setup() {
  Serial.begin(SERIALMIDI_BAUD_RATE);
  Serial.println("ESPNow/Basic/Slave Example");
  //Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the Slave in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  Serial.println("Register received callback");

  MIDI.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages

  WifiEspNow.onReceive(printReceivedMessage, nullptr);
}



void loop() {
     // Read incoming messages
     MIDI.read();
}