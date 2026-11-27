extends Node3D
class_name PitchRenderer3D

const PLAYER_SCENE: PackedScene = preload("res://scenes/player_3d.tscn")
const CamRigScript = preload("res://scripts/camera_rig_3d.gd")
const BALL_RADIUS: float = 0.11

@onready var ball_root: Node3D = $BallRoot
@onready var ball_mesh: MeshInstance3D = $BallRoot/BallMesh
@onready var ball_shadow: MeshInstance3D = $BallRoot/BallShadow
@onready var players_container: Node3D = $PlayersContainer
@onready var ball_carrier_ring: MeshInstance3D = $BallCarrierRing
@onready var camera_rig: CamRigScript = $CameraRig3D

var player_nodes: Array[Node3D] = []
var ball_shadow_mat: ShaderMaterial = null
var prev_ball_xz: Vector3 = Vector3.ZERO
var last_ball_tick: int = -100
var has_prev_ball: bool = false

func _ready() -> void:
	_setup_players()
	if ball_shadow and ball_shadow.get_active_material(0) is ShaderMaterial:
		ball_shadow_mat = ball_shadow.get_active_material(0) as ShaderMaterial

func _setup_players() -> void:
	if not players_container:
		return

	var mat_home_torso := _make_kit_material(Color("#1e88e5"), 0.72)
	var mat_home_gk_torso := _make_kit_material(Color("#f1c40f"), 0.72)
	var mat_away_torso := _make_kit_material(Color("#e53935"), 0.72)
	var mat_away_gk_torso := _make_kit_material(Color("#26a69a"), 0.72)

	var mat_white_shorts := _make_kit_material(Color("#f2f4f8"), 0.78)
	var mat_black_shorts := _make_kit_material(Color("#181a1f"), 0.78)

	var mat_home_cone := _make_kit_material(Color("#ffffff"), 0.45)
	var mat_away_cone := _make_kit_material(Color("#ffd54f"), 0.45)
	var mat_gk_cone := _make_kit_material(Color("#181a1f"), 0.45)

	player_nodes.clear()
	for i in range(22):
		var p_node := PLAYER_SCENE.instantiate() as Node3D
		p_node.name = "Player_%02d" % i
		players_container.add_child(p_node)

		var is_home := (i < 11)
		var is_gk := (i == 0 or i == 11)

		var torso := p_node.get_node_or_null("ModelPivot/TorsoMesh") as MeshInstance3D
		var shorts := p_node.get_node_or_null("ModelPivot/ShortsMesh") as MeshInstance3D
		var cone := p_node.get_node_or_null("ModelPivot/FacingCone") as MeshInstance3D
		var num_label := p_node.get_node_or_null("NumberLabel") as Label3D

		if torso:
			if is_home:
				torso.material_override = mat_home_gk_torso if is_gk else mat_home_torso
			else:
				torso.material_override = mat_away_gk_torso if is_gk else mat_away_torso

		if shorts:
			shorts.material_override = mat_black_shorts if is_gk else mat_white_shorts

		if cone:
			if is_gk:
				cone.material_override = mat_gk_cone
			else:
				cone.material_override = mat_home_cone if is_home else mat_away_cone

		if num_label:
			var jersey_num := (i + 1) if is_home else (i - 10)
			num_label.text = str(jersey_num)
			num_label.modulate = Color("#ffd54f") if is_gk else Color(1.0, 1.0, 1.0, 0.96)

		player_nodes.append(p_node)

func _make_kit_material(color: Color, roughness_val: float) -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.roughness = roughness_val
	return mat

func reset_motion_state() -> void:
	has_prev_ball = false
	last_ball_tick = -100
	if ball_mesh:
		ball_mesh.basis = Basis.IDENTITY

func update_frame_data(
	p_tick: int,
	p_ball_pos: Vector3,
	p_ball_owner: int,
	p_positions: Array[Vector2],
	p_facings: Array[float],
	_p_staminas: Array[float],
	p_discontinuous: bool = false
) -> void:
	var ball_3d := Vector3(p_ball_pos.x, maxf(0.0, p_ball_pos.z) + BALL_RADIUS, -p_ball_pos.y)
	var ball_xz := Vector3(p_ball_pos.x, 0.0, -p_ball_pos.y)

	if ball_root:
		ball_root.position = ball_xz
	if ball_mesh:
		ball_mesh.position = Vector3(0.0, maxf(0.0, p_ball_pos.z) + BALL_RADIUS, 0.0)
		if has_prev_ball and not p_discontinuous and abs(p_tick - last_ball_tick) <= 2:
			var delta_p := ball_xz - prev_ball_xz
			delta_p.y = 0.0
			var dist := delta_p.length()
			if dist > 0.0005 and dist <= 6.0:
				var roll_axis := Vector3.UP.cross(delta_p / dist)
				if roll_axis.length_squared() > 0.0001:
					var roll_angle := dist / BALL_RADIUS
					ball_mesh.basis = (Basis(roll_axis.normalized(), roll_angle) * ball_mesh.basis).orthonormalized()

	prev_ball_xz = ball_xz
	last_ball_tick = p_tick
	has_prev_ball = true

	if ball_shadow:
		ball_shadow.position = Vector3(0.0, 0.015, 0.0)
		var h_norm := clampf(maxf(0.0, p_ball_pos.z) / 12.0, 0.0, 1.0)
		var shadow_scale := lerpf(0.38, 1.10, h_norm)
		var shadow_alpha := lerpf(0.65, 0.14, h_norm)
		ball_shadow.scale = Vector3(shadow_scale, 1.0, shadow_scale)
		if ball_shadow_mat:
			ball_shadow_mat.set_shader_parameter("shadow_alpha", shadow_alpha)

	var centroid := Vector3.ZERO
	var outfield_count: int = 0
	for i in range(min(22, player_nodes.size())):
		var p_3d := Vector3(p_positions[i].x, 0.0, -p_positions[i].y)
		var p_node := player_nodes[i]
		p_node.position = p_3d
		p_node.rotation.y = p_facings[i] - PI * 0.5
		if i != 0 and i != 11:
			centroid += p_3d
			outfield_count += 1

	if outfield_count > 0:
		centroid /= float(outfield_count)

	if ball_carrier_ring:
		if p_ball_owner >= 0 and p_ball_owner < 22:
			ball_carrier_ring.visible = true
			ball_carrier_ring.position = Vector3(p_positions[p_ball_owner].x, 0.02, -p_positions[p_ball_owner].y)
		else:
			ball_carrier_ring.visible = false

	if camera_rig:
		camera_rig.set_tracking_targets(ball_3d, centroid)
		if p_discontinuous:
			camera_rig.snap_to_target()
