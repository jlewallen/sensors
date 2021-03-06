#include "ParallelizedAtlasScientificSensors.h"

const char *CMD_RESPONSE1 = "RESPONSE,1";
const char *CMD_STATUS = "STATUS";
const char *CMD_SLOPE = "SLOPE,?";
const char *CMD_FACTORY = "FACTORY";
const char *CMD_DEVICE_INFORMATION = "I";
const char *CMD_LED_ON = "L,1";
const char *CMD_LED_OFF = "L,0";
const char *CMD_CONTINUOUS_OFF = "C,0";
const char *CMD_SLEEP = "SLEEP";
const char *CMD_READ = "R";
const char *CMD_CALIBRATE_TEMP = "T,";

uint8_t numberOfOccurences(String &str, char chr) {
    uint8_t number = 0;
    for (uint16_t i = 0; i < str.length(); ++i) {
        if (str[i] == chr) {
            number++;
        }
    }
    return number;
}

String getFirstLine(String &str) {
    str.trim();
    uint8_t cr = str.indexOf('\r');
    if (cr >= 0) return str.substring(0, cr + 1);
    uint8_t nl = str.indexOf('\n');
    if (nl >= 0) return str.substring(0, nl + 1);
    return str;
}

ParallelizedAtlasScientificSensors::ParallelizedAtlasScientificSensors(Stream *debug, SerialPortExpander *serialPortExpander, bool disableSleep, uint8_t maximumNumberOfValues, uint8_t maximumNumberOfRead0s) :
    NonBlockingSerialProtocol(debug), debug(debug), serialPortExpander(serialPortExpander), portNumber(0), disableSleep(disableSleep), numberOfValues(0), maximumNumberOfValues(maximumNumberOfValues), maximumNumberOfRead0s(maximumNumberOfRead0s) {
    values = new float[maximumNumberOfValues];
}

void ParallelizedAtlasScientificSensors::takeReading() {
    start();
    state = ParallelizedAtlasScientificSensorsState::LedsOnBeforeRead;
}

void ParallelizedAtlasScientificSensors::start() {
    portNumber = 0;
    numberOfValues = 0;
    numberOfRead0s = 0;
    if (runs == 0) {
        for (uint8_t i = 0; i < serialPortExpander->getNumberOfPorts(); ++i) {
            hasPortFailed[i] = 0;
            kinds[i] = AtlasSensorKind::Unknown;
        }
    }
    for (size_t i = 0; i < maximumNumberOfValues; ++i) {
        values[0] = 0.0f;
    }
    state = ParallelizedAtlasScientificSensorsState::Start;
    serialPortExpander->select(portNumber);
    setSerial(serialPortExpander->getSerial());
    open();
}

void ParallelizedAtlasScientificSensors::transition(ParallelizedAtlasScientificSensorsState newState) {
    state = newState;
    lastTransisitonAt = millis();
    clearSendsCounter();
}

bool ParallelizedAtlasScientificSensors::tick() {
    if (NonBlockingSerialProtocol::tick()) {
        return true;
    }
    if (getSendsCounter() == 3) {
        hasPortFailed[serialPortExpander->getPort()] = true;
        Serial.print("Port failed ");
        Serial.print(serialPortExpander->getPort());
        Serial.println("");
    }
    if (hasPortFailed[serialPortExpander->getPort()]) {
        debug->print(uint32_t(state));
        debug->print(" ");
        debug->print(serialPortExpander->getPort());
        debug->println(" SKIP");

        if (state == ParallelizedAtlasScientificSensorsState::Waiting) {
            numberOfRead0s = 0;
            transition(ParallelizedAtlasScientificSensorsState::Read0);
        }
        else {
            handle("", true);
        }
        return true;
    }
    switch (state) {
        case ParallelizedAtlasScientificSensorsState::Start: {
            portNumber = 0;
            serialPortExpander->select(portNumber);
            setSerial(serialPortExpander->getSerial());
            transition(ParallelizedAtlasScientificSensorsState::Factory);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Factory: {
            sendCommand(CMD_STATUS);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::DisableContinuousReading: {
            sendCommand(CMD_CONTINUOUS_OFF);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::ConfigureResponse: {
            sendCommand(CMD_RESPONSE1);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::DeviceInformation: {
            sendCommand(CMD_DEVICE_INFORMATION);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Status0: {
            sendCommand(CMD_STATUS);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Status1: {
            sendCommand(CMD_STATUS);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::LedsOn: {
            sendCommand(CMD_LED_ON);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Configure: {
            sendCommand(CMD_CONTINUOUS_OFF);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Waiting: {
            if (millis() - lastTransisitonAt > 5000) {
                numberOfRead0s = 0;
                transition(ParallelizedAtlasScientificSensorsState::Read0);
            }
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Read0: {
            debug->print("Start Read0: ");
            debug->println(portNumber);
            sendCommand(CMD_READ);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::LedsOnBeforeRead: {
            sendCommand(CMD_LED_ON);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Calibrate: {
            if (kinds[portNumber] == AtlasSensorKind::DO ||
                kinds[portNumber] == AtlasSensorKind::PH ||
                kinds[portNumber] == AtlasSensorKind::EC) {
                String cal = "T,";
                cal += temperature;
                sendCommand(cal.c_str());
            } else {
                transition(ParallelizedAtlasScientificSensorsState::Read1);
            }
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Read1: {
            sendCommand(CMD_READ);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::LedsOff: {
            sendCommand(CMD_LED_OFF);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Sleeping: {
            sendCommand(CMD_SLEEP);
            break;
        }
        case ParallelizedAtlasScientificSensorsState::Done: {
            debug->println("DONE");
            return false;
        }
    }
    return true;
}

size_t ParallelizedAtlasScientificSensors::parseReadings(String reply) {
    String firstLine = getFirstLine(reply);
    int8_t position = 0;

    while (true) {
        int16_t index = firstLine.indexOf(',', position);
        if (index < 0) {
            index = firstLine.indexOf('\r', position);
        }
        if (index < 0) {
            index = firstLine.indexOf('\n', position);
        }
        if (index > position && numberOfValues < maximumNumberOfValues) {
            String part = firstLine.substring(position, index);
            values[numberOfValues++] = part.toFloat();
            position = index + 1;

            if (kinds[portNumber] == AtlasSensorKind::RTD) {
                temperature = part.toFloat();
                debug->print("Temp: ");
                debug->println(temperature);
            }
        }
        else {
            if (numberOfValues >= maximumNumberOfValues) {
                Serial.println("Too many values!");
            }
            break;
        }
    }

    return numberOfValues;
}

NonBlockingHandleStatus ParallelizedAtlasScientificSensors::handle(String reply, bool forceTransition) {
    if (forceTransition || reply.indexOf("*OK") >= 0) {
        if (!forceTransition) {
            debug->print(uint32_t(state));
            debug->print(" ");
            debug->print(serialPortExpander->getPort());
            debug->print(">");
            debug->println(reply);
        }

        switch (state) {
            case ParallelizedAtlasScientificSensorsState::Factory: {
                transition(ParallelizedAtlasScientificSensorsState::DisableContinuousReading);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::DisableContinuousReading: {
                transition(ParallelizedAtlasScientificSensorsState::ConfigureResponse);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::ConfigureResponse: {
                transition(ParallelizedAtlasScientificSensorsState::DeviceInformation);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::DeviceInformation: {
                transition(ParallelizedAtlasScientificSensorsState::Status0);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::Status0: {
                transition(ParallelizedAtlasScientificSensorsState::Status1);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::Status1: {
                transition(ParallelizedAtlasScientificSensorsState::LedsOn);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::LedsOn: {
                transition(ParallelizedAtlasScientificSensorsState::Configure);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::Configure: {
                portNumber++;
                if (portNumber < serialPortExpander->getNumberOfPorts()) {
                    transition(ParallelizedAtlasScientificSensorsState::Factory);
                }
                else {
                    debug->println("Waiting...");
                    portNumber = 0;
                    transition(ParallelizedAtlasScientificSensorsState::Waiting);
                }
                serialPortExpander->select(portNumber);
                setSerial(serialPortExpander->getSerial());
                break;
            }
            case ParallelizedAtlasScientificSensorsState::LedsOnBeforeRead: {
                portNumber++;
                if (portNumber < serialPortExpander->getNumberOfPorts()) {
                    transition(ParallelizedAtlasScientificSensorsState::LedsOnBeforeRead);
                }
                else {
                    portNumber = 0;
                    transition(ParallelizedAtlasScientificSensorsState::Calibrate);
                }
                serialPortExpander->select(portNumber);
                setSerial(serialPortExpander->getSerial());
                break;
            }
            case ParallelizedAtlasScientificSensorsState::Calibrate: {
                transition(ParallelizedAtlasScientificSensorsState::Read1);
                break;
            }
            case ParallelizedAtlasScientificSensorsState::Read0: {
                if (!forceTransition) {
                    parseReadings(reply);
                    numberOfValues = 0;
                }

                portNumber++;
                if (portNumber == serialPortExpander->getNumberOfPorts()) {
                    if (numberOfRead0s < maximumNumberOfRead0s) {
                        portNumber = 0;
                        numberOfRead0s++;
                    }
                    else {
                        portNumber = 0;
                        debug->println("Starting Real Reads");
                        transition(ParallelizedAtlasScientificSensorsState::Read1);
                    }
                }

                serialPortExpander->select(portNumber);
                setSerial(serialPortExpander->getSerial());
                break;
            }
            case ParallelizedAtlasScientificSensorsState::Read1: {
                parseReadings(reply);

                if (disableSleep) {
                    transition(ParallelizedAtlasScientificSensorsState::LedsOff);
                }
                else {
                    transition(ParallelizedAtlasScientificSensorsState::Sleeping);
                }

                break;
            }
            case ParallelizedAtlasScientificSensorsState::LedsOff: {
                portNumber++;
                if (portNumber < serialPortExpander->getNumberOfPorts()) {
                    transition(ParallelizedAtlasScientificSensorsState::Calibrate);
                }
                else {
                    transition(ParallelizedAtlasScientificSensorsState::Done);
                    runs++;
                }
                serialPortExpander->select(portNumber);
                setSerial(serialPortExpander->getSerial());
                break;
            }
            case ParallelizedAtlasScientificSensorsState::Sleeping: {
                if (reply.indexOf("*SL") >= 0 || forceTransition) {
                    portNumber++;
                    if (portNumber < serialPortExpander->getNumberOfPorts()) {
                        transition(ParallelizedAtlasScientificSensorsState::Read1);
                    }
                    else {
                        transition(ParallelizedAtlasScientificSensorsState::Done);
                        runs++;
                    }
                    serialPortExpander->select(portNumber);
                    setSerial(serialPortExpander->getSerial());
                }
                else {
                    return NonBlockingHandleStatus::Ignored;
                }
                break;
            }
        }

        return NonBlockingHandleStatus::Handled;
    }
    else if (reply.indexOf("?I") >= 0) {
        if (reply.indexOf("pH") >= 0) {
            kinds[portNumber] = AtlasSensorKind::PH;
        }
        else if (reply.indexOf("DO") >= 0) {
            kinds[portNumber] = AtlasSensorKind::DO;
        }
        else if (reply.indexOf("ORP") >= 0) {
            kinds[portNumber] = AtlasSensorKind::ORP;
        }
        else if (reply.indexOf("RTD") >= 0) {
            kinds[portNumber] = AtlasSensorKind::RTD;
        }
        else if (reply.indexOf("EC") >= 0) {
            kinds[portNumber] = AtlasSensorKind::EC;
        }
        else {
            debug->println("Unknown AtlasBoardKind");
        }
    }
    else if (reply.indexOf("?STATUS") >= 0) {
        return NonBlockingHandleStatus::Ignored;
    }
    else if (reply.indexOf("*ER") >= 0) {
        Serial.println("*ER");
        return NonBlockingHandleStatus::Handled;
    }
    else {
        switch (state) {
        case ParallelizedAtlasScientificSensorsState::Read0:
        case ParallelizedAtlasScientificSensorsState::Read1: {
            if (numberOfOccurences(reply, '\n') <= 1) {
                break;
            }
        }
        default: {
            Serial.print("Unknown: ");
            Serial.println(reply);
            break;
        }
        }

        return NonBlockingHandleStatus::Unknown;
    }

    return NonBlockingHandleStatus::Ignored;
}
