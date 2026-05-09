def prepare_canvas_input(payload, callbacks):
    state = callbacks["state"]
    ensure_project_context = callbacks["ensure_project_context"]
    seed_missing_segment_denominators = callbacks["seed_missing_segment_denominators"]
    sync_anchor_placement_with_host_mode = callbacks["sync_anchor_placement_with_host_mode"]
    sync_anchor_selection_from_host_notes = callbacks["sync_anchor_selection_from_host_notes"]
    now_ms = callbacks["now_ms"]

    context = payload.get("context", {}) if isinstance(payload, dict) else {}
    event = payload.get("event", {}) if isinstance(payload, dict) else {}
    state["last_context"] = context if isinstance(context, dict) else {}
    ensure_project_context(state["last_context"])
    seed_missing_segment_denominators(state["last_context"])
    sync_anchor_placement_with_host_mode(state["last_context"])
    sync_anchor_selection_from_host_notes(state["last_context"])

    et = str(event.get("type", ""))
    x = float(event.get("x", 0.0))
    y = float(event.get("y", 0.0))
    button = int(event.get("button", 0))
    ts = int(event.get("timestamp_ms", now_ms()))

    return {
        "context": context,
        "event": event,
        "event_type": et,
        "x": x,
        "y": y,
        "button": button,
        "timestamp_ms": ts,
    }


def build_canvas_response(consumed, cursor, status_text, request_undo_checkpoint, callbacks):
    build_overlay = callbacks["build_overlay"]
    context = callbacks["context"]
    checkpoint_prefix = callbacks["checkpoint_prefix"]
    return {
        "consumed": consumed,
        "overlay": build_overlay(context),
        "cursor": cursor,
        "status_text": status_text,
        "preview_batch_edit": {"add": [], "remove": [], "move": []},
        "request_undo_checkpoint": request_undo_checkpoint,
        "undo_checkpoint_label": checkpoint_prefix,
    }


def should_passthrough_for_note_curve_snap(event_type, event, button, left_button):
    if event_type not in ("mouse_down", "mouse_move", "mouse_up"):
        return False
    buttons = int(event.get("buttons", 0))
    return button == left_button or (buttons & left_button)


def sync_anchor_placement_with_host_mode(context, state):
    if not isinstance(context, dict):
        return
    host_sel = context.get("host_selection_tool", {})
    if not isinstance(host_sel, dict):
        return
    mode = str(host_sel.get("mode", "")).strip().lower()
    if mode == "anchor_place":
        state["anchor_placement_enabled"] = True
    elif mode in ("place_note", "place_rain", "delete", "select"):
        state["anchor_placement_enabled"] = False


def host_controls_anchor_mode(context):
    if not isinstance(context, dict):
        return False
    host_sel = context.get("host_selection_tool", {})
    if not isinstance(host_sel, dict):
        return False
    mode = str(host_sel.get("mode", "")).strip().lower()
    return mode in ("anchor_place", "place_note", "place_rain", "delete", "select")


def sync_anchor_selection_from_host_notes(context, callbacks):
    selection_enabled = callbacks["selection_enabled"]
    anchor_index_map = callbacks["anchor_index_map"]
    state = callbacks["state"]

    if not isinstance(context, dict):
        return
    if not selection_enabled("notes") or not selection_enabled("anchors"):
        state["last_host_selected_note_ids"] = []
        return
    if state.get("drag", {}).get("mode") or bool(state.get("box_select", {}).get("active", False)):
        return

    raw_ids = context.get("selected_note_ids")
    selected_ids = [str(v) for v in raw_ids if isinstance(v, str) and v] if isinstance(raw_ids, list) else []
    if selected_ids == list(state.get("last_host_selected_note_ids", [])):
        return
    state["last_host_selected_note_ids"] = list(selected_ids)

    selected_notes = context.get("selected_notes")
    if not isinstance(selected_notes, list) or not selected_notes:
        return

    idx_map = anchor_index_map()
    anchors = state.get("anchors", [])
    if not idx_map or not anchors:
        return

    picked = []
    used_anchor_ids = set()
    for raw in selected_notes:
        if not isinstance(raw, dict):
            continue
        try:
            lane_x = float(raw.get("lane_x", raw.get("x", 0.0)))
            beat = float(raw.get("beat", 0.0))
        except Exception:
            continue

        best_id = -1
        best_dist = 1e18
        for aid, idx in idx_map.items():
            if aid in used_anchor_ids:
                continue
            a = anchors[idx]
            dx = float(a.get("lane_x", 0.0)) - lane_x
            dy = float(a.get("beat", 0.0)) - beat
            dist2 = dx * dx + dy * dy
            if dist2 < best_dist:
                best_dist = dist2
                best_id = aid
        if best_id > 0:
            used_anchor_ids.add(best_id)
            picked.append(best_id)

    if not picked:
        return
    picked.sort(key=lambda aid: idx_map.get(aid, 1 << 30))
    state["selected_anchor_ids"] = picked
