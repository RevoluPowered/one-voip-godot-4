## VOIP One for Godot 4

A plugin with the goal of merging all the VOIP plugins into one general purpose plugin for godot4 

The goal a drop in module/extension which can re-use the networking within the engine. (Undecided gdextension etc for now)

We are aiming to have the following features:
- Opus codec support
- Echo cancellation

Is it a module or a gdextension:
- GDExtension should be possible but need to work out if Mac signing is going to work

Usability:
- *Must* have documentation
- Ideally works on all platforms godot supports

Nodes:
- VOIPAudioStream extends AudioStream - positional/non positional
- VOIPMicInput extends AudioEffectCapture - microphone input

Settings:
- use separate networking thread or audio thread, or both.

Networking:
- We will look at using PacketPeer for handling our networking so that the implementation of the network is split from the plugin.
- Godot has done the heavy lifting here so we should just re-use `PacketPeer`

Documentation:
- Should provide a very short 5 line explanation of how to use it
- Should have a docs page for the complex portions
