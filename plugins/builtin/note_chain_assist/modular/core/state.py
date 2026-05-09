import os
import uuid

LEFT_BUTTON = 1
RIGHT_BUTTON = 2
CTRL_MODIFIER_MASK = 0x04000000
SHIFT_MODIFIER_MASK = 0x02000000
KEY_A = 0x41
KEY_DELETE = 0x01000007
KEY_BACKSPACE = 0x01000003
MAX_HISTORY = 128
SERIALIZE_DEN = 288
CURVE_CHECKPOINT_PREFIX = "Plugin Curve Edit"
CURVE_SIDECAR_FORMAT_VERSION = 3
DEFAULT_NODE_GROUP_ID = 1
DEFAULT_CURVE_GROUP_ID = 1

STYLE_PRESETS = [
    [4, 8, 12, 16],
    [8, 8, 12, 16, 24],
    [4, 6, 8, 12, 16, 24],
    [12, 16, 24, 32],
]


def build_initial_state():
    return {
        # Anchors are stored in chart space:
        # lane_x in [0..lane_width], beat in timeline beats.
        "anchors": [],
        "links": [],
        "drag": {"mode": "", "index": -1},
        "selected_anchor_ids": [],
        "selected_links": [],
        "selection_targets": {"anchors": True, "segments": True, "notes": False},
        "segment_denominators": {},
        "segment_shapes": {},
        "context_menu_links": [],
        "box_select": {"active": False, "start": [0.0, 0.0], "end": [0.0, 0.0], "append": False},
        "pending_connect_anchor_id": -1,
        "next_anchor_id": 1,
        "last_context": {},
        "last_click_anchor": -1,
        "last_click_ms": 0,
        "style": {"denominators": [4, 8, 12, 16], "style_name": "balanced"},
        "curve_visible": True,
        "active_link_shape": "curve",
        "note_curve_snap_enabled": False,
        "anchor_placement_enabled": False,
        "project_path": "",
        "project_dirty": False,
        "history": [],
        "history_index": -1,
        "lang": "en",
        "curve_revision": 0,
        "curve_samples_cache": {},
        "suppress_persist_once": False,
        "link_drag": {"active": False, "source_anchor_id": -1, "hover_anchor_id": -1, "x": 0.0, "y": 0.0},
        "shift_down": False,
        "last_host_selected_note_ids": [],
        "anchor_group_ids": {},
        "anchor_reserved": {},
        "anchor_compat_handles": {},
        "curve_id_by_link": {},
        "curve_no_by_link": {},
        "curve_group_ids_by_link": {},
        "curve_density_mode_by_link": {},
        "curve_reserved_by_link": {},
        "curve_special_joystick_by_link": {},
        "node_groups": [{"group_id": DEFAULT_NODE_GROUP_ID, "group_name": "base", "reserved": {}}],
        "curve_groups": [{"group_id": DEFAULT_CURVE_GROUP_ID, "group_name": "base", "reserved": {}}],
        "next_curve_id": 1,
        "next_group_id": 2,
        "project_revision": 0,
        "project_file_uuid": "",
        "project_last_writer_instance": "",
        "last_save_error": "",
        "last_save_error_detail": "",
        "instance_id": f"{os.getpid()}-{uuid.uuid4().hex[:12]}",
    }


STATE = build_initial_state()
