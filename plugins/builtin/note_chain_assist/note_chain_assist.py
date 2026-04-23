import json
import math
import os
import sys
import time

LEFT_BUTTON = 1
RIGHT_BUTTON = 2
CTRL_MODIFIER_MASK = 0x04000000
KEY_Z = 0x5A
KEY_Y = 0x59
MAX_HISTORY = 128

STYLE_PRESETS = [
    [4, 8, 12, 16],
    [8, 8, 12, 16, 24],
    [4, 6, 8, 12, 16, 24],
    [12, 16, 24, 32],
]

STATE = {
    "anchors": [],
    "drag": {"mode": "", "index": -1},
    "last_context": {},
    "last_click_anchor": -1,
    "last_click_ms": 0,
    "style": {"denominators": [4, 8, 12, 16], "style_name": "balanced"},
    "project_path": "",
    "project_dirty": False,
    "history": [],
    "history_index": -1,
}


def _write(msg):
    line = json.dumps(msg, ensure_ascii=False) + "\n"
    sys.stdout.write(line)
    sys.stdout.flush()


def _respond(req_id, result):
    _write({"type": "response", "id": req_id, "result": result})


def _distance(x1, y1, x2, y2):
    return math.hypot(x1 - x2, y1 - y2)


def _clamp(v, lo, hi):
    return max(lo, min(hi, v))


def _capture_snapshot():
    return {
        "anchors": json.loads(json.dumps(STATE.get("anchors", []))),
        "style": json.loads(json.dumps(STATE.get("style", {"denominators": [4, 8, 12, 16], "style_name": "balanced"}))),
    }


def _restore_snapshot(snapshot):
    if not isinstance(snapshot, dict):
        return
    anchors = snapshot.get("anchors", [])
    style = snapshot.get("style", {"denominators": [4, 8, 12, 16], "style_name": "balanced"})
    STATE["anchors"] = anchors if isinstance(anchors, list) else []
    STATE["style"] = style if isinstance(style, dict) else {"denominators": [4, 8, 12, 16], "style_name": "balanced"}


def _push_history():
    snap = _capture_snapshot()
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))
    if idx >= 0 and idx < len(hist):
        if hist[idx] == snap:
            return
        hist = hist[: idx + 1]
    else:
        hist = []
    hist.append(snap)
    if len(hist) > MAX_HISTORY:
        hist = hist[-MAX_HISTORY:]
    STATE["history"] = hist
    STATE["history_index"] = len(hist) - 1


def _undo_history(context):
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))
    if not hist or idx <= 0:
        return False
    idx -= 1
    STATE["history_index"] = idx
    _restore_snapshot(hist[idx])
    _mark_dirty(context)
    return True


def _redo_history(context):
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))
    if not hist or idx >= len(hist) - 1:
        return False
    idx += 1
    STATE["history_index"] = idx
    _restore_snapshot(hist[idx])
    _mark_dirty(context)
    return True


def _safe_denominator_set(context):
    values = context.get("safe_denominators") if isinstance(context, dict) else None
    out = set()
    if isinstance(values, list):
        for v in values:
            try:
                iv = int(v)
            except Exception:
                continue
            if iv > 0:
                out.add(iv)
    if not out:
        out = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 192, 288}
    return out


def _sanitize_denominators(values, context):
    safe = _safe_denominator_set(context)
    cleaned = []
    if isinstance(values, list):
        for d in values:
            try:
                iv = int(d)
            except Exception:
                continue
            if iv in safe and iv > 0:
                cleaned.append(iv)
    if not cleaned:
        cleaned = [4, 8, 12, 16]
    return cleaned


def _first_style_path(context):
    if not isinstance(context, dict):
        return ""
    paths = context.get("style_library_paths")
    if not isinstance(paths, list):
        return ""
    for p in paths:
        if isinstance(p, str) and p.strip():
            folder = p.strip()
            os.makedirs(folder, exist_ok=True)
            return os.path.join(folder, "note_chain_default.nca_style.json")
    return ""


def _save_style(context):
    path = _first_style_path(context)
    if not path:
        return False
    payload = {
        "format_version": 1,
        "style_id": "default",
        "style_name": str(STATE.get("style", {}).get("style_name", "custom")),
        "denominators": list(STATE.get("style", {}).get("denominators", [4, 8, 12, 16])),
    }
    try:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)
        return True
    except Exception:
        return False


def _load_style(context):
    path = _first_style_path(context)
    if not path or not os.path.exists(path):
        return False
    try:
        with open(path, "r", encoding="utf-8") as f:
            payload = json.load(f)
        dens = _sanitize_denominators(payload.get("denominators"), context)
        STATE["style"] = {
            "style_name": str(payload.get("style_name", "imported")),
            "denominators": dens,
        }
        return True
    except Exception:
        return False


def _anchor_in_abs(a):
    return a["x"] + a["in"][0], a["y"] + a["in"][1]


def _anchor_out_abs(a):
    return a["x"] + a["out"][0], a["y"] + a["out"][1]


def _set_anchor_in_abs(a, x, y, mirror=False):
    a["in"] = [x - a["x"], y - a["y"]]
    if mirror and a.get("smooth", True):
        a["out"] = [-a["in"][0], -a["in"][1]]


def _set_anchor_out_abs(a, x, y, mirror=False):
    a["out"] = [x - a["x"], y - a["y"]]
    if mirror and a.get("smooth", True):
        a["in"] = [-a["out"][0], -a["out"][1]]


def _default_anchors():
    return []


def _find_anchor_hit(x, y, threshold=16.0):
    best = -1
    best_dist = 1e9
    for i, a in enumerate(STATE["anchors"]):
        d = _distance(x, y, a["x"], a["y"])
        if d < threshold and d < best_dist:
            best = i
            best_dist = d
    return best


def _find_handle_hit(x, y, threshold=14.0):
    best = ("", -1)
    best_dist = 1e9
    for i, a in enumerate(STATE["anchors"]):
        ix, iy = _anchor_in_abs(a)
        d1 = _distance(x, y, ix, iy)
        if d1 < threshold and d1 < best_dist:
            best = ("in", i)
            best_dist = d1
        ox, oy = _anchor_out_abs(a)
        d2 = _distance(x, y, ox, oy)
        if d2 < threshold and d2 < best_dist:
            best = ("out", i)
            best_dist = d2
    return best


def _cubic_point(a, b, c, d, t):
    u = 1.0 - t
    tt = t * t
    uu = u * u
    uuu = uu * u
    ttt = tt * t
    x = uuu * a[0] + 3 * uu * t * b[0] + 3 * u * tt * c[0] + ttt * d[0]
    y = uuu * a[1] + 3 * uu * t * b[1] + 3 * u * tt * c[1] + ttt * d[1]
    return x, y


def _sample_curve(samples_per_segment=24):
    anchors = STATE["anchors"]
    if len(anchors) < 2:
        return []

    pts = []
    for i in range(len(anchors) - 1):
        a0 = anchors[i]
        a1 = anchors[i + 1]
        p0 = (a0["x"], a0["y"])
        p1 = _anchor_out_abs(a0)
        p2 = _anchor_in_abs(a1)
        p3 = (a1["x"], a1["y"])
        for j in range(samples_per_segment + 1):
            t = j / float(samples_per_segment)
            if i > 0 and j == 0:
                continue
            pts.append(_cubic_point(p0, p1, p2, p3, t))
    return pts


def _save_project(path):
    if not isinstance(path, str) or not path.strip():
        return False
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        payload = {
            "format_version": 1,
            "anchors": STATE["anchors"],
            "style": STATE.get("style", {"denominators": [4, 8, 12, 16], "style_name": "balanced"}),
        }
        with open(path, "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)
        STATE["project_dirty"] = False
        return True
    except Exception:
        return False


def _load_project(path, context):
    if not isinstance(path, str) or not path.strip() or not os.path.exists(path):
        return False
    try:
        with open(path, "r", encoding="utf-8") as f:
            payload = json.load(f)
        anchors = payload.get("anchors")
        if isinstance(anchors, list):
            normalized = []
            for a in anchors:
                if not isinstance(a, dict):
                    continue
                normalized.append(
                    {
                        "x": float(a.get("x", 0.0)),
                        "y": float(a.get("y", 0.0)),
                        "in": [float((a.get("in") or [0.0, 0.0])[0]), float((a.get("in") or [0.0, 0.0])[1])],
                        "out": [float((a.get("out") or [0.0, 0.0])[0]), float((a.get("out") or [0.0, 0.0])[1])],
                        "smooth": bool(a.get("smooth", True)),
                    }
                )
            STATE["anchors"] = normalized
        style = payload.get("style")
        if isinstance(style, dict):
            STATE["style"] = {
                "style_name": str(style.get("style_name", "loaded")),
                "denominators": _sanitize_denominators(style.get("denominators"), context),
            }
        STATE["project_dirty"] = False
        return True
    except Exception:
        return False


def _ensure_project_context(context):
    path = ""
    if isinstance(context, dict):
        path = str(context.get("curve_project_path", "") or "").strip()
    if not path:
        return

    if STATE["project_path"] != path:
        if STATE["project_path"] and STATE["project_dirty"]:
            _save_project(STATE["project_path"])
        STATE["project_path"] = path
        if not _load_project(path, context):
            STATE["anchors"] = _default_anchors()
            STATE["project_dirty"] = True
        STATE["history"] = []
        STATE["history_index"] = -1
        _push_history()


def _mark_dirty(context):
    STATE["project_dirty"] = True
    if isinstance(context, dict):
        _ensure_project_context(context)
        if STATE["project_path"]:
            _save_project(STATE["project_path"])


def _build_overlay(context):
    _ensure_project_context(context)
    toggles = context.get("overlay_toggles", {}) if isinstance(context, dict) else {}
    if not bool(toggles.get("overlay_enabled", True)):
        return []

    show_preview = bool(toggles.get("preview", True))
    show_points = bool(toggles.get("control_points", True))
    show_handles = bool(toggles.get("handles", True))
    show_samples = bool(toggles.get("sample_points", True))
    show_labels = bool(toggles.get("labels", True))

    items = []
    anchors = STATE["anchors"]

    if show_preview and len(anchors) >= 2:
        sampled = _sample_curve(24)
        for i in range(len(sampled) - 1):
            a = sampled[i]
            b = sampled[i + 1]
            items.append(
                {
                    "kind": "line",
                    "x1": a[0],
                    "y1": a[1],
                    "x2": b[0],
                    "y2": b[1],
                    "color": "#33CCFF",
                    "width": 2.0,
                }
            )
        if show_samples:
            for x, y in sampled[::4]:
                items.append(
                    {
                        "kind": "rect",
                        "x": x - 2,
                        "y": y - 2,
                        "w": 4,
                        "h": 4,
                        "color": "#88FFFFFF",
                        "fill_color": "#88FFFFFF",
                        "width": 1.0,
                    }
                )

    for i, a in enumerate(anchors):
        is_drag_anchor = STATE["drag"]["mode"] == "anchor" and STATE["drag"]["index"] == i

        if show_handles:
            ix, iy = _anchor_in_abs(a)
            ox, oy = _anchor_out_abs(a)
            items.append(
                {
                    "kind": "line",
                    "x1": a["x"],
                    "y1": a["y"],
                    "x2": ix,
                    "y2": iy,
                    "color": "#66A0A0A0",
                    "width": 1.0,
                }
            )
            items.append(
                {
                    "kind": "line",
                    "x1": a["x"],
                    "y1": a["y"],
                    "x2": ox,
                    "y2": oy,
                    "color": "#66A0A0A0",
                    "width": 1.0,
                }
            )
            items.append(
                {
                    "kind": "rect",
                    "x": ix - 4,
                    "y": iy - 4,
                    "w": 8,
                    "h": 8,
                    "color": "#FFFFFFFF",
                    "fill_color": "#AAEEAA55",
                    "width": 1.0,
                }
            )
            items.append(
                {
                    "kind": "rect",
                    "x": ox - 4,
                    "y": oy - 4,
                    "w": 8,
                    "h": 8,
                    "color": "#FFFFFFFF",
                    "fill_color": "#AAEEAA55",
                    "width": 1.0,
                }
            )

        if show_points:
            items.append(
                {
                    "kind": "rect",
                    "x": a["x"] - 6,
                    "y": a["y"] - 6,
                    "w": 12,
                    "h": 12,
                    "color": "#FFFFFF",
                    "fill_color": "#AA0077FF" if is_drag_anchor else "#AA00A3FF",
                    "width": 1.5,
                }
            )

        if show_labels:
            mode = "S" if a.get("smooth", True) else "C"
            items.append(
                {
                    "kind": "text",
                    "x1": a["x"] + 8,
                    "y1": a["y"] - 8,
                    "text": f"A{i}({mode})",
                    "color": "#FFFFFF",
                    "font_px": 12,
                }
            )

    if show_labels:
        dens = STATE.get("style", {}).get("denominators", [4, 8, 12, 16])
        label = "Dens: " + "/".join(str(d) for d in dens)
        items.append({"kind": "text", "x1": 16, "y1": 18, "text": label, "color": "#DDEEFF", "font_px": 12})

    return items


def _reset_anchors(context):
    _push_history()
    STATE["anchors"] = _default_anchors()
    STATE["drag"] = {"mode": "", "index": -1}
    _mark_dirty(context)


def _append_anchor(context, x, y):
    x, y = _snap_anchor_canvas_point(context, x, y)
    if not STATE["anchors"]:
        STATE["anchors"].append({"x": x, "y": y, "in": [-30.0, 0.0], "out": [30.0, 0.0], "smooth": True})
        return len(STATE["anchors"]) - 1

    prev = STATE["anchors"][-1]
    dx = x - prev["x"]
    dy = y - prev["y"]
    length = max(20.0, min(80.0, math.hypot(dx, dy) * 0.25))
    angle = math.atan2(dy, dx)
    ox = math.cos(angle) * length
    oy = math.sin(angle) * length
    STATE["anchors"].append({"x": x, "y": y, "in": [-ox, -oy], "out": [ox, oy], "smooth": True})
    return len(STATE["anchors"]) - 1


def _float_beat_to_triplet(beat, den):
    den = max(1, int(den))
    if beat < 0.0:
        beat = 0.0
    ticks = int(round(beat * den))
    beat_num = ticks // den
    num = ticks % den
    if num < 0:
        num += den
        beat_num -= 1
    return beat_num, num, den


def _canvas_to_beat(context, y):
    ch = max(1.0, float(context.get("canvas_height", 800)))
    scroll = float(context.get("scroll_beat", 0.0))
    vr = max(1e-6, float(context.get("visible_beat_range", 8.0)))
    vertical_flip = bool(context.get("vertical_flip", False))
    if vertical_flip:
        return scroll + ((ch - y) / ch) * vr
    return scroll + (y / ch) * vr


def _beat_to_canvas_y(context, beat):
    ch = max(1.0, float(context.get("canvas_height", 800)))
    scroll = float(context.get("scroll_beat", 0.0))
    vr = max(1e-6, float(context.get("visible_beat_range", 8.0)))
    vertical_flip = bool(context.get("vertical_flip", False))
    t = (beat - scroll) / vr
    if vertical_flip:
        return ch - t * ch
    return t * ch


def _snap_anchor_canvas_point(context, x, y):
    cw = float(context.get("canvas_width", 1200))
    ch = max(1.0, float(context.get("canvas_height", 800)))
    l = float(context.get("left_margin", 0.0))
    r = float(context.get("right_margin", 0.0))
    lane_w = max(1, int(context.get("lane_width", 512)))
    available = max(1.0, cw - l - r)

    grid_snap = bool(context.get("grid_snap", False))
    grid_div = max(1, int(context.get("grid_division", 8)))
    time_div = max(1, int(context.get("time_division", 1)))

    nx = _clamp((x - l) / available, 0.0, 1.0)
    lane_x = int(round(nx * lane_w))
    if grid_snap and grid_div > 0:
        lane_x = int(round((lane_x / float(lane_w)) * grid_div) * (lane_w / float(grid_div)))
    lane_x = int(_clamp(lane_x, 0, lane_w))
    x_snapped = l + (lane_x / float(lane_w)) * available

    beat = _canvas_to_beat(context, y)
    beat = round(beat * time_div) / float(time_div)
    y_snapped = _beat_to_canvas_y(context, beat)
    y_snapped = _clamp(y_snapped, 0.0, ch)
    return x_snapped, y_snapped


def _canvas_to_note(context, x, y, den):
    cw = float(context.get("canvas_width", 1200))
    l = float(context.get("left_margin", 0.0))
    r = float(context.get("right_margin", 0.0))
    lane_w = max(1, int(context.get("lane_width", 512)))
    available = max(1.0, cw - l - r)
    grid_snap = bool(context.get("grid_snap", False))
    grid_div = max(1, int(context.get("grid_division", 8)))
    time_div = max(1, int(context.get("time_division", 1)))

    nx = _clamp((x - l) / available, 0.0, 1.0)
    lane_x = int(round(nx * lane_w))
    if grid_snap and grid_div > 0:
        lane_x = int(round((lane_x / float(lane_w)) * grid_div) * (lane_w / float(grid_div)))
    lane_x = int(_clamp(lane_x, 0, lane_w))
    beat = _canvas_to_beat(context, y)
    beat = round(beat * time_div) / float(time_div)

    b, n, d = _float_beat_to_triplet(beat, den)
    return {"beat": [b, n, d], "x": lane_x, "type": 0}


def _build_batch_from_curve(context):
    if not isinstance(context, dict):
        return {"add": [], "remove": [], "move": []}

    sampled = _sample_curve(16)
    if not sampled:
        return {"add": [], "remove": [], "move": []}

    dens = _sanitize_denominators(STATE.get("style", {}).get("denominators", [4]), context)

    out = []
    seen = set()
    for i, (x, y) in enumerate(sampled[::2]):
        den = int(dens[i % len(dens)])
        note = _canvas_to_note(context, x, y, den)
        beat = note["beat"]
        key = (beat[0], beat[1], beat[2], note["x"])
        if key in seen:
            continue
        seen.add(key)
        out.append(note)

    return {"add": out, "remove": [], "move": []}


def _cycle_density_style(context):
    _push_history()
    current = STATE.get("style", {}).get("denominators", [4, 8, 12, 16])
    normalized = tuple(_sanitize_denominators(current, context))
    target_index = 0
    for i, preset in enumerate(STYLE_PRESETS):
        if tuple(_sanitize_denominators(preset, context)) == normalized:
            target_index = (i + 1) % len(STYLE_PRESETS)
            break
    next_values = _sanitize_denominators(STYLE_PRESETS[target_index], context)
    STATE["style"] = {
        "style_name": f"preset_{target_index + 1}",
        "denominators": next_values,
    }
    _mark_dirty(context)
    return next_values


def _handle_canvas_input(payload):
    context = payload.get("context", {}) if isinstance(payload, dict) else {}
    event = payload.get("event", {}) if isinstance(payload, dict) else {}
    STATE["last_context"] = context if isinstance(context, dict) else {}
    _ensure_project_context(STATE["last_context"])

    et = str(event.get("type", ""))
    x = float(event.get("x", 0.0))
    y = float(event.get("y", 0.0))
    button = int(event.get("button", 0))
    ts = int(event.get("timestamp_ms", int(time.time() * 1000)))

    consumed = False
    status = ""
    cursor = "arrow"

    if et == "mouse_down":
        hkind, hidx = _find_handle_hit(x, y)
        aidx = _find_anchor_hit(x, y)

        if button == RIGHT_BUTTON:
            consumed = True
            status = "Right click is reserved (no delete)"
        elif button == LEFT_BUTTON:
            if hidx >= 0:
                _push_history()
                STATE["drag"] = {"mode": hkind, "index": hidx}
                consumed = True
                cursor = "crosshair"
                status = f"Dragging {hkind} handle A{hidx}"
            elif aidx >= 0:
                is_double = (STATE["last_click_anchor"] == aidx and ts - STATE["last_click_ms"] <= 280)
                STATE["last_click_anchor"] = aidx
                STATE["last_click_ms"] = ts
                if is_double:
                    _push_history()
                    STATE["anchors"][aidx]["smooth"] = not bool(STATE["anchors"][aidx].get("smooth", True))
                    _mark_dirty(STATE["last_context"])
                    consumed = True
                    mode = "smooth" if STATE["anchors"][aidx]["smooth"] else "corner"
                    status = f"Anchor A{aidx} -> {mode}"
                else:
                    _push_history()
                    STATE["drag"] = {"mode": "anchor", "index": aidx}
                    consumed = True
                    cursor = "size_all"
                    status = f"Dragging anchor A{aidx}"
            else:
                _push_history()
                new_idx = _append_anchor(STATE["last_context"], x, y)
                STATE["drag"] = {"mode": "anchor", "index": new_idx}
                _mark_dirty(STATE["last_context"])
                consumed = True
                cursor = "size_all"
                status = f"Anchor A{new_idx} added"

    elif et == "mouse_move":
        mode = STATE["drag"]["mode"]
        idx = STATE["drag"]["index"]
        if mode and 0 <= idx < len(STATE["anchors"]):
            a = STATE["anchors"][idx]
            if mode == "anchor":
                # Keep handle vectors unchanged when moving anchor.
                # Handles are stored as local vectors relative to anchor.
                a["x"] = x
                a["y"] = y
                cursor = "size_all"
            elif mode == "in":
                _set_anchor_in_abs(a, x, y, mirror=True)
                cursor = "crosshair"
            elif mode == "out":
                _set_anchor_out_abs(a, x, y, mirror=True)
                cursor = "crosshair"
            _mark_dirty(STATE["last_context"])
            consumed = True
            status = f"Editing A{idx}"
        else:
            hkind, hidx = _find_handle_hit(x, y)
            if hidx >= 0:
                cursor = "pointing_hand"
            elif _find_anchor_hit(x, y) >= 0:
                cursor = "pointing_hand"

    elif et == "mouse_up":
        if STATE["drag"]["mode"]:
            status = "Curve edit applied"
            consumed = True
        STATE["drag"] = {"mode": "", "index": -1}

    elif et == "wheel":
        _push_history()
        direction = float(event.get("wheel_delta", 0.0))
        dens = list(_sanitize_denominators(STATE.get("style", {}).get("denominators", [4, 8, 12, 16]), context))
        shift = 1 if direction > 0 else -1
        dens = dens[shift:] + dens[:shift]
        STATE["style"]["denominators"] = dens
        _mark_dirty(STATE["last_context"])
        consumed = True
        status = "Density sequence rotated"

    elif et == "cancel":
        STATE["drag"] = {"mode": "", "index": -1}
        consumed = True
        status = "Interaction cancelled"
    elif et == "key_down":
        key = int(event.get("key", 0))
        mods = int(event.get("modifiers", 0))
        ctrl = (mods & CTRL_MODIFIER_MASK) != 0
        if ctrl and key == KEY_Z:
            if _undo_history(STATE["last_context"]):
                consumed = True
                status = "Undo curve edit"
        elif ctrl and key == KEY_Y:
            if _redo_history(STATE["last_context"]):
                consumed = True
                status = "Redo curve edit"

    if et in ("mouse_down", "mouse_up") and not consumed:
        # In tool mode, canvas click ownership belongs to this plugin.
        consumed = True
    if et == "mouse_move" and not consumed and int(event.get("buttons", 0)) != 0:
        consumed = True

    return {
        "consumed": consumed,
        "overlay": _build_overlay(STATE["last_context"]),
        "cursor": cursor,
        "status_text": status,
        "preview_batch_edit": {"add": [], "remove": [], "move": []},
    }


def _list_tool_actions():
    return [
        {
            "action_id": "commit_curve_to_notes",
            "title": "Commit Curve",
            "description": "Generate normal notes from current pen curve",
            "placement": "top_toolbar",
            "requires_undo_snapshot": True,
        },
        {
            "action_id": "commit_curve_to_notes_sidebar",
            "title": "Commit Curve",
            "description": "Generate normal notes from current pen curve",
            "placement": "left_sidebar",
            "requires_undo_snapshot": True,
        },
        {
            "action_id": "reset_curve",
            "title": "Reset Curve",
            "description": "Reset all anchors and handles",
            "placement": "top_toolbar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "cycle_density_style",
            "title": "Cycle Density",
            "description": "Rotate predefined denominator sequence styles",
            "placement": "top_toolbar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "export_style_preset",
            "title": "Export Style",
            "description": "Export current style",
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "import_style_preset",
            "title": "Import Style",
            "description": "Import style from shared style file",
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        },
    ]


def _run_tool_action(payload):
    action_id = str((payload or {}).get("action_id", ""))
    context = (payload or {}).get("context", {}) or {}
    _ensure_project_context(context)
    if action_id == "reset_curve":
        _reset_anchors(context)
        return True
    if action_id in ("commit_curve_to_notes", "commit_curve_to_notes_sidebar"):
        if STATE["project_path"] and STATE["project_dirty"]:
            _save_project(STATE["project_path"])
        return True
    if action_id == "cycle_density_style":
        _cycle_density_style(context)
        return True
    if action_id == "export_style_preset":
        return _save_style(context)
    if action_id == "import_style_preset":
        _push_history()
        ok = _load_style(context)
        if ok:
            _mark_dirty(context)
        return ok
    return False


def _workspace_config(_payload):
    return {
        "workspace_id": "note_chain_workspace",
        "docking_supported": True,
        "tab_merge_supported": True,
        "default_layout": "advanced",
        "window_group": "note_chain",
    }


def _build_batch_edit(payload):
    action_id = str((payload or {}).get("action_id", ""))
    context = (payload or {}).get("context", {}) or {}
    _ensure_project_context(context)
    if action_id not in ("commit_curve_to_notes", "commit_curve_to_notes_sidebar"):
        return {"add": [], "remove": [], "move": []}
    return _build_batch_from_curve(context)


def run_one_shot(action_id):
    known = {
        "reset_curve",
        "commit_curve_to_notes",
        "commit_curve_to_notes_sidebar",
        "cycle_density_style",
        "export_style_preset",
        "import_style_preset",
    }
    return 0 if action_id in known else 1


def run_plugin_loop():
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            msg = json.loads(raw)
        except Exception:
            continue

        mtype = msg.get("type")
        if mtype == "notify":
            if msg.get("event") == "shutdown":
                if STATE["project_path"] and STATE["project_dirty"]:
                    _save_project(STATE["project_path"])
                break
            continue

        if mtype != "request":
            continue

        req_id = msg.get("id", str(int(time.time() * 1000)))
        method = msg.get("method", "")
        payload = msg.get("payload", {}) or {}

        if method == "listToolActions":
            _respond(req_id, _list_tool_actions())
        elif method == "runToolAction":
            _respond(req_id, bool(_run_tool_action(payload)))
        elif method == "listCanvasOverlays":
            _respond(req_id, _build_overlay(payload if isinstance(payload, dict) else {}))
        elif method == "handleCanvasInput":
            _respond(req_id, _handle_canvas_input(payload))
        elif method == "getPanelWorkspaceConfig":
            _respond(req_id, _workspace_config(payload))
        elif method == "buildBatchEdit":
            _respond(req_id, _build_batch_edit(payload))
        else:
            _respond(req_id, False)


def main():
    args = sys.argv[1:]
    if "--run-tool-action" in args:
        i = args.index("--run-tool-action")
        action_id = args[i + 1] if i + 1 < len(args) else ""
        raise SystemExit(run_one_shot(action_id))

    if "--plugin" in args:
        run_plugin_loop()
        return

    print("Use --plugin for protocol mode", file=sys.stderr)


if __name__ == "__main__":
    main()
