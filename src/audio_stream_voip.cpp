#include "audio_stream_voip.h"
#include "audio_stream_playback_voip.h"

using namespace godot;


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

void AudioStreamVOIP::push_packet(const PackedByteArray& packet){

}