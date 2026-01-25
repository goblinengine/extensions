@tool extends Node
## TODO suggestions
## - Expose rates in Options and allow per-system dynamic adjustments.
## - Add jitter buffers or smoothing for slow frames.
## FIXME
## - Guard timer recreation on scene reload; ensure single registration per group.

#region static
static var tick: int = 0 # ticks since game started

## Trigger on every t ticks
func on_tick(t: int) -> bool: return tick % t == 0

## Create a global Engine-bound timer.
func create_timer(time: float, callback: Callable, process_always: bool = true, process_in_physics: bool = true, ignore_time_scale: bool = false) -> void:
	Engine.get_main_loop().create_timer(time, process_always, process_in_physics, ignore_time_scale).timeout.connect(callback)
#endregion

var _tick_rate := 0.0
var _anim_rate := 0.0
var _effect_rate := 0.0
var _low_rate := 0.0
var _decay_rate := 0.0

var _time := 0.0
var _anim_interval_usec := 0
var _anim_last_mod_usec := 0

@onready var tree := get_tree()

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_tick_rate = Consts.TICK_RATE
	_anim_rate = Consts.FAST_ANIM_RATE
	_effect_rate = 1.0
	_low_rate = 2.5
	_decay_rate = Consts.DECAY_RATE
	_time = 0.0
	_anim_interval_usec = maxi(1, int(_anim_rate * 1000000.0))
	_anim_last_mod_usec = int(Time.get_ticks_usec() % _anim_interval_usec)
	add_to_group(&"tick_process")
	add_to_group(&"effect_process")
	add_to_group(&"decay_process")
	set_process(true)

func _process(delta: float) -> void:
	# `delta` is scaled by `Engine.time_scale`. When time_scale is 0, delta is 0,
	# so time-scale-dependent groups won't run.
	if delta > 0.0:
		_time += delta
		if fmod(_time, _tick_rate) < delta: tree.call_group(&"tick_process", &"_tick_process")
		if fmod(_time, _effect_rate) < delta: tree.call_group(&"effect_process", &"_effect_process")
		if fmod(_time, _low_rate) < delta: tree.call_group(&"low_process", &"_low_process")
		if fmod(_time, _decay_rate) < delta: tree.call_group(&"decay_process", &"_decay_process")

	# UI/visual animation tick: constant real-time, independent of Engine.time_scale.
	var anim_mod_usec := int(Time.get_ticks_usec() % _anim_interval_usec)
	if anim_mod_usec < _anim_last_mod_usec: tree.call_group(&"anim_process", &"_anim_process")
	_anim_last_mod_usec = anim_mod_usec

func _tick_process() -> void:
	tick += 1

func _effect_process() -> void:
	Effects.process_effects()

func _decay_process() -> void:
	Condition.process_decay()
