import json
import math
import locale
import os
import sys
import time

LEFT_BUTTON = 1
RIGHT_BUTTON = 2

TRANSLATIONS = {
    "anchor_deleted": {"en": "Anchor A{index} deleted", "zh": "已删除锚点 A{index}", "ja": "アンカー A{index} を削除しました"},
    "dragging_handle": {"en": "Dragging {kind} handle A{index}", "zh": "正在拖动 A{index} 的{kind}控制柄", "ja": "A{index} の{kind}ハンドルをドラッグ中"},
    "handle_in": {"en": "in", "zh": "入侧", "ja": "内側"},
    "handle_out": {"en": "out", "zh": "出侧", "ja": "外側"},
    "anchor_mode_changed": {"en": "Anchor A{index} -> {mode}", "zh": "锚点 A{index} 已切换为{mode}", "ja": "アンカー A{index} を{mode}に切り替えました"},
    "mode_smooth": {"en": "smooth", "zh": "平滑", "ja": "スムーズ"},
    "mode_corner": {"en": "corner", "zh": "折角", "ja": "コーナー"},
    "dragging_anchor": {"en": "Dragging anchor A{index}", "zh": "正在拖动锚点 A{index}", "ja": "アンカー A{index} をドラッグ中"},
    "anchor_added": {"en": "Anchor A{index} added", "zh": "已添加锚点 A{index}", "ja": "アンカー A{index} を追加しました"},
    "editing_anchor": {"en": "Editing A{index}", "zh": "正在编辑 A{index}", "ja": "A{index} を編集中"},
    "curve_edit_applied": {"en": "Curve edit applied", "zh": "曲线编辑已应用", "ja": "曲線編集を適用しました"},
    "interaction_cancelled": {"en": "Interaction cancelled", "zh": "交互已取消", "ja": "操作をキャンセルしました"},
    "action_reset": {"en": "Reset Curve", "zh": "重置曲线", "ja": "曲線をリセット"},
    "action_reset_desc": {"en": "Reset pen anchors/handles", "zh": "重置钢笔锚点与控制柄", "ja": "ペンのアンカーとハンドルをリセットします"},
    "action_commit": {"en": "Commit Curve -> Notes", "zh": "提交曲线 -> 音符", "ja": "曲線を確定 -> ノート"},
    "action_commit_desc": {"en": "Generate normal notes from current pen curve", "zh": "根据当前钢笔曲线生成普通音符", "ja": "現在のペン曲線から通常ノートを生成します"},
    "action_export": {"en": "Export Style Preset", "zh": "导出样式预设", "ja": "スタイルプリセットを書き出す"},
    "action_export_desc": {"en": "Export denominators style to shared style file", "zh": "将分母样式导出到共享样式文件", "ja": "分母スタイルを共有スタイルファイルへ書き出します"},
    "action_import": {"en": "Import Style Preset", "zh": "导入样式预设", "ja": "スタイルプリセットを読み込む"},
    "action_import_desc": {"en": "Import denominators style from shared style file", "zh": "从共享样式文件导入分母样式", "ja": "共有スタイルファイルから分母スタイルを読み込みます"},
}


def _normalize_lang(value, default="en"):
    if not isinstance(value, str) or not value.strip():
        return default
    lower = value.strip().lower()
    if lower.startswith("zh"):
        return "zh"
    if lower.startswith("ja"):
        return "ja"
    if lower.startswith("en"):
        return "en"
    return default


def _detect_lang(context=None, default="en"):
    if isinstance(context, dict):
        locale_value = context.get("locale")
        language_value = context.get("language")
        if isinstance(locale_value, str) and locale_value.strip():
            return _normalize_lang(locale_value, default)
        if isinstance(language_value, str) and language_value.strip():
            return _normalize_lang(language_value, default)

    locale_env = os.environ.get("MALODY_LOCALE", "")
    language_env = os.environ.get("MALODY_LANGUAGE", "")
    if locale_env:
        return _normalize_lang(locale_env, default)
    if language_env:
        return _normalize_lang(language_env, default)

    sys_locale = locale.getlocale()[0]
    if sys_locale:
        return _normalize_lang(sys_locale, default)
    return default


def tr(context, key, **kwargs):
    lang = _detect_lang(context=context)
    table = TRANSLATIONS.get(key, {})
    text = table.get(lang, table.get("en", key))
    return text.format(**kwargs)

STATE = {
    "anchors": [
        {"x": 220.0, "y": 220.0, "in": [-40.0, 0.0], "out": [40.0, 0.0], "smooth": True},
        {"x": 420.0, "y": 340.0, "in": [-40.0, -20.0], "out": [40.0, 20.0], "smooth": True},
        {"x": 620.0, "y": 260.0, "in": [-40.0, 0.0], "out": [40.0, 0.0], "smooth": True},
    ],
    "drag": {"mode": "", "index": -1},
    "last_context": {},
    "last_click_anchor": -1,
    "last_click_ms": 0,
    "style": {"denominators": [4, 8, 12, 16]},
    "project_path": "",
    "project_dirty": False,
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
            return os.path.join(folder, "default.style_tbd.json")
    return ""


def _save_style(context):
    path = _first_style_path(context)
    if not path:
        return False
    payload = {
        "format_version": 1,
        "style_id": "default",
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
        dens = payload.get("denominators")
        if not isinstance(dens, list):
            return False
        cleaned = []
        for d in dens:
            try:
                di = int(d)
            except Exception:
                continue
            if di > 0:
                cleaned.append(di)
        if not cleaned:
            return False
        STATE["style"] = {"denominators": cleaned}
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
    return [
        {"x": 220.0, "y": 220.0, "in": [-40.0, 0.0], "out": [40.0, 0.0], "smooth": True},
        {"x": 420.0, "y": 340.0, "in": [-40.0, -20.0], "out": [40.0, 20.0], "smooth": True},
        {"x": 620.0, "y": 260.0, "in": [-40.0, 0.0], "out": [40.0, 0.0], "smooth": True},
    ]


def _find_anchor_hit(x, y, threshold=12.0):
    best = -1
    best_dist = 1e9
    for i, a in enumerate(STATE["anchors"]):
        d = _distance(x, y, a["x"], a["y"])
        if d < threshold and d < best_dist:
            best = i
            best_dist = d
    return best


def _find_handle_hit(x, y, threshold=10.0):
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
            "style": STATE.get("style", {"denominators": [4, 8, 12, 16]}),
        }
        with open(path, "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)
        STATE["project_dirty"] = False
        return True
    except Exception:
        return False


def _load_project(path):
    if not isinstance(path, str) or not path.strip() or not os.path.exists(path):
        return False
    try:
        with open(path, "r", encoding="utf-8") as f:
            payload = json.load(f)
        anchors = payload.get("anchors")
        if isinstance(anchors, list) and len(anchors) >= 2:
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
            if len(normalized) >= 2:
                STATE["anchors"] = normalized
        style = payload.get("style")
        if isinstance(style, dict):
            dens = style.get("denominators")
            if isinstance(dens, list) and dens:
                cleaned = []
                for d in dens:
                    try:
                        di = int(d)
                    except Exception:
                        continue
                    if di > 0:
                        cleaned.append(di)
                if cleaned:
                    STATE["style"] = {"denominators": cleaned}
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
        if not _load_project(path):
            STATE["anchors"] = _default_anchors()
            STATE["project_dirty"] = True


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

    return items


def _reset_anchors(context):
    STATE["anchors"] = _default_anchors()
    STATE["drag"] = {"mode": "", "index": -1}
    _mark_dirty(context)


def _append_anchor(x, y):
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


def _canvas_to_note(context, x, y, den):
    cw = float(context.get("canvas_width", 1200))
    ch = max(1.0, float(context.get("canvas_height", 800)))
    l = float(context.get("left_margin", 0.0))
    r = float(context.get("right_margin", 0.0))
    lane_w = int(context.get("lane_width", 512))
    available = max(1.0, cw - l - r)

    nx = _clamp((x - l) / available, 0.0, 1.0)
    lane_x = int(round(nx * lane_w))

    scroll = float(context.get("scroll_beat", 0.0))
    vr = max(1e-6, float(context.get("visible_beat_range", 8.0)))
    vertical_flip = bool(context.get("vertical_flip", False))

    if vertical_flip:
        base_y = ch
        beat = scroll + ((base_y - y) / ch) * vr
    else:
        beat = scroll + (y / ch) * vr

    b, n, d = _float_beat_to_triplet(beat, den)
    return {"beat": [b, n, d], "x": lane_x, "type": 0}


def _build_batch_from_curve(context):
    if not isinstance(context, dict):
        return {"add": [], "remove": [], "move": []}

    sampled = _sample_curve(16)
    if not sampled:
        return {"add": [], "remove": [], "move": []}

    dens = STATE.get("style", {}).get("denominators", [4])
    if not dens:
        dens = [4]

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

        if button == RIGHT_BUTTON and aidx >= 0:
            if len(STATE["anchors"]) > 1:
                STATE["anchors"].pop(aidx)
                STATE["drag"] = {"mode": "", "index": -1}
                _mark_dirty(STATE["last_context"])
                consumed = True
                status = tr(STATE["last_context"], "anchor_deleted", index=aidx)
        elif button == LEFT_BUTTON:
            if hidx >= 0:
                STATE["drag"] = {"mode": hkind, "index": hidx}
                consumed = True
                cursor = "crosshair"
                status = tr(STATE["last_context"], "dragging_handle", kind=tr(STATE["last_context"], f"handle_{hkind}"), index=hidx)
            elif aidx >= 0:
                is_double = (STATE["last_click_anchor"] == aidx and ts - STATE["last_click_ms"] <= 280)
                STATE["last_click_anchor"] = aidx
                STATE["last_click_ms"] = ts
                if is_double:
                    STATE["anchors"][aidx]["smooth"] = not bool(STATE["anchors"][aidx].get("smooth", True))
                    _mark_dirty(STATE["last_context"])
                    consumed = True
                    mode = tr(STATE["last_context"], "mode_smooth") if STATE["anchors"][aidx]["smooth"] else tr(STATE["last_context"], "mode_corner")
                    status = tr(STATE["last_context"], "anchor_mode_changed", index=aidx, mode=mode)
                else:
                    STATE["drag"] = {"mode": "anchor", "index": aidx}
                    consumed = True
                    cursor = "size_all"
                    status = tr(STATE["last_context"], "dragging_anchor", index=aidx)
            else:
                new_idx = _append_anchor(x, y)
                STATE["drag"] = {"mode": "anchor", "index": new_idx}
                _mark_dirty(STATE["last_context"])
                consumed = True
                cursor = "size_all"
                status = tr(STATE["last_context"], "anchor_added", index=new_idx)

    elif et == "mouse_move":
        mode = STATE["drag"]["mode"]
        idx = STATE["drag"]["index"]
        if mode and 0 <= idx < len(STATE["anchors"]):
            a = STATE["anchors"][idx]
            if mode == "anchor":
                dx = x - a["x"]
                dy = y - a["y"]
                a["x"] = x
                a["y"] = y
                a["in"][0] += dx
                a["in"][1] += dy
                a["out"][0] += dx
                a["out"][1] += dy
                cursor = "size_all"
            elif mode == "in":
                _set_anchor_in_abs(a, x, y, mirror=True)
                cursor = "crosshair"
            elif mode == "out":
                _set_anchor_out_abs(a, x, y, mirror=True)
                cursor = "crosshair"
            _mark_dirty(STATE["last_context"])
            consumed = True
            status = tr(STATE["last_context"], "editing_anchor", index=idx)
        else:
            hkind, hidx = _find_handle_hit(x, y)
            if hidx >= 0:
                cursor = "pointing_hand"
            elif _find_anchor_hit(x, y) >= 0:
                cursor = "pointing_hand"

    elif et == "mouse_up":
        if STATE["drag"]["mode"]:
            status = tr(STATE["last_context"], "curve_edit_applied")
            consumed = True
        STATE["drag"] = {"mode": "", "index": -1}

    elif et == "cancel":
        STATE["drag"] = {"mode": "", "index": -1}
        consumed = True
        status = tr(STATE["last_context"], "interaction_cancelled")

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
            "action_id": "reset_demo_points",
            "title": tr(STATE.get("last_context", {}), "action_reset"),
            "description": tr(STATE.get("last_context", {}), "action_reset_desc"),
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "commit_curve_to_notes",
            "title": tr(STATE.get("last_context", {}), "action_commit"),
            "description": tr(STATE.get("last_context", {}), "action_commit_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": True,
        },
        {
            "action_id": "export_style_preset",
            "title": tr(STATE.get("last_context", {}), "action_export"),
            "description": tr(STATE.get("last_context", {}), "action_export_desc"),
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "import_style_preset",
            "title": tr(STATE.get("last_context", {}), "action_import"),
            "description": tr(STATE.get("last_context", {}), "action_import_desc"),
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        },
    ]


def _run_tool_action(payload):
    action_id = str((payload or {}).get("action_id", ""))
    context = (payload or {}).get("context", {}) or {}
    _ensure_project_context(context)
    if action_id == "reset_demo_points":
        _reset_anchors(context)
        return True
    if action_id == "commit_curve_to_notes":
        if STATE["project_path"] and STATE["project_dirty"]:
            _save_project(STATE["project_path"])
        return True
    if action_id == "export_style_preset":
        return _save_style(context)
    if action_id == "import_style_preset":
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
    if action_id != "commit_curve_to_notes":
        return {"add": [], "remove": [], "move": []}
    return _build_batch_from_curve(context)


def run_one_shot(action_id):
    if action_id in ("reset_demo_points", "commit_curve_to_notes", "export_style_preset", "import_style_preset"):
        return 0
    return 1


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
