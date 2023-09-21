#ifndef SPSC_JITTER_BUFFER_H
#define SPSC_JITTER_BUFFER_H

#include <godot_cpp/classes/packed_data_container.hpp>

#include "speex/speex_jitter.h"

class SPSCJitterBuffer{

protected:
    JitterBuffer* _speex_buffer;

public:
    SPSCJitterBuffer();
    ~SPSCJitterBuffer();

    void push_back(int timestamp, godot::PackedVector2Array samples);
    godot::PackedVector2Array get_samples(int frames);
};

#endif