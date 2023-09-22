#include "spsc_jitter_buffer.h"

#include <godot_cpp/variant/utility_functions.hpp>


SPSCJitterBuffer::SPSCJitterBuffer(int frame_size){
}

SPSCJitterBuffer::~SPSCJitterBuffer(){
}


void SPSCJitterBuffer::push_samples(int timestamp, godot::PackedVector2Array samples){
    int num_written = sample_queue.write(samples.size(), samples.ptr());

    if(num_written < samples.size()){
        godot::UtilityFunctions::print("TOO MANY SAMPLES - samples: ", samples.size(), " written: ", num_written);
        godot::PackedVector2Array temp;
        temp.resize(2048);
        sample_queue.read(temp.size(), (godot::Vector2*) temp.ptr()); // Skip forward
    }
    // godot::UtilityFunctions::print("num_written: ",num_written);
}

void SPSCJitterBuffer::pop_samples(godot::AudioFrame* samples, int frames){
    int num_read = sample_queue.read(frames, (godot::Vector2*) samples);

    if(num_read < frames) godot::UtilityFunctions::print("NOT ENOUGH SAMPLES - frames: ", frames, " num_read: ", num_read);
    // godot::UtilityFunctions::print("num_read: ",num_read);
}