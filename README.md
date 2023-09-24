# VoIP extension for Godot 4

Add voice chat to your game in minutes!

Features:
- Uses native godot bus effects and audio streams ✅
- Opus compression ✅
- Jitter buffer 🚧
- Echo cancelling 🚧

Builds for:
- [Windows](#windows) ✅
- Linux 🚧
- Android 🚧
- Mac 🚧
- [Web](#web-still-broken-on-godot-4) 🚧

## How to Use

Install webrtc-native by downloading [the latest release](https://github.com/godotengine/webrtc-native/releases) and dragging the webrtc-native folder into your godot project's root folder

Install one-voip by downloading [the latest release](https://github.com/RevoluPowered/one-voip-godot-4/releases) and dragging the one-voip folder into your godot project's root folder

Spawn in a new bus in your project's audio mixer for your microphone, mute the bus (unless you want to listen to yourself), and add an VOIPInputCapture.

Add an AudioStreamPlayer somewhere in the scene, add a new AudioStreamMicrophone as the stream, set the Bus to the one you made (e.g. in the demo it's called "Mic") and turn on autoplay.

To send audio, connect a function to the packet_ready signal on the effect:
```
var idx = AudioServer.get_bus_index("Mic")
var effect = AudioServer.get_bus_effect(idx, 0)
effect.packet_ready.connect(_on_packet_ready)
```
And send the packet using any method you like in the _on_packet_ready function.

To receive audio, instantiate an AudioStreamVOIP every time a peer connects, add it to some kind of audio stream player, and MAKE SURE you play it. When you receive a packet, figure out what user sent it, and then push it to their AudioStreamVOIP:
```
stream.push_packet(packet)
```

Check out the demos for a full example.

## Compiling

### Windows

Install CMake and Visual Studio build tools (MinGW will also work, instructions are for MSBuild)

In thirdparty/opus: `cmake -Bbuild`

In thirdparty/opus/build: `msbuild Opus.sln /p:Configuration=Release`

In the project root: `scons`

This will build to demo_rtc/bin

### Web (STILL BROKEN ON GODOT 4)

Install emscripten (chocolatey can help with dependencies: `choco install emscripten`)

In thirdparty/opus: `emcmake cmake -Bbuild`

In thirdparty/opus/build: `make`

In the project root: `scons platform=javascript`