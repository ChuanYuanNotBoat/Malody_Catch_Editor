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
