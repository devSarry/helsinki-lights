#include <ConfigManager.h>
#include <esp_now.h>
#include <WifiEspNow.h>
#include <WiFi.h>

// Global copy of slave
esp_now_peer_info_t slave;
#define CHANNEL 3
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void sendData();
void deletePeer();
bool manageSlave();
void ScanForSlave();
void InitESPNow();


// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  bool ok = WifiEspNow.begin();
  if (!ok) {
    Serial.println("WifiEspNow.begin() failed");
    ESP.restart();
  }
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

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
      }
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
    if (DELETEBEFOREPAIR) {
      deletePeer();
    }

    Serial.print("Slave Status: ");
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if ( exists) {
      // Slave already paired.
      Serial.println("Already Paired");
      return true;
    } else {
      // Slave not paired, attempt pair
      bool ok;
      ok = WifiEspNow.addPeer(slave.peer_addr);
        if (!ok) {
            Serial.println("WifiEspNow.addPeer() failed");
        }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
    return false;
  }
}

void deletePeer() {
  esp_err_t delStatus = esp_now_del_peer(slave.peer_addr);
  Serial.print("Slave Delete Status: ");
  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println("Success");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW Not Init");
  } else if (delStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}


void sendData() {
    const uint8_t *peer_addr = slave.peer_addr;
    char msg[60];
    int len = snprintf(msg, sizeof(msg), "hello ESP-NOW from %s at %lu", WiFi.softAPmacAddress().c_str(), millis());
    WifiEspNow.send(slave.peer_addr, reinterpret_cast<const uint8_t*>(msg), len);
    Serial.print("Sending: "); Serial.println(msg);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

struct Config {
    char name[20];
    bool enabled;
    int8_t hour;
    char password[20];
} config;

struct Metadata {
    int8_t version;
} meta;

ConfigManager configManager;

void createCustomRoute(WebServer *server) {
    server->on("/custom", HTTPMethod::HTTP_GET, [server](){
        server->send(200, "text/plain", "Hello, World!");
    });
}

void setup() {
    DEBUG_MODE = true; // will enable debugging and log to serial monitor
    Serial.begin(115200);
    DebugPrintln("");

    meta.version = 3;

    // Setup config manager
    configManager.setAPName("Demo");
    configManager.setAPFilename("/index.html");
    configManager.addParameter("penis ", config.name, 20);
    configManager.addParameter("enabled", &config.enabled);
    configManager.addParameter("hour", &config.hour);
    configManager.addParameter("password", config.password, 20, set);
    configManager.addParameter("version", &meta.version, get);

    configManager.setAPCallback(createCustomRoute);
    configManager.setAPICallback(createCustomRoute);

    configManager.begin(config);
    //Set device in STA mode to begin with
    WiFi.mode(WIFI_STA);
    Serial.println("ESPNow/Basic/Master Example");
    // This is the mac address of the Master in Station Mode
    Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
    // Init ESPNow with a fallback logic
    
    InitESPNow();
    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Trasnmitted packet
    esp_now_register_send_cb(OnDataSent);
}

void loop() {
    configManager.loop();

    // Add your loop code here
    ScanForSlave();
    // If Slave is found, it would be populate in `slave` variable
    // We will check if `slave` is defined and then we proceed further
    if (slave.channel == CHANNEL) { // check if slave channel is defined
    // `slave` is defined
    // Add slave as peer if it has not been added already
    bool isPaired = manageSlave();
    if (isPaired) {
        // pair success or already paired
        // Send data to device
        sendData();
    } else {
        // slave pair failed
        Serial.println("Slave pair failed!");
    }
    }
    else {
    // No slave found to process
    }
    delay(3000);
}