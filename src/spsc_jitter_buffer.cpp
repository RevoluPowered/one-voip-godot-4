#include "spsc_jitter_buffer.h"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;


SPSCJitterBuffer::SPSCJitterBuffer(int frame_size){
}

SPSCJitterBuffer::~SPSCJitterBuffer(){
}


void SPSCJitterBuffer::push_samples(int timestamp, PackedVector2Array samples){
    int num_written = sample_queue.write(samples.size(), samples.ptr());

    if(num_written < samples.size()){
        UtilityFunctions::print("TOO MANY SAMPLES - samples: ", samples.size(), " written: ", num_written);
        PackedVector2Array temp;
        temp.resize(2048);
        sample_queue.read(temp.size(), temp.ptrw()); // Skip forward
    }
    // UtilityFunctions::print("num_written: ",num_written);
}

void SPSCJitterBuffer::pop_samples(AudioFrame* samples, int frames){
    int num_read = sample_queue.read(frames, (Vector2*) samples);

    if(num_read < frames) UtilityFunctions::print("NOT ENOUGH SAMPLES - frames: ", frames, " num_read: ", num_read);
    // UtilityFunctions::print("num_read: ",num_read);
}