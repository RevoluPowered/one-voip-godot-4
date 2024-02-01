#ifndef VOIP_PACKET_H
#define VOIP_PACKET_H

#include <godot_cpp/classes/packed_data_container.hpp>

namespace godot {

struct VOIPPacket {
    uint16_t sequence_number;
    uint32_t timestamp; // in ms
    PackedByteArray opus_packet;

    // Convert to byte array to send
    void to_byte_array(PackedByteArray* byte_array){
        byte_array->resize(6);
        byte_array->encode_u16(0, sequence_number);
        byte_array->encode_u32(2, timestamp);
        byte_array->append_array(opus_packet);
    }

    // Convert back from byte array after received
    static void from_byte_array(VOIPPacket* opus_packet, const PackedByteArray& byte_array){
        opus_packet->sequence_number = byte_array.decode_u16(0);
        opus_packet->timestamp = byte_array.decode_u32(2);
        opus_packet->opus_packet = byte_array.slice(6);
    }
};

}

#endif