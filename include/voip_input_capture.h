#ifndef VOIP_INPUT_CAPTURE_H
#define VOIP_INPUT_CAPTURE_H

#include <godot_cpp/classes/audio_effect_capture.hpp>

#include "audio_stream_voip.h"


namespace godot {


class VOIPInputCapture : public AudioEffectCapture {
    GDCLASS(VOIPInputCapture, AudioEffectCapture)

protected:
    static void _bind_methods();

private:
    PackedByteArray _sample_buf_to_packet(PackedVector2Array samples);

public:
    int OPUS_FRAME_SIZE = 480;

    // Properties

    bool muted = false;
    float volume = 1; // [0, 1]


    // Property Set / Get

    void set_muted(const bool _muted) { muted = _muted; }
    bool is_muted() const { return muted; }

    void set_volume(const float _volume) { volume = _volume; } // Error condition for outside [0,1]?
    float get_volume() const { return volume; }


    // Methods

    void send_test_packet();

};


}

#endif // VOIP_INPUT_CAPTURE_H