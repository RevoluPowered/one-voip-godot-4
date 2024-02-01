#ifndef SPSC_JITTER_BUFFER_H
#define SPSC_JITTER_BUFFER_H

#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/classes/audio_frame.hpp>

#include "ring_buffer.h"
#include "opus.h"
#include "speex/speex_resampler.h"
#include "speex/speex_jitter.h"


namespace godot {


class SPSCJitterBuffer{

protected:
    const int RESAMPLING_QUALITY = 10; // 0 to 10

    int opus_frame_size;
    int opus_sample_rate;
    int output_sample_rate;

    JitterBuffer* speex_jitter_buffer;

    RingBuffer<Vector2> extra_samples; //This can store e.g. up to 441 samples if the audio thread doesn't want a multiple of 441

    int _last_opus_error = 0;
    int _last_resampler_error = 0;
    OpusDecoder* _opus_decoder;
    SpeexResamplerState* _resampler;

    PackedVector2Array _resample_buf; // Decode opus packet into here at 48k
    PackedVector2Array _sample_buf; // Decode into here at 44.1k
    std::vector<Vector2> _accumulation_buf; //Push into here before copying to sample output

public:
    SPSCJitterBuffer(int opus_frame_size, int opus_sample_rate, int output_sample_rate, int channels);
    ~SPSCJitterBuffer();

    // Push samples here and the time they were received
    void push_packet(PackedByteArray opus_packet, uint32_t timestamp, uint16_t sequence_number);

    // Pop samples from here on the realtime audio thread
    void pop_samples(AudioFrame* samples, int frames);

    // Advance by one tick
    void tick();
};


}

#endif