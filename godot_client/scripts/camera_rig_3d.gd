extends Node3D
class_name CameraRig3D

enum CameraMode {
	BROADCAST = 0,
	TACTICAL_3D = 1,
	BEHIND_GOAL = 2,
	FREE_ORBIT = 3,
	CLASSIC_2D = 4,
}

@onready var camera: Camera3D = $Camera3D

var current_mode: CameraMode = CameraMode.BROADCAST
var target_ball_pos: Vector3 = Vector3.ZERO
var target_action_centroid: Vector3 = Vector3.ZERO
var behind_goal_side: int = 1

var current_cam_pos: Vector3 = Vector3(0.0, 22.0, 38.0)
var current_look_target: Vector3 = Vector3.ZERO
var current_fov: float = 38.0

# Free Orbit state
var orbit_target: Vector3 = Vector3.ZERO
var orbit_yaw: float = 0.0
var orbit_pitch: float = 0.56
var orbit_distance: float = 42.0
var is_orbit_dragging: bool = false

func _ready() -> void:
	set_mode(CameraMode.BROADCAST, true)

func is_3d_mode() -> bool:
	return current_mode != CameraMode.CLASSIC_2D

func get_mode_name(mode: CameraMode = current_mode) -> String:
	match mode:
		CameraMode.BROADCAST: return "Broadcast (TV)"
		CameraMode.TACTICAL_3D: return "Tactical 3D"
		CameraMode.BEHIND_GOAL: return "Behind Goal"
		CameraMode.FREE_ORBIT: return "Free Orbit"
		CameraMode.CLASSIC_2D: return "2D Classic"
	return "Broadcast (TV)"

func set_tracking_targets(ball_pos_3d: Vector3, action_centroid_3d: Vector3) -> void:
	target_ball_pos = ball_pos_3d
	target_action_centroid = action_centroid_3d

	if target_ball_pos.x > 12.0:
		behind_goal_side = 1
	elif target_ball_pos.x < -12.0:
		behind_goal_side = -1

func set_mode(mode: CameraMode, should_snap: bool = true) -> void:
	var prev_mode := current_mode
	current_mode = mode
	is_orbit_dragging = false

	if not camera:
		return

	if mode == CameraMode.CLASSIC_2D:
		camera.current = false
		return

	camera.current = true

	if mode == CameraMode.FREE_ORBIT and prev_mode != CameraMode.FREE_ORBIT:
		if prev_mode == CameraMode.CLASSIC_2D:
			orbit_target = Vector3(target_ball_pos.x, 0.0, target_ball_pos.z)
			orbit_yaw = 0.0
			orbit_pitch = 0.56
			orbit_distance = 42.0
		else:
			orbit_target = Vector3(
				clampf(current_look_target.x, -65.0, 65.0),
				0.0,
				clampf(current_look_target.z, -45.0, 45.0)
			)
			var rel: Vector3 = current_cam_pos - orbit_target
			orbit_distance = clampf(rel.length(), 8.0, 120.0)
			if orbit_distance > 0.05:
				orbit_pitch = clampf(asin(clampf(rel.y / orbit_distance, -0.99, 0.99)), 0.18, 1.48)
				orbit_yaw = atan2(rel.x, rel.z)
		snap_to_target()
	elif should_snap:
		snap_to_target()

func snap_to_target() -> void:
	if not camera or current_mode == CameraMode.CLASSIC_2D:
		return
	var desired := _compute_desired_camera()
	current_cam_pos = desired["pos"]
	current_look_target = desired["look"]
	current_fov = desired["fov"]
	_apply_camera_transform()

func _process(delta: float) -> void:
	if not camera or current_mode == CameraMode.CLASSIC_2D:
		return

	if current_mode == CameraMode.FREE_ORBIT:
		_process_orbit_pan(delta)
		var desired_orbit := _compute_desired_camera()
		var orbit_alpha := 1.0 - exp(-16.0 * delta)
		current_cam_pos = current_cam_pos.lerp(desired_orbit["pos"], orbit_alpha)
		current_look_target = current_look_target.lerp(desired_orbit["look"], orbit_alpha)
		current_fov = lerpf(current_fov, desired_orbit["fov"], orbit_alpha)
		_apply_camera_transform()
		return

	var desired := _compute_desired_camera()
	var pos_alpha := 1.0 - exp(-4.5 * delta)
	var look_alpha := 1.0 - exp(-6.0 * delta)
	var fov_alpha := 1.0 - exp(-4.0 * delta)

	current_cam_pos = current_cam_pos.lerp(desired["pos"], pos_alpha)
	current_look_target = current_look_target.lerp(desired["look"], look_alpha)
	current_fov = lerpf(current_fov, desired["fov"], fov_alpha)
	_apply_camera_transform()

func _unhandled_input(event: InputEvent) -> void:
	if not is_visible_in_tree() or current_mode != CameraMode.FREE_ORBIT:
		is_orbit_dragging = false
		return

	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT:
			is_orbit_dragging = event.pressed
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
			orbit_distance = clampf(orbit_distance * 0.9, 8.0, 120.0)
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			orbit_distance = clampf(orbit_distance * 1.1, 8.0, 120.0)
	elif event is InputEventMouseMotion and is_orbit_dragging:
		orbit_yaw -= event.relative.x * 0.006
		orbit_pitch = clampf(orbit_pitch + event.relative.y * 0.006, 0.18, 1.48)

func _process_orbit_pan(delta: float) -> void:
	var move_input := Vector2.ZERO
	if Input.is_key_pressed(KEY_W):
		move_input.y += 1.0
	if Input.is_key_pressed(KEY_S):
		move_input.y -= 1.0
	if Input.is_key_pressed(KEY_D):
		move_input.x += 1.0
	if Input.is_key_pressed(KEY_A):
		move_input.x -= 1.0

	if move_input.length_squared() < 0.001:
		return

	move_input = move_input.normalized()
	var forward := Vector3(-sin(orbit_yaw), 0.0, -cos(orbit_yaw))
	var right := Vector3(cos(orbit_yaw), 0.0, -sin(orbit_yaw))
	var pan_speed := clampf(orbit_distance * 0.75, 16.0, 52.0)
	orbit_target += (forward * move_input.y + right * move_input.x) * pan_speed * delta
	orbit_target.x = clampf(orbit_target.x, -75.0, 75.0)
	orbit_target.z = clampf(orbit_target.z, -55.0, 55.0)
	orbit_target.y = 0.0

func _compute_desired_camera() -> Dictionary:
	match current_mode:
		CameraMode.BROADCAST:
			var focus: Vector3 = target_ball_pos * 0.75 + target_action_centroid * 0.25
			var pos := Vector3(clampf(focus.x * 0.85, -32.0, 32.0), 22.0, 38.0)
			var look := Vector3(clampf(focus.x, -46.0, 46.0), 0.5, clampf(focus.z * 0.8, -24.0, 24.0))
			var box_t: float = clampf((absf(target_ball_pos.x) - 18.0) / 18.0, 0.0, 1.0)
			var fov: float = lerpf(38.0, 30.0, box_t)
			return {"pos": pos, "look": look, "fov": fov}

		CameraMode.TACTICAL_3D:
			var pos := Vector3(clampf(target_ball_pos.x * 0.45, -18.0, 18.0), 38.0, 48.0)
			var look := Vector3(
				clampf(target_ball_pos.x * 0.35, -20.0, 20.0),
				0.0,
				clampf(target_ball_pos.z * 0.35, -10.0, 10.0)
			)
			return {"pos": pos, "look": look, "fov": 42.0}

		CameraMode.BEHIND_GOAL:
			var side_f := float(behind_goal_side)
			var pos := Vector3(side_f * 60.0, 6.5, 0.0)
			var look_x := minf(target_ball_pos.x, 49.0) if behind_goal_side > 0 else maxf(target_ball_pos.x, -49.0)
			var look := Vector3(
				look_x,
				clampf(target_ball_pos.y * 0.4, 0.5, 2.5),
				clampf(target_ball_pos.z * 0.75, -26.0, 26.0)
			)
			return {"pos": pos, "look": look, "fov": 44.0}

		CameraMode.FREE_ORBIT:
			var cos_p := cos(orbit_pitch)
			var offset := Vector3(
				sin(orbit_yaw) * cos_p,
				sin(orbit_pitch),
				cos(orbit_yaw) * cos_p
			) * orbit_distance
			return {"pos": orbit_target + offset, "look": orbit_target, "fov": 42.0}

	return {"pos": Vector3(0.0, 22.0, 38.0), "look": Vector3.ZERO, "fov": 38.0}

func _apply_camera_transform() -> void:
	camera.global_position = current_cam_pos
	camera.fov = current_fov
	if current_cam_pos.distance_squared_to(current_look_target) > 0.001:
		camera.look_at(current_look_target, Vector3.UP)
