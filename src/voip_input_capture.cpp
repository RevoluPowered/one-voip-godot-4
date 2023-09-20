#include "voip_input_capture.h"

using namespace godot;


VOIPInputCapture::VOIPInputCapture(){
    _opus_encoder = opus_encoder_create(OPUS_SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &_last_opus_error);
    _resampler = speex_resampler_init(CHANNELS, GODOT_SAMPLE_RATE, OPUS_SAMPLE_RATE, RESAMPLING_QUALITY, &_last_resampler_error);
    if (_resampler == NULL) ; // ...
    _sample_buf.resize(OPUS_FRAME_SIZE);
}

VOIPInputCapture::~VOIPInputCapture(){
    opus_encoder_destroy(_opus_encoder);
    speex_resampler_destroy(_resampler);
}

void VOIPInputCapture::_bind_methods(){


    // Methods

    ClassDB::bind_method(D_METHOD("send_test_packets"), &VOIPInputCapture::send_test_packets);


    // Property Get / Set

    ClassDB::bind_method(D_METHOD("set_muted", "muted"), &VOIPInputCapture::set_muted);
    ClassDB::bind_method(D_METHOD("is_muted"), &VOIPInputCapture::is_muted);

    ClassDB::bind_method(D_METHOD("set_volume", "volume"), &VOIPInputCapture::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume"), &VOIPInputCapture::get_volume);


    // Properties

    ClassDB::add_property(
        "VOIPInputCapture",
        PropertyInfo(Variant::BOOL, "muted", godot::PROPERTY_HINT_NONE, "", 6U, "bool"),
        "set_muted",
        "is_muted"
    );

    ClassDB::add_property(
        "VOIPInputCapture",
        PropertyInfo(Variant::FLOAT, "volume", godot::PROPERTY_HINT_NONE, "", 6U, "float"),
        "set_volume",
        "get_volume"
    );


    // Signals

    ClassDB::add_signal(
        "VOIPInputCapture",
        MethodInfo(
            "packet_ready",
            PropertyInfo(Variant::PACKED_BYTE_ARRAY, "audio_packet", PROPERTY_HINT_NONE, "", 6U, "bytearray")
        )
    );

}


void VOIPInputCapture::send_test_packets(){
    int godot_frame_size = OPUS_FRAME_SIZE * GODOT_SAMPLE_RATE / OPUS_SAMPLE_RATE; // please don't be a fraction

    while( get_frames_available() >= godot_frame_size ){
        PackedVector2Array samples = get_buffer(godot_frame_size);
        PackedByteArray packet = _sample_buf_to_packet(samples);
        emit_signal("packet_ready", packet);
    }
}


PackedByteArray VOIPInputCapture::_sample_buf_to_packet(PackedVector2Array samples){
    // ASSERT( _sample_buf.size() == OPUS_FRAME_SIZE );
    // ASSERT( samples.size() == OPUS_FRAME_SIZE * GODOT_SAMPLE_RATE / OPUS_SAMPLE_RATE );

    PackedByteArray packet;
    packet.resize( sizeof(float) * CHANNELS * OPUS_FRAME_SIZE );

    unsigned int num_samples = samples.size();
    unsigned int num_buffer_samples = OPUS_FRAME_SIZE;
    int resampling_result = speex_resampler_process_interleaved_float(_resampler, (float*) samples.ptr(), &num_samples, (float*) _sample_buf.ptr(), &num_buffer_samples);
    if (resampling_result != 0) ; // ...

    int packet_size = opus_encode_float(_opus_encoder, (float*) _sample_buf.ptr(), OPUS_FRAME_SIZE, (unsigned char*) packet.ptr(), packet.size());
    if (packet_size < 0) ; // ...
    packet.resize( packet_size );

    return packet;
}