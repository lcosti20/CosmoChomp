extends Node
## Autoload that outlives level changes (port of BP_CosmoGameInstance).
##
## Owns the background music so it keeps playing across level loads, round restarts
## included. Both game modes call start_music when their level starts; it does nothing
## if the music is already playing.

const MUSIC := preload("res://assets/sounds/music/alex-morgan-video-game-pixel-chiptune-music-583271.mp3")

## UE: SpawnSound2D volume 0.5
@export var music_volume: float = 0.5

var _music: AudioStreamPlayer = null


func start_music() -> void:
	if _music and _music.playing:
		return
	if _music == null:
		var stream := MUSIC.duplicate() as AudioStreamMP3
		stream.loop = true
		_music = AudioStreamPlayer.new()
		_music.stream = stream
		_music.volume_db = linear_to_db(music_volume)
		# Keep playing while the game is paused
		_music.process_mode = Node.PROCESS_MODE_ALWAYS
		add_child(_music)
	_music.play()
