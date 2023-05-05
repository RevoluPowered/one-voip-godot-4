## Usage Requirements

Install the extension.

Spawn in a new bus in your project's audio mixer for your microphone, mute the bus (unless you want to listen to yourself), and add an VOIPInputCapture.

Add an AudioStreamPlayer somewhere in the scene, add a new AudioStreamMicrophone as the stream, set the Bus to the one you made (e.g. in the demo it's called "Mic") and turn on autoplay.

To use a PacketPeer (e.g. an ENetPacketPeer) to start sending and receiving audio, just get the effect somewhere in a script like this, and trade the peer for an audio stream:
```
var idx = AudioServer.get_bus_index("Mic")
var effect = AudioServer.get_bus_effect(idx, 0)
var audio_stream = effect.add_peer(debug_channel)
```

Then put the stream into any AudioStreamPlayer. Remember to stop/delete the player when the peer disconnects.

An example of this setup will be in the Demo folder.

## GDScript Objects
```
VOIPInputCapture : AudioEffectCapture
properties
    bool muted
    float volume
methods
    VOIPAudioStream add_peer(PacketPeer)

VOIPAudioStream : AudioStream

VOIPAudioStreamPlayback : AudioStreamPlayback
```