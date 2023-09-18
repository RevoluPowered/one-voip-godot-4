#include "voip_input_capture.h"

using namespace godot;


void VOIPInputCapture::_bind_methods(){


    // Methods

    ClassDB::bind_method(D_METHOD("send_test_packet"), &VOIPInputCapture::send_test_packet);


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


void VOIPInputCapture::send_test_packet(){
    if( get_frames_available() >= OPUS_FRAME_SIZE ){
        PackedVector2Array samples = get_buffer(OPUS_FRAME_SIZE);
        PackedByteArray packet = _sample_buf_to_packet(samples);
        emit_signal("packet_ready", packet);
    }
}


PackedByteArray VOIPInputCapture::_sample_buf_to_packet(PackedVector2Array samples){
    PackedByteArray packet;
    for(int s=0; s<samples.size(); s++){
        packet.push_back(samples[s].x); // mono
    }

    return packet;
}