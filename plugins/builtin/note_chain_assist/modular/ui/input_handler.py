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


def apply_note_selection_consume_policy(event_type, consumed, button, pressed_buttons, notes_selectable, left_button):
    if event_type in ("mouse_down", "mouse_up") and (not consumed) and button == left_button and (not notes_selectable):
        return True
    if event_type == "mouse_move" and (not consumed) and int(pressed_buttons) != 0 and (not notes_selectable):
        return True
    return consumed


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


def handle_key_down(event, callbacks):
    state = callbacks["state"]
    tr = callbacks["tr"]
    event_has_shift = callbacks["event_has_shift"]
    host_controls_anchor_mode = callbacks["host_controls_anchor_mode"]
    record_history_state = callbacks["record_history_state"]
    disconnect_selected_segments = callbacks["disconnect_selected_segments"]
    delete_selected_anchors = callbacks["delete_selected_anchors"]
    ctrl_modifier_mask = callbacks["ctrl_modifier_mask"]
    key_a = callbacks["key_a"]
    key_delete = callbacks["key_delete"]
    key_backspace = callbacks["key_backspace"]
    key_shift = callbacks["key_shift"]
    key_escape = callbacks["key_escape"]

    key = int(event.get("key", 0))
    mods = int(event.get("modifiers", 0))
    ctrl = (mods & ctrl_modifier_mask) != 0
    consumed = False
    status = ""
    request_checkpoint = False

    if key == key_shift or event_has_shift(event):
        state["shift_down"] = True
    if not ctrl and key == key_a and not host_controls_anchor_mode(state["last_context"]):
        state["anchor_placement_enabled"] = not bool(state.get("anchor_placement_enabled", False))
        consumed = True
        status = tr(state["last_context"], "anchor_enabled") if state["anchor_placement_enabled"] else tr(state["last_context"], "anchor_disabled")
        record_history_state(state["last_context"])
        request_checkpoint = True
    elif key in (key_delete, key_backspace):
        changed_segments = disconnect_selected_segments(state["last_context"])
        changed_anchors = delete_selected_anchors(state["last_context"])
        if changed_segments or changed_anchors:
            consumed = True
            request_checkpoint = True
            status = tr(state["last_context"], "status_selection_deleted")
    elif key == key_escape:
        if state.get("selected_anchor_ids") or state.get("selected_links"):
            state["selected_anchor_ids"] = []
            state["selected_links"] = []
            consumed = True
            status = tr(state["last_context"], "status_selection_cleared")

    return {
        "consumed": consumed,
        "status": status,
        "request_checkpoint": request_checkpoint,
    }


def handle_key_up(event, callbacks):
    state = callbacks["state"]
    event_has_shift = callbacks["event_has_shift"]
    key_shift = callbacks["key_shift"]

    key = int(event.get("key", 0))
    if key == key_shift or not event_has_shift(event):
        state["shift_down"] = False


def resolve_link_drag_mouse_up(callbacks):
    state = callbacks["state"]
    add_link = callbacks["add_link"]
    anchor_index_map = callbacks["anchor_index_map"]
    cleanup_links_and_selection = callbacks["cleanup_links_and_selection"]
    invalidate_curve_cache = callbacks["invalidate_curve_cache"]
    record_history_state = callbacks["record_history_state"]
    tr = callbacks["tr"]

    link_drag = state.get("link_drag", {})
    if not bool(link_drag.get("active", False)):
        return None

    source_id = int(link_drag.get("source_anchor_id", -1))
    target_id = int(link_drag.get("hover_anchor_id", -1))
    state["link_drag"] = {"active": False, "source_anchor_id": -1, "hover_anchor_id": -1, "x": 0.0, "y": 0.0}
    request_checkpoint = False

    if source_id > 0 and target_id > 0 and source_id != target_id:
        added = add_link(source_id, target_id)
        idx_map = anchor_index_map()
        src_i = idx_map.get(source_id, -1)
        dst_i = idx_map.get(target_id, -1)
        if added:
            cleanup_links_and_selection()
            invalidate_curve_cache()
            record_history_state(state["last_context"])
            request_checkpoint = True
            status = tr(state["last_context"], "status_link_connected", from_idx=src_i, to_idx=dst_i)
        else:
            status = tr(state["last_context"], "status_link_already_connected", from_idx=src_i, to_idx=dst_i)
    else:
        status = tr(state["last_context"], "status_link_cancelled")

    return {
        "consumed": True,
        "status": status,
        "request_checkpoint": request_checkpoint,
    }


def resolve_mouse_up_after_link_drag(callbacks):
    state = callbacks["state"]
    apply_box_selection = callbacks["apply_box_selection"]
    tr = callbacks["tr"]
    record_history_state = callbacks["record_history_state"]

    if bool(state.get("box_select", {}).get("active", False)):
        changed = apply_box_selection(state["last_context"])
        state["drag"] = {"mode": "", "index": -1}
        status = tr(state["last_context"], "status_box_selection_applied") if changed else tr(state["last_context"], "status_box_selection_cleared")
        return {
            "consumed": True,
            "status": status,
            "request_checkpoint": False,
            "should_return": True,
        }

    if state["drag"]["mode"]:
        state["drag"] = {"mode": "", "index": -1}
        record_history_state(state["last_context"])
        status = tr(state["last_context"], "curve_edit_applied")
        return {
            "consumed": True,
            "status": status,
            "request_checkpoint": True,
            "should_return": False,
        }

    state["drag"] = {"mode": "", "index": -1}
    return {
        "consumed": False,
        "status": "",
        "request_checkpoint": False,
        "should_return": False,
    }


def handle_cancel(state, tr_fn):
    state["drag"] = {"mode": "", "index": -1}
    state["link_drag"] = {"active": False, "source_anchor_id": -1, "hover_anchor_id": -1, "x": 0.0, "y": 0.0}
    return {
        "consumed": True,
        "status": tr_fn(state["last_context"], "interaction_cancelled"),
    }


def update_link_drag_on_mouse_move(x, y, callbacks):
    state = callbacks["state"]
    selection_enabled = callbacks["selection_enabled"]
    find_anchor_hit = callbacks["find_anchor_hit"]
    anchor_index_map = callbacks["anchor_index_map"]
    tr = callbacks["tr"]

    link_drag = state.get("link_drag", {})
    if not bool(link_drag.get("active", False)):
        return None

    link_drag["x"] = x
    link_drag["y"] = y
    hidx = find_anchor_hit(state["last_context"], x, y) if selection_enabled("anchors") else -1
    hover_id = -1
    if hidx >= 0:
        hit_id = int(state["anchors"][hidx].get("id", 0))
        if hit_id > 0 and hit_id != int(link_drag.get("source_anchor_id", -1)):
            hover_id = hit_id
    link_drag["hover_anchor_id"] = hover_id
    state["link_drag"] = link_drag

    if hover_id > 0:
        idx_map = anchor_index_map()
        src_i = idx_map.get(int(link_drag.get("source_anchor_id", -1)), -1)
        dst_i = idx_map.get(hover_id, -1)
        status = tr(state["last_context"], "status_link_drag_target", from_idx=src_i, to_idx=dst_i)
    else:
        status = tr(state["last_context"], "status_link_dragging")

    return {
        "consumed": True,
        "cursor": "pointing_hand",
        "status": status,
    }


def maybe_switch_anchor_drag_to_link_drag(x, y, shift_now, callbacks):
    state = callbacks["state"]
    tr = callbacks["tr"]

    if not shift_now:
        return None
    if state.get("drag", {}).get("mode") != "anchor":
        return None

    drag_idx = int(state.get("drag", {}).get("index", -1))
    if not (0 <= drag_idx < len(state.get("anchors", []))):
        return None

    source_id = int(state["anchors"][drag_idx].get("id", 0))
    if source_id <= 0:
        return None

    state["drag"] = {"mode": "", "index": -1}
    state["link_drag"] = {
        "active": True,
        "source_anchor_id": source_id,
        "hover_anchor_id": -1,
        "x": x,
        "y": y,
    }
    return {
        "consumed": True,
        "cursor": "pointing_hand",
        "status": tr(state["last_context"], "status_link_dragging"),
    }


def update_box_select_on_mouse_move(x, y, callbacks):
    state = callbacks["state"]
    tr = callbacks["tr"]

    box = state.get("box_select", {})
    if not bool(box.get("active", False)):
        return None

    box["end"] = [x, y]
    state["box_select"] = box
    return {
        "consumed": True,
        "cursor": "crosshair",
        "status": tr(state["last_context"], "status_box_selecting"),
    }


def handle_drag_edit_on_mouse_move(x, y, callbacks):
    state = callbacks["state"]
    canvas_to_chart = callbacks["canvas_to_chart"]
    snap_chart_point = callbacks["snap_chart_point"]
    enforce_anchor_time_order = callbacks["enforce_anchor_time_order"]
    enforce_handle_time_constraints = callbacks["enforce_handle_time_constraints"]
    set_anchor_in_abs_chart = callbacks["set_anchor_in_abs_chart"]
    set_anchor_out_abs_chart = callbacks["set_anchor_out_abs_chart"]
    invalidate_curve_cache = callbacks["invalidate_curve_cache"]
    mark_dirty = callbacks["mark_dirty"]
    tr = callbacks["tr"]

    mode = state["drag"]["mode"]
    idx = state["drag"]["index"]
    if not mode or not (0 <= idx < len(state["anchors"])):
        return None

    a = state["anchors"][idx]
    cursor = "arrow"
    if mode == "anchor":
        lane_x, beat = canvas_to_chart(state["last_context"], x, y)
        lane_x, beat = snap_chart_point(state["last_context"], lane_x, beat, snap_beat=True, snap_lane=False)
        a["lane_x"] = lane_x
        a["beat"] = beat
        enforce_anchor_time_order(idx, state["last_context"])
        enforce_handle_time_constraints(idx, state["last_context"])
        if idx > 0:
            enforce_handle_time_constraints(idx - 1, state["last_context"])
        cursor = "size_all"
    elif mode == "in":
        lane_x, beat = canvas_to_chart(state["last_context"], x, y)
        set_anchor_in_abs_chart(a, lane_x, beat, mirror=True)
        enforce_handle_time_constraints(idx, state["last_context"])
        cursor = "crosshair"
    elif mode == "out":
        lane_x, beat = canvas_to_chart(state["last_context"], x, y)
        set_anchor_out_abs_chart(a, lane_x, beat, mirror=True)
        enforce_handle_time_constraints(idx, state["last_context"])
        cursor = "crosshair"

    invalidate_curve_cache()
    mark_dirty(state["last_context"])
    return {
        "consumed": True,
        "cursor": cursor,
        "status": tr(state["last_context"], "editing_anchor", index=idx),
    }


def resolve_hover_cursor_on_mouse_move(x, y, callbacks):
    state = callbacks["state"]
    find_handle_hit = callbacks["find_handle_hit"]
    find_anchor_hit = callbacks["find_anchor_hit"]

    _hkind, hidx = find_handle_hit(state["last_context"], x, y)
    if hidx >= 0:
        return "pointing_hand"
    if find_anchor_hit(state["last_context"], x, y) >= 0:
        return "pointing_hand"
    return None


def analyze_mouse_down_context(x, y, event, callbacks):
    state = callbacks["state"]
    find_handle_hit = callbacks["find_handle_hit"]
    find_anchor_hit = callbacks["find_anchor_hit"]
    find_segment_hit = callbacks["find_segment_hit"]
    selection_enabled = callbacks["selection_enabled"]
    event_has_shift = callbacks["event_has_shift"]
    ctrl_modifier_mask = callbacks["ctrl_modifier_mask"]

    hkind, hidx = find_handle_hit(state["last_context"], x, y)
    aidx = find_anchor_hit(state["last_context"], x, y) if selection_enabled("anchors") else -1
    seg_hit = find_segment_hit(state["last_context"], x, y) if selection_enabled("segments") else None
    mods = int(event.get("modifiers", 0))
    ctrl = (mods & ctrl_modifier_mask) != 0
    shift = event_has_shift(event) or bool(state.get("shift_down", False))
    host_sel = state["last_context"].get("host_selection_tool", {}) if isinstance(state["last_context"], dict) else {}
    is_select_mode = bool(host_sel.get("is_select_mode", False)) if isinstance(host_sel, dict) else False
    notes_selectable = selection_enabled("notes")
    anchor_placement_enabled = bool(state.get("anchor_placement_enabled", False))
    host_select_passthrough = bool(is_select_mode and notes_selectable)
    blank_hit = hidx < 0 and aidx < 0 and seg_hit is None
    had_selection = bool(state.get("selected_anchor_ids")) or bool(state.get("selected_links"))

    return {
        "hkind": hkind,
        "hidx": hidx,
        "aidx": aidx,
        "seg_hit": seg_hit,
        "ctrl": ctrl,
        "shift": shift,
        "is_select_mode": is_select_mode,
        "notes_selectable": notes_selectable,
        "anchor_placement_enabled": anchor_placement_enabled,
        "host_select_passthrough": host_select_passthrough,
        "blank_hit": blank_hit,
        "had_selection": had_selection,
    }


def handle_mouse_down_right_button(x, y, callbacks):
    state = callbacks["state"]
    set_context_menu_links_for_hit = callbacks["set_context_menu_links_for_hit"]
    set_context_menu_links_for_hit(state["last_context"], x, y)
    return {"consumed": False}


def handle_mouse_down_left_prebranches(hkind, hidx, callbacks):
    state = callbacks["state"]
    tr = callbacks["tr"]
    host_select_passthrough = callbacks["host_select_passthrough"]
    blank_hit = callbacks["blank_hit"]
    had_selection = callbacks["had_selection"]

    if blank_hit and had_selection:
        state["selected_anchor_ids"] = []
        state["selected_links"] = []
        state["pending_connect_anchor_id"] = -1
        state["drag"] = {"mode": "", "index": -1}
        return {
            "handled": True,
            "consumed": True,
            "cursor": "arrow",
            "status": tr(state["last_context"], "status_selection_cleared"),
        }
    if host_select_passthrough:
        return {
            "handled": True,
            "consumed": False,
            "cursor": "arrow",
            "status": "",
        }
    if hidx >= 0:
        state["drag"] = {"mode": hkind, "index": hidx}
        return {
            "handled": True,
            "consumed": True,
            "cursor": "crosshair",
            "status": tr(
                state["last_context"],
                "dragging_handle",
                handle_kind=tr(state["last_context"], f"handle_kind_{hkind}"),
                index=hidx,
            ),
        }
    return {"handled": False}


def handle_mouse_down_anchor_hit(aidx, x, y, ts, ctrl, shift, callbacks):
    state = callbacks["state"]
    tr = callbacks["tr"]
    toggle_selected_anchor = callbacks["toggle_selected_anchor"]
    add_selected_anchor = callbacks["add_selected_anchor"]
    invalidate_curve_cache = callbacks["invalidate_curve_cache"]
    record_history_state = callbacks["record_history_state"]

    anchor_id = int(state["anchors"][aidx].get("id", 0))
    if shift:
        state["drag"] = {"mode": "", "index": -1}
        state["link_drag"] = {
            "active": True,
            "source_anchor_id": anchor_id,
            "hover_anchor_id": -1,
            "x": x,
            "y": y,
        }
        return {
            "consumed": True,
            "cursor": "pointing_hand",
            "status": tr(state["last_context"], "status_link_dragging"),
            "request_checkpoint": False,
            "immediate_return": True,
        }

    if ctrl:
        toggle_selected_anchor(anchor_id)
    else:
        add_selected_anchor(anchor_id)
        state["selected_links"] = []

    is_double = state["last_click_anchor"] == aidx and ts - state["last_click_ms"] <= 280
    state["last_click_anchor"] = aidx
    state["last_click_ms"] = ts
    if is_double:
        state["anchors"][aidx]["smooth"] = not bool(state["anchors"][aidx].get("smooth", True))
        invalidate_curve_cache()
        record_history_state(state["last_context"])
        mode = tr(state["last_context"], "mode_smooth") if state["anchors"][aidx]["smooth"] else tr(state["last_context"], "mode_corner")
        return {
            "consumed": True,
            "cursor": "arrow",
            "status": tr(state["last_context"], "anchor_mode_changed", index=aidx, mode=mode),
            "request_checkpoint": True,
            "immediate_return": False,
        }

    state["drag"] = {"mode": "anchor", "index": aidx}
    return {
        "consumed": True,
        "cursor": "size_all",
        "status": tr(state["last_context"], "dragging_anchor", index=aidx),
        "request_checkpoint": False,
        "immediate_return": False,
    }


def handle_mouse_down_segment_hit(seg_hit, ctrl, callbacks):
    state = callbacks["state"]
    tr = callbacks["tr"]
    toggle_selected_link = callbacks["toggle_selected_link"]
    set_single_selected_link = callbacks["set_single_selected_link"]
    selection_enabled = callbacks["selection_enabled"]

    if ctrl:
        toggle_selected_link(seg_hit[0], seg_hit[1])
    else:
        set_single_selected_link(seg_hit[0], seg_hit[1])

    if selection_enabled("anchors"):
        seg_anchor_ids = []
        for aid in (int(seg_hit[0]), int(seg_hit[1])):
            if aid > 0 and aid not in seg_anchor_ids:
                seg_anchor_ids.append(aid)
        if ctrl:
            merged = [int(v) for v in state.get("selected_anchor_ids", []) if int(v) > 0]
            for aid in seg_anchor_ids:
                if aid not in merged:
                    merged.append(aid)
            state["selected_anchor_ids"] = merged
        else:
            state["selected_anchor_ids"] = seg_anchor_ids
    else:
        state["selected_anchor_ids"] = []

    return {
        "consumed": True,
        "cursor": "pointing_hand",
        "status": tr(state["last_context"], "status_segment_selected"),
    }


def maybe_start_box_selection_on_mouse_down(x, y, ctrl, is_select_mode, notes_selectable, callbacks):
    state = callbacks["state"]
    tr = callbacks["tr"]

    if not ((ctrl or is_select_mode) and (not notes_selectable)):
        return None

    state["box_select"] = {"active": True, "start": [x, y], "end": [x, y], "append": bool(ctrl)}
    return {
        "consumed": True,
        "cursor": "arrow",
        "status": tr(state["last_context"], "status_box_selecting"),
    }
