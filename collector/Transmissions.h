#ifndef TRANSMISSIONS_H_INCLUDED
#define TRANSMISSIONS_H_INCLUDED

#include <Arduino.h>
#include "protocol.h"
#include "core.h"
#include "Configuration.h"
#include "WeatherStation.h"
#include "TransmissionStatus.h"
#include "FuelGauge.h"
#include "RockBlock.h"

class Transmissions : RockBlockMessages {
private:
    WeatherStation *weatherStation;
    RtcAbstractSystemClock *systemClock;
    Configuration *configuration;
    TransmissionStatus *status;
    FuelGauge *fuel;
    CorePlatform *core;
    Memory *memory;
    bool forceStatus = false;

public:
    Transmissions(CorePlatform *core, WeatherStation *weatherStation, RtcAbstractSystemClock *systemClock, Configuration *configuration, TransmissionStatus *status, FuelGauge *fuel, Memory *memory);

public:
    void handleTransmissionIfNecessary();
    bool sendStatusTransmission();

public:
    virtual void onMessage(String message) override;

private:
    String atlasPacketToMessage(atlas_sensors_packet_t *packet);
    String sonarPacketToMessage(sonar_station_packet_t *packet);
    String weatherStationPacketToMessage(weather_station_packet_t *packet);
    String locationToMessage(gps_fix_t *location);
    String diagnosticsToMessage();

    void sendSensorTransmission(bool sendAtlas, bool sendWeather, bool sendSonar);
    void sendLocationTransmission();
    bool transmission(String message);
};

#endif
