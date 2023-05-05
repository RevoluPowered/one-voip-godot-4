#include "voip_input_capture.h"

using namespace godot;


void VOIPInputCapture::_bind_methods(){

}

Ref<AudioStreamVOIP> VOIPInputCapture::add_peer(Ref<PacketPeer> peer) {
    return new Ref<AudioStreamVOIP>();
}