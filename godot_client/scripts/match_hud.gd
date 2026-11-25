extends Control
class_name MatchHUD

@onready var score_label: Label = $ScoreboardBar/ScoreboardHBox/ScoreLabel
@onready var clock_label: Label = $ScoreboardBar/ScoreboardHBox/ClockLabel
@onready var half_label: Label = $ScoreboardBar/ScoreboardHBox/HalfLabel
@onready var play_state_label: Label = $ScoreboardBar/ScoreboardHBox/PlayStateLabel
@onready var stats_label: Label = $EventPanel/Margin/VBox/StatsLabel
@onready var event_log: RichTextLabel = $EventPanel/Margin/VBox/EventLog

var all_events: Array = []
var last_rendered_tick: int = -1

const MAJOR_EVENT_TYPES := {
	0: true,  # Kickoff
	3: true,  # PassIntercepted
	5: true,  # TackleWon
	6: true,  # Shot
	7: true,  # ShotOnTarget
	8: true,  # Save
	9: true,  # Goal
	12: true, # Corner
	13: true, # GoalKick
	14: true, # ThrowIn
	15: true, # HalfTime
	16: true  # FullTime
}

func _ready() -> void:
	if event_log:
		event_log.bbcode_enabled = true
		event_log.scroll_following = true

func setup_events(events: Array) -> void:
	all_events = events
	last_rendered_tick = -1
	if event_log:
		event_log.text = ""

func _format_clock(tick: int) -> String:
	var total_seconds: int = int(float(tick) * 0.05)
	var mins: int = total_seconds / 60
	var secs: int = total_seconds % 60
	return "%02d:%02d" % [mins, secs]

func update_hud(tick: int, hud_state: Dictionary) -> void:
	var score_home: int = int(hud_state.get("score_home", 0))
	var score_away: int = int(hud_state.get("score_away", 0))
	var play_state_name: String = str(hud_state.get("play_state_name", "Open Play"))
	var play_state_id: int = int(hud_state.get("play_state", 1))
	var half_num: int = int(hud_state.get("half", 1))

	if score_label:
		score_label.text = "HOME  %d - %d  AWAY" % [score_home, score_away]

	if clock_label:
		clock_label.text = _format_clock(tick)

	if half_label:
		if play_state_id == 7:
			half_label.text = "FT"
		elif play_state_id == 6:
			half_label.text = "HT"
		else:
			half_label.text = "1ST HALF" if half_num == 1 else "2ND HALF"

	if play_state_label:
		play_state_label.text = play_state_name.to_upper()
		match play_state_id:
			5: # GoalScored
				play_state_label.add_theme_color_override("font_color", Color(1.0, 0.85, 0.2, 1.0))
			1: # OpenPlay
				play_state_label.add_theme_color_override("font_color", Color(0.35, 1.0, 0.55, 1.0))
			6, 7: # HalfTime / FullTime
				play_state_label.add_theme_color_override("font_color", Color(0.85, 0.85, 0.95, 1.0))
			_: # Set pieces
				play_state_label.add_theme_color_override("font_color", Color(0.45, 0.85, 1.0, 1.0))

	if tick == last_rendered_tick:
		return
	last_rendered_tick = tick

	var shots_h: int = 0
	var shots_a: int = 0
	var passes_h: int = 0
	var passes_a: int = 0
	var saves_h: int = 0
	var saves_a: int = 0
	var lines: PackedStringArray = []

	for evt in all_events:
		var evt_tick: int = int(evt.get("tick", 0))
		if evt_tick > tick:
			break
		var etype: int = int(evt.get("type", -1))
		var team_id: int = int(evt.get("team_id", -1))

		if etype == 6: # Shot
			if team_id == 0: shots_h += 1
			elif team_id == 1: shots_a += 1
		elif etype == 2: # PassComplete
			if team_id == 0: passes_h += 1
			elif team_id == 1: passes_a += 1
		elif etype == 8: # Save
			if team_id == 0: saves_h += 1
			elif team_id == 1: saves_a += 1

		if MAJOR_EVENT_TYPES.has(etype):
			var ts := _format_clock(evt_tick)
			var msg := str(evt.get("message", ""))
			var color_hex := "#d8dee9"
			if etype == 9: # Goal
				color_hex = "#ffd54f"
			elif etype == 6 or etype == 7: # Shot
				color_hex = "#88c0d0"
			elif etype == 8: # Save
				color_hex = "#a3be8c"
			elif etype == 15 or etype == 16: # HT / FT
				color_hex = "#ebcb8b"
			lines.append("[color=%s][%s] %s[/color]" % [color_hex, ts, msg])

	if stats_label:
		stats_label.text = "Shots: %d - %d  |  Saves: %d - %d  |  Passes: %d - %d" % [
			shots_h, shots_a, saves_h, saves_a, passes_h, passes_a
		]

	if event_log:
		var max_lines: int = 45
		if lines.size() > max_lines:
			lines = lines.slice(lines.size() - max_lines, lines.size())
		event_log.text = "\n".join(lines)
