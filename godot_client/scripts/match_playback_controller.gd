extends Node
class_name MatchPlaybackController

signal match_loaded(seed: int, total_ticks: int, meta: Dictionary)
signal frame_updated(tick: int, frame_float: float, hud_state: Dictionary)
signal playback_state_changed(is_playing: bool, playback_speed: float)
signal rollback_completed(target_tick: int, sim_time_ms: float)
signal view_mode_changed(mode: int)

const CamRigScript = preload("res://scripts/camera_rig_3d.gd")
const Pitch2DScript = preload("res://scripts/pitch_renderer.gd")
const Pitch3DScript = preload("res://scripts/pitch_renderer_3d.gd")
const MatchHUDScript = preload("res://scripts/match_hud.gd")

const SPEED_MULTIPLIERS: Array[float] = [1.0, 2.0, 4.0, 8.0, 16.0]

var bridge: Node = null
var view_container_2d: Node2D = null
var pitch_2d: Pitch2DScript = null
var view_container_3d: Node3D = null
var pitch_3d: Pitch3DScript = null
var camera_rig_3d: CamRigScript = null
var match_hud: MatchHUDScript = null

var current_seed: int = 12345
var total_frames: int = 108000
var current_frame_float: float = 0.0
var is_playing: bool = true
var playback_speed: float = 1.0
var is_scrubbing: bool = false
var current_view_mode: int = CamRigScript.CameraMode.BROADCAST

var pos_arr: Array[Vector2] = []
var facing_arr: Array[float] = []
var stamina_arr: Array[float] = []

func _init() -> void:
	pos_arr.resize(22)
	facing_arr.resize(22)
	stamina_arr.resize(22)
	for i in range(22):
		pos_arr[i] = Vector2.ZERO
		facing_arr[i] = 0.0
		stamina_arr[i] = 1.0

func setup(
	p_view_2d: Node2D,
	p_pitch_2d: Pitch2DScript,
	p_view_3d: Node3D,
	p_pitch_3d: Pitch3DScript,
	p_hud: MatchHUDScript
) -> void:
	view_container_2d = p_view_2d
	pitch_2d = p_pitch_2d
	view_container_3d = p_view_3d
	pitch_3d = p_pitch_3d
	match_hud = p_hud
	if pitch_3d:
		camera_rig_3d = pitch_3d.get_node_or_null("CameraRig3D") as CamRigScript

func initialize_engine(initial_seed: int = 12345) -> bool:
	current_seed = initial_seed
	if ClassDB.class_exists("MatchEngineBridge"):
		if not bridge:
			bridge = ClassDB.instantiate("MatchEngineBridge")
			add_child(bridge)
		run_new_match(current_seed)
		set_view_mode(CamRigScript.CameraMode.BROADCAST)
		return true

	printerr("[GDExtension Warning] MatchEngineBridge not available in ClassDB.")
	set_view_mode(CamRigScript.CameraMode.BROADCAST)
	return false

func _process(delta: float) -> void:
	if is_playing and not is_scrubbing and total_frames > 0:
		current_frame_float += delta * 20.0 * playback_speed
		if current_frame_float >= float(total_frames - 1):
			current_frame_float = float(total_frames - 1)
			is_playing = false
			emit_signal("playback_state_changed", is_playing, playback_speed)
		_update_views_for_frame(current_frame_float, false)

func run_new_match(seed: int) -> void:
	if not bridge:
		return
	current_seed = seed
	bridge.initialize_match(seed)
	bridge.simulate_full_match()
	total_frames = bridge.get_total_ticks()
	current_frame_float = 0.0
	if pitch_3d:
		pitch_3d.reset_motion_state()

	var meta: Dictionary = bridge.get_match_metadata()
	print("[GDExtension] Simulated 90m match (seed=%d, ticks=%d, checkpoints=%d, final=%d-%d, time=%.1f ms)" % [
		seed, total_frames, int(meta.get("checkpoint_count", 0)),
		int(meta.get("score_home", 0)), int(meta.get("score_away", 0)),
		float(meta.get("last_sim_ms", 0.0))
	])
	if match_hud:
		match_hud.setup_events(bridge.get_match_events())
	emit_signal("match_loaded", seed, total_frames, meta)
	_update_views_for_frame(0.0, true)

func get_current_tick() -> int:
	return clampi(int(floor(current_frame_float)), 0, max(0, total_frames - 1))

func get_tick_hud_state(tick: int) -> Dictionary:
	if not bridge or total_frames <= 0:
		return {}
	return bridge.get_tick_hud_state(clampi(tick, 0, total_frames - 1))

func get_status_summary() -> Dictionary:
	if not bridge:
		return {}
	return {
		"version": bridge.get_engine_version(),
		"checkpoints": bridge.get_checkpoint_count(),
		"last_sim_ms": bridge.get_last_sim_time_ms(),
	}

func toggle_play_pause() -> void:
	is_playing = not is_playing
	emit_signal("playback_state_changed", is_playing, playback_speed)

func set_playing(playing: bool) -> void:
	is_playing = playing
	emit_signal("playback_state_changed", is_playing, playback_speed)

func step_back() -> void:
	is_playing = false
	current_frame_float = maxf(0.0, floor(current_frame_float) - 1.0)
	emit_signal("playback_state_changed", is_playing, playback_speed)
	_update_views_for_frame(current_frame_float, false)

func step_forward() -> void:
	is_playing = false
	current_frame_float = minf(float(max(0, total_frames - 1)), floor(current_frame_float) + 1.0)
	emit_signal("playback_state_changed", is_playing, playback_speed)
	_update_views_for_frame(current_frame_float, false)

func restart() -> void:
	current_frame_float = 0.0
	is_playing = true
	if pitch_3d:
		pitch_3d.reset_motion_state()
	emit_signal("playback_state_changed", is_playing, playback_speed)
	_update_views_for_frame(0.0, true)

func start_next_seed() -> void:
	run_new_match(current_seed + 1)
	is_playing = true
	emit_signal("playback_state_changed", is_playing, playback_speed)

func set_speed_index(idx: int) -> void:
	var clamped_idx := clampi(idx, 0, SPEED_MULTIPLIERS.size() - 1)
	playback_speed = SPEED_MULTIPLIERS[clamped_idx]
	emit_signal("playback_state_changed", is_playing, playback_speed)

func set_scrubbing(scrubbing: bool) -> void:
	is_scrubbing = scrubbing

func scrub_to_frame(value: float) -> void:
	current_frame_float = clampf(value, 0.0, float(max(0, total_frames - 1)))
	_update_views_for_frame(current_frame_float, true)

func jump_to_tick(target_tick: int) -> void:
	if total_frames <= 0:
		return
	var clamped := clampi(target_tick, 0, total_frames - 1)
	current_frame_float = float(clamped)
	_update_views_for_frame(current_frame_float, true)

func apply_rollback(team_id: int, formation_id: int, def_line: float, pressing: float) -> Dictionary:
	if not bridge or total_frames <= 0:
		return {}
	var target_tick := get_current_tick()
	bridge.apply_tactical_change(team_id, formation_id, def_line, pressing)
	bridge.rollback_and_resimulate(target_tick)

	total_frames = bridge.get_total_ticks()
	if pitch_3d:
		pitch_3d.reset_motion_state()
	if match_hud:
		match_hud.setup_events(bridge.get_match_events())

	current_frame_float = float(target_tick)
	is_playing = true
	emit_signal("playback_state_changed", is_playing, playback_speed)
	_update_views_for_frame(current_frame_float, true)

	var sim_ms := float(bridge.get_last_sim_time_ms())
	emit_signal("rollback_completed", target_tick, sim_ms)
	return {
		"target_tick": target_tick,
		"total_frames": total_frames,
		"sim_ms": sim_ms,
	}

func set_view_mode(mode: int) -> void:
	current_view_mode = clampi(mode, CamRigScript.CameraMode.BROADCAST, CamRigScript.CameraMode.CLASSIC_2D)
	var is_2d := (current_view_mode == CamRigScript.CameraMode.CLASSIC_2D)
	if view_container_2d:
		view_container_2d.visible = is_2d
	if view_container_3d:
		view_container_3d.visible = not is_2d
	if camera_rig_3d:
		camera_rig_3d.set_mode(current_view_mode, true)
	_update_views_for_frame(current_frame_float, true)
	emit_signal("view_mode_changed", current_view_mode)

func cycle_view_mode() -> void:
	var next_mode := (current_view_mode + 1) % 5
	set_view_mode(next_mode)

func _update_views_for_frame(frame_val: float, force_discontinuous: bool) -> void:
	if total_frames <= 0 or not bridge:
		return

	var clamped_frame := clampf(frame_val, 0.0, float(total_frames - 1))
	var t_floor := int(floor(clamped_frame))
	var t_ceil := mini(t_floor + 1, total_frames - 1)
	var frac := clamped_frame - float(t_floor)

	var data_a: PackedFloat32Array = bridge.get_frame_data(t_floor)
	if data_a.size() < 93:
		return

	var ball_result: Vector3
	var ball_owner := int(data_a[4])
	var ball_teleported := false

	if t_floor == t_ceil or frac < 0.001:
		ball_result = Vector3(data_a[1], data_a[2], data_a[3])
		for i in range(22):
			var base := 5 + i * 4
			pos_arr[i] = Vector2(data_a[base + 0], data_a[base + 1])
			facing_arr[i] = data_a[base + 2]
			stamina_arr[i] = data_a[base + 3]
	else:
		var data_b: PackedFloat32Array = bridge.get_frame_data(t_ceil)
		if data_b.size() < 93:
			return
		var ball_a := Vector3(data_a[1], data_a[2], data_a[3])
		var ball_b := Vector3(data_b[1], data_b[2], data_b[3])

		# Teleport Guard: snap ball to frame_b if displacement > 6.0m in a single tick
		if ball_a.distance_to(ball_b) > 6.0:
			ball_result = ball_b
			ball_owner = int(data_b[4])
			ball_teleported = true
		else:
			ball_result = ball_a.lerp(ball_b, frac)

		for i in range(22):
			var base := 5 + i * 4
			var p_a := Vector2(data_a[base + 0], data_a[base + 1])
			var p_b := Vector2(data_b[base + 0], data_b[base + 1])
			if p_a.distance_to(p_b) > 6.0:
				pos_arr[i] = p_b
				facing_arr[i] = data_b[base + 2]
				stamina_arr[i] = data_b[base + 3]
			else:
				pos_arr[i] = p_a.lerp(p_b, frac)
				facing_arr[i] = lerp_angle(data_a[base + 2], data_b[base + 2], frac)
				stamina_arr[i] = lerpf(data_a[base + 3], data_b[base + 3], frac)

	if pitch_2d:
		pitch_2d.update_frame_data(t_floor, ball_result, ball_owner, pos_arr, facing_arr, stamina_arr)

	if pitch_3d:
		pitch_3d.update_frame_data(
			t_floor,
			ball_result,
			ball_owner,
			pos_arr,
			facing_arr,
			stamina_arr,
			force_discontinuous or ball_teleported
		)

	var hud_state: Dictionary = bridge.get_tick_hud_state(t_floor)
	if match_hud:
		match_hud.update_hud(t_floor, hud_state)

	emit_signal("frame_updated", t_floor, clamped_frame, hud_state)
