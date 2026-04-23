import json
import math
import os
import sys
import time

LEFT_BUTTON = 1
RIGHT_BUTTON = 2
CTRL_MODIFIER_MASK = 0x04000000
SHIFT_MODIFIER_MASK = 0x02000000
KEY_Z = 0x5A
KEY_Y = 0x59
KEY_A = 0x41
MAX_HISTORY = 128
SERIALIZE_DEN = 288

STYLE_PRESETS = [
    [4, 8, 12, 16],
    [8, 8, 12, 16, 24],
    [4, 6, 8, 12, 16, 24],
    [12, 16, 24, 32],
]

STATE = {
    # Anchors are stored in chart space:
    # lane_x in [0..lane_width], beat in timeline beats.
    "anchors": [],
    "drag": {"mode": "", "index": -1},
    "last_context": {},
    "last_click_anchor": -1,
    "last_click_ms": 0,
    "style": {"denominators": [4, 8, 12, 16], "style_name": "balanced"},
    "anchor_placement_enabled": False,
    "project_path": "",
    "project_dirty": False,
    "history": [],
    "history_index": -1,
    "last_shortcut_key": 0,
    "last_shortcut_ms": 0,
}


def _clone(v):
    return json.loads(json.dumps(v))


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
    return {"anchors": _clone(STATE.get("anchors", [])), "style": _clone(STATE.get("style", {}))}


def _restore_snapshot(snapshot):
    if not isinstance(snapshot, dict):
        return
    STATE["anchors"] = _clone(snapshot.get("anchors", [])) if isinstance(snapshot.get("anchors"), list) else []
    style = snapshot.get("style")
    STATE["style"] = _clone(style) if isinstance(style, dict) else {"denominators": [4, 8, 12, 16], "style_name": "balanced"}


def _push_history():
    snap = _capture_snapshot()
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))

    if 0 <= idx < len(hist):
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


def _float_to_triplet(beat, den):
    den = max(1, int(den))
    ticks = int(round(float(beat) * den))
    beat_num = ticks // den
    num = ticks % den
    if num < 0:
        num += den
        beat_num -= 1
    return [int(beat_num), int(num), int(den)]


def _triplet_to_float(tri):
    if not isinstance(tri, list) or len(tri) != 3:
        return None
    try:
        b = int(tri[0])
        n = int(tri[1])
        d = int(tri[2])
    except Exception:
        return None
    if d <= 0:
        return None
    return float(b) + float(n) / float(d)


def _context_dims(context):
    cw = float(context.get("canvas_width", 1200.0))
    ch = max(1.0, float(context.get("canvas_height", 800.0)))
    l = float(context.get("left_margin", 0.0))
    r = float(context.get("right_margin", 0.0))
    lane_w = max(1.0, float(context.get("lane_width", 512.0)))
    available = max(1.0, cw - l - r)
    return cw, ch, l, r, lane_w, available


def _canvas_to_chart(context, x, y):
    _, ch, l, _, lane_w, available = _context_dims(context)
    nx = _clamp((float(x) - l) / available, 0.0, 1.0)
    lane_x = nx * lane_w

    scroll = float(context.get("scroll_beat", 0.0))
    vr = max(1e-6, float(context.get("visible_beat_range", 8.0)))
    vertical_flip = bool(context.get("vertical_flip", False))
    if vertical_flip:
        beat = scroll + ((ch - float(y)) / ch) * vr
    else:
        beat = scroll + (float(y) / ch) * vr
    return lane_x, beat


def _chart_to_canvas(context, lane_x, beat):
    _, ch, l, _, lane_w, available = _context_dims(context)
    lane_x = _clamp(float(lane_x), 0.0, lane_w)
    x = l + (lane_x / lane_w) * available

    scroll = float(context.get("scroll_beat", 0.0))
    vr = max(1e-6, float(context.get("visible_beat_range", 8.0)))
    vertical_flip = bool(context.get("vertical_flip", False))
    t = (float(beat) - scroll) / vr
    y = ch - t * ch if vertical_flip else t * ch
    return x, y


def _snap_chart_point(context, lane_x, beat, snap_beat=True):
    lane_w = max(1.0, float(context.get("lane_width", 512.0)))
    grid_snap = bool(context.get("grid_snap", False))
    grid_div = max(1, int(context.get("grid_division", 8)))
    time_div = max(1, int(context.get("time_division", 1)))

    lane_x = _clamp(float(lane_x), 0.0, lane_w)
    if grid_snap and grid_div > 0:
        lane_x = round((lane_x / lane_w) * grid_div) * (lane_w / float(grid_div))

    if snap_beat:
        beat = round(float(beat) * time_div) / float(time_div)
    return lane_x, beat


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
        STATE["style"] = {"style_name": str(payload.get("style_name", "imported")), "denominators": dens}
        return True
    except Exception:
        return False


def _anchor_in_abs_chart(a):
    return a["lane_x"] + a["in"][0], a["beat"] + a["in"][1]


def _anchor_out_abs_chart(a):
    return a["lane_x"] + a["out"][0], a["beat"] + a["out"][1]


def _set_anchor_in_abs_chart(a, lane_x, beat, mirror=False):
    a["in"] = [float(lane_x) - a["lane_x"], float(beat) - a["beat"]]
    if mirror and a.get("smooth", True):
        a["out"] = [-a["in"][0], -a["in"][1]]


def _set_anchor_out_abs_chart(a, lane_x, beat, mirror=False):
    a["out"] = [float(lane_x) - a["lane_x"], float(beat) - a["beat"]]
    if mirror and a.get("smooth", True):
        a["in"] = [-a["out"][0], -a["out"][1]]


def _default_anchors():
    return []


def _find_anchor_hit(context, cx, cy, threshold=16.0):
    best = -1
    best_dist = 1e9
    for i, a in enumerate(STATE["anchors"]):
        ax, ay = _chart_to_canvas(context, a["lane_x"], a["beat"])
        d = _distance(cx, cy, ax, ay)
        if d < threshold and d < best_dist:
            best = i
            best_dist = d
    return best


def _find_handle_hit(context, cx, cy, threshold=14.0):
    best = ("", -1)
    best_dist = 1e9
    for i, a in enumerate(STATE["anchors"]):
        ilx, ib = _anchor_in_abs_chart(a)
        olx, ob = _anchor_out_abs_chart(a)
        ix, iy = _chart_to_canvas(context, ilx, ib)
        ox, oy = _chart_to_canvas(context, olx, ob)

        d1 = _distance(cx, cy, ix, iy)
        if d1 < threshold and d1 < best_dist:
            best = ("in", i)
            best_dist = d1
        d2 = _distance(cx, cy, ox, oy)
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


def _sample_curve_chart(samples_per_segment=24):
    anchors = STATE["anchors"]
    if len(anchors) < 2:
        return []

    pts = []
    for i in range(len(anchors) - 1):
        a0 = anchors[i]
        a1 = anchors[i + 1]
        p0 = (a0["lane_x"], a0["beat"])
        p1 = _anchor_out_abs_chart(a0)
        p2 = _anchor_in_abs_chart(a1)
        p3 = (a1["lane_x"], a1["beat"])
        for j in range(samples_per_segment + 1):
            t = j / float(samples_per_segment)
            if i > 0 and j == 0:
                continue
            pts.append(_cubic_point(p0, p1, p2, p3, t))
    return pts


def _normalize_samples_by_beat(samples):
    if not samples:
        return []
    ordered = sorted(samples, key=lambda p: (p[1], p[0]))
    out = []
    eps = 1e-6
    for lane_x, beat in ordered:
        bx = float(lane_x)
        bb = float(beat)
        if not out:
            out.append([bx, bb])
            continue
        if abs(bb - out[-1][1]) <= eps:
            out[-1][0] = (out[-1][0] + bx) * 0.5
        else:
            out.append([bx, bb])
    return out


def _lane_x_at_beat(samples_by_beat, beat):
    if not samples_by_beat:
        return 0.0
    if beat <= samples_by_beat[0][1]:
        return samples_by_beat[0][0]
    if beat >= samples_by_beat[-1][1]:
        return samples_by_beat[-1][0]

    for i in range(len(samples_by_beat) - 1):
        a = samples_by_beat[i]
        b = samples_by_beat[i + 1]
        if a[1] <= beat <= b[1]:
            db = b[1] - a[1]
            if abs(db) <= 1e-9:
                return a[0]
            t = (beat - a[1]) / db
            return a[0] + (b[0] - a[0]) * t
    return samples_by_beat[-1][0]


def _enforce_anchor_time_order(index, context):
    if not (0 <= index < len(STATE["anchors"])):
        return
    den = max(1, int(context.get("time_division", 4)))
    eps = 1.0 / float(den * 4)
    cur = STATE["anchors"][index]
    if index > 0:
        prev_b = STATE["anchors"][index - 1]["beat"]
        cur["beat"] = max(cur["beat"], prev_b + eps)
    if index + 1 < len(STATE["anchors"]):
        next_b = STATE["anchors"][index + 1]["beat"]
        cur["beat"] = min(cur["beat"], next_b - eps)


def _enforce_handle_time_constraints(index, context):
    if not (0 <= index < len(STATE["anchors"])):
        return
    den = max(1, int(context.get("time_division", 4)))
    eps = 1.0 / float(den * 4)
    a = STATE["anchors"][index]

    if index + 1 < len(STATE["anchors"]):
        next_b = STATE["anchors"][index + 1]["beat"]
        max_out = max(0.0, next_b - a["beat"] - eps)
        a["out"][1] = _clamp(a["out"][1], 0.0, max_out)

    if index > 0:
        prev_b = STATE["anchors"][index - 1]["beat"]
        min_in = min(0.0, prev_b - a["beat"] + eps)
        a["in"][1] = _clamp(a["in"][1], min_in, 0.0)


def _serialize_anchor(a):
    return {
        "lane_x": float(a.get("lane_x", 0.0)),
        "beat": _float_to_triplet(float(a.get("beat", 0.0)), SERIALIZE_DEN),
        "in": {
            "lane_dx": float((a.get("in") or [0.0, 0.0])[0]),
            "beat_delta": _float_to_triplet(float((a.get("in") or [0.0, 0.0])[1]), SERIALIZE_DEN),
        },
        "out": {
            "lane_dx": float((a.get("out") or [0.0, 0.0])[0]),
            "beat_delta": _float_to_triplet(float((a.get("out") or [0.0, 0.0])[1]), SERIALIZE_DEN),
        },
        "smooth": bool(a.get("smooth", True)),
    }


def _deserialize_anchor(raw, context):
    if not isinstance(raw, dict):
        return None

    # New chart-space format.
    if "lane_x" in raw:
        lane_x = float(raw.get("lane_x", 0.0))
        beat = _triplet_to_float(raw.get("beat"))
        if beat is None:
            beat = float(raw.get("beat_float", 0.0))

        in_raw = raw.get("in")
        out_raw = raw.get("out")

        if isinstance(in_raw, dict):
            in_dx = float(in_raw.get("lane_dx", 0.0))
            in_db = _triplet_to_float(in_raw.get("beat_delta"))
            if in_db is None:
                in_db = float(in_raw.get("beat_delta_float", 0.0))
        else:
            in_arr = in_raw if isinstance(in_raw, list) else [0.0, 0.0]
            in_dx = float(in_arr[0])
            in_db = float(in_arr[1])

        if isinstance(out_raw, dict):
            out_dx = float(out_raw.get("lane_dx", 0.0))
            out_db = _triplet_to_float(out_raw.get("beat_delta"))
            if out_db is None:
                out_db = float(out_raw.get("beat_delta_float", 0.0))
        else:
            out_arr = out_raw if isinstance(out_raw, list) else [0.0, 0.0]
            out_dx = float(out_arr[0])
            out_db = float(out_arr[1])

        lane_x, beat = _snap_chart_point(context, lane_x, beat)
        return {
            "lane_x": lane_x,
            "beat": beat,
            "in": [in_dx, in_db],
            "out": [out_dx, out_db],
            "smooth": bool(raw.get("smooth", True)),
        }

    # Legacy canvas-space format migration.
    if "x" in raw and "y" in raw:
        lane_x, beat = _canvas_to_chart(context, float(raw.get("x", 0.0)), float(raw.get("y", 0.0)))
        lane_x, beat = _snap_chart_point(context, lane_x, beat)
        return {
            "lane_x": lane_x,
            "beat": beat,
            "in": [float((raw.get("in") or [0.0, 0.0])[0]), float((raw.get("in") or [0.0, 0.0])[1])],
            "out": [float((raw.get("out") or [0.0, 0.0])[0]), float((raw.get("out") or [0.0, 0.0])[1])],
            "smooth": bool(raw.get("smooth", True)),
        }

    return None


def _save_project(path, context=None):
    if not isinstance(path, str) or not path.strip():
        return False
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        payload = {
            "format_version": 2,
            "coordinate_space": "chart",
            "anchors": [_serialize_anchor(a) for a in STATE.get("anchors", [])],
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

        anchors_raw = payload.get("anchors")
        parsed = []
        if isinstance(anchors_raw, list):
            for raw in anchors_raw:
                a = _deserialize_anchor(raw, context)
                if a is not None:
                    parsed.append(a)
        STATE["anchors"] = parsed

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
            _save_project(STATE["project_path"], context)
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
            _save_project(STATE["project_path"], context)


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
        sampled = _sample_curve_chart(24)
        for i in range(len(sampled) - 1):
            a = _chart_to_canvas(context, sampled[i][0], sampled[i][1])
            b = _chart_to_canvas(context, sampled[i + 1][0], sampled[i + 1][1])
            items.append({"kind": "line", "x1": a[0], "y1": a[1], "x2": b[0], "y2": b[1], "color": "#33CCFF", "width": 2.0})

        if show_samples:
            for lane_x, beat in sampled[::4]:
                sx, sy = _chart_to_canvas(context, lane_x, beat)
                items.append({"kind": "rect", "x": sx - 2, "y": sy - 2, "w": 4, "h": 4, "color": "#88FFFFFF", "fill_color": "#88FFFFFF", "width": 1.0})

    for i, a in enumerate(anchors):
        is_drag_anchor = STATE["drag"]["mode"] == "anchor" and STATE["drag"]["index"] == i
        ax, ay = _chart_to_canvas(context, a["lane_x"], a["beat"])

        if show_handles:
            ilx, ib = _anchor_in_abs_chart(a)
            olx, ob = _anchor_out_abs_chart(a)
            ix, iy = _chart_to_canvas(context, ilx, ib)
            ox, oy = _chart_to_canvas(context, olx, ob)

            items.append({"kind": "line", "x1": ax, "y1": ay, "x2": ix, "y2": iy, "color": "#66A0A0A0", "width": 1.0})
            items.append({"kind": "line", "x1": ax, "y1": ay, "x2": ox, "y2": oy, "color": "#66A0A0A0", "width": 1.0})
            items.append({"kind": "rect", "x": ix - 4, "y": iy - 4, "w": 8, "h": 8, "color": "#FFFFFFFF", "fill_color": "#AAEEAA55", "width": 1.0})
            items.append({"kind": "rect", "x": ox - 4, "y": oy - 4, "w": 8, "h": 8, "color": "#FFFFFFFF", "fill_color": "#AAEEAA55", "width": 1.0})

        if show_points:
            items.append({
                "kind": "rect",
                "x": ax - 6,
                "y": ay - 6,
                "w": 12,
                "h": 12,
                "color": "#FFFFFF",
                "fill_color": "#AA0077FF" if is_drag_anchor else "#AA00A3FF",
                "width": 1.5,
            })

        if show_labels:
            mode = "S" if a.get("smooth", True) else "C"
            items.append({"kind": "text", "x1": ax + 8, "y1": ay - 8, "text": f"A{i}({mode})", "color": "#FFFFFF", "font_px": 12})

    if show_labels:
        dens = STATE.get("style", {}).get("denominators", [4, 8, 12, 16])
        override_den = int(context.get("plugin_time_division_override", 0) or 0) if isinstance(context, dict) else 0
        effective_den = override_den if override_den > 0 else max(1, int(context.get("time_division", 4))) if isinstance(context, dict) else 4
        anchor_mode = "ON" if bool(STATE.get("anchor_placement_enabled", False)) else "OFF"
        label = "Dens: " + "/".join(str(d) for d in dens) + f" | Place: 1/{effective_den} | Anchor: {anchor_mode}"
        items.append({"kind": "text", "x1": 16, "y1": 18, "text": label, "color": "#DDEEFF", "font_px": 12})

    return items


def _reset_anchors(context):
    _push_history()
    STATE["anchors"] = _default_anchors()
    STATE["drag"] = {"mode": "", "index": -1}
    _mark_dirty(context)


def _append_anchor(context, cx, cy):
    lane_x, beat = _canvas_to_chart(context, cx, cy)
    lane_x, beat = _snap_chart_point(context, lane_x, beat)

    if not STATE["anchors"]:
        STATE["anchors"].append({"lane_x": lane_x, "beat": beat, "in": [-16.0, 0.0], "out": [16.0, 0.0], "smooth": True})
        return len(STATE["anchors"]) - 1

    prev = STATE["anchors"][-1]
    dl = lane_x - prev["lane_x"]
    db = beat - prev["beat"]
    # Split axis scaling to avoid overly weak handles on lane axis.
    ol = _clamp(dl * 0.25, -96.0, 96.0)
    ob = _clamp(db * 0.25, -2.0, 2.0)
    STATE["anchors"].append({"lane_x": lane_x, "beat": beat, "in": [-ol, -ob], "out": [ol, ob], "smooth": True})
    idx = len(STATE["anchors"]) - 1
    _enforce_anchor_time_order(idx, context)
    _enforce_handle_time_constraints(idx, context)
    _enforce_handle_time_constraints(idx - 1, context)
    return idx


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


def _chart_to_note(context, lane_x, beat, den):
    lane_w = max(1.0, float(context.get("lane_width", 512.0)))
    # Keep beat precision in the selected denominator space; only snap X to grid.
    lane_x, beat = _snap_chart_point(context, lane_x, beat, snap_beat=False)
    lane_x = _clamp(lane_x, 0.0, lane_w)

    b, n, d = _float_beat_to_triplet(beat, den)
    return {"beat": [b, n, d], "x": int(round(lane_x)), "type": 0}


def _build_batch_from_curve(context):
    if not isinstance(context, dict):
        return {"add": [], "remove": [], "move": []}

    sampled = _sample_curve_chart(32)
    if len(sampled) < 2:
        return {"add": [], "remove": [], "move": []}

    override_den = int(context.get("plugin_time_division_override", 0) or 0)
    current_den = max(1, override_den if override_den > 0 else int(context.get("time_division", 4)))
    samples_by_beat = _normalize_samples_by_beat(sampled)
    if len(samples_by_beat) < 2:
        return {"add": [], "remove": [], "move": []}

    start_beat = min(STATE["anchors"][0]["beat"], STATE["anchors"][-1]["beat"])
    end_beat = max(STATE["anchors"][0]["beat"], STATE["anchors"][-1]["beat"])
    start_tick = int(round(start_beat * current_den))
    end_tick = int(round(end_beat * current_den))
    if end_tick < start_tick:
        start_tick, end_tick = end_tick, start_tick

    out = []
    for tick in range(start_tick, end_tick + 1):
        beat = float(tick) / float(current_den)
        lane_x = _lane_x_at_beat(samples_by_beat, beat)
        note = _chart_to_note(context, lane_x, beat, current_den)
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
    STATE["style"] = {"style_name": f"preset_{target_index + 1}", "denominators": next_values}
    _mark_dirty(context)


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
        hkind, hidx = _find_handle_hit(STATE["last_context"], x, y)
        aidx = _find_anchor_hit(STATE["last_context"], x, y)

        if button == RIGHT_BUTTON:
            consumed = False
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
                consumed = True
                if not bool(STATE.get("anchor_placement_enabled", False)):
                    status = "Anchor placement is OFF (toggle Anchor Place first)"
                else:
                    _push_history()
                    new_idx = _append_anchor(STATE["last_context"], x, y)
                    STATE["drag"] = {"mode": "anchor", "index": new_idx}
                    _mark_dirty(STATE["last_context"])
                    cursor = "size_all"
                    status = f"Anchor A{new_idx} added"

    elif et == "mouse_move":
        mode = STATE["drag"]["mode"]
        idx = STATE["drag"]["index"]
        if mode and 0 <= idx < len(STATE["anchors"]):
            a = STATE["anchors"][idx]
            if mode == "anchor":
                lane_x, beat = _canvas_to_chart(STATE["last_context"], x, y)
                lane_x, beat = _snap_chart_point(STATE["last_context"], lane_x, beat)
                a["lane_x"] = lane_x
                a["beat"] = beat
                _enforce_anchor_time_order(idx, STATE["last_context"])
                _enforce_handle_time_constraints(idx, STATE["last_context"])
                if idx > 0:
                    _enforce_handle_time_constraints(idx - 1, STATE["last_context"])
                cursor = "size_all"
            elif mode == "in":
                lane_x, beat = _canvas_to_chart(STATE["last_context"], x, y)
                _set_anchor_in_abs_chart(a, lane_x, beat, mirror=True)
                _enforce_handle_time_constraints(idx, STATE["last_context"])
                cursor = "crosshair"
            elif mode == "out":
                lane_x, beat = _canvas_to_chart(STATE["last_context"], x, y)
                _set_anchor_out_abs_chart(a, lane_x, beat, mirror=True)
                _enforce_handle_time_constraints(idx, STATE["last_context"])
                cursor = "crosshair"
            _mark_dirty(STATE["last_context"])
            consumed = True
            status = f"Editing A{idx}"
        else:
            hkind, hidx = _find_handle_hit(STATE["last_context"], x, y)
            if hidx >= 0:
                cursor = "pointing_hand"
            elif _find_anchor_hit(STATE["last_context"], x, y) >= 0:
                cursor = "pointing_hand"

    elif et == "mouse_up":
        if STATE["drag"]["mode"]:
            status = "Curve edit applied"
            consumed = True
        STATE["drag"] = {"mode": "", "index": -1}

    elif et == "cancel":
        STATE["drag"] = {"mode": "", "index": -1}
        consumed = True
        status = "Interaction cancelled"

    elif et == "key_down":
        key = int(event.get("key", 0))
        mods = int(event.get("modifiers", 0))
        ctrl = (mods & CTRL_MODIFIER_MASK) != 0
        shift = (mods & SHIFT_MODIFIER_MASK) != 0
        if not ctrl and key == KEY_A:
            STATE["anchor_placement_enabled"] = not bool(STATE.get("anchor_placement_enabled", False))
            consumed = True
            status = "Anchor placement enabled" if STATE["anchor_placement_enabled"] else "Anchor placement disabled"
            _mark_dirty(STATE["last_context"])

        is_undo = ctrl and key == KEY_Z and not shift
        is_redo = ctrl and (key == KEY_Y or (key == KEY_Z and shift))
        if is_undo or is_redo:
            last_key = int(STATE.get("last_shortcut_key", 0))
            last_ts = int(STATE.get("last_shortcut_ms", 0))
            if key == last_key and ts - last_ts < 70:
                consumed = True
            else:
                STATE["last_shortcut_key"] = key
                STATE["last_shortcut_ms"] = ts
                if is_undo and _undo_history(STATE["last_context"]):
                    consumed = True
                    status = "Curve undo"
                elif is_redo and _redo_history(STATE["last_context"]):
                    consumed = True
                    status = "Curve redo"

    # In tool mode, left-button canvas operations belong to plugin.
    if et in ("mouse_down", "mouse_up") and not consumed and button == LEFT_BUTTON:
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
            "action_id": "toggle_anchor_placement",
            "title": "Anchor Place",
            "description": "Toggle anchor placement mode to prevent misclick additions",
            "placement": "top_toolbar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "undo_curve_edit",
            "title": "Undo Curve",
            "description": "Undo latest curve anchor/handle edit",
            "placement": "top_toolbar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "redo_curve_edit",
            "title": "Redo Curve",
            "description": "Redo latest curve anchor/handle edit",
            "placement": "top_toolbar",
            "requires_undo_snapshot": False,
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
            _save_project(STATE["project_path"], context)
        return True
    if action_id == "toggle_anchor_placement":
        STATE["anchor_placement_enabled"] = not bool(STATE.get("anchor_placement_enabled", False))
        _mark_dirty(context)
        return True
    if action_id == "cycle_density_style":
        _cycle_density_style(context)
        return True
    if action_id == "undo_curve_edit":
        return _undo_history(context)
    if action_id == "redo_curve_edit":
        return _redo_history(context)
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
        "toggle_anchor_placement",
        "undo_curve_edit",
        "redo_curve_edit",
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
                    _save_project(STATE["project_path"], STATE.get("last_context", {}))
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
