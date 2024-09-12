#include "audio_stream_playback_voip.h"

#include <cassert>

using namespace godot;


void AudioStreamPlaybackVOIP::_start(double _){
}

void AudioStreamPlaybackVOIP::_stop(){
}

bool AudioStreamPlaybackVOIP::_is_playing() const{
    return false;
}



// WARNING: REALTIME THREAD
// DO NOT MUTEX LOCK
// DO NOT MALLOC/NEW
int32_t AudioStreamPlaybackVOIP::_mix(AudioFrame *buffer, double rate_scale, int32_t frames){
    memset(buffer, 0, sizeof(AudioFrame) * frames); // Clear contents of buffer

    int32_t write_index = 0;
    while(write_index < frames){
        std::optional<OpusPacket> possible_packet = base->jitter_buffer.pop_packet();
        if(!possible_packet.has_value()) break;
        OpusPacket packet = possible_packet.value();

        // Convert packet to PackedVector2Array in 44100 kHz

        PackedVector2Array samples;
        samples.resize(base->OPUS_FRAME_SIZE * base->GODOT_SAMPLE_RATE / base->OPUS_SAMPLE_RATE);

        int decoded_samples = opus_decode_float(base->_opus_decoder, packet.bytes, sizeof(packet.bytes), (float*) base->_sample_buf.ptrw(), base->OPUS_FRAME_SIZE, 0);
        assert( decoded_samples > 0 );

        unsigned int num_samples = samples.size();
        unsigned int num_buffer_samples = base->_sample_buf.size();
        int resampling_result = speex_resampler_process_interleaved_float(base->_resampler, (float*) base->_sample_buf.ptr(), &num_buffer_samples, (float*) samples.ptrw(), &num_samples);
        samples.resize(num_samples);
        assert( resampling_result == 0 );

        // Write index: x  Written: /  Unwritten: 0
        // Case 1:  ////x000  samples_to_write = 4 -> samples_to_write + write_index = 4 + 4 <= frames = 8
        // Case 2:  ////x00   samples_to_write = 4 -> samples_to_write + write_index = 4 + 4 > frames = 7, so samples_to_write = frames - write_index = 7 - 4 = 3, this means stuff gets thrown out
        int samples_to_write = samples.size();
        if(samples_to_write + write_index > frames) samples_to_write = frames - write_index;
        memcpy(buffer + write_index, &samples, samples_to_write);
        write_index += samples_to_write;
    }

    return frames;
}