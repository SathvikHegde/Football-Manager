extends Node

@onready var status_label: Label = $CanvasLayer/StatusLabel

func _ready() -> void:
	print("==================================================")
	print("Football Manager Client Bootstrapping (Stage 0)")
	print("==================================================")
	
	if ClassDB.class_exists("MatchEngineBridge"):
		var bridge = ClassDB.instantiate("MatchEngineBridge")
		var ping_msg: String = bridge.ping()
		var version_msg: String = bridge.get_engine_version()
		print("[GDExtension] Bridge initialized successfully!")
		print("[GDExtension] ping(): ", ping_msg)
		print("[GDExtension] version: ", version_msg)
		
		if status_label:
			status_label.text = "GDExtension Ready!\n" + ping_msg + "\nVersion: " + version_msg
	else:
		var err_msg = "[GDExtension Warning] MatchEngineBridge not found in ClassDB.\nCompile fm_gdextension to link with Godot."
		print(err_msg)
		if status_label:
			status_label.text = err_msg

