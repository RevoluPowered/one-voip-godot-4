#include "spsc_jitter_buffer.h"

#include <cassert>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;


SPSCJitterBuffer::SPSCJitterBuffer(int _opus_frame_size, int _opus_sample_rate, int _output_sample_rate, int channels){
    opus_frame_size = _opus_frame_size;
    opus_sample_rate = _opus_sample_rate;
    output_sample_rate = _output_sample_rate;

    _opus_decoder = opus_decoder_create(_opus_sample_rate, channels, &_last_opus_error);
    assert(_opus_decoder != NULL);

    _resampler = speex_resampler_init(channels, _opus_sample_rate, _output_sample_rate, RESAMPLING_QUALITY, &_last_resampler_error);
    assert( _resampler != NULL );

    speex_jitter_buffer = jitter_buffer_init(10);

    // Allocate buffers in advance
    extra_samples.resize(12); // 4096 samples
    _accumulation_buf.reserve(16384); // Max buffer size
    _resample_buf.resize(_opus_frame_size);
    _sample_buf.resize(_opus_frame_size * _opus_sample_rate / _output_sample_rate);
}

SPSCJitterBuffer::~SPSCJitterBuffer(){
    opus_decoder_destroy(_opus_decoder);
    speex_resampler_destroy(_resampler);
    jitter_buffer_destroy(speex_jitter_buffer);
}


void SPSCJitterBuffer::push_packet(PackedByteArray opus_packet, uint32_t timestamp, uint16_t sequence_number){
    const JitterBufferPacket speex_packet = {
        (char*) opus_packet.ptr(),
        (uint32_t) opus_packet.size(),
        timestamp,
        (uint32_t) (1000 * opus_frame_size / opus_sample_rate),
        sequence_number,
        (uint32_t) 0
    };
    
    jitter_buffer_put(speex_jitter_buffer, &speex_packet);

    UtilityFunctions::print("Added a packet of size ", opus_packet.size());
}

void SPSCJitterBuffer::pop_samples(AudioFrame* samples, int frames){
    int frames_left = frames;

    _accumulation_buf.resize(0);

    // Read from the leftover samples first
    while(extra_samples.data_left() > 0 && frames_left > 0){
        _accumulation_buf.push_back(extra_samples.read());
        frames_left--;
    }

    while(frames_left > 0){
        JitterBufferPacket speex_packet;
        spx_int32_t current_timestamp;

        UtilityFunctions::print("Test");
        int err = jitter_buffer_get(speex_jitter_buffer, &speex_packet, 1000 * opus_frame_size / opus_sample_rate, &current_timestamp);

        if(err == 0){
            UtilityFunctions::print("Got packet of length ", speex_packet.len);

            // The number of samples the opus packet should have
            /*_sample_buf.resize(opus_frame_size * output_sample_rate / opus_sample_rate);

            // Decode opus packet into _resample_buf (48000, 480 samples)
            int decoded_samples = opus_decode_float(_opus_decoder, (unsigned char*) speex_packet.data, speex_packet.len, (float*) _resample_buf.ptrw(), opus_frame_size, 0);
            assert( decoded_samples > 0 );

            // Resample from _resample_buf (48000, 480 samples) into samples (44100, 441 samples)
            unsigned int num_samples = _sample_buf.size();
            unsigned int num_buffer_samples = _resample_buf.size();
            int resampling_result = speex_resampler_process_interleaved_float(_resampler, (float*) _resample_buf.ptr(), &num_buffer_samples, (float*) _sample_buf.ptrw(), &num_samples);
            assert( resampling_result == 0 );
            _sample_buf.resize(num_samples);

            int num_copied = 0;
            for(; num_copied<num_samples && frames_left > 0; num_copied++){
                _accumulation_buf.push_back(_sample_buf[num_copied]);
                frames_left--;
            }

            // Copy to extra samples if we have any left over
            for(int i=num_copied; i<num_samples; i++){
                extra_samples.write(_sample_buf[i]);
            }*/

            // For debug
            for(;frames_left>0;){
                _accumulation_buf.push_back(Vector2(0,0));
                frames_left--;
            }
        }
        else{
            UtilityFunctions::print("Unable to get packet, code ", err);
            
            for(;frames_left>0;){
                _accumulation_buf.push_back(Vector2(0,0));
                frames_left--;
            }
        }
    }

    //UtilityFunctions::print("To copy: ", _accumulation_buf.size(), " frames: ", frames);
    assert(_accumulation_buf.size() < frames);
    memcpy(samples, _accumulation_buf.data(), _accumulation_buf.size() * sizeof(Vector2));
}

void SPSCJitterBuffer::tick(){
    //jitter_buffer_tick(speex_jitter_buffer);
}