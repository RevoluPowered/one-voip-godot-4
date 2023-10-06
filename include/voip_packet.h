#ifndef VOIP_PACKET_H
#define VOIP_PACKET_H

#include <godot_cpp/classes/packed_data_container.hpp>

namespace godot {

struct VOIPPacket {
    uint16_t sequence_number;
    uint32_t timestamp;
    PackedByteArray opus_packet;

    // Convert to byte array to send
    PackedByteArray as_byte_array(){
        PackedByteArray to_return;

        to_return.resize(6);
        to_return.encode_u16(0, sequence_number);
        to_return.encode_u32(2, timestamp);
        to_return.append_array(opus_packet);

        return to_return;
    }

    // Convert back from byte array after received
    VOIPPacket(PackedByteArray byte_array){
        sequence_number = byte_array.decode_u16(0);
        timestamp = byte_array.decode_u32(2);
        opus_packet = byte_array.slice(6);
    }

    VOIPPacket() {}
};

}

#endif