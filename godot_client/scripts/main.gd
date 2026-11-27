extends Node

const PlaybackScript = preload("res://scripts/match_playback_controller.gd")
const Pitch2DScript = preload("res://scripts/pitch_renderer.gd")
const Pitch3DScript = preload("res://scripts/pitch_renderer_3d.gd")
const MatchHUDScript = preload("res://scripts/match_hud.gd")
const CamRigScript = preload("res://scripts/camera_rig_3d.gd")

@onready var playback: PlaybackScript = $MatchPlaybackController
@onready var view_container_2d: Node2D = $ViewContainer2D
@onready var pitch_view: Pitch2DScript = $ViewContainer2D/PitchView
@onready var view_container_3d: Node3D = $ViewContainer3D
@onready var pitch_3d: Pitch3DScript = $ViewContainer3D/Pitch3D
@onready var match_hud: MatchHUDScript = $UI/MatchHUD

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

@onready var cam_broadcast_btn: Button = $UI/TopBar/ViewButtons/CamBroadcastBtn
@onready var cam_tactical_btn: Button = $UI/TopBar/ViewButtons/CamTacticalBtn
@onready var cam_goal_btn: Button = $UI/TopBar/ViewButtons/CamGoalBtn
@onready var cam_orbit_btn: Button = $UI/TopBar/ViewButtons/CamOrbitBtn
@onready var cam_2d_btn: Button = $UI/TopBar/ViewButtons/Cam2DBtn
@onready var orbit_hint_label: Label = $UI/OrbitHintLabel

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

var view_buttons: Array[Button] = []

func _ready() -> void:
	print("==================================================")
	print("Football Manager 3D & 2D Match Viewer (Stage 4.5A)")
	print("==================================================")

	view_buttons = [
		cam_broadcast_btn,
		cam_tactical_btn,
		cam_goal_btn,
		cam_orbit_btn,
		cam_2d_btn,
	]

	_setup_tactical_controls()
	_connect_playback_signals()
	_connect_ui_controls()

	playback.setup(view_container_2d, pitch_view, view_container_3d, pitch_3d, match_hud)
	var ok := playback.initialize_engine(12345)
	if not ok and status_label:
		status_label.text = "Warning: Bridge missing"

func _process(_delta: float) -> void:
	_update_ui_text()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.is_pressed() and not event.is_echo():
		match event.keycode:
			KEY_SPACE:
				playback.toggle_play_pause()
			KEY_LEFT:
				_on_step_back_pressed()
			KEY_RIGHT:
				_on_step_forward_pressed()
			KEY_R:
				playback.restart()
			KEY_1:
				_set_speed_index(0)
			KEY_2:
				_set_speed_index(1)
			KEY_3:
				_set_speed_index(2)
			KEY_4:
				_set_speed_index(3)
			KEY_5:
				_set_speed_index(4)
			KEY_F1:
				playback.set_view_mode(CamRigScript.CameraMode.BROADCAST)
			KEY_F2:
				playback.set_view_mode(CamRigScript.CameraMode.TACTICAL_3D)
			KEY_F3:
				playback.set_view_mode(CamRigScript.CameraMode.BEHIND_GOAL)
			KEY_F4:
				playback.set_view_mode(CamRigScript.CameraMode.FREE_ORBIT)
			KEY_F5:
				playback.set_view_mode(CamRigScript.CameraMode.CLASSIC_2D)
			KEY_V:
				playback.cycle_view_mode()

func _connect_playback_signals() -> void:
	playback.match_loaded.connect(_on_match_loaded)
	playback.frame_updated.connect(_on_frame_updated)
	playback.playback_state_changed.connect(_on_playback_state_changed)
	playback.rollback_completed.connect(_on_rollback_completed)
	playback.view_mode_changed.connect(_on_view_mode_changed)

func _connect_ui_controls() -> void:
	if scrub_slider:
		scrub_slider.value_changed.connect(func(v: float): playback.scrub_to_frame(v))
		scrub_slider.drag_started.connect(func(): playback.set_scrubbing(true))
		scrub_slider.drag_ended.connect(func(_val): playback.set_scrubbing(false))

	if play_pause_btn:
		play_pause_btn.pressed.connect(func(): playback.toggle_play_pause())
		_update_play_pause_text(playback.is_playing)

	if step_back_btn:
		step_back_btn.pressed.connect(_on_step_back_pressed)
	if step_forward_btn:
		step_forward_btn.pressed.connect(_on_step_forward_pressed)
	if restart_btn:
		restart_btn.pressed.connect(func(): playback.restart())
	if new_seed_btn:
		new_seed_btn.pressed.connect(func(): playback.start_next_seed())

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
		speed_options.item_selected.connect(func(idx: int): playback.set_speed_index(idx))

	for mode_idx in range(view_buttons.size()):
		var btn := view_buttons[mode_idx]
		if btn:
			btn.pressed.connect(func(): playback.set_view_mode(mode_idx))

func _on_match_loaded(_seed: int, total_ticks: int, meta: Dictionary) -> void:
	if scrub_slider:
		scrub_slider.min_value = 0
		scrub_slider.max_value = max(0, total_ticks - 1)
		scrub_slider.set_value_no_signal(0)
	_sync_tactical_controls_to_tick(0)
	_update_status_banner()
	if rollback_status_label:
		rollback_status_label.text = "Pre-simulated %d ticks (%d checkpoints)" % [
			total_ticks, int(meta.get("checkpoint_count", 0))
		]

func _on_frame_updated(_tick: int, frame_float: float, _hud_state: Dictionary) -> void:
	if scrub_slider and not playback.is_scrubbing:
		scrub_slider.set_value_no_signal(floor(frame_float))

func _on_playback_state_changed(playing: bool, _speed: float) -> void:
	_update_play_pause_text(playing)

func _on_rollback_completed(target_tick: int, sim_ms: float) -> void:
	if scrub_slider:
		scrub_slider.max_value = max(0, playback.total_frames - 1)
		scrub_slider.set_value_no_signal(float(target_tick))
	_update_status_banner()
	if rollback_status_label:
		rollback_status_label.text = "Rolled back @ %s (tick %d) in %.1f ms" % [
			_format_clock(target_tick), target_tick, sim_ms
		]

func _on_view_mode_changed(mode: int) -> void:
	for i in range(view_buttons.size()):
		var btn := view_buttons[i]
		if btn:
			btn.set_pressed_no_signal(i == mode)
	if orbit_hint_label:
		orbit_hint_label.visible = (mode == CamRigScript.CameraMode.FREE_ORBIT)
	_update_status_banner()

func _on_step_back_pressed() -> void:
	playback.step_back()
	if scrub_slider:
		scrub_slider.set_value_no_signal(playback.current_frame_float)

func _on_step_forward_pressed() -> void:
	playback.step_forward()
	if scrub_slider:
		scrub_slider.set_value_no_signal(playback.current_frame_float)

func _set_speed_index(idx: int) -> void:
	if speed_options:
		speed_options.selected = idx
	playback.set_speed_index(idx)

func jump_to_tick(target_tick: int) -> void:
	playback.jump_to_tick(target_tick)
	var tick := playback.get_current_tick()
	if scrub_slider:
		scrub_slider.set_value_no_signal(float(tick))
	_sync_tactical_controls_to_tick(tick)

func _update_ui_text() -> void:
	var tick_int := playback.get_current_tick()
	if tick_label:
		tick_label.text = "Tick: %d / %d" % [tick_int, playback.total_frames]
	if fps_label:
		fps_label.text = "%d FPS" % Engine.get_frames_per_second()
	if apply_rollback_btn:
		apply_rollback_btn.text = "Apply & Resim @ %s" % _format_clock(tick_int)

func _update_play_pause_text(playing: bool) -> void:
	if play_pause_btn:
		play_pause_btn.text = "⏸ Pause" if playing else "▶ Play"

func _format_clock(tick: int) -> String:
	var total_seconds: int = int(float(tick) * 0.05)
	return "%02d:%02d" % [total_seconds / 60, total_seconds % 60]

func _setup_tactical_controls() -> void:
	if team_option:
		team_option.clear()
		team_option.add_item("Home (Blue)", 0)
		team_option.add_item("Away (Red)", 1)
		team_option.item_selected.connect(func(_idx): _sync_tactical_controls_to_tick(playback.get_current_tick()))

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
	if not team_option:
		return
	var hud_state: Dictionary = playback.get_tick_hud_state(tick)
	if hud_state.is_empty():
		return
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
	if not status_label:
		return
	var summary: Dictionary = playback.get_status_summary()
	if summary.is_empty():
		return
	var mode_str := ""
	if playback.camera_rig_3d:
		mode_str = " | " + playback.camera_rig_3d.get_mode_name(playback.current_view_mode)
	status_label.text = "%s | %d CPs | %.1f ms%s" % [
		str(summary.get("version", "")),
		int(summary.get("checkpoints", 0)),
		float(summary.get("last_sim_ms", 0.0)),
		mode_str
	]

func _on_apply_rollback_pressed() -> void:
	var team_id := team_option.selected if team_option else 0
	var formation_id := formation_option.selected if formation_option else 0
	var def_line := float(line_slider.value) if line_slider else 0.5
	var pressing := float(press_slider.value) if press_slider else 0.5
	playback.apply_rollback(team_id, formation_id, def_line, pressing)
