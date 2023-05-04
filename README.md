## VOIP One for Godot 4

A plugin which allows you to implement voice chat into your game.

**We are aiming to have the following features:**
- Drop in and use it right away, no insane complex rocket calculations to configure it. 🚀
- Opus codec support
- Mute support for individual speakers or global
- Echo cancellation
- Sensible default settings
- Simplified examples under /examples/
- Optional ability to stream and play arbitrary streams to clients (stretch goal)

**Is it a module or a gdextension:**
- It will be both

**Usability:**
- *Must* have documentation
- Ideally works on all platforms godot supports

**Nodes:**

Simple to use nodes:
- VoIPAudioStreamPlayer2D - 3D positional
- VoIPAudioStreamPlayer2D - 2D positional
- VoIPAudioStreamPlayer - non positional

Advanced nodes - which can be used for custom cases (like sending arbitrary non mic input) 
- VoIPCapture extends AudioEffectCapture - microphone input
- VoIPAudioStream extends AudioStream

**Settings:**
- Use separate networking thread or audio thread, or both.

**Networking:**
- We will look at using PacketPeer for handling our networking so that the implementation of the network is split from the plugin.
- Godot has done the heavy lifting here so we should just re-use `PacketPeer`

**Documentation:**
- Should provide a very short 5 line explanation of how to use it
- Should have a docs page for the complex portions
