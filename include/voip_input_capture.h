#ifndef VOIP_INPUT_CAPTURE_H
#define VOIP_INPUT_CAPTURE_H

#include <godot_cpp/classes/audio_effect_capture.hpp>
#include <godot_cpp/classes/packet_peer.hpp>

#include "audio_stream_voip.h"


namespace godot {


class VOIPInputCapture : public AudioEffectCapture {
    GDCLASS(VOIPInputCapture, AudioEffectCapture)

protected:
    static void _bind_methods();

public:
    // Properties

    bool muted;
    float volume; // [0, 1]


    // Methods

    Ref<AudioStreamVOIP> add_peer(Ref<PacketPeer> peer);

};


}

#endif // VOIP_INPUT_CAPTURE_H