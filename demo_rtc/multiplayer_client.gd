extends "ws_webrtc_client.gd"

@export var RECORD_VOICE = false

@export var SERVER_URL = ""
@export var LOBBY_NAME = ""

var user = load("res://user.tscn")

var rtc_mp: WebRTCMultiplayerPeer = WebRTCMultiplayerPeer.new()
var sealed := false
var mic_capture: VOIPInputCapture
var master_record: AudioEffectRecord

var users = {} # {WebRTCPeerConnection ID: AudioStreamPlayer}


func _ready():
	print("Audio devices: ", AudioServer.get_input_device_list())
	print("Current device: ", AudioServer.get_input_device())
	
	start(SERVER_URL, LOBBY_NAME)
	
	var mic_bus = AudioServer.get_bus_index("Mic")
	mic_capture = AudioServer.get_bus_effect(mic_bus, 0)
	mic_capture.packet_ready.connect(self._packet_ready)
	
	var master_bus = AudioServer.get_bus_index("Master")
	master_record = AudioServer.get_bus_effect(master_bus, 0)
	if RECORD_VOICE:
		master_record.set_recording_active(true)


func _exit_tree():
	if RECORD_VOICE:
		var recording = master_record.get_recording()
		master_record.set_recording_active(false)
		recording.save_to_wav('test.wav')


func _init():
	connected.connect(self._connected)
	disconnected.connect(self._disconnected)

	offer_received.connect(self._offer_received)
	answer_received.connect(self._answer_received)
	candidate_received.connect(self._candidate_received)

	lobby_joined.connect(self._lobby_joined)
	lobby_sealed.connect(self._lobby_sealed)
	peer_connected.connect(self._peer_connected)
	peer_disconnected.connect(self._peer_disconnected)


func _process(delta):
	super._process(delta)
	mic_capture.send_test_packets()
	
	var peers = rtc_mp.get_peers()
	for peer_id in peers.keys():
		peers[peer_id]["connection"].poll()
		var channel_0 = peers[peer_id]["channels"][0]
		if channel_0.get_ready_state() == WebRTCDataChannel.STATE_OPEN:
			while channel_0.get_available_packet_count() > 0:
				users[peer_id].stream.push_packet(channel_0.get_packet())


func _packet_ready(packet): #New packet from mic to send
	var peers = rtc_mp.get_peers()
	for peer_id in peers.keys():
		var channel_0 = peers[peer_id]["channels"][0]
		if channel_0.get_ready_state() == WebRTCDataChannel.STATE_OPEN:
			channel_0.put_packet(packet)


func start(url, lobby = "", mesh:=true):
	stop()
	sealed = false
	self.mesh = mesh
	self.lobby = lobby
	connect_to_url(url)


func stop():
	rtc_mp.close()
	close()


func _create_peer(id):
	var peer: WebRTCPeerConnection = WebRTCPeerConnection.new()
	
	peer.initialize({
		"iceServers": [ { "urls": ["stun:stun.l.google.com:19302"] } ]
	})
	peer.session_description_created.connect(self._offer_created.bind(id))
	peer.ice_candidate_created.connect(self._new_ice_candidate.bind(id))
	rtc_mp.add_peer(peer, id)  # For some reason this opens 3 data channels
	if id < rtc_mp.get_unique_id(): # So lobby creator never creates offers.
		peer.create_offer()
	return peer


func _new_ice_candidate(mid_name, index_name, sdp_name, id):
	#print("Sending candidate: %s %s %d" % [index_name, sdp_name, id])
	send_candidate(id, mid_name, index_name, sdp_name)


func _offer_created(type, data, id):
	if not rtc_mp.has_peer(id):
		return
	#print("created", type)
	rtc_mp.get_peer(id).connection.set_local_description(type, data)
	if type == "offer": send_offer(id, data)
	else: send_answer(id, data)


func _connected(id, use_mesh):
	#print("Connected %d, mesh: %s" % [id, use_mesh])
	if use_mesh:
		rtc_mp.create_mesh(id, [MultiplayerPeer.TRANSFER_MODE_UNRELIABLE, MultiplayerPeer.TRANSFER_MODE_UNRELIABLE, MultiplayerPeer.TRANSFER_MODE_UNRELIABLE])
	elif id == 1:
		rtc_mp.create_server([MultiplayerPeer.TRANSFER_MODE_UNRELIABLE, MultiplayerPeer.TRANSFER_MODE_UNRELIABLE, MultiplayerPeer.TRANSFER_MODE_UNRELIABLE])
	else:
		rtc_mp.create_client(id, [MultiplayerPeer.TRANSFER_MODE_UNRELIABLE, MultiplayerPeer.TRANSFER_MODE_UNRELIABLE, MultiplayerPeer.TRANSFER_MODE_UNRELIABLE])


func _lobby_joined(lobby):
	print("Joined lobby " + lobby)
	self.lobby = lobby


func _lobby_sealed():
	sealed = true


func _disconnected():
	print("Disconnected: %d: %s" % [code, reason])
	if not sealed:
		stop() # Unexpected disconnect


func _peer_connected(id):
	print("Peer connected %d" % id)
	_create_peer(id)
	users[id] = user.instantiate()
	add_child(users[id])


func _peer_disconnected(id):
	print("Peer disconnected %d" % id)
	if rtc_mp.has_peer(id): rtc_mp.remove_peer(id)
	users[id].queue_free()
	users.erase(id)


func _offer_received(id, offer):
	#print("Got offer: %d" % id)
	if rtc_mp.has_peer(id):
		rtc_mp.get_peer(id).connection.set_remote_description("offer", offer)


func _answer_received(id, answer):
	#print("Got answer: %d" % id)
	if rtc_mp.has_peer(id):
		rtc_mp.get_peer(id).connection.set_remote_description("answer", answer)


func _candidate_received(id, mid, index, sdp):
	if rtc_mp.has_peer(id):
		#print("Received candidate: %s %s %s" % [mid, index, sdp])
		rtc_mp.get_peer(id).connection.add_ice_candidate(mid, index, sdp)
