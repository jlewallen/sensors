#include <Adafruit_SleepyDog.h>

#include "Platforms.h"

#define BATTERY_VOLTAGE_DIVIDER_RATIO                         2.0f
#define BATTERY_VOLTAGE_REFERENCE                             3.3f
#define BATTERY_VOLTAGE_OPTIMAL                               4.2f
#define BATTERY_VOLTAGE_LOW                                   3.4f
// I've adjusted these based on this. 3.4V is effectively "dead" and that's what
// I've seen in practice, but sometimes things "work" around the 3.5v range.
// https://learn.adafruit.com/li-ion-and-lipoly-batteries/voltages


#ifdef ARDUINO_AVR_FEATHER32U4

#include <avr/wdt.h>

void platformRestart() {
    wdt_enable(WDTO_15MS);
    while (true) {
    }
}

#endif

#ifdef ARDUINO_SAMD_FEATHER_M0

SerialType &portExpanderSerial = Serial1;
SerialType &conductivitySerial = Serial2;

extern "C" char *sbrk(int32_t i);

uint32_t platformFreeMemory() {
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
}

void platformRestart() {
    NVIC_SystemReset();
}

float platformBatteryVoltage() {
    analogRead(A1);
    delay(2);
    analogRead(A1);
    delay(2);
    float value = analogRead(A1);
    return value * BATTERY_VOLTAGE_DIVIDER_RATIO * BATTERY_VOLTAGE_REFERENCE / 1024.0f;
}

float platformBatteryLevel() {
    float constrained = max(min(platformBatteryVoltage(), BATTERY_VOLTAGE_OPTIMAL), BATTERY_VOLTAGE_LOW);
    return (constrained - BATTERY_VOLTAGE_LOW) / (BATTERY_VOLTAGE_OPTIMAL - BATTERY_VOLTAGE_LOW);
}

#endif

void platformBlinks(uint8_t pin, uint8_t number) {
    for (uint8_t i = 0; i < number; ++i) {
        delay(100);
        digitalWrite(pin, HIGH);
        delay(100);
        digitalWrite(pin, LOW);
    }
}

void platformBlink(uint8_t pin) {
    delay(500);
    digitalWrite(pin, HIGH);
    delay(500);
    digitalWrite(pin, LOW);
}

void platformPulse(uint8_t pin) {
    delay(1000);
    analogWrite(pin, 256);
    delay(1000);
    analogWrite(pin, 128);
}

void platformCatastrophe(uint8_t pin, uint8_t mode) {
    uint32_t restartAfter = 30 * 1000;
    uint32_t started = millis();

    DEBUG_PRINTLN("Catastrophe!");

    while (true) {
        if (millis() - started < restartAfter) {
            Watchdog.reset();
        }
        else {
            // pinMode(PIN_POWER_HARD_RESET, OUTPUT);
            // digitalWrite(PIN_POWER_HARD_RESET, HIGH);
            delay(1000);
            platformRestart();
        }
        switch (mode) {
        case PLATFORM_CATASTROPHE_FAST_BLINK:
            delay(100);
            digitalWrite(pin, HIGH);
            delay(100);
            digitalWrite(pin, LOW);
            break;
        default:
            platformPulse(pin);
            break;
        }
    }
}

static uint32_t uptime = 0;

uint32_t platformUptime() {
    return uptime + millis();
}

uint32_t platformAdjustUptime(uint32_t by) {
    return uptime += by;
}

uint32_t platformDeepSleep(bool forceDelay) {
    if (forceDelay || Serial) {
        delay(8192);
        return 8192;
    }
    uint32_t actual = Watchdog.sleep(8192);
    platformAdjustUptime(actual);
    return actual;
}
