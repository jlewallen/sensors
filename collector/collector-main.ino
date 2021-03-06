#include <Adafruit_SleepyDog.h>
#include "Collector.h"

Pcf8523SystemClock Clock;
Collector collector;

void setup() {
    pinMode(PIN_ROCK_BLOCK, OUTPUT);
    digitalWrite(PIN_ROCK_BLOCK, LOW);

    pinMode(PIN_WEATHER_STATION_RESET, OUTPUT);
    digitalWrite(PIN_WEATHER_STATION_RESET, LOW);

    Watchdog.enable();

    Serial.begin(115200);

    #ifdef WAIT_FOR_SERIAL
    while (!Serial) {
        delay(100);
        if (millis() > WAIT_FOR_SERIAL) {
            break;
        }
    }
    #endif

    collector.setup();
}

void loop() {
    collector.loop();
}

// vim: set ft=cpp:
