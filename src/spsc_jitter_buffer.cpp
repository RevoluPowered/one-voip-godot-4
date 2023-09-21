#include "spsc_jitter_buffer.h"

SPSCJitterBuffer::SPSCJitterBuffer(){
    _speex_buffer = jitter_buffer_init(441);
}

SPSCJitterBuffer::~SPSCJitterBuffer(){
    jitter_buffer_destroy(_speex_buffer);
}


void SPSCJitterBuffer::push_back(int timestamp, godot::PackedVector2Array samples){

}

godot::PackedVector2Array SPSCJitterBuffer::get_samples(int frames){
    return godot::PackedVector2Array();
}