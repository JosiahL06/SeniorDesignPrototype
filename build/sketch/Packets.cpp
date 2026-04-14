#line 1 "C:\\Users\\Josia\\OneDrive\\Documents\\SeniorDesignPrototype\\FunctionalPrototype\\Packets.cpp"
#include "Packets.h"

// =============================
// Unpack BLE Command Packet
// =============================
CommandPacket unpackCommand(const std::string& value) {

    CommandPacket cmd{};

    if (value.size() < 3) {
        return cmd; // invalid packet
    }

    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(value.data());

    uint32_t packet =
        ((uint32_t)data[0] << 16) |
        ((uint32_t)data[1] << 8)  |
        ((uint32_t)data[2]);

    cmd.commandId = static_cast<CommandId>(
        (packet >> 18) & 0b11
    );
    cmd.inSteps   = (packet >> 17) & 0b1;
    cmd.reverse   = (packet >> 16) & 0b1;

    cmd.interval  = packet & 0xFFFF;
    cmd.degrees   = (cmd.interval >> 8) & 0xFF;
    cmd.speed     =  cmd.interval & 0xFF;

    return cmd;
}