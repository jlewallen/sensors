#include <SPI.h>
#include <Adafruit_SleepyDog.h>
#include "WifiConnection.h"

#define PIN_WINC_CS   8
#define PIN_WINC_IRQ  7
#define PIN_WINC_RST  4
#define PIN_WINC_EN   2

Adafruit_WINC1500 WiFi(PIN_WINC_CS, PIN_WINC_IRQ, PIN_WINC_RST);
Adafruit_WINC1500Client client;

WifiConnection::WifiConnection(const char *ssid, const char *psk) :
    ssid(ssid), psk(psk), status(WL_IDLE_STATUS) {
}

void WifiConnection::on() {
    pinMode(PIN_WINC_EN, OUTPUT);
    digitalWrite(PIN_WINC_EN, HIGH);

}

void WifiConnection::off() {
    digitalWrite(PIN_WINC_EN, LOW);

}

bool WifiConnection::open() {
    on();

    if (WiFi.status() == WL_NO_SHIELD) {
        DEBUG_PRINTLN("Wifi: missing");
        return false;
    }

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINT("Wifi: attempting ");
        DEBUG_PRINTLN(ssid);

        status = WiFi.begin(ssid, psk);

        uint8_t seconds = 10;
        while (seconds > 0 && (WiFi.status() != WL_CONNECTED)) {
            seconds--;
            delay(1000);
            Watchdog.reset();
        }

        if (millis() - started > ONE_MINUTE) {
            break;
        }
    }

    IPAddress ip = WiFi.localIP();
    DEBUG_PRINTLN("IP Address: ");
    DEBUG_PRINTLN(ip);
    DEBUG_PRINTLN(ip);

    status = WiFi.status();

    return status == WL_CONNECTED;
}

bool WifiConnection::post(const char *server, const char *path, const char *contentType, const char *body) {
    Watchdog.disable();

    if (client.connect(server, 80)) {
        DEBUG_PRINTLN("connected to server");

        client.print("POST ");
        client.print(path);
        client.println(" HTTP/1.1");
        client.print("Host: "); client.println(server);
        client.print("Content-Type: "); client.println(contentType);
        client.print("Content-Length: "); client.println(strlen(body));
        client.println("Connection: close");
        client.println();
        client.println(body);

        uint32_t started = millis();
        while (millis() - started < ONE_MINUTE) {
            delay(10);

            Watchdog.reset();

            while (client.available()) {
                Serial.write(client.read());
            }

            if (!client.connected()) {
                DEBUG_PRINTLN();
                DEBUG_PRINTLN("disconnecting");
                client.stop();
                break;
            }
        }
    }
    return true;
}
