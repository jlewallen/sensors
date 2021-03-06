#ifndef LOGGING_ATLAS_SENSOR_BOARD_INCLUDED
#define LOGGING_ATLAS_SENSOR_BOARD_INCLUDED

#include "core.h"
#include "protocol.h"
#include "SerialPortExpander.h"
#include "AtlasSensorBoard.h"

class DataBoatReadingHandler {
public:
    virtual void handleReading(data_boat_packet_t *packet, size_t numberOfValues) = 0;
};

class LoggingAtlasSensorBoard : public AtlasSensorBoard {
private:
    data_boat_packet_t packet;
    DataBoatReadingHandler *handler = nullptr;
    uint32_t lastClockAdjustment = 0;

public:
    LoggingAtlasSensorBoard(CorePlatform *corePlatform, SerialPortExpander *serialPortExpander, SensorBoard *sensorBoard, FuelGauge *fuelGauge, DataBoatReadingHandler *handler);

public:
    void done(SensorBoard *board) override;
    void waitForBattery(size_t numberOfValues);

private:
    void logPacketLocally(size_t numberOfValues);
    void writePacket(Stream &stream, data_boat_packet_t *packet, size_t numberOfValues);
    bool shouldWaitForBattery();
    void updateGps();
    void updateAndHandlePacket(size_t numberOfValues);
    int32_t deepSleep(int32_t ms);

};

#endif
