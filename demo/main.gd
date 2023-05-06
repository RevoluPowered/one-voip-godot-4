extends Node3D

var user_scene = preload("res://user.tscn")

var enet: ENetMultiplayerPeer
var voip: VOIPInputCapture

var peers = {}
var peer_scenes = {}


func _enter_tree():
	var args: Dictionary = parse_args(OS.get_cmdline_args())
	
	var port: int = 80
	if args.has("port"):
		port = int(args["port"])
	
	var ip = "127.0.0.1"
	if args.has("ip"):
		ip = args["ip"]
	
	var idx = AudioServer.get_bus_index("Mic")
	voip = AudioServer.get_bus_effect(idx, 0)
	
	multiplayer.peer_connected.connect(peer_connected)
	multiplayer.peer_disconnected.connect(peer_disconnected)
	
	multiplayer.connected_to_server.connect(connected_to_server)
	multiplayer.connection_failed.connect(connection_failed)
	multiplayer.server_disconnected.connect(server_disconnected)
	
	enet = ENetMultiplayerPeer.new()
	enet.create_client(ip, port)
	if enet.get_connection_status() == MultiplayerPeer.CONNECTION_DISCONNECTED:
		connection_failed()
		return
	multiplayer.set_multiplayer_peer(enet)
	
	print("Connecting...")


func connected_to_server():
	print("Connected to server.")
	
func connection_failed():
	print("Failed to connect to server.")
	
func server_disconnected():
	print("Server disconnected.")


func peer_connected(id):
	if id != 1: # No VOIP with server
		print("User connected: ", id)
		peers[id] = enet.get_peer(id)
		peer_scenes[id] = user_scene.instantiate()
		add_child(peer_scenes[id])
		peer_scenes[id].stream = voip.add_peer(peers[id])
		peer_scenes[id].play() # Necessary?

func peer_disconnected(id):
	if id != 1: # No VOIP with server
		print("User disconnected: ", id)
		peers.erase(id)


func parse_args(args: PackedStringArray):
	var arguments = {}
	for argument in args:
		# Parse valid command-line arguments into a dictionary
		if argument.find("=") > -1:
			var key_value = argument.split("=")
			arguments[key_value[0].lstrip("--")] = key_value[1]
			
	return arguments
