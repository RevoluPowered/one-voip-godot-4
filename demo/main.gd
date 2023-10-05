extends Node

@export var SERVER_URL = ""
@export var SERVER_PORT = 80

var user = load("res://user.tscn")

var mic_capture: VOIPInputCapture

var users = {} # {Peer ID: AudioStreamPlayer}

func _ready():
	var peer = ENetMultiplayerPeer.new()
	peer.create_client(SERVER_URL, SERVER_PORT)
	multiplayer.multiplayer_peer = peer
	
	multiplayer.peer_connected.connect(_peer_connected)
	
	var mic_bus = AudioServer.get_bus_index("Mic")
	mic_capture = AudioServer.get_bus_effect(mic_bus, 0)
	mic_capture.packet_ready.connect(self._voice_packet_ready)
	

func _peer_connected(id):
	print("Peer connected with ID ", id)
	users[id] = user.instantiate()
	add_child(users[id])
	
func _peer_disconnected(id):
	print("Peer disconnected with ID ", id)
	users[id].queue_free()
	users.erase(id)
	
	
func _connected_to_server():
	print("Connected to server ", SERVER_URL, ":", SERVER_PORT)
	
func _connection_failed():
	print("Failed to connect to server ", SERVER_URL, ":", SERVER_PORT)
	
func _server_disconnected():
	print("Server disconnected.")


func _voice_packet_ready(packet):
	_voice_packet_received.rpc(packet)
	
@rpc("any_peer", "unreliable")
func _voice_packet_received(packet):
	var sender_id = multiplayer.get_remote_sender_id()
	users[sender_id].stream.push_packet(packet)


func _process(delta):
	mic_capture.send_test_packets()
