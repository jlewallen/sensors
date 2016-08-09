#include "network.h"

#define RETRY_DELAY         2500

NetworkProtocolState::NetworkProtocolState(NetworkState state, CorePlatform *platform)
    : state(state), platform(platform), stateDelay(0), lastTick(0), lastTickNonDelayed(0), pingAgainAfterDequeue(true), packetsReceived(0) {
}

void NetworkProtocolState::tick() {
    /**
     * Some caveats to this:
     * 1) lastTickNonDelayed should be set on transition or the order here should
     *    be changed. So we would transition to a "delayed" state but the last
     *    time we recorded the lastTickNonDelayed was long past and so we would 
          immediately leave it.
     * 2) Avoid more than one transition per tick.
     */
    bool wasDelayed = false;
    bool inDelay = false;
    if (stateDelay > 0) {
        wasDelayed = true;
        inDelay = millis() - lastTickNonDelayed < stateDelay;
        if (!inDelay) {
            stateDelay = 0;
        }
    }

    lastTick = millis();
    if (!inDelay) {
        lastTickNonDelayed = lastTick;
    }

    switch (state) {
        case NetworkState::EnqueueFromNetwork: {
            checkForPacket();

            break;
        }
        case NetworkState::PingForListener: {
            sendPing();

            transition(NetworkState::ListenForPong, RETRY_DELAY);

            break;
        }
        case NetworkState::ListenForAck: {
            if (!inDelay) {
                /*
                if (wasDelayed) {
                    DEBUG_PRINT("DELAYED ");
                }
                */
                DEBUG_PRINTLN("Nack");
                if (platform->radio()->resend()) {
                    transition(NetworkState::ListenForAck, RETRY_DELAY);
                }
                else {
                    transition(NetworkState::GiveListenerABreak, RETRY_DELAY);
                }
            }
            else {
                checkForPacket();
            }
            break;
        }
        case NetworkState::ListenForPong: {
            if (!inDelay) {
                if (platform->radio()->resend()) {
                    /*
                    if (wasDelayed) {
                        DEBUG_PRINT("DELAYED ");
                    }
                    */
                    DEBUG_PRINTLN("Ping retry");
                    transition(NetworkState::ListenForPong, RETRY_DELAY);
                }
                else {
                    DEBUG_PRINTLN("No pong");
                    platform->radio()->sleep();
                    transition(NetworkState::NobodyListening, 0);
                }
            }
            else {
                checkForPacket();
            }
            break;
        }
        case NetworkState::GiveListenerABreak: {
            if (!inDelay) {
                transition(NetworkState::PingForListener, 0);
            }

            break;
        }
    }
}

void NetworkProtocolState::handle(fk_network_packet_t *packet) {
    packetsReceived++;

    switch (packet->kind) {
    case FK_PACKET_KIND_PING: {
        DEBUG_PRINTLN("Ponging!");

        fk_network_pong_t pong;
        memzero((uint8_t *)&pong, sizeof(fk_network_pong_t));
        pong.fk.kind = FK_PACKET_KIND_PONG;
        pong.time = 0;
        platform->radio()->send((uint8_t *)&pong, sizeof(fk_network_pong_t));
        platform->radio()->waitPacketSent();
        checkForPacket();
        break;
    }
    case FK_PACKET_KIND_ATLAS_SENSORS: {
        if (state == NetworkState::EnqueueFromNetwork) {
            delay(100);
            // Enqueue
            // platform->enqueue((uint8_t *)packet);

            sendAck();

            // DEBUG_PRINTLN("Acked");
        }
        else {
            // DEBUG_PRINTLN("Rougue sensors!");
        }
        break;
    }
    case FK_PACKET_KIND_PONG: {
        if (state == NetworkState::ListenForPong) {
            DEBUG_PRINTLN("Ponged");
            platform->queue()->startAtBeginning();
            dequeueAndSend();
        }
        else {
            DEBUG_PRINTLN("Ignore pong");
        }
        break;
    }
    case FK_PACKET_KIND_ACK: {
        // DEBUG_PRINTLN("Ack");
        dequeueAndSend();
        break;
    }
    default: {
        DEBUG_PRINTLN("Unknown");

        // So this is actually weird. I'm thinking that sometimes things get corrupted and we
        // should avoid that rather than causing the remote end to retry all over again. I saw
        // this happen during testing (ie restarting during writes, etc..) and I'm just worried
        // we'll see this in production.
        sendAck();
        break;
    }
    }
}

void NetworkProtocolState::sendPing() {
    DEBUG_PRINTLN("Pinging");

    fk_network_ping_t ping;
    memzero((uint8_t *)&ping, sizeof(fk_network_ping_t));
    ping.fk.kind = FK_PACKET_KIND_PING;
    platform->radio()->send((uint8_t *)&ping, sizeof(fk_network_ping_t));
    platform->radio()->waitPacketSent();
    checkForPacket();
}

void NetworkProtocolState::sendAck() {
    fk_network_ack_t ack;
    memzero((uint8_t *)&ack, sizeof(fk_network_ack_t));
    ack.fk.kind = FK_PACKET_KIND_ACK;
    platform->radio()->send((uint8_t *)&ack, sizeof(fk_network_ack_t));
    platform->radio()->waitPacketSent();
    checkForPacket();
}

void NetworkProtocolState::checkForPacket() {
    platform->radio()->tick();

    if (platform->radio()->hasPacket()) {
        handle((fk_network_packet_t *)platform->radio()->getPacket());
        platform->radio()->clear();
    }
}

void NetworkProtocolState::startOver(NetworkState newState) {
    packetsReceived = 0;
    transition(newState, 0);
}

void NetworkProtocolState::transition(NetworkState newState, uint32_t newDelay) {
    state = newState;
    stateDelay = newDelay;
    lastTickNonDelayed = millis();
}

void NetworkProtocolState::dequeueAndSend() {
    if (state == NetworkState::ListenForPong || state == NetworkState::ListenForAck) {
        uint8_t *packet = platform->dequeue();
        if (packet != NULL) {
            // This size is bad. -jlewallen
            platform->radio()->send(packet, sizeof(atlas_sensors_packet_t));
            platform->radio()->waitPacketSent();
            packet = NULL;

            transition(NetworkState::ListenForAck, 5000);
        }
        else {
            DEBUG_PRINTLN("Empty");
            platform->radio()->sleep();
            transition(NetworkState::QueueEmpty, 0);
        }
    }
}


