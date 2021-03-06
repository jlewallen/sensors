#include <Adafruit_GPS.h>
#include <Adafruit_SleepyDog.h>
#include <wdt.h>
#include <system.h>
#include <power.h>

#include "LoggingAtlasSensorBoard.h"
#include "Logger.h"
#include "Queue.h"

LoggingAtlasSensorBoard::LoggingAtlasSensorBoard(CorePlatform *corePlatform, SerialPortExpander *serialPortExpander, SensorBoard *sensorBoard, FuelGauge *fuelGauge, DataBoatReadingHandler *handler) :
    AtlasSensorBoard(corePlatform, serialPortExpander, sensorBoard, fuelGauge), handler(handler) {
}

int32_t LoggingAtlasSensorBoard::deepSleep(int32_t ms) {
    if (Serial) {
        delay(ms);
        return ms;
    }
    uint32_t time = Watchdog.sleep(ms);
    platformAdjustUptime(time);
    return time;
}

void LoggingAtlasSensorBoard::done(SensorBoard *board) {
    size_t numberOfValues = 0;

    for (size_t i = 0; i < FK_ATLAS_SENSORS_PACKET_NUMBER_VALUES; ++i) {
        packet.values[i] = 0.0f;
    }

    for (size_t i = 0; i < board->getNumberOfValues(); ++i) {
        if (numberOfValues < FK_ATLAS_SENSORS_PACKET_NUMBER_VALUES) {
            packet.values[numberOfValues++] = board->getValues()[i];
        }
        else {
            Serial.println("Too many values (in done)!");
        }
    }

    updateGps();

    updateAndHandlePacket(numberOfValues);

    logPrinter.flush();

    if (shouldWaitForBattery()) {
        waitForBattery(numberOfValues);
    }
    else {
        board->takeReading();
    }
}

static void blink() {
    digitalWrite(13, LOW);
    delay(500);
    digitalWrite(13, HIGH);
    delay(1000);
    digitalWrite(13, LOW);
}

void LoggingAtlasSensorBoard::waitForBattery(size_t numberOfValues) {
    while (shouldWaitForBattery()) {
        DEBUG_PRINTLN("Waiting for charge, updating GPS...");

        updateGps();

        DEBUG_PRINTLN("Sending packet...");

        updateAndHandlePacket(numberOfValues);

        DEBUG_PRINTLN("Sleeping...");

        logPrinter.flush();

        int32_t remaining = (int32_t)300000;
        while (remaining >= 0) {
            digitalWrite(13, HIGH);
            int sleepingFor = wdt_enable(WDT_PERIOD_8X);
            remaining -= sleepingFor;
            if (!Serial && sleepingFor > 0) {
                system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);
                system_sleep();
            }

            wdt_checkin();

            DEBUG_PRINT(sleepingFor);
            DEBUG_PRINT(" ");
            DEBUG_PRINTLN(remaining);
            logPrinter.flush();

            blink();
            blink();
            delay(1000);
        }
    }
}

void LoggingAtlasSensorBoard::updateAndHandlePacket(size_t numberOfValues) {
    uint32_t now = SystemClock->now();
    packet.time = now;
    packet.battery = gauge != nullptr ? gauge->stateOfCharge() : 0;
    packet.fk.kind = FK_PACKET_KIND_DATA_BOAT_SENSORS;

    logPacketLocally(numberOfValues);

    if (handler != nullptr) {
        handler->handleReading(&packet, numberOfValues);
    }
}

void LoggingAtlasSensorBoard::updateGps() {
    portExpander->select(5);

    HardwareSerial *serial = portExpander->getSerial();
    Adafruit_GPS gps(serial);

    gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("Reading GPS...");

    bool haveFix = false;
    uint32_t started = millis();
    while (!haveFix && millis() - started < 10 * 1000) {
        while (serial->available()) {
            char c = gps.read();

            Serial.print(c);

            if (gps.newNMEAreceived()) {
                if (gps.parse(gps.lastNMEA())) {
                    if (gps.fix) {
                        DEBUG_PRINTLN("Fix");

                        if (lastClockAdjustment == 0) {
                            DateTime dateTime = DateTime(gps.year, gps.month, gps.day, gps.hour, gps.minute, gps.seconds);
                            uint32_t time = dateTime.unixtime();
                            SystemClock->set(time);

                            Serial.print(gps.year);
                            Serial.print(" ");
                            Serial.print(gps.month);
                            Serial.print(" ");
                            Serial.print(gps.day);
                            Serial.print(" ");
                            Serial.print(gps.hour);
                            Serial.print(" ");
                            Serial.print(gps.minute);
                            Serial.print(" ");
                            Serial.print(gps.seconds);
                            Serial.print(" ");
                            Serial.println(time);

                            lastClockAdjustment = millis();
                        }

                        packet.latitude = gps.latitudeDegrees;
                        packet.longitude = gps.longitudeDegrees;
                        packet.altitude = gps.altitude;
                        packet.angle = gps.angle;
                        packet.speed = gps.speed;

                        haveFix = true;

                        break;
                    }
                    else {
                        DEBUG_PRINTLN("No fix");
                    }
                }
            }
        }
    }

    serial->end();
}

bool LoggingAtlasSensorBoard::shouldWaitForBattery() {
    if (gauge == nullptr) {
        return false;
    }

    float charge = gauge->stateOfCharge();

    return charge < 15.0f;
}

void LoggingAtlasSensorBoard::writePacket(Stream &stream, data_boat_packet_t *packet, size_t numberOfValues) {
    stream.print(packet->time);
    stream.print(",");
    stream.print(packet->battery);
    stream.print(",");
    stream.print(packet->latitude, 6);
    stream.print(",");
    stream.print(packet->longitude, 6);
    stream.print(",");
    stream.print(packet->altitude, 6);
    stream.print(",");
    stream.print(packet->angle);
    stream.print(",");
    stream.print(packet->speed);

    for (uint8_t i = 0; i < numberOfValues; ++i) {
        stream.print(",");
        stream.print(packet->values[i]);
    }

    stream.println();
}

void LoggingAtlasSensorBoard::logPacketLocally(size_t numberOfValues) {
    FileQueue queue;
    queue.enqueue((uint8_t *)&packet, sizeof(data_boat_packet_t));
    queue.startAtBeginning();

    File file = Logger::open(FK_SETTINGS_ATLAS_DATA_FILENAME);
    if (file) {
        writePacket(file, &packet, numberOfValues);
        file.close();
    }

    writePacket(Serial, &packet, numberOfValues);
}
