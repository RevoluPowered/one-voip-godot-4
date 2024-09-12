#ifndef SPSC_JITTER_BUFFER_H
#define SPSC_JITTER_BUFFER_H

#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/classes/audio_frame.hpp>

#include <optional>
#include "ring_buffer.h"
#include "opus_packet.h"


namespace godot {


class FixedJitterBuffer{

protected:
    RingBuffer<OpusPacket> packet_queue;

    enum BUFFER_STATE {
        REGENERATING,
        OK
    } buffer_state = REGENERATING;

public:
    FixedJitterBuffer(unsigned buffer_size);
    ~FixedJitterBuffer();

    // Push packets here, they do not have to be in order
    void push_packet(OpusPacket packet);

    // Pop packets from here on the realtime audio thread
    std::optional<OpusPacket> pop_packet();
};


}

#endif