#include <ArduinoJson.h>
#include <SD.h>
#include <Adafruit_SleepyDog.h>

#include "DataBoat.h"
#include "WifiConnection.h"
#include "AtlasSensorBoard.h"

class DataBoatConfiguration {
private:
    String ssid = "Cottonwood";
    String psk = "asdfasdf";
    String server = "intotheokavango.org";
    String path = "/ingest/databoat";

public:
    const char *getSsid() { return ssid.c_str(); }
    const char *getPsk() { return psk.c_str(); }
    const char *getServer() { return server.c_str(); }
    const char *getPath() { return path.c_str(); }

public:
    bool read();
};

bool DataBoatConfiguration::read() {
    if (!SD.exists(FK_SETTINGS_CONFIGURATION_FILENAME)) {
        return false;
    }

    File file = SD.open(FK_SETTINGS_CONFIGURATION_FILENAME, FILE_READ);
    if (!file) {
        return false;
    }

    file.close();

    return true;
}


DataBoat::DataBoat(HardwareSerial *gpsStream, uint8_t pinGpsEnable, atlas_sensors_packet_t *atlasPacket) :
    gps(gpsStream, pinGpsEnable), atlasPacket(atlasPacket) {
}

bool DataBoat::setup() {
    gps.setup();

    return true;
}

bool DataBoat::tick() {
    data_boat_packet_t reading;
    memzero((uint8_t *)&reading, sizeof(data_boat_packet_t));

    reading.water_temperature = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_WATER_TEMPERATURE];
    reading.conductivity      = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_EC];
    reading.salinity          = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_SALINITY];
    reading.ph                = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_PH];
    reading.dissolved_oxygen  = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_DO];
    reading.orp               = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_ORP];
    reading.temperature       = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_AIR_TEMPERATURE];
    reading.humidity          = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_HUMIDITY];
    reading.pressure          = atlasPacket->values[FK_ATLAS_SENSORS_FIELD_PRESSURE]; // If Bme280

    if (gps.tick(&reading)) {
        String json = readingToJson(&reading);
        upload(json);

        delay(1000);

        return false;
    }

    return true;
}

String DataBoat::readingToJson(data_boat_packet_t *reading) {
    StaticJsonBuffer<1024> jsonBuffer;

    JsonArray &root = jsonBuffer.createArray();

    JsonObject &rootData = jsonBuffer.createObject();
    root.add(rootData);
    rootData["t_local"] = reading->time;

    JsonObject &data = rootData.createNestedObject("data");
    data["conductivity"] = reading->conductivity;
    data["salinity"] = reading->salinity;
    data["ph"] = reading->ph;
    data["dissolved_oxygen"] = reading->dissolved_oxygen;
    data["orp"] = reading->orp;
    data["water temp"] = reading->water_temperature;
    data["humidity"] = reading->humidity;
    data["air_temp"] = reading->temperature;
    data["altitude"] = reading->altitude;
    data["barometric_pressure"] = reading->pressure;
    data.set("gps_lat", reading->latitude, 9);
    data.set("gps_long", reading->longitude, 9);
    data["speed"] = reading->speed;

    String json;
    json.reserve(2048);
    root.prettyPrintTo(json);

    return json;
}

void DataBoat::upload(String &json) {
    DataBoatConfiguration configuration;
    
    configuration.read();

    Serial.println("JSON: ");
    Serial.println(json);

    WifiConnection wifi(configuration.getSsid(), configuration.getPsk());
    if (wifi.open()) {
        wifi.post(configuration.getServer(), configuration.getPath(), "application/json", json.c_str());
    }

    wifi.off();

    DEBUG_PRINTLN("DataBoat Done");
}
