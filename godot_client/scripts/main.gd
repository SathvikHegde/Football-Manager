extends Node

@onready var pitch_view: PitchRenderer = $PitchView
@onready var match_hud: MatchHUD = $UI/MatchHUD
@onready var play_pause_btn: Button = $UI/BottomBar/PlayPauseButton
@onready var step_back_btn: Button = $UI/BottomBar/StepBackButton
@onready var step_forward_btn: Button = $UI/BottomBar/StepForwardButton
@onready var scrub_slider: HSlider = $UI/BottomBar/ScrubSlider
@onready var speed_options: OptionButton = $UI/BottomBar/SpeedOptionButton
@onready var restart_btn: Button = $UI/BottomBar/RestartButton
@onready var new_seed_btn: Button = $UI/BottomBar/NewSeedButton
@onready var jump_0_btn: Button = $UI/BottomBar/Jump0Button
@onready var jump_ht_btn: Button = $UI/BottomBar/JumpHTButton
@onready var jump_60_btn: Button = $UI/BottomBar/Jump60Button
@onready var jump_75_btn: Button = $UI/BottomBar/Jump75Button

@onready var team_option: OptionButton = $UI/TacticalPanel/Margin/VBox/TeamRow/TeamOption
@onready var formation_option: OptionButton = $UI/TacticalPanel/Margin/VBox/FormationRow/FormationOption
@onready var line_slider: HSlider = $UI/TacticalPanel/Margin/VBox/LineRow/LineSlider
@onready var line_value: Label = $UI/TacticalPanel/Margin/VBox/LineRow/LineValue
@onready var press_slider: HSlider = $UI/TacticalPanel/Margin/VBox/PressRow/PressSlider
@onready var press_value: Label = $UI/TacticalPanel/Margin/VBox/PressRow/PressValue
@onready var apply_rollback_btn: Button = $UI/TacticalPanel/Margin/VBox/ApplyRollbackButton
@onready var rollback_status_label: Label = $UI/TacticalPanel/Margin/VBox/RollbackStatusLabel

@onready var tick_label: Label = $UI/TopBar/TickLabel
@onready var fps_label: Label = $UI/TopBar/FpsLabel
@onready var status_label: Label = $UI/TopBar/StatusLabel

var bridge: Node = null
var current_seed: int = 12345
var total_frames: int = 108000
var current_frame_float: float = 0.0
var is_playing: bool = true
var playback_speed: float = 1.0
var is_scrubbing: bool = false

# Pre-allocated arrays for per-frame interpolation
var pos_arr: Array[Vector2] = []
var facing_arr: Array[float] = []
var stamina_arr: Array[float] = []

func _ready() -> void:
	pos_arr.resize(22)
	facing_arr.resize(22)
	stamina_arr.resize(22)
	for i in range(22):
		pos_arr[i] = Vector2.ZERO
		facing_arr[i] = 0.0
		stamina_arr[i] = 1.0

	print("==================================================")
	print("Football Manager 2D Match Viewer (Stage 3)")
	print("==================================================")

	_setup_tactical_controls()

	if ClassDB.class_exists("MatchEngineBridge"):
		bridge = ClassDB.instantiate("MatchEngineBridge")
		add_child(bridge)
		_run_new_match(current_seed)
		_update_status_banner()
	else:
		printerr("[GDExtension Warning] MatchEngineBridge not available in ClassDB.")
		if status_label:
			status_label.text = "Warning: Bridge missing"

	if scrub_slider:
		scrub_slider.value_changed.connect(_on_scrub_slider_value_changed)
		scrub_slider.drag_started.connect(func(): is_scrubbing = true)
		scrub_slider.drag_ended.connect(func(_val): is_scrubbing = false)

	if play_pause_btn:
		play_pause_btn.pressed.connect(_on_play_pause_pressed)
		_update_play_pause_text()

	if step_back_btn:
		step_back_btn.pressed.connect(_on_step_back_pressed)

	if step_forward_btn:
		step_forward_btn.pressed.connect(_on_step_forward_pressed)

	if restart_btn:
		restart_btn.pressed.connect(_on_restart_pressed)

	if new_seed_btn:
		new_seed_btn.pressed.connect(_on_new_seed_pressed)

	if jump_0_btn:
		jump_0_btn.pressed.connect(func(): jump_to_tick(0))
	if jump_ht_btn:
		jump_ht_btn.pressed.connect(func(): jump_to_tick(54000))
	if jump_60_btn:
		jump_60_btn.pressed.connect(func(): jump_to_tick(72000))
	if jump_75_btn:
		jump_75_btn.pressed.connect(func(): jump_to_tick(90000))

	if speed_options:
		speed_options.clear()
		speed_options.add_item("1x Speed", 0)
		speed_options.add_item("2x Speed", 1)
		speed_options.add_item("4x Speed", 2)
		speed_options.add_item("8x Speed", 3)
		speed_options.add_item("16x Speed", 4)
		speed_options.item_selected.connect(_on_speed_selected)

	_update_view_for_frame(0.0)

func _run_new_match(seed: int) -> void:
	if not bridge:
		return
	current_seed = seed
	bridge.initialize_match(seed)
	bridge.simulate_full_match()
	total_frames = bridge.get_total_ticks()
	current_frame_float = 0.0
	var meta: Dictionary = bridge.get_match_metadata()
	print("[GDExtension] Simulated 90m match (seed=%d, ticks=%d, checkpoints=%d, final=%d-%d, time=%.1f ms)" % [
		seed, total_frames, int(meta.get("checkpoint_count", 0)),
		int(meta.get("score_home", 0)), int(meta.get("score_away", 0)),
		float(meta.get("last_sim_ms", 0.0))
	])
	if match_hud:
		match_hud.setup_events(bridge.get_match_events())
	if scrub_slider:
		scrub_slider.min_value = 0
		scrub_slider.max_value = max(0, total_frames - 1)
		scrub_slider.set_value_no_signal(0)
	_sync_tactical_controls_to_tick(0)
	_update_status_banner()
	if rollback_status_label:
		rollback_status_label.text = "Pre-simulated %d ticks (%d checkpoints)" % [total_frames, bridge.get_checkpoint_count()]

func _process(delta: float) -> void:
	if is_playing and not is_scrubbing and total_frames > 0:
		current_frame_float += delta * 20.0 * playback_speed
		if current_frame_float >= float(total_frames - 1):
			current_frame_float = float(total_frames - 1)
			is_playing = false
			_update_play_pause_text()
		_update_view_for_frame(current_frame_float)
		if scrub_slider and not is_scrubbing:
			scrub_slider.set_value_no_signal(floor(current_frame_float))

	_update_ui_text()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.is_pressed() and not event.is_echo():
		if event.keycode == KEY_SPACE:
			_on_play_pause_pressed()
		elif event.keycode == KEY_LEFT:
			_on_step_back_pressed()
		elif event.keycode == KEY_RIGHT:
			_on_step_forward_pressed()
		elif event.keycode == KEY_R:
			_on_restart_pressed()
		elif event.keycode == KEY_1:
			_set_speed_index(0)
		elif event.keycode == KEY_2:
			_set_speed_index(1)
		elif event.keycode == KEY_3:
			_set_speed_index(2)
		elif event.keycode == KEY_4:
			_set_speed_index(3)
		elif event.keycode == KEY_5:
			_set_speed_index(4)

func _update_view_for_frame(frame_val: float) -> void:
	if total_frames <= 0 or not bridge:
		return

	var clamped_frame = clamp(frame_val, 0.0, float(total_frames - 1))
	var t_floor = int(floor(clamped_frame))
	var t_ceil = min(t_floor + 1, total_frames - 1)
	var frac = clamped_frame - float(t_floor)

	var data_a: PackedFloat32Array = bridge.get_frame_data(t_floor)
	if data_a.size() < 93:
		return

	var ball_result: Vector3
	var ball_owner = int(data_a[4])

	if t_floor == t_ceil or frac < 0.001:
		ball_result = Vector3(data_a[1], data_a[2], data_a[3])
		for i in range(22):
			var base = 5 + i * 4
			pos_arr[i] = Vector2(data_a[base + 0], data_a[base + 1])
			facing_arr[i] = data_a[base + 2]
			stamina_arr[i] = data_a[base + 3]
	else:
		var data_b: PackedFloat32Array = bridge.get_frame_data(t_ceil)
		var ball_a = Vector3(data_a[1], data_a[2], data_a[3])
		var ball_b = Vector3(data_b[1], data_b[2], data_b[3])
		ball_result = ball_a.lerp(ball_b, frac)

		for i in range(22):
			var base = 5 + i * 4
			var p_a = Vector2(data_a[base + 0], data_a[base + 1])
			var p_b = Vector2(data_b[base + 0], data_b[base + 1])
			pos_arr[i] = p_a.lerp(p_b, frac)
			facing_arr[i] = lerp_angle(data_a[base + 2], data_b[base + 2], frac)
			stamina_arr[i] = lerp(data_a[base + 3], data_b[base + 3], frac)

	if pitch_view:
		pitch_view.update_frame_data(t_floor, ball_result, ball_owner, pos_arr, facing_arr, stamina_arr)

	if match_hud:
		var hud_state: Dictionary = bridge.get_tick_hud_state(t_floor)
		match_hud.update_hud(t_floor, hud_state)

func _update_ui_text() -> void:
	var tick_int := clampi(int(floor(current_frame_float)), 0, max(0, total_frames - 1))
	if tick_label:
		tick_label.text = "Tick: %d / %d" % [tick_int, total_frames]
	if fps_label:
		fps_label.text = "%d FPS" % Engine.get_frames_per_second()
	if apply_rollback_btn:
		apply_rollback_btn.text = "Apply & Resim @ %s" % _format_clock(tick_int)

func _update_play_pause_text() -> void:
	if play_pause_btn:
		play_pause_btn.text = "⏸ Pause" if is_playing else "▶ Play"

func _on_play_pause_pressed() -> void:
	is_playing = not is_playing
	_update_play_pause_text()

func _on_step_back_pressed() -> void:
	is_playing = false
	current_frame_float = max(0.0, floor(current_frame_float) - 1.0)
	_update_play_pause_text()
	if scrub_slider:
		scrub_slider.set_value_no_signal(current_frame_float)
	_update_view_for_frame(current_frame_float)

func _on_step_forward_pressed() -> void:
	is_playing = false
	current_frame_float = min(float(total_frames - 1), floor(current_frame_float) + 1.0)
	_update_play_pause_text()
	if scrub_slider:
		scrub_slider.set_value_no_signal(current_frame_float)
	_update_view_for_frame(current_frame_float)

func _on_restart_pressed() -> void:
	current_frame_float = 0.0
	is_playing = true
	_update_play_pause_text()
	if scrub_slider:
		scrub_slider.set_value_no_signal(0)
	_update_view_for_frame(0.0)

func _on_new_seed_pressed() -> void:
	_run_new_match(current_seed + 1)
	is_playing = true
	_update_play_pause_text()
	_update_view_for_frame(0.0)

func _on_scrub_slider_value_changed(value: float) -> void:
	current_frame_float = value
	_update_view_for_frame(current_frame_float)

func _on_speed_selected(index: int) -> void:
	match index:
		0: playback_speed = 1.0
		1: playback_speed = 2.0
		2: playback_speed = 4.0
		3: playback_speed = 8.0
		4: playback_speed = 16.0

func _set_speed_index(idx: int) -> void:
	if speed_options:
		speed_options.selected = idx
	_on_speed_selected(idx)

func _format_clock(tick: int) -> String:
	var total_seconds: int = int(float(tick) * 0.05)
	return "%02d:%02d" % [total_seconds / 60, total_seconds % 60]

func jump_to_tick(target_tick: int) -> void:
	if total_frames <= 0:
		return
	var clamped := clampi(target_tick, 0, total_frames - 1)
	current_frame_float = float(clamped)
	if scrub_slider:
		scrub_slider.set_value_no_signal(current_frame_float)
	_update_view_for_frame(current_frame_float)
	_sync_tactical_controls_to_tick(clamped)

func _setup_tactical_controls() -> void:
	if team_option:
		team_option.clear()
		team_option.add_item("Home (Blue)", 0)
		team_option.add_item("Away (Red)", 1)
		team_option.item_selected.connect(func(_idx): _sync_tactical_controls_to_tick(int(floor(current_frame_float))))

	if formation_option:
		formation_option.clear()
		formation_option.add_item("4-4-2", 0)
		formation_option.add_item("4-3-3", 1)
		formation_option.add_item("4-2-3-1", 2)
		formation_option.add_item("3-5-2", 3)
		formation_option.add_item("5-3-2", 4)

	if line_slider:
		line_slider.value_changed.connect(func(v):
			if line_value:
				line_value.text = "%.2f" % v
		)

	if press_slider:
		press_slider.value_changed.connect(func(v):
			if press_value:
				press_value.text = "%.2f" % v
		)

	if apply_rollback_btn:
		apply_rollback_btn.pressed.connect(_on_apply_rollback_pressed)

func _sync_tactical_controls_to_tick(tick: int) -> void:
	if not bridge or not team_option:
		return
	var hud_state: Dictionary = bridge.get_tick_hud_state(clampi(tick, 0, max(0, total_frames - 1)))
	var team_id: int = team_option.selected
	var form_key := "home_formation" if team_id == 0 else "away_formation"
	var line_key := "home_def_line" if team_id == 0 else "away_def_line"
	var press_key := "home_pressing" if team_id == 0 else "away_pressing"

	if formation_option:
		formation_option.selected = clampi(int(hud_state.get(form_key, 0)), 0, 4)
	if line_slider:
		var line_v := float(hud_state.get(line_key, 0.5))
		line_slider.set_value_no_signal(line_v)
		if line_value:
			line_value.text = "%.2f" % line_v
	if press_slider:
		var press_v := float(hud_state.get(press_key, 0.5))
		press_slider.set_value_no_signal(press_v)
		if press_value:
			press_value.text = "%.2f" % press_v

func _update_status_banner() -> void:
	if not bridge or not status_label:
		return
	status_label.text = "%s | %d Checkpoints | %.1f ms" % [
		bridge.get_engine_version(),
		bridge.get_checkpoint_count(),
		bridge.get_last_sim_time_ms()
	]

func _on_apply_rollback_pressed() -> void:
	if not bridge or total_frames <= 0:
		return
	var target_tick := clampi(int(floor(current_frame_float)), 0, total_frames - 1)
	var team_id := team_option.selected if team_option else 0
	var formation_id := formation_option.selected if formation_option else 0
	var def_line := float(line_slider.value) if line_slider else 0.5
	var pressing := float(press_slider.value) if press_slider else 0.5

	bridge.apply_tactical_change(team_id, formation_id, def_line, pressing)
	bridge.rollback_and_resimulate(target_tick)

	total_frames = bridge.get_total_ticks()
	if scrub_slider:
		scrub_slider.max_value = max(0, total_frames - 1)
		scrub_slider.set_value_no_signal(float(target_tick))
	if match_hud:
		match_hud.setup_events(bridge.get_match_events())

	current_frame_float = float(target_tick)
	is_playing = true
	_update_play_pause_text()
	_update_view_for_frame(current_frame_float)
	_update_status_banner()

	if rollback_status_label:
		rollback_status_label.text = "Rolled back @ %s (tick %d) in %.1f ms" % [
			_format_clock(target_tick), target_tick, bridge.get_last_sim_time_ms()
		]
