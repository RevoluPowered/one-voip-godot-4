## HOW TO SET UP DEMO

- Drag the webrtc folder from the webrtc-native extension into demo_rtc - https://github.com/godotengine/webrtc-native/releases

- Run `scons` in the root folder (make sure you have it installed - see https://docs.godotengine.org/en/stable/contributing/development/compiling/introduction_to_the_buildsystem.html) OR just drag the one-voip folder from a release into demo_rtc - https://github.com/RevoluPowered/one-voip-godot-4/releases

- Open demo_rtc in the newest godot editor

- Set SERVER_URL and LOBBY_NAME in multiplayer_client.gd - the server from the webRTC signaling example will work (https://github.com/godotengine/godot-demo-projects/tree/master/networking/webrtc_signaling/server_node)

- Set Debug > Run Multiple Instances to Run 2 Instances to test the mic

- Set your default windows microphone to a working microphone

- Run!