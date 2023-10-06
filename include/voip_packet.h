#ifndef VOIP_PACKET_H
#define VOIP_PACKET_H

#include <godot_cpp/classes/packed_data_container.hpp>

namespace godot {

struct VOIPPacket {
    uint16_t sequence_number;
    uint32_t timestamp;
    PackedByteArray opus_packet;
};

}

#endif