#ifndef AUDIO_STREAM_VOIP_H
#define AUDIO_STREAM_VOIP_H

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>
#include <godot_cpp/classes/packet_peer.hpp>


namespace godot {


class AudioStreamVOIP : public AudioStream {
    GDCLASS(AudioStreamVOIP, AudioStream)

private:
    friend class AudioStreamPlaybackVOIP;

protected:
    static void _bind_methods() {};

public:
    // Properties

    Ref<PacketPeer> peer_conn; // WARNING: starts null


    // Overrides

    virtual Ref<AudioStreamPlayback> _instantiate_playback() const override;

    virtual String _get_stream_name() const override { return "VOIP Peer"; }
    virtual double _get_length() const override { return 0; }
    virtual bool _is_monophonic() const override { return true; }
    virtual double _get_bpm() const override { return 0.0; }
    virtual int32_t _get_beat_count() const override { return 0; }

};


}

#endif // AUDIO_STREAM_VOIP_H