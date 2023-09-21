#ifndef SPSC_JITTER_BUFFER_H
#define SPSC_JITTER_BUFFER_H

#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/classes/audio_frame.hpp>

#include "spsc_queue.hpp"

class SPSCJitterBuffer{

protected:
    deaod::spsc_queue<godot::Vector2, 4096> sample_queue;

public:
    SPSCJitterBuffer(int frame_size);
    ~SPSCJitterBuffer();

    // Push samples here and the time they were received
    void push_samples(int timestamp, godot::PackedVector2Array samples);

    // Pop samples from here on the realtime audio thread
    void SPSCJitterBuffer::pop_samples(godot::AudioFrame* samples, int frames);
};

#endif