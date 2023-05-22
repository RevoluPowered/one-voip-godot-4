## Usage Requirements

Install the extension.

Spawn in a new bus in your project's audio mixer for your microphone, mute the bus (unless you want to listen to yourself), and add an VOIPInputCapture.

Add an AudioStreamPlayer somewhere in the scene, add a new AudioStreamMicrophone as the stream, set the Bus to the one you made (e.g. in the demo it's called "Mic") and turn on autoplay.

To send audio, connect a function to the packet_ready signal on the effect:
```
var idx = AudioServer.get_bus_index("Mic")
var effect = AudioServer.get_bus_effect(idx, 0)
effect.packet_ready.connect(_on_packet_ready)
```
And send the packet using any method you like in the _on_packet_ready function.

To receive audio, instantiate an AudioStreamVOIP every time a peer connects, and add it to some kind of AudioStreamPlayer. When you receive a packet, figure out what user sent it, and then push it to their AudioStreamVOIP:
```
stream.push_packet(packet)
```

An example of this setup will be in the Demo folder. The demo project, when two clients are opened, should begin transmitting audio with no errors.

## GDScript Objects
```
VOIPInputCapture : AudioEffectCapture
properties
    bool muted
    float volume
signals
    void packet_ready(PackedByteArray)

AudioStreamVOIP : AudioStream
methods
    void push_packet(PackedByteArray)

AudioStreamPlaybackVOIP : AudioStreamPlayback
```