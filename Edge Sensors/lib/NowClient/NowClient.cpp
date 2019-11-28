#include <NowClient.h>

#include <esp_now.h>
#include <WiFi.h>



class NowClient {
    char* SSID;
    char* Wifi_Password;

    public:
    NowClient(char* ssid, char* password) 
    {
        SSID = ssid;
        Wifi_Password = password;
    }
    void configDeviceAP() {
        bool result = WiFi.softAP(SSID, Wifi_Password, WIFI_CHANNEL, 0);
        if (!result) {
            Serial.println("AP Config failed.");
        } else {
            Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
        }
    }

};