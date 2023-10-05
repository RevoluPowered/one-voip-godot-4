extends Node

@export var SERVER_PORT = 80

func _ready():
	var peer = ENetMultiplayerPeer.new()
	peer.create_server(SERVER_PORT)
	multiplayer.multiplayer_peer = peer

@rpc("any_peer", "unreliable")
func _voice_packet_received(_packet):
	pass
