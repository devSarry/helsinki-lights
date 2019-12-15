#include "ConfigManager.h"

const byte DNS_PORT = 53;
const char magicBytes[MAGIC_LENGTH] = {'C', 'M'};
const char magicBytesEmpty[MAGIC_LENGTH] = {'\0', '\0'};

const char mimeHTML[] PROGMEM = "text/html";
const char mimeJSON[] PROGMEM = "application/json";
const char mimePlain[] PROGMEM = "text/plain";
const char mimeCSS[] PROGMEM = "text/css";
const char mimeJS[] PROGMEM = "application/javascript";

bool DEBUG_MODE = false;

Mode ConfigManager::getMode() {
    return this->mode;
}

void ConfigManager::setAPName(const char *name) {
    this->apName = (char *)name;
}

void ConfigManager::setAPPassword(const char *password) {
    this->apPassword = (char *)password;
}

void ConfigManager::setAPFilename(const char *filename) {
    this->apFilename = (char *)filename;
}

void ConfigManager::setAPTimeout(const int timeout) {
    this->apTimeout = timeout;
}

void ConfigManager::setWifiConnectRetries(const int retries) {
    this->wifiConnectRetries = retries;
}

void ConfigManager::setWifiConnectInterval(const int interval) {
    this->wifiConnectInterval = interval;
}

void ConfigManager::setWebPort(const int port) {
    this->webPort = port;
}

void ConfigManager::setAPCallback(std::function<void(WebServer*)> callback) {
    this->apCallback = callback;
}

void ConfigManager::setAPICallback(std::function<void(WebServer*)> callback) {
    this->apiCallback = callback;
}

void ConfigManager::loop() {
    if (mode == ap && apTimeout > 0 && ((millis() - apStart) / 1000) > apTimeout) {
        ESP.restart();
    }

    if (dnsServer) {
        dnsServer->processNextRequest();
    }

    if (server) {
        server->handleClient();
    }
}

void ConfigManager::save() {
    this->writeConfig();
}

JsonObject &ConfigManager::decodeJson(String jsonString)
{
    DynamicJsonBuffer jsonBuffer;

    if (jsonString.length() == 0) {
        return jsonBuffer.createObject();
    }

    JsonObject& obj = jsonBuffer.parseObject(jsonString);

    if (!obj.success()) {
        return jsonBuffer.createObject();
    }

    return obj;
}

void ConfigManager::streamFile(const char *file, const char mime[]) {
    SPIFFS.begin();

    File f = SPIFFS.open(file, "r");
    if (!f) {
        DebugPrintln(F("file open failed"));
        handleNotFound();
        return;
    }

    server->streamFile(f, FPSTR(mime));
    f.close();
}

void ConfigManager::handleAPGet() {
    streamFile(apFilename, mimeHTML);
}

void ConfigManager::handleAPPost() {
    bool isJson = server->header("Content-Type") == FPSTR(mimeJSON);
    String pitch;
    String velocity;

    if (isJson) {
        JsonObject& obj = this->decodeJson(server->arg("plain"));

        pitch = obj.get<String>("pitch");
        velocity = obj.get<String>("velocity");
    } else {
        pitch = server->arg("pitch");
        velocity = server->arg("velocity");
    }

    if (pitch.length() == 0) {
        server->send(400, FPSTR(mimePlain), F("Invalid ssid."));
        return;
    }

    storeMidiValues(pitch, velocity, false);

    server->send(204, FPSTR(mimePlain), F("Saved. Will attempt to reboot."));

}

void ConfigManager::handleScanGet() {
    DynamicJsonBuffer jsonBuffer;
    JsonArray& jsonArray = jsonBuffer.createArray();

    DebugPrintln("Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    DebugPrintln("scan complete");
    if (n == 0) {
        DebugPrintln("no networks found");
    } else {
        DebugPrint(n);
        DebugPrintln(" networks found:");

        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            String security = WiFi.encryptionType(i) == WIFI_OPEN ? "none" : "enabled";

            DebugPrint("Name: ");
            DebugPrint(ssid);
            DebugPrint(" - Strength: ");
            DebugPrint(rssi);
            DebugPrint(" - Security: ");
            DebugPrintln(security);

            DynamicJsonBuffer jsonBufferItem;
            JsonObject& obj = jsonBuffer.createObject();
            obj.set("ssid", ssid);
            obj.set("strength", rssi);
            obj.set("security", security == "none" ? false : true);
            jsonArray.add(obj);
        }
    }

    String body;
    jsonArray.printTo(body);

    server->send(200, FPSTR(mimeJSON), body);
}

void ConfigManager::handleRESTGet() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& obj = jsonBuffer.createObject();

    std::list<BaseParameter*>::iterator it;
    for (it = parameters.begin(); it != parameters.end(); ++it) {
        if ((*it)->getMode() == set) {
            continue;
        }

        (*it)->toJson(&obj);
    }

    String body;
    obj.printTo(body);

    server->send(200, FPSTR(mimeJSON), body);
}

void ConfigManager::handleRESTPut() {
    JsonObject& obj = this->decodeJson(server->arg("plain"));
    if (!obj.success()) {
        server->send(400, FPSTR(mimeJSON), "");
        return;
    }

    std::list<BaseParameter*>::iterator it;
    for (it = parameters.begin(); it != parameters.end(); ++it) {
        if ((*it)->getMode() == get) {
            continue;
        }

        (*it)->fromJson(&obj);
    }

    writeConfig();

    server->send(204, FPSTR(mimeJSON), "");
}

void ConfigManager::handleNotFound() {
    String URI = toStringIP(server->client().localIP()) + String(":") + String(webPort);
    String header = server->hostHeader();

    if ( !isIp(header) && header != URI) {
        DebugPrint(F("Unknown URL: "));
        DebugPrintln(header);
        server->sendHeader("Location", String("http://") + URI, true);
        server->send(302, FPSTR(mimePlain), "");
        // Empty content inhibits Content-length header so we have to close the socket ourselves.
        server->client().stop();
        return;
    }

    server->send(404, FPSTR(mimePlain), "File Not Found");
}

bool ConfigManager::wifiConnected() {
    DebugPrint(F("Waiting for WiFi to connect"));

    int i = 0;
    while (i < wifiConnectRetries) {
        if (WiFi.status() == WL_CONNECTED) {
            DebugPrintln("");
            return true;
        }

        DebugPrint(".");

        delay(wifiConnectInterval);
        i++;
    }

    DebugPrintln("");
    DebugPrintln(F("Connection timed out"));

    return false;
}

void ConfigManager::setup() {
    char magic[MAGIC_LENGTH];
    char pitch[MIDI_LENGTH];
    char velocity[MIDI_LENGTH];

    DebugPrintln(F("Reading saved configuration"));

    DebugPrint(F("MAC: "));
    DebugPrintln(WiFi.macAddress());

    EEPROM.get(0, magic);
    EEPROM.get(MAGIC_LENGTH, pitch);
    DebugPrint(F("Pitch: \""));
    DebugPrint(pitch);
    DebugPrintln(F("\""));

    EEPROM.get(MAGIC_LENGTH + MIDI_LENGTH, velocity);
    DebugPrint(F("Velocity: \""));
    DebugPrint(velocity);
    DebugPrintln(F("\""));
    readConfig();
}

void ConfigManager::startAP() {
    mode = ap;

    DebugPrintln(F("Starting Access Point"));

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName, apPassword);

    delay(500); // Need to wait to get IP

    IPAddress ip(192, 168, 1, 1);
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(ip, ip, NMask);

    DebugPrint("AP Name: ");
    DebugPrintln(apName);

    IPAddress myIP = WiFi.softAPIP();
    DebugPrint("AP IP address: ");
    DebugPrintln(myIP);

    dnsServer.reset(new DNSServer);
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(DNS_PORT, "*", ip);

    createBaseWebServer();

    if (apCallback) {
        apCallback(server.get());
    }

    server->begin();

    apStart = millis();
}

void ConfigManager::startApi() {
    mode = api;

    createBaseWebServer();
    server->on("/settings", HTTPMethod::HTTP_GET, std::bind(&ConfigManager::handleRESTGet, this));
    server->on("/settings", HTTPMethod::HTTP_PUT, std::bind(&ConfigManager::handleRESTPut, this));

    if (apiCallback) {
        apiCallback(server.get());
    }

    server->begin();
}

void ConfigManager::createBaseWebServer() {
    const char* headerKeys[] = {"Content-Type"};
    size_t headerKeysSize = sizeof(headerKeys)/sizeof(char*);

    server.reset(new WebServer(this->webPort));
    DebugPrint(F("Webserver enabled on port: "));
    DebugPrintln(webPort);

    server->collectHeaders(headerKeys, headerKeysSize);

    server->on("/", HTTPMethod::HTTP_GET, std::bind(&ConfigManager::handleAPGet, this));
    server->on("/", HTTPMethod::HTTP_POST, std::bind(&ConfigManager::handleAPPost, this));
    server->on("/scan", HTTPMethod::HTTP_GET, std::bind(&ConfigManager::handleScanGet, this));
    server->onNotFound(std::bind(&ConfigManager::handleNotFound, this));
}


void ConfigManager::clearMidiValues(bool reboot) {
    char pitch[MIDI_LENGTH];
    char velocity[MIDI_LENGTH];
    memset(pitch, 0, MIDI_LENGTH);
    memset(velocity, 0, MIDI_LENGTH);

    DebugPrintln(F("Clearing WiFi connection."));
    storeMidiValues(pitch, velocity, true);

    if (reboot) {
        ESP.restart();
    }
}

void ConfigManager::storeMidiValues(String pitch, String velocity, bool resetMagic) {
    char pitchChar[MIDI_LENGTH];

    char velocityChar[MIDI_LENGTH];

    // if (EEPROM.length() == 0) {
    //     DebugPrintln(F("WiFi Settings cannot be stored before ConfigManager::begin()"));
    //     return;
    // }

    strncpy(pitchChar, pitch.c_str(), MIDI_LENGTH);
    strncpy(velocityChar, velocity.c_str(), MIDI_LENGTH);

    DebugPrint(F("Storing MIDI values for pitch: \""));
    DebugPrint(pitchChar);
    DebugPrint(F(" velocity: \""));
    DebugPrint(velocityChar);

    DebugPrintln(F("\""));

    EEPROM.put(0, resetMagic ? magicBytesEmpty : magicBytes);
    EEPROM.put(MAGIC_LENGTH, pitchChar);
    EEPROM.put(MAGIC_LENGTH + MIDI_LENGTH, velocityChar);
    bool wroteChange = EEPROM.commit();

    DebugPrint(F("EEPROM committed: "));
    DebugPrintln(wroteChange ? F("true") : F("false"));
}

void ConfigManager::clearSettings(bool reboot) {
    DebugPrintln(F("Clearing Settings...."));
    std::list<BaseParameter*>::iterator it;
    for (it = parameters.begin(); it != parameters.end(); ++it) {
        (*it)->clearData();
    }

    writeConfig();

    if (reboot) {
        ESP.restart();
    }
}

void ConfigManager::readConfig() {
    byte *ptr = (byte *)config;

    for (int i = 0; i < configSize; i++) {
        *(ptr++) = EEPROM.read(CONFIG_OFFSET + i);
    }
}

void ConfigManager::writeConfig() {
    byte *ptr = (byte *)config;

    for (int i = 0; i < configSize; i++) {
        EEPROM.write(CONFIG_OFFSET + i, *(ptr++));
    }
    EEPROM.commit();
}

boolean ConfigManager::isIp(String str) {
  for (uint i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

String ConfigManager::toStringIP(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}
