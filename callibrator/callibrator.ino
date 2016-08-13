#include <SPI.h>
#include <SD.h>

#include "Platforms.h"
#include "core.h"
#include "SerialPortExpander.h"
#include "AtlasSensorBoard.h"
#include "SimpleBuffer.h"

String phScript[] = {
    "c,0",
    "c,0",
    "L,1",
    "R",
    "STATUS"
};

String doScript[] = {
    "c,0",
    "c,0",
    "L,1",
    "R",
    "STATUS"
};

String orpScript[] = {
    "c,0",
    "c,0",
    "L,1",
    "R",
    "STATUS"
};

String ecScript[] = {
    "c,0",
    "c,0",
    "L,1",
    "R",
    "STATUS"
};

ConductivityConfig conductivityConfig = ConductivityConfig::OnExpanderPort4;

SerialType *getSerialForPort(uint8_t port) {
    if (port < 3 || conductivityConfig != ConductivityConfig::OnSerial2) {
        return &Serial1;
    }
    else if (port == 4) {
        return &Serial2;
    }
    return &Serial1;
}

typedef enum ScriptRunnerState {
    WaitingOnReply,
    DeviceIdle
} ScriptRunnerState;

class ScriptRunner : public NonBlockingSerialProtocol {
private:
    SerialPortExpander *portExpander;
    ScriptRunnerState state;
    uint8_t position;
    const char *activeCommand;
    const char *expectedResponse;
    String *commands;
    size_t length;
    uint8_t nextPort;

public:
    ScriptRunner(SerialPortExpander *portExpander) :
        NonBlockingSerialProtocol(10000, true), portExpander(portExpander), expectedResponse(NULL),
        commands(NULL), activeCommand(NULL), position(0), length(0), nextPort(0) {
    }

    void select(uint8_t port) {
        portExpander->select(port);
        setSerial(getSerialForPort(port));
    }

    template<size_t N>
    void setScript(String (&newCommands)[N]) {
        commands = newCommands;
        position = 0;
        length = N;
    }

    const char *currentCommand() {
        if (length == 0 || position >= length) {
            return "";
        }
        return commands[position].c_str();
    }

    void startOver() {
        position = 0;
    }

    void send() {
        send(currentCommand(), "*OK");
        if (position < length) {
            position++;
        }
    }

    void sendEverywhere(const char *command, const char *expected) {
        nextPort = 1;
        send(command, expected);
    }

    void send(const char *command, const char *expected) {
        activeCommand = command;
        expectedResponse = expected;
        state = ScriptRunnerState::WaitingOnReply;
        sendCommand(command);
    }

    bool isIdle() {
        return state == ScriptRunnerState::DeviceIdle;
    }

    bool tick() {
        if (state == ScriptRunnerState::WaitingOnReply) {
            if (NonBlockingSerialProtocol::tick()) {
                return true;
            }
        }

        return false;
    }

    bool isAtEnd() {
        return sizeof(commands) / sizeof(String);
    }

    bool handle(String reply) {
        Serial.println(reply);
        if (expectedResponse != NULL && reply.indexOf(expectedResponse) == 0) {
            if (nextPort >= 4) {
                state = ScriptRunnerState::DeviceIdle;
                return true;
            }
            else if (nextPort > 0) {
                Serial.print("Port: ");
                Serial.println(nextPort);
                select(nextPort);
                send(activeCommand, expectedResponse);
                nextPort++;
                return false;
            }
        }
        else if (reply.indexOf("*ER") == 0) {
            state = ScriptRunnerState::DeviceIdle;
            return true;
        }
        return false;
    }
};

typedef enum ReplState {
    WaitingOnDevice,
    ShowPrompt,
    Command
} ReplState;

class Repl {
private:
    ReplState state = ReplState::Command;
    SimpleBuffer buffer;
    ScriptRunner *scriptRunner;

public:
    Repl(ScriptRunner *scriptRunner) : scriptRunner(scriptRunner) {
    }

    bool tick() {
        switch (state) {
        case ReplState::WaitingOnDevice: {
            if (!scriptRunner->tick()) {
                state = ReplState::ShowPrompt;
            }

            break;
        }
        case ReplState::ShowPrompt: {
            showPrompt();
            state = ReplState::Command;
            break;
        }
        case ReplState::Command: {
            if (Serial.available()) {
                int16_t c = (int16_t)Serial.read();
                if (c < 0) {
                    return false;
                }

                char newChar = (char)c;

                Serial.print(newChar);

                if (newChar == '\r') {
                    Serial.println();

                    state = ReplState::ShowPrompt;

                    handle(buffer.c_str());
                    buffer.clear();
                }
                else {
                    buffer.append(newChar);
                }
            }
            break;
        }
        }
        return false;
    }

    void showPrompt() {
        Serial.print(scriptRunner->currentCommand());
        Serial.print("> ");
    }
    
    void handle(String command) {
        if (command.startsWith("p ")) {
            uint8_t number = command.substring(2).toInt();
            scriptRunner->select(number);
        }
        else if (command.startsWith("ph")) {
            Serial.println("Ph Mode");
            scriptRunner->setScript(phScript);
        }
        else if (command.startsWith("do")) {
            Serial.println("Do Mode");
            scriptRunner->setScript(doScript);
        }
        else if (command.startsWith("orp")) {
            Serial.println("Orp Mode");
            scriptRunner->setScript(orpScript);
        }
        else if (command.startsWith("ec")) {
            Serial.println("Ec Mode");
            scriptRunner->setScript(ecScript);
        }
        else if (command.startsWith("factory")) {
            Serial.println("Factory Reset All The Things!");
            scriptRunner->select(0);
            scriptRunner->sendEverywhere("factory", "*RE");
            state = ReplState::WaitingOnDevice;
        }
        else if (command.startsWith("!")) {
            scriptRunner->select(0);
            scriptRunner->sendEverywhere(command.substring(1).c_str(), "*OK");
            state = ReplState::WaitingOnDevice;
        }
        else if (command == "") {
            scriptRunner->send();
            state = ReplState::WaitingOnDevice;
        }
        else {
            scriptRunner->send(command.c_str(), "*OK");
            state = ReplState::WaitingOnDevice;
        }
    }
};

CorePlatform corePlatform;

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

    Serial1.begin(9600);
    platformSerial2Begin(9600);

    Serial.println("Loop");
}

void loop() {
    SerialPortExpander portExpander(PORT_EXPANDER_SELECT_PIN_0, PORT_EXPANDER_SELECT_PIN_1);
    ScriptRunner scriptRunner(&portExpander);
    Repl repl(&scriptRunner);

    repl.showPrompt();

    while (1) {
        repl.tick();

        delay(50);
    }
}

// vim: set ft=cpp: