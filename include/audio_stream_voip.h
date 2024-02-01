#ifndef AUDIO_STREAM_VOIP_H
#define AUDIO_STREAM_VOIP_H

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>

#include "voip_packet.h"
#include "spsc_jitter_buffer.h"


namespace godot {


class AudioStreamVOIP : public AudioStream {
    GDCLASS(AudioStreamVOIP, AudioStream)

private:
    friend class AudioStreamPlaybackVOIP;
    
    VOIPPacket _opus_packet_buf; // Decode incoming VOIP packets into here

protected:
    static void _bind_methods();

public:
    // Constants

    const int GODOT_SAMPLE_RATE = 44100;
    const int OPUS_FRAME_SIZE = 480;
    const int OPUS_SAMPLE_RATE = 48000;
    const int CHANNELS = 2;


    // Properties

    SPSCJitterBuffer jitter_buffer;


    // Overrides

    virtual Ref<AudioStreamPlayback> _instantiate_playback() const override;

    virtual String _get_stream_name() const override { return "VOIP Peer"; }
    virtual double _get_length() const override { return 0; }
    virtual bool _is_monophonic() const override { return true; }
    virtual double _get_bpm() const override { return 0.0; }
    virtual int32_t _get_beat_count() const override { return 0; }


    // Methods

    AudioStreamVOIP();
    ~AudioStreamVOIP();

    void push_packet(const PackedByteArray&);
    void tick();

};


}

#endif // AUDIO_STREAM_VOIP_H