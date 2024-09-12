#include "fixed_jitter_buffer.h"

#include <cassert>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;


FixedJitterBuffer::FixedJitterBuffer(unsigned frame_size){
    packet_queue.resize(16);
}

FixedJitterBuffer::~FixedJitterBuffer(){
}


void FixedJitterBuffer::push_packet(const PackedByteArray& packet){
    if(packet_queue.space_left() == 0){
        for(int i=0; i<8; i++) packet_queue.read(); // delete 8 oldest packets
    }

    packet_queue.write(packet);
}

std::optional<const PackedByteArray&> FixedJitterBuffer::pop_packet(){
    if(packet_queue.data_left() > 0) return packet_queue.read();
    else return std::nullopt;
}