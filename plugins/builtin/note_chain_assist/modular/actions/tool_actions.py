
def run_tool_action(payload, callbacks):
    state = callbacks["state"]
    ensure_project_context = callbacks["ensure_project_context"]
    reset_anchors = callbacks["reset_anchors"]
    save_project = callbacks["save_project"]
    record_history_state = callbacks["record_history_state"]
    toggle_polyline_for_active_or_selected = callbacks["toggle_polyline_for_active_or_selected"]
    toggle_polyline_for_context_links = callbacks["toggle_polyline_for_context_links"]
    note_curve_snap_enabled = callbacks["note_curve_snap_enabled"]
    set_density_for_selected_segments = callbacks["set_density_for_selected_segments"]
    connect_selected_anchors = callbacks["connect_selected_anchors"]
    disconnect_selected_segments = callbacks["disconnect_selected_segments"]
    delete_selected_anchors = callbacks["delete_selected_anchors"]
    save_style = callbacks["save_style"]
    load_style = callbacks["load_style"]

    action_id = str((payload or {}).get("action_id", ""))
    context = (payload or {}).get("context", {}) or {}
    ensure_project_context(context)

    if action_id == "reset_curve":
        reset_anchors(context)
        return True
    if action_id in ("commit_curve_to_notes", "commit_curve_to_notes_sidebar", "commit_context_segments_to_notes"):
        if state["project_path"] and state["project_dirty"]:
            if not save_project(state["project_path"], context):
                return False
        return True
    if action_id == "toggle_anchor_placement":
        state["anchor_placement_enabled"] = not bool(state.get("anchor_placement_enabled", False))
        record_history_state(context)
        return True
    if action_id == "toggle_curve_visible":
        state["curve_visible"] = not bool(state.get("curve_visible", True))
        record_history_state(context)
        return True
    if action_id == "toggle_polyline_mode":
        return toggle_polyline_for_active_or_selected(context)
    if action_id == "toggle_context_polyline_mode":
        return toggle_polyline_for_context_links(context)
    if action_id == "toggle_note_curve_snap":
        state["note_curve_snap_enabled"] = not note_curve_snap_enabled()
        record_history_state(context)
        return True
    if action_id == "toggle_select_anchors":
        cur = bool(state.get("selection_targets", {}).get("anchors", True))
        state["selection_targets"]["anchors"] = not cur
        if not state["selection_targets"]["anchors"]:
            state["selected_anchor_ids"] = []
        record_history_state(context)
        return True
    if action_id == "toggle_select_segments":
        cur = bool(state.get("selection_targets", {}).get("segments", True))
        state["selection_targets"]["segments"] = not cur
        if not state["selection_targets"]["segments"]:
            state["selected_links"] = []
        record_history_state(context)
        return True
    if action_id == "toggle_select_notes":
        cur = bool(state.get("selection_targets", {}).get("notes", False))
        state["selection_targets"]["notes"] = not cur
        record_history_state(context)
        return True
    if action_id == "set_segment_density_follow":
        return set_density_for_selected_segments(context, 0)
    if action_id.startswith("set_segment_density_"):
        suffix = action_id[len("set_segment_density_") :]
        try:
            den = int(suffix)
        except Exception:
            return False
        return set_density_for_selected_segments(context, den)
    if action_id in ("connect_selected_nodes", "connect_selected_nodes_ctx"):
        return connect_selected_anchors(context)
    if action_id == "disconnect_selected_segments":
        return disconnect_selected_segments(context)
    if action_id == "disconnect_selected_segments_ctx":
        changed_segments = disconnect_selected_segments(context)
        changed_anchors = delete_selected_anchors(context)
        return changed_segments or changed_anchors
    if action_id == "export_style_preset":
        return save_style(context)
    if action_id == "import_style_preset":
        ok = load_style(context)
        if ok:
            record_history_state(context)
        return ok
    return False


def run_one_shot(action_id):
    if isinstance(action_id, str):
        if action_id == "set_segment_density_follow" or action_id.startswith("set_segment_density_"):
            return 0
    known = {
        "reset_curve",
        "commit_curve_to_notes",
        "commit_curve_to_notes_sidebar",
        "commit_context_segments_to_notes",
        "toggle_anchor_placement",
        "toggle_curve_visible",
        "toggle_polyline_mode",
        "toggle_context_polyline_mode",
        "toggle_note_curve_snap",
        "toggle_select_anchors",
        "toggle_select_segments",
        "toggle_select_notes",
        "connect_selected_nodes",
        "disconnect_selected_segments",
        "connect_selected_nodes_ctx",
        "disconnect_selected_segments_ctx",
        "export_style_preset",
        "import_style_preset",
    }
    return 0 if action_id in known else 1
