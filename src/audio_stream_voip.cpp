#include <godot_cpp/variant/utility_functions.hpp>

#include "audio_stream_voip.h"
#include "audio_stream_playback_voip.h"

#include <cassert>

using namespace godot;


AudioStreamVOIP::AudioStreamVOIP() : jitter_buffer(OPUS_FRAME_SIZE * GODOT_SAMPLE_RATE / OPUS_SAMPLE_RATE) {
    _opus_decoder = opus_decoder_create(OPUS_SAMPLE_RATE, CHANNELS, &_last_opus_error);
    assert(_opus_decoder != NULL);

    _resampler = speex_resampler_init(CHANNELS, OPUS_SAMPLE_RATE, GODOT_SAMPLE_RATE, RESAMPLING_QUALITY, &_last_resampler_error);
    assert( _resampler != NULL );

    _sample_buf.resize(OPUS_FRAME_SIZE);
}

AudioStreamVOIP::~AudioStreamVOIP(){
    opus_decoder_destroy(_opus_decoder);
    speex_resampler_destroy(_resampler);
}


void AudioStreamVOIP::_bind_methods(){

    // Methods

    ClassDB::bind_method(D_METHOD("push_packet", "packet"), &AudioStreamVOIP::push_packet);
}


Ref<AudioStreamPlayback> AudioStreamVOIP::_instantiate_playback() const{
    Ref<AudioStreamPlaybackVOIP> playback;
    playback.instantiate();
    playback->base = Ref<AudioStreamVOIP>(this);
    return playback;
}

void AudioStreamVOIP::push_packet(const PackedByteArray& packet_bytes){
    // UtilityFunctions::print("Received bytes: ", packet.size());

    // Push to the jitter buffer

    OpusPacket packet;
    memcpy(&packet, packet_bytes.ptr(), packet_bytes.size());
    jitter_buffer.push_packet(packet);
}