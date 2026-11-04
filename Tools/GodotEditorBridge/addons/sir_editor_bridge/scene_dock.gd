@tool
extends VBoxContainer

const HOST_SETTING: String = "sir_editor/host_binary"
const MAX_TREE_ROWS: int = 2048
const MAX_JSON_BYTES: int = 4 * 1024 * 1024
const PROCESS_TIMEOUT_MSEC: int = 30000

var _binary: LineEdit
var _status: Label
var _tree: Tree
var _items_by_id: Dictionary = {}
var _snapshot_scene: String = ""
var _snapshot_hash: String = ""
var _rebuilding: bool = false

var _request_generation: int = 0
var _active_pid: int = 0
var _active_scene: String = ""
var _active_disk_path: String = ""
var _active_source_hash: String = ""
var _active_output: String = ""
var _active_started: int = 0


func _ready() -> void:
    var settings: EditorSettings = EditorInterface.get_editor_settings()
    if not settings.has_setting(HOST_SETTING):
        settings.set_setting(HOST_SETTING, "")

    var controls := HBoxContainer.new()
    add_child(controls)
    _binary = LineEdit.new()
    _binary.placeholder_text = "Path to SIREditor executable"
    _binary.text = String(settings.get_setting(HOST_SETTING))
    _binary.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    _binary.text_submitted.connect(_on_binary_submitted)
    controls.add_child(_binary)

    var refresh := Button.new()
    refresh.text = "Refresh"
    refresh.pressed.connect(_on_refresh_pressed)
    controls.add_child(refresh)

    _status = Label.new()
    _status.text = "Read-only native scene snapshot. Save the scene, then Refresh."
    _status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    add_child(_status)

    _tree = Tree.new()
    _tree.hide_root = true
    _tree.columns = 1
    _tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
    _tree.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    _tree.item_selected.connect(_on_item_selected)
    add_child(_tree)


func _exit_tree() -> void:
    shutdown_bridge()


func shutdown_bridge() -> void:
    _cancel_active_request()


func _remove_output_files(path: String) -> void:
    if path.is_empty():
        return
    if FileAccess.file_exists(path):
        DirAccess.remove_absolute(path)
    if FileAccess.file_exists(path + ".tmp"):
        DirAccess.remove_absolute(path + ".tmp")


func _cancel_active_request() -> void:
    _request_generation += 1
    if _active_pid > 0 and OS.is_process_running(_active_pid):
        OS.kill(_active_pid)
    _active_pid = 0
    _remove_output_files(_active_output)
    _active_output = ""
    _active_scene = ""
    _active_disk_path = ""
    _active_source_hash = ""
    _active_started = 0


func invalidate_snapshot() -> void:
    _cancel_active_request()
    _snapshot_scene = ""
    _snapshot_hash = ""
    _items_by_id.clear()
    if _tree != null:
        _tree.clear()
    if _status != null:
        _status.text = "Scene changed. Save and Refresh for a native snapshot."


func _on_binary_submitted(_text: String) -> void:
    EditorInterface.get_editor_settings().set_setting(HOST_SETTING, _binary.text.strip_edges())


func _on_refresh_pressed() -> void:
    var scene_root: Node = EditorInterface.get_edited_scene_root()
    if scene_root == null or scene_root.scene_file_path.is_empty():
        _status.text = "The scene must be saved to disk before inspection."
        return

    var executable: String = _binary.text.strip_edges()
    if executable.is_empty():
        _status.text = "Configure the SIREditor executable path first."
        return

    _cancel_active_request()
    _snapshot_scene = ""
    _snapshot_hash = ""
    _items_by_id.clear()
    _tree.clear()
    _on_binary_submitted("")

    var disk_path: String = ProjectSettings.globalize_path(scene_root.scene_file_path)
    var source_hash: String = FileAccess.get_sha256(disk_path)
    if source_hash.is_empty():
        _status.text = "Cannot read the saved scene file."
        return

    # Each request writes one unique cache file, never to the authored scene.
    _active_output = ProjectSettings.globalize_path("user://sir_scene_%s_%s.json" % [
        str(get_instance_id()), str(_request_generation)])
    _remove_output_files(_active_output)
    _active_scene = scene_root.scene_file_path
    _active_disk_path = disk_path
    _active_source_hash = source_hash
    _active_started = Time.get_ticks_msec()
    _active_pid = OS.create_process(executable, PackedStringArray([
        "--inspect-scene-json-file", disk_path, _active_output]))
    if _active_pid <= 0:
        _cancel_active_request()
        _status.text = "Cannot launch the native SIREditor executable."
        return
    _status.text = "Inspecting saved scene without blocking the editor..."


func _process(_delta: float) -> void:
    if _active_pid <= 0:
        return
    if OS.is_process_running(_active_pid):
        if Time.get_ticks_msec() - _active_started >= PROCESS_TIMEOUT_MSEC:
            _cancel_active_request()
            _status.text = "Native scene inspection timed out."
        return

    var exit_code: int = OS.get_process_exit_code(_active_pid)
    var output_path: String = _active_output
    var scene_path: String = _active_scene
    var disk_path: String = _active_disk_path
    var source_hash: String = _active_source_hash
    _active_pid = 0
    _active_output = ""
    _active_scene = ""
    _active_disk_path = ""
    _active_source_hash = ""

    var current_root: Node = EditorInterface.get_edited_scene_root()
    if current_root == null or current_root.scene_file_path != scene_path:
        _remove_output_files(output_path)
        return
    if FileAccess.get_sha256(disk_path) != source_hash:
        _remove_output_files(output_path)
        _status.text = "Saved scene changed during inspection. Refresh again."
        return
    if exit_code != 0 or not FileAccess.file_exists(output_path):
        _remove_output_files(output_path)
        _status.text = "Native scene inspection failed (exit %d)." % exit_code
        return

    var output_file: FileAccess = FileAccess.open(output_path, FileAccess.READ)
    if output_file == null:
        _remove_output_files(output_path)
        _status.text = "Cannot read native scene snapshot."
        return
    if output_file.get_length() > MAX_JSON_BYTES:
        output_file.close()
        _remove_output_files(output_path)
        _status.text = "Native scene snapshot exceeds dock limit."
        return
    var output_text: String = output_file.get_as_text()
    output_file.close()
    _remove_output_files(output_path)

    var parsed: Variant = JSON.parse_string(output_text)
    if typeof(parsed) != TYPE_DICTIONARY:
        _status.text = "Native output is not a JSON object."
        return
    if parsed.get("scene", "") != disk_path or not _valid_snapshot(parsed):
        _status.text = "Native snapshot failed scene identity or schema validation."
        return

    _snapshot_hash = source_hash
    _show_snapshot(parsed, scene_path)


func _valid_snapshot(snapshot: Dictionary) -> bool:
    if snapshot.get("schema", 0) != 1:
        return false
    if typeof(snapshot.get("revision", null)) != TYPE_STRING:
        return false
    var nodes: Variant = snapshot.get("nodes", null)
    var resources: Variant = snapshot.get("resources", null)
    if typeof(nodes) != TYPE_ARRAY or typeof(resources) != TYPE_ARRAY:
        return false
    if nodes.is_empty() or nodes.size() > MAX_TREE_ROWS:
        return false

    var seen: Dictionary = {}
    for node_data in nodes:
        if typeof(node_data) != TYPE_DICTIONARY:
            return false
        var id_value: Variant = node_data.get("id", null)
        var parent: Variant = node_data.get("parent", null)
        var path: Variant = node_data.get("path", null)
        var name: Variant = node_data.get("name", null)
        if typeof(id_value) != TYPE_STRING or typeof(parent) != TYPE_STRING:
            return false
        if typeof(path) != TYPE_STRING or typeof(name) != TYPE_STRING:
            return false
        if id_value.is_empty() or seen.has(id_value):
            return false
        if parent != "0" and not seen.has(parent):
            return false
        seen[id_value] = true
    return true


func _show_snapshot(snapshot: Dictionary, source_scene: String) -> void:
    _rebuilding = true
    _tree.clear()
    _items_by_id.clear()
    var tree_root: TreeItem = _tree.create_item()
    for node_data in snapshot["nodes"]:
        var id_value: String = node_data["id"]
        var parent_id: String = node_data["parent"]
        var parent_item: TreeItem = tree_root if parent_id == "0" else _items_by_id[parent_id]
        var item: TreeItem = _tree.create_item(parent_item)
        item.set_text(0, node_data["name"])
        item.set_metadata(0, node_data["path"])
        item.set_tooltip_text(0, "Source line %s | Native ID %s" % [str(node_data["line"]), id_value])
        _items_by_id[id_value] = item

    _snapshot_scene = source_scene
    _rebuilding = false
    _status.text = "%d nodes, %d resources | native revision %s | saved file only" % [
        snapshot["nodes"].size(), snapshot["resources"].size(), str(snapshot["revision"])]


func _on_item_selected() -> void:
    if _rebuilding or _snapshot_scene.is_empty():
        return
    var scene_root: Node = EditorInterface.get_edited_scene_root()
    if scene_root == null or scene_root.scene_file_path != _snapshot_scene:
        invalidate_snapshot()
        return
    if FileAccess.get_sha256(ProjectSettings.globalize_path(_snapshot_scene)) != _snapshot_hash:
        invalidate_snapshot()
        return

    var selected_item: TreeItem = _tree.get_selected()
    if selected_item == null:
        return
    var relative_path: String = String(selected_item.get_metadata(0))
    var selected_node: Node = scene_root if relative_path.is_empty() else scene_root.get_node_or_null(NodePath(relative_path))
    if selected_node == null:
        _status.text = "The saved snapshot is stale. Save and Refresh."
        return
    var editor_selection: EditorSelection = EditorInterface.get_selection()
    editor_selection.clear()
    editor_selection.add_node(selected_node)
    EditorInterface.edit_node(selected_node)
