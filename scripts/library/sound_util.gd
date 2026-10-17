class_name SoundUtil
extends RefCounted
## UE: Play Sound at Location. The UE sounds have no attenuation settings, so they play
## unspatialized at full volume; a one-shot AudioStreamPlayer does the same.


static func play_at(context: Node, stream: AudioStream, _location: Vector3 = Vector3.ZERO, volume_linear: float = 1.0) -> void:
	if stream == null or context == null or not context.is_inside_tree():
		return
	var player := AudioStreamPlayer.new()
	player.stream = stream
	player.volume_db = linear_to_db(volume_linear)
	context.get_tree().root.add_child(player)
	player.finished.connect(player.queue_free)
	player.play()
