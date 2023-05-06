extends Node

var enet: ENetMultiplayerPeer


func _enter_tree():
	var args: Dictionary = parse_args(OS.get_cmdline_args())
	
	var port = 80
	if args.has("port"):
		port = args["port"]
	
	enet = ENetMultiplayerPeer.new()
	if enet.create_server(port) == OK:
		multiplayer.set_multiplayer_peer(enet)
		print("Server running on port ", port)
	else:
		print("Failed to create server.")


func parse_args(args: PackedStringArray):
	var arguments = {}
	for argument in args:
		# Parse valid command-line arguments into a dictionary
		if argument.find("=") > -1:
			var key_value = argument.split("=")
			arguments[key_value[0].lstrip("--")] = key_value[1]
			
	return arguments
