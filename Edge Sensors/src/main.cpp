#include <ConfigManager.h>
#include <esp_now.h>
#include <WifiEspNow.h>
#include <WiFi.h>
#include <EasyButtonTouch.h>

#define CHANNEL 1
#define SETUP_PIN 19
#define TOUCH_PIN 27

EasyButtonTouch touchPad(TOUCH_PIN, 35, 50);
EasyButton apSetupButton(SETUP_PIN);

bool inAPMode = false;
esp_now_peer_info_t slave;

struct Config {
} config;

struct Metadata {
    int8_t version;
} meta;

ConfigManager configManager;

// -- Midi message will have the following format
//     pitchInt velocityInt statusInt ie. 100 50 0
char midiMessageBuffer[12];

void InitESPNow();
void ScanForSlave();
bool manageSlave();
void sendData();
void initMidiMessage();
void setupButtonCallback();
void midiOnHelper();
void midiOffHelper();


void InitESPNow() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESPNOW", nullptr, 3);
  WiFi.softAPdisconnect(false);

  Serial.print("MAC address of this node is ");
  Serial.println(WiFi.softAPmacAddress());

  bool ok = WifiEspNow.begin();
  if (!ok) {
    Serial.println("WifiEspNow.begin() failed");
    ESP.restart();
  }


  ok = WifiEspNow.addPeer(slave.peer_addr);
  if (!ok) {
    Serial.println("WifiEspNow.addPeer() failed");
    ESP.restart();
  }
}

void initMidiMessage() {
    char pitch[MIDI_LENGTH];
    char velocity[MIDI_LENGTH];

    DebugPrintln(F("Reading saved configuration"));

    EEPROM.get(MAGIC_LENGTH, pitch);
    EEPROM.get(MAGIC_LENGTH + MIDI_LENGTH, velocity);
    int len = snprintf(midiMessageBuffer, sizeof(midiMessageBuffer), "%s %s", pitch, velocity);
}
// Scan for slaves in AP mode
void ScanForSlave() {
  int8_t scanResults = WiFi.scanNetworks();
  // reset on each scan
  bool slaveFound = 0;
  memset(&slave, 0, sizeof(slave));

  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
      delay(10);
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Slave") == 0) {
        // SSID of interest
        Serial.println("Found a Slave.");
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        // Get BSSID => Mac Address of the Slave
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slave.peer_addr[ii] = (uint8_t) mac[ii];
          }
        }

        slave.channel = CHANNEL; // pick a channel
        slave.encrypt = 0; // no encryption

        slaveFound = 1;
        // we are planning to have only one slave in this example;
        // Hence, break after we find one, to be a bit efficient
        break;
      }
    }
  }

  if (slaveFound) {
    Serial.println("Slave Found, processing..");
  } else {
    Serial.println("Slave Not Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
}


// Check if the slave is already paired with the master.
// If not, pair the slave with master
bool manageSlave() {
  if (slave.channel == CHANNEL) {

    // check if the peer exists
    bool exists = WifiEspNow.hasPeer(slave.peer_addr);
    if ( exists) {
      // Slave already paired.
      return true;
    } else {
      // Slave not paired, attempt pair
      bool ok;
      ok = WifiEspNow.addPeer(slave.peer_addr);
        if (!ok) {
            Serial.println("Slave Status: WifiEspNow.addPeer() failed");
        }
    }
  } else {
    // No slave found to process
    Serial.println("Slave Status: No Slave found to process");
    return false;
  }
}

void sendData(char* message) {
    unsigned int timeout_ms = 5000;
    char msg[60];

    int len = snprintf(msg, sizeof(msg), "%s", message);

    if (WifiEspNow.hasPeer(slave.peer_addr)) {
      WifiEspNow.send(slave.peer_addr, reinterpret_cast<const uint8_t*>(msg), len);
    }
    Serial.print("Sending: "); Serial.println(msg);

    WifiEspNowSendStatus status;
    unsigned long starttime = millis();
    do {
        status = WifiEspNow.getSendStatus();
        yield();
    } while ((status == WifiEspNowSendStatus::NONE) && ((millis() - starttime) < timeout_ms));

    if (status == WifiEspNowSendStatus::OK) {
      Serial.println("Message Sent succesfully");
    } else {
      Serial.println("Message wasn't received.");
    }

}

void setupButtonCallback() {
  Serial.println("Turning on AP Config Mode");
  inAPMode = true;
  WifiEspNow.end();
  configManager.startAP();
}

void midiOffHelper() {
  Serial.println("Midi Pad Status: OFF");
  char msg[60];
  int len = snprintf(msg, sizeof(msg), "128 %s", midiMessageBuffer); // NOTE OFF
  sendData(msg);
}

void midiOnHelper() {
  Serial.println("Midi Pad Status: ON");
  char msg[60];
  int len = snprintf(msg, sizeof(msg), "144 %s", midiMessageBuffer); // NOTE ON
  sendData(msg);
}
void printReceivedMessage(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg) {
  Serial.printf("Message from %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  for (int i = 0; i < count; ++i) {
    Serial.print(static_cast<char>(buf[i]));
  }
  Serial.println();
}

void setup() {
  DEBUG_MODE = true; // will enable debugging and log to serial monitor
  Serial.begin(115200);
  DebugPrintln("");

  meta.version = 3;

  // Setup config manager
  configManager.setAPName("Demo");
  configManager.setAPFilename("/index.html");


  configManager.begin(config);

  initMidiMessage();
  Serial.println();

  InitESPNow();

  touchPad.onPressed(midiOffHelper);
  apSetupButton.onPressed(setupButtonCallback);
}

void loop() {
  
  apSetupButton.read();
  configManager.loop();
  
  if (slave.channel == CHANNEL) {
    bool isPaired = false;
    if(!inAPMode) {
       isPaired = manageSlave();
    }

    if (isPaired) {
      touchPad.read();

      if(touchPad.wasPressed()) {
        midiOnHelper();
      }
    }
  } else {
    ScanForSlave();
  }
}