#include "voip_input_capture.h"

using namespace godot;


void VOIPInputCapture::_bind_methods(){

    // Property Get / Set

    ClassDB::bind_method(D_METHOD("set_muted"), &VOIPInputCapture::set_muted);
    ClassDB::bind_method(D_METHOD("is_muted"), &VOIPInputCapture::is_muted);

    ClassDB::bind_method(D_METHOD("set_volume"), &VOIPInputCapture::set_volume);
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


    // Methods

    ClassDB::bind_method(D_METHOD("add_peer"), &VOIPInputCapture::add_peer);
    ClassDB::bind_method(D_METHOD("remove_peer"), &VOIPInputCapture::remove_peer);

}

Ref<AudioStreamVOIP> VOIPInputCapture::add_peer(Ref<PacketPeer> peer) {
    return new Ref<AudioStreamVOIP>();
}

void VOIPInputCapture::remove_peer(Ref<PacketPeer> peer){
}