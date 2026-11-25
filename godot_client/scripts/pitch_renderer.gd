extends Node2D
class_name PitchRenderer

# Pitch Dimensions in meters (FIFA Standard)
const PITCH_LENGTH: float = 105.0
const PITCH_WIDTH: float = 68.0
const HALF_LENGTH: float = 52.5
const HALF_WIDTH: float = 34.0
const GOAL_WIDTH: float = 7.32
const GOAL_DEPTH: float = 2.44
const PENALTY_LENGTH: float = 16.5
const PENALTY_WIDTH: float = 40.32
const GOAL_AREA_LENGTH: float = 5.5
const GOAL_AREA_WIDTH: float = 18.32
const CENTER_CIRCLE_RADIUS: float = 9.15
const PENALTY_SPOT_DIST: float = 11.0
const CORNER_ARC_RADIUS: float = 1.0

# Render Configuration (positioned cleanly left of MatchHUD event log panel)
var pitch_center: Vector2 = Vector2(480, 360)
var meter_scale: float = 7.6 # pixels per meter

# Frame Data
var current_tick: int = 0
var ball_pos: Vector3 = Vector3.ZERO
var ball_owner: int = -1
var player_positions: Array[Vector2] = []
var player_facings: Array[float] = []
var player_staminas: Array[float] = []

func _init() -> void:
	player_positions.resize(22)
	player_facings.resize(22)
	player_staminas.resize(22)
	for i in range(22):
		player_positions[i] = Vector2.ZERO
		player_facings[i] = 0.0
		player_staminas[i] = 1.0

func to_screen(sim_x: float, sim_y: float) -> Vector2:
	return pitch_center + Vector2(sim_x * meter_scale, sim_y * meter_scale)

func update_frame_data(
	p_tick: int,
	p_ball_pos: Vector3,
	p_ball_owner: int,
	p_positions: Array[Vector2],
	p_facings: Array[float],
	p_staminas: Array[float]
) -> void:
	current_tick = p_tick
	ball_pos = p_ball_pos
	ball_owner = p_ball_owner
	for i in range(22):
		player_positions[i] = p_positions[i]
		player_facings[i] = p_facings[i]
		player_staminas[i] = p_staminas[i]
	queue_redraw()

func _draw() -> void:
	_draw_field_surface()
	_draw_boundary_lines()
	_draw_ball_shadow()
	_draw_players()
	_draw_ball()

func _draw_field_surface() -> void:
	var field_rect = Rect2(
		pitch_center.x - HALF_LENGTH * meter_scale,
		pitch_center.y - HALF_WIDTH * meter_scale,
		PITCH_LENGTH * meter_scale,
		PITCH_WIDTH * meter_scale
	)

	var apron_margin = 4.0 * meter_scale
	var apron_rect = Rect2(
		field_rect.position.x - apron_margin,
		field_rect.position.y - apron_margin,
		field_rect.size.x + apron_margin * 2.0,
		field_rect.size.y + apron_margin * 2.0
	)
	draw_rect(apron_rect, Color("#164b23"), true)
	draw_rect(field_rect, Color("#1e6b30"), true)

	var num_stripes = 10
	var stripe_w = field_rect.size.x / float(num_stripes)
	var stripe_color = Color("#237a38")
	for i in range(0, num_stripes, 2):
		var s_rect = Rect2(field_rect.position.x + i * stripe_w, field_rect.position.y, stripe_w, field_rect.size.y)
		draw_rect(s_rect, stripe_color, true)

func _draw_boundary_lines() -> void:
	var line_color = Color(1.0, 1.0, 1.0, 0.90)
	var line_w = 2.0

	var top_left = to_screen(-HALF_LENGTH, -HALF_WIDTH)
	var bot_right = to_screen(HALF_LENGTH, HALF_WIDTH)
	var field_rect = Rect2(top_left, bot_right - top_left)

	draw_rect(field_rect, line_color, false, line_w)
	draw_line(to_screen(0.0, -HALF_WIDTH), to_screen(0.0, HALF_WIDTH), line_color, line_w)
	draw_arc(pitch_center, CENTER_CIRCLE_RADIUS * meter_scale, 0.0, TAU, 64, line_color, line_w)
	draw_circle(pitch_center, 3.5, line_color)

	# Home Penalty Box (Left)
	var home_pen_top_left = to_screen(-HALF_LENGTH, -PENALTY_WIDTH * 0.5)
	var home_pen_size = Vector2(PENALTY_LENGTH * meter_scale, PENALTY_WIDTH * meter_scale)
	draw_rect(Rect2(home_pen_top_left, home_pen_size), line_color, false, line_w)

	# Away Penalty Box (Right)
	var away_pen_top_left = to_screen(HALF_LENGTH - PENALTY_LENGTH, -PENALTY_WIDTH * 0.5)
	var away_pen_size = Vector2(PENALTY_LENGTH * meter_scale, PENALTY_WIDTH * meter_scale)
	draw_rect(Rect2(away_pen_top_left, away_pen_size), line_color, false, line_w)

	# Home Goal Area
	var home_ga_top_left = to_screen(-HALF_LENGTH, -GOAL_AREA_WIDTH * 0.5)
	var home_ga_size = Vector2(GOAL_AREA_LENGTH * meter_scale, GOAL_AREA_WIDTH * meter_scale)
	draw_rect(Rect2(home_ga_top_left, home_ga_size), line_color, false, line_w)

	# Away Goal Area
	var away_ga_top_left = to_screen(HALF_LENGTH - GOAL_AREA_LENGTH, -GOAL_AREA_WIDTH * 0.5)
	var away_ga_size = Vector2(GOAL_AREA_LENGTH * meter_scale, GOAL_AREA_WIDTH * meter_scale)
	draw_rect(Rect2(away_ga_top_left, away_ga_size), line_color, false, line_w)

	var home_pen_spot = to_screen(-HALF_LENGTH + PENALTY_SPOT_DIST, 0.0)
	var away_pen_spot = to_screen(HALF_LENGTH - PENALTY_SPOT_DIST, 0.0)
	draw_circle(home_pen_spot, 2.5, line_color)
	draw_circle(away_pen_spot, 2.5, line_color)

	draw_arc(home_pen_spot, CENTER_CIRCLE_RADIUS * meter_scale, -0.92, 0.92, 32, line_color, line_w)
	draw_arc(away_pen_spot, CENTER_CIRCLE_RADIUS * meter_scale, PI - 0.92, PI + 0.92, 32, line_color, line_w)

	draw_arc(to_screen(-HALF_LENGTH, -HALF_WIDTH), CORNER_ARC_RADIUS * meter_scale, 0.0, 0.5 * PI, 16, line_color, line_w)
	draw_arc(to_screen(-HALF_LENGTH, HALF_WIDTH), CORNER_ARC_RADIUS * meter_scale, 1.5 * PI, 2.0 * PI, 16, line_color, line_w)
	draw_arc(to_screen(HALF_LENGTH, -HALF_WIDTH), CORNER_ARC_RADIUS * meter_scale, 0.5 * PI, PI, 16, line_color, line_w)
	draw_arc(to_screen(HALF_LENGTH, HALF_WIDTH), CORNER_ARC_RADIUS * meter_scale, PI, 1.5 * PI, 16, line_color, line_w)

	var goal_line_color = Color(0.9, 0.9, 0.9, 0.8)
	var net_fill_color = Color(0.8, 0.8, 0.8, 0.15)
	var h_goal_top = to_screen(-HALF_LENGTH, -GOAL_WIDTH * 0.5)
	var h_goal_rect = Rect2(h_goal_top.x - GOAL_DEPTH * meter_scale, h_goal_top.y, GOAL_DEPTH * meter_scale, GOAL_WIDTH * meter_scale)
	draw_rect(h_goal_rect, net_fill_color, true)
	draw_rect(h_goal_rect, goal_line_color, false, 1.5)

	var a_goal_top = to_screen(HALF_LENGTH, -GOAL_WIDTH * 0.5)
	var a_goal_rect = Rect2(a_goal_top.x, a_goal_top.y, GOAL_DEPTH * meter_scale, GOAL_WIDTH * meter_scale)
	draw_rect(a_goal_rect, net_fill_color, true)
	draw_rect(a_goal_rect, goal_line_color, false, 1.5)

func _draw_ball_shadow() -> void:
	var ground_screen = to_screen(ball_pos.x, ball_pos.y)
	var shadow_offset = Vector2(ball_pos.z * 1.5, ball_pos.z * 1.2)
	var shadow_pos = ground_screen + shadow_offset

	var shadow_rad_x = 4.5 + min(ball_pos.z * 0.6, 6.0)
	var alpha = clamp(0.55 - ball_pos.z * 0.08, 0.12, 0.55)
	var shadow_color = Color(0.0, 0.0, 0.0, alpha)
	draw_circle(shadow_pos, shadow_rad_x, shadow_color)

	if ball_pos.z > 0.4:
		var ball_screen = ground_screen - Vector2(0.0, ball_pos.z * meter_scale * 0.85)
		draw_line(ground_screen, ball_screen, Color(1.0, 1.0, 1.0, 0.25), 1.0)

func _draw_ball() -> void:
	var ground_screen = to_screen(ball_pos.x, ball_pos.y)
	var elevated_screen = ground_screen - Vector2(0.0, ball_pos.z * meter_scale * 0.85)
	var ball_radius = 4.8 + min(ball_pos.z * 0.35, 3.5)

	draw_circle(elevated_screen, ball_radius + 1.2, Color(0.12, 0.12, 0.12, 0.95))
	draw_circle(elevated_screen, ball_radius, Color(0.98, 0.98, 0.98, 1.0))
	draw_circle(elevated_screen, ball_radius * 0.4, Color(0.18, 0.18, 0.18, 0.9))

func _draw_players() -> void:
	var font = ThemeDB.fallback_font
	var font_size = 9

	for i in range(22):
		var p_pos = to_screen(player_positions[i].x, player_positions[i].y)
		var facing = player_facings[i]
		var is_home = (i < 11)
		var is_gk = (i == 0 or i == 11)
		var has_ball = (i == ball_owner)

		# Possession highlight ring
		if has_ball:
			draw_arc(p_pos, 12.5, 0.0, TAU, 24, Color("#ffd54f"), 2.0)

		draw_circle(p_pos + Vector2(1.5, 2.0), 8.5, Color(0.0, 0.0, 0.0, 0.35))

		var dir = Vector2(cos(facing), sin(facing))
		var tip = p_pos + dir * 13.5
		var left_wing = p_pos + dir.rotated(2.45) * 7.5
		var right_wing = p_pos + dir.rotated(-2.45) * 7.5
		var pointer_color = Color(1.0, 1.0, 1.0, 0.85) if is_home else Color(1.0, 0.85, 0.3, 0.9)
		draw_colored_polygon(PackedVector2Array([tip, left_wing, right_wing]), pointer_color)

		var kit_color: Color
		var border_color: Color
		var text_color: Color

		if is_home:
			if is_gk:
				kit_color = Color("#f1c40f")
				border_color = Color("#000000")
				text_color = Color("#000000")
			else:
				kit_color = Color("#1e88e5")
				border_color = Color("#ffffff")
				text_color = Color("#ffffff")
		else:
			if is_gk:
				kit_color = Color("#26a69a")
				border_color = Color("#000000")
				text_color = Color("#000000")
			else:
				kit_color = Color("#e53935")
				border_color = Color("#ffffff")
				text_color = Color("#ffffff")

		draw_circle(p_pos, 8.0 + 1.1, border_color)
		draw_circle(p_pos, 8.0, kit_color)

		var jersey_num = 1 if is_gk else ((i + 1) if is_home else (i - 10))
		var num_str = str(jersey_num)
		var text_width = 16.0
		var text_pos = p_pos + Vector2(-text_width * 0.5, font_size * 0.35)
		draw_string(font, text_pos, num_str, HORIZONTAL_ALIGNMENT_CENTER, text_width, font_size, text_color)
