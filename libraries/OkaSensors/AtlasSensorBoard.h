#ifndef ATLAS_SENSOR_BOARD_H
#define ATLAS_SENSOR_BOARD_H

#include <Adafruit_BME280.h>
#include <OneWire.h>

#include "Platforms.h"

#include "core.h"
#include "protocol.h"
#include "network.h"
#include "Logger.h"

#include "AtlasScientific.h"
#include "SerialPortExpander.h"

#define FK_ATLAS_SENSORS_FIELD_ORP                           0
#define FK_ATLAS_SENSORS_FIELD_PH                            1
#define FK_ATLAS_SENSORS_FIELD_DO                            2
#define FK_ATLAS_SENSORS_FIELD_EC                            3
#define FK_ATLAS_SENSORS_FIELD_TDS                           4
#define FK_ATLAS_SENSORS_FIELD_SAL                           5
#define FK_ATLAS_SENSORS_FIELD_SG                            6
#define FK_ATLAS_SENSORS_FIELD_WATER_TEMPERATURE             11

/**
 * A board containing primarily Atlas sensors and a few others, including water temperature.
 */
class AtlasSensorBoard {
private:
    CorePlatform *corePlatform;
    SerialPortExpander portExpander;
    AtlasScientificBoard board;
    Adafruit_BME280 bme;
    atlas_sensors_packet_t packet;
    uint8_t packetValueIndex = 0;
    bool hasConductivity;

public:
    AtlasSensorBoard(CorePlatform *corePlatform, bool hasConductivity = true);

public:
    bool tick();
    void setup();
    virtual void doneReadingSensors(Queue *queue, atlas_sensors_packet_t *packet);

private:
    float getWaterTemperature();

    void populatePacket();
    void logPacketLocally();
};

#endif