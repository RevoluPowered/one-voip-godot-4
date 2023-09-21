#include "spsc_jitter_buffer.h"

#include <godot_cpp/variant/utility_functions.hpp>


SPSCJitterBuffer::SPSCJitterBuffer(int frame_size){
    //_speex_buffer = jitter_buffer_init(frame_size);
}

SPSCJitterBuffer::~SPSCJitterBuffer(){
    //jitter_buffer_destroy(_speex_buffer);
}


void SPSCJitterBuffer::push_samples(int timestamp, godot::PackedVector2Array samples){
    sample_queue.write(samples.size(), samples.ptr());
}

void SPSCJitterBuffer::pop_samples(godot::AudioFrame* samples, int frames){
    int num_read = sample_queue.read(frames, (godot::Vector2*) samples);

    godot::UtilityFunctions::print("num_read: ",num_read);
}