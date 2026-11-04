@tool
extends EditorPlugin

const SceneDockScript = preload("res://addons/sir_editor_bridge/scene_dock.gd")

var _dock: EditorDock
var _content: VBoxContainer


func _enter_tree() -> void:
    _dock = EditorDock.new()
    _dock.name = "SIREngineSceneDock"
    _dock.title = "SIREngine Scene"
    _dock.default_slot = EditorDock.DOCK_SLOT_LEFT_BL
    _content = SceneDockScript.new()
    _content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    _content.size_flags_vertical = Control.SIZE_EXPAND_FILL
    _dock.add_child(_content)
    add_dock(_dock)
    scene_changed.connect(_on_scene_changed)
    scene_saved.connect(_on_scene_saved)


func _exit_tree() -> void:
    scene_changed.disconnect(_on_scene_changed)
    scene_saved.disconnect(_on_scene_saved)
    remove_dock(_dock)
    _dock.queue_free()
    _dock = null
    _content = null


func _on_scene_changed(_scene_root: Node) -> void:
    # A native snapshot must never be displayed as if it belonged to the new scene.
    _content.invalidate_snapshot()


func _on_scene_saved(_scene_path: String) -> void:
    # Edits in Godot invalidate previously captured native file snapshots.
    _content.invalidate_snapshot()
