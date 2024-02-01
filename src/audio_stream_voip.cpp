#include <godot_cpp/variant/utility_functions.hpp>

#include "audio_stream_voip.h"
#include "audio_stream_playback_voip.h"

#include <cassert>

using namespace godot;


AudioStreamVOIP::AudioStreamVOIP() : jitter_buffer(OPUS_FRAME_SIZE, OPUS_SAMPLE_RATE, GODOT_SAMPLE_RATE, CHANNELS) {
    // Allocate buffers in advance
    _opus_packet_buf.opus_packet.resize(sizeof(float) * CHANNELS * OPUS_FRAME_SIZE); // max possible size of an opus packet
}

AudioStreamVOIP::~AudioStreamVOIP(){
}


void AudioStreamVOIP::_bind_methods(){

    // Methods

    ClassDB::bind_method(D_METHOD("push_packet", "packet"), &AudioStreamVOIP::push_packet);

    ClassDB::bind_method(D_METHOD("tick"), &AudioStreamVOIP::tick);
}


Ref<AudioStreamPlayback> AudioStreamVOIP::_instantiate_playback() const{
    Ref<AudioStreamPlaybackVOIP> playback;
    playback.instantiate();
    playback->base = Ref<AudioStreamVOIP>(this);
    return playback;
}

// GDScript dev just received a packet, they want to push it into the buffer
void AudioStreamVOIP::push_packet(const PackedByteArray& bytes){
    // UtilityFunctions::print("Received bytes: ", packet.size());

    // Convert to opus packet
    VOIPPacket::from_byte_array(&_opus_packet_buf, bytes);

    jitter_buffer.push_packet(_opus_packet_buf.opus_packet, _opus_packet_buf.timestamp, _opus_packet_buf.sequence_number);

    /*
    // The number of samples the opus packet should have
    _sample_buf.resize(OPUS_FRAME_SIZE * GODOT_SAMPLE_RATE / OPUS_SAMPLE_RATE);

    // Decode opus packet into _resample_buf (48000, 480 samples)
    int decoded_samples = opus_decode_float(_opus_decoder, _opus_packet_buf.opus_packet.ptr(), _opus_packet_buf.opus_packet.size(), (float*) _resample_buf.ptrw(), OPUS_FRAME_SIZE, 0);
    assert( decoded_samples > 0 );

    // Resample from _resample_buf (48000, 480 samples) into samples (44100, 441 samples)
    unsigned int num_samples = _sample_buf.size();
    unsigned int num_buffer_samples = _resample_buf.size();
    int resampling_result = speex_resampler_process_interleaved_float(_resampler, (float*) _resample_buf.ptr(), &num_buffer_samples, (float*) _sample_buf.ptrw(), &num_samples);
    _sample_buf.resize(num_samples);
    assert( resampling_result == 0 );

    // Push to the jitter buffer
    jitter_buffer.push_samples(_opus_packet_buf.sequence_number, _opus_packet_buf.timestamp, _sample_buf);*/
}

void AudioStreamVOIP::tick(){
    jitter_buffer.tick();
}