#include <SPI.h>
#include <SD.h>

#include "Platforms.h"
#include "core.h"
#include "AtlasSensorBoard.h"
#include "DataBoat.h"

class WifiAtlasSensorBoard : public AtlasSensorBoard {
public:
    WifiAtlasSensorBoard(CorePlatform *corePlatform);

public:
    void doneReadingSensors(Queue *queue);
};

WifiAtlasSensorBoard::WifiAtlasSensorBoard(CorePlatform *corePlatform) :
    AtlasSensorBoard(corePlatform, false) {
}

void WifiAtlasSensorBoard::doneReadingSensors(Queue *queue) {


    DataBoat dataBoat(&Serial2, 9);
    dataBoat.setup();
    while (dataBoat.tick()) {
        delay(10);
    }
}

CorePlatform corePlatform;
WifiAtlasSensorBoard wifiAtlasSensorBoard(&corePlatform);

void setup() {
    Serial.begin(115200);

    #ifdef WAIT_FOR_SERIAL
    while (!Serial) {
        delay(100);
        if (millis() > WAIT_FOR_SERIAL) {
            break;
        }
    }
    #endif

    Serial.println("Begin");

    corePlatform.setup();
    wifiAtlasSensorBoard.setup();

    Serial.println("Loop");
}

void loop() {
    wifiAtlasSensorBoard.tick();

    delay(50);
}

// vim: set ft=cpp:

