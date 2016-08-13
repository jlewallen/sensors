#ifndef NON_BLOCKING_SERIAL_H
#define NON_BLOCKING_SERIAL_H

#include "Platforms.h"

enum NonBlockingSerialProtocolState {
    Reading,
    Idle,
    Closed
};

class NonBlockingSerialProtocol {
private:
    SerialType *serial;
    NonBlockingSerialProtocolState state = Idle;
    uint32_t lastStateChangeAt;
    uint16_t replyWait;
    uint8_t sendsCounter;
    bool emptyBufferAfterEveryLine;
    bool addNewLines;
    String buffer;

public:
    NonBlockingSerialProtocol(uint16_t replyWait = 5000, bool emptyBufferAfterEveryLine = false, bool addNewLine = true);

    void setup();

    virtual bool tick();

    void setSerial(SerialType *newSerial) {
        serial = newSerial;
    }

    SerialType *getSerial() {
        return serial;
    }

    void open() {
        transition(Idle);
    }

protected:
    void clearSendsCounter() {
        sendsCounter = 0;
    }
    int8_t getSendsCounter() {
        return sendsCounter;
    }
    void sendCommand(const char *cmd);
    void transition(NonBlockingSerialProtocolState newState);
    virtual bool handle(String reply);
    void appendToBuffer(char newChar);
    void close();
};

#endif
