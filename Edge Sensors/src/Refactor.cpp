// #include <ConfigManager.h>
// #include <Arduino.h>
// #include <EasyButton.h>
// #include <WiFi.h>


// /**
//  * Description:
//  *   The sensor will have 2 operating modes.
//  * 
//  *   MAIN Mode:
//  *   The sensor will transmit via ESP-NOW a message that contains three parts
//  *      Status (on/off), Pitch (note), Velocity (loudness)
//  * 
//  *   CONFIGURATION Mode:
//  *   By long pressing (5 seconds) the button at boot the device will be put into
//  *   configuration mode. It will turn the device into a captive portal 
//  *   where you can change the devices values.
//  *   This means, they will be loaded from/saved to EEPROM,
//  *   and will appear in the config portal.
//  * 
//  */




// struct Config {
//     bool name[20];
//     bool enabled;
//     int8_t hour;
//     char password[20];
// } config;

// struct Metadata {
//     int8_t version;
// } meta;

// ConfigManager configManager;

// void createCustomRoute(WebServer *server) {
//     server->on("/custom", HTTPMethod::HTTP_GET, [server](){
//         server->send(200, "text/plain", "Hello, World!");
//     });
// }

// void setup() {
//     DEBUG_MODE = true; // will enable debugging and log to serial monitor
//     Serial.begin(115200);
//     DebugPrintln("");

//     meta.version = 3;

//     // Setup config manager
//     configManager.setAPName("LightPad");
//     configManager.setAPFilename("/index.html");
//     configManager.addParameter("name", config.name, 20);
//     configManager.addParameter("enabled", &config.enabled);
//     configManager.addParameter("hour", &config.hour);
//     configManager.addParameter("password", config.password, 20, set);
//     configManager.addParameter("version", &meta.version, get);

//     configManager.setAPCallback(createCustomRoute);
//     configManager.setAPICallback(createCustomRoute);

//     configManager.begin(config);
// }

// void loop() {
//     configManager.loop();

//     // Add your loop code here
// }