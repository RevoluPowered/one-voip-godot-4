#include "spsc_jitter_buffer.h"

SPSCJitterBuffer::SPSCJitterBuffer(int frame_size){
    _speex_buffer = jitter_buffer_init(frame_size);
}

SPSCJitterBuffer::~SPSCJitterBuffer(){
    jitter_buffer_destroy(_speex_buffer);
}


void SPSCJitterBuffer::push_samples(int timestamp, godot::PackedVector2Array samples){

}

godot::PackedVector2Array SPSCJitterBuffer::pop_samples(int frames){
    return godot::PackedVector2Array();
}