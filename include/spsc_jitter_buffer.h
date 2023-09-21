#ifndef SPSC_JITTER_BUFFER_H
#define SPSC_JITTER_BUFFER_H

#include <godot_cpp/classes/packed_data_container.hpp>

#include "speex/speex_jitter.h"

class SPSCJitterBuffer{

protected:
    JitterBuffer* _speex_buffer;

public:
    SPSCJitterBuffer(int frame_size);
    ~SPSCJitterBuffer();

    // Push samples here and the time they were received
    void push_samples(int timestamp, godot::PackedVector2Array samples);

    // Pop samples from here on the realtime audio thread
    godot::PackedVector2Array pop_samples(int frames);
};

#endif