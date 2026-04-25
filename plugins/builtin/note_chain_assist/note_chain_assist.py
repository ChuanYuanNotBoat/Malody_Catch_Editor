import json
import locale
import math
import os
import shutil
import sys
import time

LEFT_BUTTON = 1
RIGHT_BUTTON = 2
CTRL_MODIFIER_MASK = 0x04000000
KEY_A = 0x41
KEY_DELETE = 0x01000007
MAX_HISTORY = 128
SERIALIZE_DEN = 288
CURVE_CHECKPOINT_PREFIX = "Plugin Curve Edit"

STYLE_PRESETS = [
    [4, 8, 12, 16],
    [8, 8, 12, 16, 24],
    [4, 6, 8, 12, 16, 24],
    [12, 16, 24, 32],
]

TRANSLATIONS = {
    "anchor_mode_smooth": {"en": "S", "zh": "平", "ja": "滑"},
    "anchor_mode_corner": {"en": "C", "zh": "角", "ja": "角"},
    "overlay_summary": {
        "en": "Dens: {dens} | Place: 1/{den} | Anchor: {anchor_mode}",
        "zh": "密度: {dens} | 放置: 1/{den} | 锚点: {anchor_mode}",
        "ja": "密度: {dens} | 配置: 1/{den} | アンカー: {anchor_mode}",
    },
    "anchor_on": {"en": "ON", "zh": "开", "ja": "ON"},
    "anchor_off": {"en": "OFF", "zh": "关", "ja": "OFF"},
    "dragging_handle": {
        "en": "Dragging {handle_kind} handle A{index}",
        "zh": "正在拖动 A{index} 的{handle_kind}控制柄",
        "ja": "A{index} の{handle_kind}ハンドルをドラッグ中",
    },
    "handle_kind_in": {"en": "in", "zh": "入侧", "ja": "内側"},
    "handle_kind_out": {"en": "out", "zh": "出侧", "ja": "外側"},
    "anchor_mode_changed": {
        "en": "Anchor A{index} -> {mode}",
        "zh": "锚点 A{index} 已切换为{mode}",
        "ja": "アンカー A{index} を{mode}に切り替えました",
    },
    "mode_smooth": {"en": "smooth", "zh": "平滑", "ja": "スムーズ"},
    "mode_corner": {"en": "corner", "zh": "折角", "ja": "コーナー"},
    "dragging_anchor": {"en": "Dragging anchor A{index}", "zh": "正在拖动锚点 A{index}", "ja": "アンカー A{index} をドラッグ中"},
    "anchor_place_off_hint": {
        "en": "Anchor placement is OFF (toggle Anchor Place first)",
        "zh": "锚点放置已关闭，请先切换“放置锚点”",
        "ja": "アンカー配置は OFF です（先に「アンカー配置」を切り替えてください）",
    },
    "anchor_added": {"en": "Anchor A{index} added", "zh": "已添加锚点 A{index}", "ja": "アンカー A{index} を追加しました"},
    "editing_anchor": {"en": "Editing A{index}", "zh": "正在编辑 A{index}", "ja": "A{index} を編集中"},
    "curve_edit_applied": {"en": "Curve edit applied", "zh": "曲线编辑已应用", "ja": "曲線編集を適用しました"},
    "interaction_cancelled": {"en": "Interaction cancelled", "zh": "交互已取消", "ja": "操作をキャンセルしました"},
    "anchor_enabled": {"en": "Anchor placement enabled", "zh": "已启用锚点放置", "ja": "アンカー配置を有効にしました"},
    "anchor_disabled": {"en": "Anchor placement disabled", "zh": "已禁用锚点放置", "ja": "アンカー配置を無効にしました"},
    "curve_undo": {"en": "Curve undo", "zh": "已撤销曲线编辑", "ja": "曲線編集を元に戻しました"},
    "curve_redo": {"en": "Curve redo", "zh": "已重做曲线编辑", "ja": "曲線編集をやり直しました"},
    "action_commit_curve": {"en": "Commit Curve", "zh": "提交曲线", "ja": "曲線を確定"},
    "action_commit_curve_desc": {
        "en": "Generate normal notes from current pen curve",
        "zh": "根据当前钢笔曲线生成普通 note",
        "ja": "現在のペン曲線から通常ノートを生成します",
    },
    "action_anchor_place": {"en": "Anchor Place", "zh": "放置锚点", "ja": "アンカー配置"},
    "action_anchor_place_desc": {
        "en": "Toggle anchor placement mode to prevent misclick additions",
        "zh": "切换锚点放置模式，避免误触新增锚点",
        "ja": "アンカー配置モードを切り替えて誤操作での追加を防ぎます",
    },
    "action_undo_curve": {"en": "Undo Curve", "zh": "撤销曲线", "ja": "曲線を元に戻す"},
    "action_undo_curve_desc": {
        "en": "Undo latest curve anchor/handle edit",
        "zh": "撤销最近一次曲线锚点或控制柄编辑",
        "ja": "直前の曲線アンカーまたはハンドル編集を取り消します",
    },
    "action_redo_curve": {"en": "Redo Curve", "zh": "重做曲线", "ja": "曲線をやり直す"},
    "action_redo_curve_desc": {
        "en": "Redo latest curve anchor/handle edit",
        "zh": "重做最近一次曲线锚点或控制柄编辑",
        "ja": "直前の曲線アンカーまたはハンドル編集をやり直します",
    },
    "action_reset_curve": {"en": "Reset Curve", "zh": "重置曲线", "ja": "曲線をリセット"},
    "action_reset_curve_desc": {
        "en": "Reset all anchors and handles",
        "zh": "重置全部锚点与控制柄",
        "ja": "すべてのアンカーとハンドルをリセットします",
    },
    "action_cycle_density": {"en": "Cycle Density", "zh": "切换密度", "ja": "密度を切替"},
    "action_cycle_density_desc": {
        "en": "Rotate predefined denominator sequence styles",
        "zh": "轮换预设的分母序列样式",
        "ja": "定義済みの分母シーケンススタイルを切り替えます",
    },
    "action_export_style": {"en": "Export Style", "zh": "导出样式", "ja": "スタイルを書き出す"},
    "action_export_style_desc": {"en": "Export current style", "zh": "导出当前样式", "ja": "現在のスタイルを書き出します"},
    "action_import_style": {"en": "Import Style", "zh": "导入样式", "ja": "スタイルを読み込む"},
    "action_import_style_desc": {
        "en": "Import style from shared style file",
        "zh": "从共享样式文件导入样式",
        "ja": "共有スタイルファイルからスタイルを読み込みます",
    },
}

STATE = {
    # Anchors are stored in chart space:
    # lane_x in [0..lane_width], beat in timeline beats.
    "anchors": [],
    "links": [],
    "drag": {"mode": "", "index": -1},
    "selected_anchor_ids": [],
    "selected_links": [],
    "selection_targets": {"anchors": True, "segments": True, "notes": False},
    "box_select": {"active": False, "start": [0.0, 0.0], "end": [0.0, 0.0], "append": False},
    "pending_connect_anchor_id": -1,
    "next_anchor_id": 1,
    "last_context": {},
    "last_click_anchor": -1,
    "last_click_ms": 0,
    "style": {"denominators": [4, 8, 12, 16], "style_name": "balanced"},
    "anchor_placement_enabled": False,
    "project_path": "",
    "project_dirty": False,
    "history": [],
    "history_index": -1,
    "lang": "en",
    "curve_revision": 0,
    "curve_samples_cache": {},
}


def _clone(v):
    return json.loads(json.dumps(v))


def _write(msg):
    line = json.dumps(msg, ensure_ascii=False) + "\n"
    if hasattr(sys.stdout, "buffer"):
        sys.stdout.buffer.write(line.encode("utf-8"))
        sys.stdout.buffer.flush()
    else:
        sys.stdout.write(line)
        sys.stdout.flush()


def _respond(req_id, result):
    _write({"type": "response", "id": req_id, "result": result})


def _distance(x1, y1, x2, y2):
    return math.hypot(x1 - x2, y1 - y2)


def _clamp(v, lo, hi):
    return max(lo, min(hi, v))


def normalize_lang(value, default="en"):
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


def detect_lang(default="en", context=None):
    if isinstance(context, dict):
        locale_value = context.get("locale")
        language_value = context.get("language")
        if isinstance(locale_value, str) and locale_value.strip():
            return normalize_lang(locale_value, default)
        if isinstance(language_value, str) and language_value.strip():
            return normalize_lang(language_value, default)

    state_lang = str(STATE.get("lang", "") or "").strip()
    if state_lang:
        return normalize_lang(state_lang, default)

    locale_env = os.environ.get("MALODY_LOCALE", "")
    language_env = os.environ.get("MALODY_LANGUAGE", "")
    if locale_env:
        return normalize_lang(locale_env, default)
    if language_env:
        return normalize_lang(language_env, default)

    sys_locale = locale.getlocale()[0]
    if sys_locale:
        return normalize_lang(sys_locale, default)
    return default


def tr(context, key, **kwargs):
    lang = detect_lang(context=context)
    table = TRANSLATIONS.get(key, {})
    text = table.get(lang, table.get("en", key))
    return text.format(**kwargs)


def _configure_stdio_utf8():
    for stream in (sys.stdin, sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass


def _capture_snapshot():
    return {
        "anchors": _clone(STATE.get("anchors", [])),
        "links": _clone(STATE.get("links", [])),
        "style": _clone(STATE.get("style", {})),
        "selection_targets": _clone(STATE.get("selection_targets", {"anchors": True, "segments": True, "notes": False})),
        "anchor_placement_enabled": bool(STATE.get("anchor_placement_enabled", False)),
        "next_anchor_id": int(STATE.get("next_anchor_id", 1)),
        "pending_connect_anchor_id": int(STATE.get("pending_connect_anchor_id", -1)),
    }


def _restore_snapshot(snapshot):
    if not isinstance(snapshot, dict):
        return
    STATE["anchors"] = _clone(snapshot.get("anchors", [])) if isinstance(snapshot.get("anchors"), list) else []
    STATE["links"] = _clone(snapshot.get("links", [])) if isinstance(snapshot.get("links"), list) else []
    style = snapshot.get("style")
    STATE["style"] = _clone(style) if isinstance(style, dict) else {"denominators": [4, 8, 12, 16], "style_name": "balanced"}
    targets = snapshot.get("selection_targets")
    if isinstance(targets, dict):
        STATE["selection_targets"] = {
            "anchors": bool(targets.get("anchors", True)),
            "segments": bool(targets.get("segments", True)),
            "notes": bool(targets.get("notes", False)),
        }
    else:
        STATE["selection_targets"] = {"anchors": True, "segments": True, "notes": False}
    STATE["anchor_placement_enabled"] = bool(snapshot.get("anchor_placement_enabled", False))
    STATE["drag"] = {"mode": "", "index": -1}
    STATE["selected_anchor_ids"] = []
    STATE["selected_links"] = []
    STATE["box_select"] = {"active": False, "start": [0.0, 0.0], "end": [0.0, 0.0], "append": False}
    STATE["pending_connect_anchor_id"] = int(snapshot.get("pending_connect_anchor_id", -1))
    STATE["next_anchor_id"] = max(1, int(snapshot.get("next_anchor_id", 1)))
    _ensure_anchor_ids()
    _cleanup_links_and_selection()
    _invalidate_curve_cache()


def _invalidate_curve_cache():
    STATE["curve_revision"] = int(STATE.get("curve_revision", 0)) + 1
    STATE["curve_samples_cache"] = {}


def _anchor_index_map():
    out = {}
    anchors = STATE.get("anchors", [])
    for idx, a in enumerate(anchors):
        aid = int(a.get("id", 0))
        if aid > 0:
            out[aid] = idx
    return out


def _ensure_anchor_ids():
    anchors = STATE.get("anchors", [])
    used = set()
    next_id = max(1, int(STATE.get("next_anchor_id", 1)))
    for a in anchors:
        aid = int(a.get("id", 0)) if isinstance(a, dict) else 0
        if aid <= 0 or aid in used:
            aid = next_id
            next_id += 1
            a["id"] = aid
        used.add(aid)
        if aid >= next_id:
            next_id = aid + 1
    STATE["next_anchor_id"] = next_id


def _new_anchor_id():
    _ensure_anchor_ids()
    aid = int(STATE.get("next_anchor_id", 1))
    STATE["next_anchor_id"] = aid + 1
    return aid


def _normalize_link(id_a, id_b):
    a = int(id_a)
    b = int(id_b)
    if a <= 0 or b <= 0 or a == b:
        return None
    idx_map = _anchor_index_map()
    if a not in idx_map or b not in idx_map:
        return None
    return (a, b) if idx_map[a] <= idx_map[b] else (b, a)


def _link_exists(id_a, id_b):
    norm = _normalize_link(id_a, id_b)
    if norm is None:
        return False
    for raw in STATE.get("links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        cur = _normalize_link(raw[0], raw[1])
        if cur == norm:
            return True
    return False


def _add_link(id_a, id_b):
    norm = _normalize_link(id_a, id_b)
    if norm is None:
        return False
    if _link_exists(norm[0], norm[1]):
        return False
    links = STATE.get("links", [])
    if not isinstance(links, list):
        links = []
    links.append([norm[0], norm[1]])
    STATE["links"] = links
    return True


def _remove_link(id_a, id_b):
    norm = _normalize_link(id_a, id_b)
    if norm is None:
        return False
    changed = False
    kept = []
    for raw in STATE.get("links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        cur = _normalize_link(raw[0], raw[1])
        if cur == norm:
            changed = True
            continue
        if cur is not None:
            kept.append([cur[0], cur[1]])
    if changed:
        STATE["links"] = kept
    return changed


def _cleanup_links_and_selection():
    idx_map = _anchor_index_map()
    valid_ids = set(idx_map.keys())

    dedup = set()
    cleaned_links = []
    for raw in STATE.get("links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        if norm in dedup:
            continue
        dedup.add(norm)
        cleaned_links.append([norm[0], norm[1]])
    STATE["links"] = cleaned_links

    selected_anchor_ids = [int(aid) for aid in STATE.get("selected_anchor_ids", []) if int(aid) in valid_ids]
    if not _selection_enabled("anchors"):
        selected_anchor_ids = []
    STATE["selected_anchor_ids"] = selected_anchor_ids

    selected_links = []
    for raw in STATE.get("selected_links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        if not _link_exists(norm[0], norm[1]):
            continue
        selected_links.append([norm[0], norm[1]])
    if not _selection_enabled("segments"):
        selected_links = []
    STATE["selected_links"] = selected_links

    pending = int(STATE.get("pending_connect_anchor_id", -1))
    if pending not in valid_ids:
        STATE["pending_connect_anchor_id"] = -1


def _default_links_for_anchors():
    anchors = STATE.get("anchors", [])
    if len(anchors) < 2:
        return []
    out = []
    for i in range(len(anchors) - 1):
        id0 = int(anchors[i].get("id", 0))
        id1 = int(anchors[i + 1].get("id", 0))
        norm = _normalize_link(id0, id1)
        if norm is not None:
            out.append([norm[0], norm[1]])
    return out


def _connected_anchor_segments():
    idx_map = _anchor_index_map()
    anchors = STATE.get("anchors", [])
    segments = []
    for raw in STATE.get("links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        id0, id1 = norm
        i0 = idx_map.get(id0, -1)
        i1 = idx_map.get(id1, -1)
        if i0 < 0 or i1 < 0 or i0 == i1:
            continue
        a0 = anchors[i0]
        a1 = anchors[i1]
        segments.append((i0, i1, id0, id1, a0, a1))
    segments.sort(key=lambda s: (min(s[0], s[1]), max(s[0], s[1])))
    return segments


def _set_single_selected_anchor(anchor_id):
    aid = int(anchor_id)
    STATE["selected_anchor_ids"] = [aid] if aid > 0 else []


def _set_single_selected_link(id_a, id_b):
    norm = _normalize_link(id_a, id_b)
    if norm is None:
        STATE["selected_links"] = []
        return
    STATE["selected_links"] = [[norm[0], norm[1]]]


def _distance_point_to_segment(px, py, x1, y1, x2, y2):
    vx = x2 - x1
    vy = y2 - y1
    wx = px - x1
    wy = py - y1
    c1 = vx * wx + vy * wy
    if c1 <= 0:
        return _distance(px, py, x1, y1)
    c2 = vx * vx + vy * vy
    if c2 <= 1e-12:
        return _distance(px, py, x1, y1)
    t = _clamp(c1 / c2, 0.0, 1.0)
    qx = x1 + t * vx
    qy = y1 + t * vy
    return _distance(px, py, qx, qy)


def _find_segment_hit(context, cx, cy, threshold=10.0):
    best = None
    best_d = 1e9
    for _i0, _i1, id0, id1, a0, a1 in _connected_anchor_segments():
        p0 = (a0["lane_x"], a0["beat"])
        p1 = _anchor_out_abs_chart(a0)
        p2 = _anchor_in_abs_chart(a1)
        p3 = (a1["lane_x"], a1["beat"])
        prev = _chart_to_canvas(context, p0[0], p0[1])
        for j in range(1, 25):
            t = j / 24.0
            lane_x, beat = _cubic_point(p0, p1, p2, p3, t)
            cur = _chart_to_canvas(context, lane_x, beat)
            d = _distance_point_to_segment(cx, cy, prev[0], prev[1], cur[0], cur[1])
            if d < threshold and d < best_d:
                best_d = d
                best = (id0, id1)
            prev = cur
    return best


def _toggle_selected_anchor(anchor_id):
    aid = int(anchor_id)
    selected = [int(v) for v in STATE.get("selected_anchor_ids", [])]
    if aid in selected:
        selected = [v for v in selected if v != aid]
    elif aid > 0:
        selected.append(aid)
    STATE["selected_anchor_ids"] = selected


def _toggle_selected_link(id_a, id_b):
    norm = _normalize_link(id_a, id_b)
    if norm is None:
        return
    selected = []
    exists = False
    for raw in STATE.get("selected_links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        cur = _normalize_link(raw[0], raw[1])
        if cur is None:
            continue
        if cur == norm:
            exists = True
            continue
        selected.append([cur[0], cur[1]])
    if not exists:
        selected.append([norm[0], norm[1]])
    STATE["selected_links"] = selected


def _selection_enabled(kind):
    targets = STATE.get("selection_targets", {})
    if not isinstance(targets, dict):
        return True
    return bool(targets.get(kind, True))


def _connect_selected_anchors(context):
    idx_map = _anchor_index_map()
    selected = [int(v) for v in STATE.get("selected_anchor_ids", []) if int(v) in idx_map]
    if len(selected) < 2:
        return False
    selected.sort(key=lambda aid: idx_map.get(aid, 1 << 30))
    changed = False
    for i in range(len(selected) - 1):
        changed = _add_link(selected[i], selected[i + 1]) or changed
    if changed:
        _cleanup_links_and_selection()
        _invalidate_curve_cache()
        _record_history_state(context)
    return changed


def _disconnect_selected_segments(context):
    changed = False
    for raw in list(STATE.get("selected_links", [])):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        changed = _remove_link(raw[0], raw[1]) or changed
    if changed:
        _cleanup_links_and_selection()
        _invalidate_curve_cache()
        _record_history_state(context)
    return changed


def _delete_selected_anchors(context):
    selected = set(int(v) for v in STATE.get("selected_anchor_ids", []))
    if not selected:
        return False
    anchors = STATE.get("anchors", [])
    kept = [a for a in anchors if int(a.get("id", 0)) not in selected]
    if len(kept) == len(anchors):
        return False
    STATE["anchors"] = kept
    _cleanup_links_and_selection()
    _invalidate_curve_cache()
    _record_history_state(context)
    return True


def _rect_normalized(x1, y1, x2, y2):
    return min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2)


def _point_in_rect(px, py, rect):
    x0, y0, x1, y1 = rect
    return x0 <= px <= x1 and y0 <= py <= y1


def _apply_box_selection(context):
    box = STATE.get("box_select", {})
    if not bool(box.get("active", False)):
        return False
    sx, sy = box.get("start", [0.0, 0.0])
    ex, ey = box.get("end", [0.0, 0.0])
    rect = _rect_normalized(float(sx), float(sy), float(ex), float(ey))
    append = bool(box.get("append", False))

    selected_anchor_ids = set(int(v) for v in STATE.get("selected_anchor_ids", [])) if append else set()
    selected_links = set()
    if append:
        for raw in STATE.get("selected_links", []):
            if isinstance(raw, list) and len(raw) == 2:
                norm = _normalize_link(raw[0], raw[1])
                if norm is not None:
                    selected_links.add(norm)

    for a in STATE.get("anchors", []):
        cx, cy = _chart_to_canvas(context, a["lane_x"], a["beat"])
        if _point_in_rect(cx, cy, rect):
            selected_anchor_ids.add(int(a.get("id", 0)))

    for _i0, _i1, id0, id1, a0, a1 in _connected_anchor_segments():
        p0 = (a0["lane_x"], a0["beat"])
        p1 = _anchor_out_abs_chart(a0)
        p2 = _anchor_in_abs_chart(a1)
        p3 = (a1["lane_x"], a1["beat"])
        hit = False
        for j in range(25):
            t = j / 24.0
            lane_x, beat = _cubic_point(p0, p1, p2, p3, t)
            cx, cy = _chart_to_canvas(context, lane_x, beat)
            if _point_in_rect(cx, cy, rect):
                hit = True
                break
        if hit:
            selected_links.add((id0, id1))

    STATE["selected_anchor_ids"] = [aid for aid in sorted(selected_anchor_ids) if aid > 0]
    STATE["selected_links"] = [[a, b] for (a, b) in sorted(selected_links)]
    STATE["box_select"] = {"active": False, "start": [0.0, 0.0], "end": [0.0, 0.0], "append": False}
    _cleanup_links_and_selection()
    return True


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
    return True


def _record_history_state(context):
    changed = _push_history()
    if changed:
        _mark_dirty(context)
    return changed


def _undo_history_from_host(context):
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))
    if not hist or idx <= 0:
        return False
    idx -= 1
    STATE["history_index"] = idx
    _restore_snapshot(hist[idx])
    _mark_dirty(context)
    return True


def _redo_history_from_host(context):
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))
    if not hist or idx >= len(hist) - 1:
        return False
    idx += 1
    STATE["history_index"] = idx
    _restore_snapshot(hist[idx])
    _mark_dirty(context)
    return True


def _is_curve_checkpoint_action(action_text):
    if not isinstance(action_text, str):
        return False
    return action_text.strip().startswith(CURVE_CHECKPOINT_PREFIX)


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


def _snap_chart_point(context, lane_x, beat, snap_beat=True, snap_lane=True):
    lane_w = max(1.0, float(context.get("lane_width", 512.0)))
    grid_snap = bool(context.get("grid_snap", False))
    grid_div = max(1, int(context.get("grid_division", 8)))
    time_div = max(1, int(context.get("time_division", 1)))

    lane_x = _clamp(float(lane_x), 0.0, lane_w)
    if snap_lane and grid_snap and grid_div > 0:
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
    segments = _connected_anchor_segments()
    if not segments:
        return []

    pts = []
    for _i0, _i1, _id0, _id1, a0, a1 in segments:
        p0 = (a0["lane_x"], a0["beat"])
        p1 = _anchor_out_abs_chart(a0)
        p2 = _anchor_in_abs_chart(a1)
        p3 = (a1["lane_x"], a1["beat"])
        for j in range(samples_per_segment + 1):
            t = j / float(samples_per_segment)
            if pts and j == 0:
                continue
            pts.append(_cubic_point(p0, p1, p2, p3, t))
    return pts


def _sample_curve_chart_cached(samples_per_segment=24):
    cache = STATE.get("curve_samples_cache")
    if not isinstance(cache, dict):
        cache = {}
        STATE["curve_samples_cache"] = cache
    key = f"{int(STATE.get('curve_revision', 0))}:{int(samples_per_segment)}"
    cached = cache.get(key)
    if isinstance(cached, list):
        return cached
    sampled = _sample_curve_chart(samples_per_segment)
    cache[key] = sampled
    return sampled


def _visible_beat_window(context, ratio_margin=0.6, min_margin=2.0):
    if not isinstance(context, dict):
        return -1e9, 1e9
    scroll = float(context.get("scroll_beat", 0.0))
    vr = max(1e-6, float(context.get("visible_beat_range", 8.0)))
    margin = max(float(min_margin), vr * float(ratio_margin))
    return scroll - margin, scroll + vr + margin


def _segment_intersects_beat_window(p0, p1, lo, hi):
    b0 = float(p0[1])
    b1 = float(p1[1])
    mn = min(b0, b1)
    mx = max(b0, b1)
    return not (mx < lo or mn > hi)


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
        "id": int(a.get("id", 0)),
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
        anchor_id = int(raw.get("id", 0))
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
            "id": anchor_id,
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
            "id": int(raw.get("id", 0)),
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
            "links": _clone(STATE.get("links", [])),
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
        _ensure_anchor_ids()

        links_raw = payload.get("links")
        if isinstance(links_raw, list):
            STATE["links"] = _clone(links_raw)
        else:
            STATE["links"] = _default_links_for_anchors()
        _cleanup_links_and_selection()

        style = payload.get("style")
        if isinstance(style, dict):
            STATE["style"] = {
                "style_name": str(style.get("style_name", "loaded")),
                "denominators": _sanitize_denominators(style.get("denominators"), context),
            }

        _invalidate_curve_cache()
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
        _try_seed_curve_project_from_source(context, path)
        if not _load_project(path, context):
            STATE["anchors"] = _default_anchors()
            _ensure_anchor_ids()
            STATE["links"] = _default_links_for_anchors()
            STATE["project_dirty"] = True
            _invalidate_curve_cache()
        STATE["history"] = []
        STATE["history_index"] = -1
        _push_history()


def _try_seed_curve_project_from_source(context, target_curve_path):
    if not isinstance(context, dict):
        return False
    if not isinstance(target_curve_path, str) or not target_curve_path.strip():
        return False
    if os.path.exists(target_curve_path):
        return True

    source_chart = str(context.get("chart_path_source", "") or "").strip()
    if not source_chart:
        return False
    if not os.path.exists(source_chart):
        return False

    source_stem = os.path.splitext(os.path.basename(source_chart))[0]
    source_curve = os.path.join(os.path.dirname(source_chart), ".mcce-plugin", source_stem + ".curve_tbd.json")
    if not os.path.exists(source_curve):
        return False

    try:
        os.makedirs(os.path.dirname(target_curve_path), exist_ok=True)
        shutil.copy2(source_curve, target_curve_path)
        return True
    except Exception:
        return False


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
    selected_link_set = set()
    for raw in STATE.get("selected_links", []):
        if isinstance(raw, list) and len(raw) == 2:
            norm = _normalize_link(raw[0], raw[1])
            if norm is not None:
                selected_link_set.add(norm)
    selected_anchor_set = set(int(v) for v in STATE.get("selected_anchor_ids", []))

    if show_preview:
        for _i0, _i1, id0, id1, a0, a1 in _connected_anchor_segments():
            p0 = (a0["lane_x"], a0["beat"])
            p1 = _anchor_out_abs_chart(a0)
            p2 = _anchor_in_abs_chart(a1)
            p3 = (a1["lane_x"], a1["beat"])
            is_selected_segment = (id0, id1) in selected_link_set
            seg_color = "#FFD66B" if is_selected_segment else "#33CCFF"
            seg_width = 3.0 if is_selected_segment else 2.0
            sampled = []
            for j in range(25):
                t = j / 24.0
                sampled.append(_cubic_point(p0, p1, p2, p3, t))
            for j in range(len(sampled) - 1):
                items.append({
                    "kind": "line",
                    "coord_space": "chart",
                    "lane_x1": sampled[j][0],
                    "beat1": sampled[j][1],
                    "lane_x2": sampled[j + 1][0],
                    "beat2": sampled[j + 1][1],
                    "color": seg_color,
                    "width": seg_width,
                })
            if show_samples:
                for lane_x, beat in sampled[::4]:
                    items.append({
                        "kind": "rect",
                        "coord_space": "chart",
                        "lane_x": lane_x,
                        "beat": beat,
                        "w": 4,
                        "h": 4,
                        "rect_anchor": "center",
                        "color": "#88FFFFFF",
                        "fill_color": "#88FFFFFF",
                        "width": 1.0,
                    })

    for i, a in enumerate(anchors):
        is_drag_anchor = STATE["drag"]["mode"] == "anchor" and STATE["drag"]["index"] == i
        is_selected_anchor = int(a.get("id", 0)) in selected_anchor_set

        if show_handles:
            ilx, ib = _anchor_in_abs_chart(a)
            olx, ob = _anchor_out_abs_chart(a)
            items.append({
                "kind": "line",
                "coord_space": "chart",
                "lane_x1": a["lane_x"],
                "beat1": a["beat"],
                "lane_x2": ilx,
                "beat2": ib,
                "color": "#66A0A0A0",
                "width": 1.0,
            })
            items.append({
                "kind": "line",
                "coord_space": "chart",
                "lane_x1": a["lane_x"],
                "beat1": a["beat"],
                "lane_x2": olx,
                "beat2": ob,
                "color": "#66A0A0A0",
                "width": 1.0,
            })
            items.append({
                "kind": "rect",
                "coord_space": "chart",
                "lane_x": ilx,
                "beat": ib,
                "w": 8,
                "h": 8,
                "rect_anchor": "center",
                "color": "#FFFFFFFF",
                "fill_color": "#AAEEAA55",
                "width": 1.0,
            })
            items.append({
                "kind": "rect",
                "coord_space": "chart",
                "lane_x": olx,
                "beat": ob,
                "w": 8,
                "h": 8,
                "rect_anchor": "center",
                "color": "#FFFFFFFF",
                "fill_color": "#AAEEAA55",
                "width": 1.0,
            })

        if show_points:
            items.append({
                "kind": "rect",
                "coord_space": "chart",
                "lane_x": a["lane_x"],
                "beat": a["beat"],
                "w": 12,
                "h": 12,
                "rect_anchor": "center",
                "color": "#FFFFFF",
                "fill_color": "#AAFF9B2F" if is_selected_anchor else ("#AA0077FF" if is_drag_anchor else "#AA00A3FF"),
                "width": 2.5 if is_selected_anchor else 1.5,
            })

        if show_labels:
            mode = tr(context, "anchor_mode_smooth") if a.get("smooth", True) else tr(context, "anchor_mode_corner")
            items.append({
                "kind": "text",
                "coord_space": "chart",
                "lane_x": a["lane_x"] + 8.0,
                "beat": a["beat"],
                "text": f"A{i}({mode})",
                "color": "#FFFFFF",
                "font_px": 12,
            })

    if show_labels:
        dens = STATE.get("style", {}).get("denominators", [4, 8, 12, 16])
        override_den = int(context.get("plugin_time_division_override", 0) or 0) if isinstance(context, dict) else 0
        effective_den = override_den if override_den > 0 else max(1, int(context.get("time_division", 4))) if isinstance(context, dict) else 4
        anchor_mode = tr(context, "anchor_on") if bool(STATE.get("anchor_placement_enabled", False)) else tr(context, "anchor_off")
        label = tr(context, "overlay_summary", dens="/".join(str(d) for d in dens), den=effective_den, anchor_mode=anchor_mode)
        items.append({"kind": "text", "x1": 16, "y1": 18, "text": label, "color": "#DDEEFF", "font_px": 12})

    box = STATE.get("box_select", {})
    if bool(box.get("active", False)):
        sx, sy = box.get("start", [0.0, 0.0])
        ex, ey = box.get("end", [0.0, 0.0])
        x0, y0, x1, y1 = _rect_normalized(float(sx), float(sy), float(ex), float(ey))
        items.append({
            "kind": "rect",
            "x": x0,
            "y": y0,
            "w": max(1.0, x1 - x0),
            "h": max(1.0, y1 - y0),
            "color": "#FFCC66",
            "fill_color": "#33FFCC66",
            "width": 1.5,
        })

    return items


def _reset_anchors(context):
    STATE["anchors"] = _default_anchors()
    STATE["links"] = []
    STATE["drag"] = {"mode": "", "index": -1}
    STATE["selected_anchor_ids"] = []
    STATE["selected_links"] = []
    STATE["pending_connect_anchor_id"] = -1
    _invalidate_curve_cache()
    _record_history_state(context)


def _append_anchor(context, cx, cy):
    lane_x, beat = _canvas_to_chart(context, cx, cy)
    lane_x, beat = _snap_chart_point(context, lane_x, beat)

    if not STATE["anchors"]:
        STATE["anchors"].append({"id": _new_anchor_id(), "lane_x": lane_x, "beat": beat, "in": [-16.0, 0.0], "out": [16.0, 0.0], "smooth": True})
        return len(STATE["anchors"]) - 1

    prev = STATE["anchors"][-1]
    dl = lane_x - prev["lane_x"]
    db = beat - prev["beat"]
    # Split axis scaling to avoid overly weak handles on lane axis.
    ol = _clamp(dl * 0.25, -96.0, 96.0)
    ob = _clamp(db * 0.25, -2.0, 2.0)
    STATE["anchors"].append({"id": _new_anchor_id(), "lane_x": lane_x, "beat": beat, "in": [-ol, -ob], "out": [ol, ob], "smooth": True})
    idx = len(STATE["anchors"]) - 1
    _enforce_anchor_time_order(idx, context)
    _enforce_handle_time_constraints(idx, context)
    _enforce_handle_time_constraints(idx - 1, context)
    _cleanup_links_and_selection()
    _invalidate_curve_cache()
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
    # Keep curve placement free on lane axis; no lane-grid snap for generated notes.
    lane_x = _clamp(lane_x, 0.0, lane_w)

    b, n, d = _float_beat_to_triplet(beat, den)
    return {"beat": [b, n, d], "x": int(round(lane_x)), "type": 0}


def _build_batch_from_curve(context):
    if not isinstance(context, dict):
        return {"add": [], "remove": [], "move": []}

    sampled = _sample_curve_chart_cached(32)
    if len(sampled) < 2:
        return {"add": [], "remove": [], "move": []}

    override_den = int(context.get("plugin_time_division_override", 0) or 0)
    current_den = max(1, override_den if override_den > 0 else int(context.get("time_division", 4)))
    samples_by_beat = _normalize_samples_by_beat(sampled)
    if len(samples_by_beat) < 2:
        return {"add": [], "remove": [], "move": []}

    start_beat = samples_by_beat[0][1]
    end_beat = samples_by_beat[-1][1]
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
    current = STATE.get("style", {}).get("denominators", [4, 8, 12, 16])
    normalized = tuple(_sanitize_denominators(current, context))
    target_index = 0
    for i, preset in enumerate(STYLE_PRESETS):
        if tuple(_sanitize_denominators(preset, context)) == normalized:
            target_index = (i + 1) % len(STYLE_PRESETS)
            break
    next_values = _sanitize_denominators(STYLE_PRESETS[target_index], context)
    STATE["style"] = {"style_name": f"preset_{target_index + 1}", "denominators": next_values}
    _record_history_state(context)


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
    request_checkpoint = False

    if et == "mouse_down":
        hkind, hidx = _find_handle_hit(STATE["last_context"], x, y)
        aidx = _find_anchor_hit(STATE["last_context"], x, y) if _selection_enabled("anchors") else -1
        seg_hit = _find_segment_hit(STATE["last_context"], x, y) if _selection_enabled("segments") else None
        mods = int(event.get("modifiers", 0))
        ctrl = (mods & CTRL_MODIFIER_MASK) != 0

        if button == RIGHT_BUTTON:
            consumed = False
        elif button == LEFT_BUTTON:
            if hidx >= 0:
                STATE["drag"] = {"mode": hkind, "index": hidx}
                consumed = True
                cursor = "crosshair"
                status = tr(STATE["last_context"], "dragging_handle", handle_kind=tr(STATE["last_context"], f"handle_kind_{hkind}"), index=hidx)
            elif aidx >= 0:
                anchor_id = int(STATE["anchors"][aidx].get("id", 0))
                if ctrl:
                    _toggle_selected_anchor(anchor_id)
                else:
                    _set_single_selected_anchor(anchor_id)
                    STATE["selected_links"] = []
                is_double = (STATE["last_click_anchor"] == aidx and ts - STATE["last_click_ms"] <= 280)
                STATE["last_click_anchor"] = aidx
                STATE["last_click_ms"] = ts
                if is_double:
                    STATE["anchors"][aidx]["smooth"] = not bool(STATE["anchors"][aidx].get("smooth", True))
                    _invalidate_curve_cache()
                    _record_history_state(STATE["last_context"])
                    request_checkpoint = True
                    consumed = True
                    mode = tr(STATE["last_context"], "mode_smooth") if STATE["anchors"][aidx]["smooth"] else tr(STATE["last_context"], "mode_corner")
                    status = tr(STATE["last_context"], "anchor_mode_changed", index=aidx, mode=mode)
                else:
                    STATE["drag"] = {"mode": "anchor", "index": aidx}
                    consumed = True
                    cursor = "size_all"
                    status = tr(STATE["last_context"], "dragging_anchor", index=aidx)
            elif seg_hit is not None:
                if ctrl:
                    _toggle_selected_link(seg_hit[0], seg_hit[1])
                else:
                    _set_single_selected_link(seg_hit[0], seg_hit[1])
                    STATE["selected_anchor_ids"] = []
                consumed = True
                cursor = "pointing_hand"
                status = "Segment selected"
            elif ctrl:
                STATE["box_select"] = {"active": True, "start": [x, y], "end": [x, y], "append": True}
                consumed = True
                status = "Box selecting"
            else:
                consumed = True
                had_selection = bool(STATE.get("selected_anchor_ids")) or bool(STATE.get("selected_links"))
                if had_selection:
                    STATE["selected_anchor_ids"] = []
                    STATE["selected_links"] = []
                    status = "Selection cleared"
                    return {
                        "consumed": consumed,
                        "overlay": _build_overlay(STATE["last_context"]),
                        "cursor": cursor,
                        "status_text": status,
                        "preview_batch_edit": {"add": [], "remove": [], "move": []},
                        "request_undo_checkpoint": request_checkpoint,
                        "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
                    }
                notes_selectable = _selection_enabled("notes")
                if not bool(STATE.get("anchor_placement_enabled", False)):
                    if notes_selectable:
                        consumed = False
                        status = ""
                    else:
                        status = tr(STATE["last_context"], "anchor_place_off_hint")
                else:
                    new_idx = _append_anchor(STATE["last_context"], x, y)
                    new_anchor_id = int(STATE["anchors"][new_idx].get("id", 0))
                    selected = [int(v) for v in STATE.get("selected_anchor_ids", []) if int(v) > 0]
                    keep_selected_new_anchor = False
                    if len(selected) == 1:
                        _add_link(selected[0], new_anchor_id)
                        STATE["pending_connect_anchor_id"] = -1
                        keep_selected_new_anchor = True
                    elif len(selected) == 0:
                        pending_id = int(STATE.get("pending_connect_anchor_id", -1))
                        if pending_id > 0:
                            _add_link(pending_id, new_anchor_id)
                            STATE["pending_connect_anchor_id"] = -1
                        else:
                            STATE["pending_connect_anchor_id"] = new_anchor_id
                    else:
                        STATE["pending_connect_anchor_id"] = -1
                        keep_selected_new_anchor = False
                    if keep_selected_new_anchor:
                        _set_single_selected_anchor(new_anchor_id)
                    else:
                        STATE["selected_anchor_ids"] = []
                    _cleanup_links_and_selection()
                    STATE["drag"] = {"mode": "anchor", "index": new_idx}
                    _mark_dirty(STATE["last_context"])
                    cursor = "size_all"
                    status = tr(STATE["last_context"], "anchor_added", index=new_idx)

    elif et == "mouse_move":
        box = STATE.get("box_select", {})
        if bool(box.get("active", False)):
            box["end"] = [x, y]
            STATE["box_select"] = box
            consumed = True
            cursor = "crosshair"
            status = "Box selecting"
            return {
                "consumed": consumed,
                "overlay": _build_overlay(STATE["last_context"]),
                "cursor": cursor,
                "status_text": status,
                "preview_batch_edit": {"add": [], "remove": [], "move": []},
                "request_undo_checkpoint": request_checkpoint,
                "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
            }

        mode = STATE["drag"]["mode"]
        idx = STATE["drag"]["index"]
        if mode and 0 <= idx < len(STATE["anchors"]):
            a = STATE["anchors"][idx]
            if mode == "anchor":
                lane_x, beat = _canvas_to_chart(STATE["last_context"], x, y)
                # Anchor drag keeps time snap, but should not apply lane-grid snap.
                lane_x, beat = _snap_chart_point(STATE["last_context"], lane_x, beat, snap_beat=True, snap_lane=False)
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
            _invalidate_curve_cache()
            _mark_dirty(STATE["last_context"])
            consumed = True
            status = tr(STATE["last_context"], "editing_anchor", index=idx)
        else:
            hkind, hidx = _find_handle_hit(STATE["last_context"], x, y)
            if hidx >= 0:
                cursor = "pointing_hand"
            elif _find_anchor_hit(STATE["last_context"], x, y) >= 0:
                cursor = "pointing_hand"

    elif et == "mouse_up":
        if bool(STATE.get("box_select", {}).get("active", False)):
            changed = _apply_box_selection(STATE["last_context"])
            consumed = True
            status = "Box selection applied" if changed else "Box selection cleared"
            STATE["drag"] = {"mode": "", "index": -1}
            return {
                "consumed": consumed,
                "overlay": _build_overlay(STATE["last_context"]),
                "cursor": cursor,
                "status_text": status,
                "preview_batch_edit": {"add": [], "remove": [], "move": []},
                "request_undo_checkpoint": request_checkpoint,
                "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
            }

        if STATE["drag"]["mode"]:
            status = tr(STATE["last_context"], "curve_edit_applied")
            consumed = True
            STATE["drag"] = {"mode": "", "index": -1}
            _record_history_state(STATE["last_context"])
            request_checkpoint = True
        else:
            STATE["drag"] = {"mode": "", "index": -1}

    elif et == "cancel":
        STATE["drag"] = {"mode": "", "index": -1}
        consumed = True
        status = tr(STATE["last_context"], "interaction_cancelled")

    elif et == "key_down":
        key = int(event.get("key", 0))
        mods = int(event.get("modifiers", 0))
        ctrl = (mods & CTRL_MODIFIER_MASK) != 0
        if not ctrl and key == KEY_A:
            STATE["anchor_placement_enabled"] = not bool(STATE.get("anchor_placement_enabled", False))
            consumed = True
            status = tr(STATE["last_context"], "anchor_enabled") if STATE["anchor_placement_enabled"] else tr(STATE["last_context"], "anchor_disabled")
            _record_history_state(STATE["last_context"])
            request_checkpoint = True
        elif key == KEY_DELETE:
            changed_segments = _disconnect_selected_segments(STATE["last_context"])
            changed_anchors = _delete_selected_anchors(STATE["last_context"])
            if changed_segments or changed_anchors:
                consumed = True
                request_checkpoint = True
                status = "Selection deleted"
        elif key == 0x01000000:  # Esc
            if STATE.get("selected_anchor_ids") or STATE.get("selected_links"):
                STATE["selected_anchor_ids"] = []
                STATE["selected_links"] = []
                consumed = True
                status = "Selection cleared"

    # In tool mode, left-button canvas operations belong to plugin.
    notes_selectable = _selection_enabled("notes")
    if et in ("mouse_down", "mouse_up") and not consumed and button == LEFT_BUTTON and not notes_selectable:
        consumed = True
    if et == "mouse_move" and not consumed and int(event.get("buttons", 0)) != 0 and not notes_selectable:
        consumed = True

    return {
        "consumed": consumed,
        "overlay": _build_overlay(STATE["last_context"]),
        "cursor": cursor,
        "status_text": status,
        "preview_batch_edit": {"add": [], "remove": [], "move": []},
        "request_undo_checkpoint": request_checkpoint,
        "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
    }


def _list_tool_actions():
    return [
        {
            "action_id": "commit_curve_to_notes",
            "title": tr(STATE.get("last_context", {}), "action_commit_curve"),
            "description": tr(STATE.get("last_context", {}), "action_commit_curve_desc"),
            "placement": "top_toolbar",
            "requires_undo_snapshot": True,
        },
        {
            "action_id": "toggle_anchor_placement",
            "title": tr(STATE.get("last_context", {}), "action_anchor_place"),
            "description": tr(STATE.get("last_context", {}), "action_anchor_place_desc"),
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("anchor_placement_enabled", False)),
            "sync_plugin_tool_mode_with_checked": True,
        },
        {
            "action_id": "toggle_select_anchors",
            "title": "Selectable: Anchors",
            "description": "Enable/disable anchor selection",
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("selection_targets", {}).get("anchors", True)),
        },
        {
            "action_id": "toggle_select_segments",
            "title": "Selectable: Segments",
            "description": "Enable/disable segment selection",
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("selection_targets", {}).get("segments", True)),
        },
        {
            "action_id": "toggle_select_notes",
            "title": "Selectable: Notes",
            "description": "Allow selecting host notes (disable for curve-only selection)",
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("selection_targets", {}).get("notes", False)),
        },
        {
            "action_id": "undo_curve_edit",
            "title": tr(STATE.get("last_context", {}), "action_undo_curve"),
            "description": tr(STATE.get("last_context", {}), "action_undo_curve_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
            "host_action": "undo",
        },
        {
            "action_id": "redo_curve_edit",
            "title": tr(STATE.get("last_context", {}), "action_redo_curve"),
            "description": tr(STATE.get("last_context", {}), "action_redo_curve_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
            "host_action": "redo",
        },
        {
            "action_id": "commit_curve_to_notes_sidebar",
            "title": tr(STATE.get("last_context", {}), "action_commit_curve"),
            "description": tr(STATE.get("last_context", {}), "action_commit_curve_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": True,
        },
        {
            "action_id": "reset_curve",
            "title": tr(STATE.get("last_context", {}), "action_reset_curve"),
            "description": tr(STATE.get("last_context", {}), "action_reset_curve_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "cycle_density_style",
            "title": tr(STATE.get("last_context", {}), "action_cycle_density"),
            "description": tr(STATE.get("last_context", {}), "action_cycle_density_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "connect_selected_nodes",
            "title": "Connect Selected",
            "description": "Connect selected anchors by time order",
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "disconnect_selected_segments",
            "title": "Delete Selected Segment",
            "description": "Disconnect selected curve segments",
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "connect_selected_nodes_ctx",
            "title": "Connect Selected",
            "description": "Connect selected anchors by time order",
            "placement": "plugin_context_menu",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "disconnect_selected_segments_ctx",
            "title": "Delete Selected Segment",
            "description": "Disconnect selected curve segments",
            "placement": "plugin_context_menu",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "export_style_preset",
            "title": tr(STATE.get("last_context", {}), "action_export_style"),
            "description": tr(STATE.get("last_context", {}), "action_export_style_desc"),
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "import_style_preset",
            "title": tr(STATE.get("last_context", {}), "action_import_style"),
            "description": tr(STATE.get("last_context", {}), "action_import_style_desc"),
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
        _record_history_state(context)
        return True
    if action_id == "toggle_select_anchors":
        cur = bool(STATE.get("selection_targets", {}).get("anchors", True))
        STATE["selection_targets"]["anchors"] = not cur
        if not STATE["selection_targets"]["anchors"]:
            STATE["selected_anchor_ids"] = []
        _record_history_state(context)
        return True
    if action_id == "toggle_select_segments":
        cur = bool(STATE.get("selection_targets", {}).get("segments", True))
        STATE["selection_targets"]["segments"] = not cur
        if not STATE["selection_targets"]["segments"]:
            STATE["selected_links"] = []
        _record_history_state(context)
        return True
    if action_id == "toggle_select_notes":
        cur = bool(STATE.get("selection_targets", {}).get("notes", False))
        STATE["selection_targets"]["notes"] = not cur
        _record_history_state(context)
        return True
    if action_id == "cycle_density_style":
        _cycle_density_style(context)
        return True
    if action_id in ("connect_selected_nodes", "connect_selected_nodes_ctx"):
        return _connect_selected_anchors(context)
    if action_id in ("disconnect_selected_segments", "disconnect_selected_segments_ctx"):
        return _disconnect_selected_segments(context)
    if action_id == "undo_curve_edit":
        return True
    if action_id == "redo_curve_edit":
        return True
    if action_id == "export_style_preset":
        return _save_style(context)
    if action_id == "import_style_preset":
        ok = _load_style(context)
        if ok:
            _record_history_state(context)
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
        "toggle_select_anchors",
        "toggle_select_segments",
        "toggle_select_notes",
        "undo_curve_edit",
        "redo_curve_edit",
        "cycle_density_style",
        "connect_selected_nodes",
        "disconnect_selected_segments",
        "connect_selected_nodes_ctx",
        "disconnect_selected_segments_ctx",
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
            event = msg.get("event")
            payload = msg.get("payload", {}) or {}
            if event == "initialize":
                locale_name = str(payload.get("locale", "") or "")
                language_name = str(payload.get("language", "") or "")
                if locale_name.strip():
                    STATE["lang"] = normalize_lang(locale_name, STATE.get("lang", "en"))
                elif language_name.strip():
                    STATE["lang"] = normalize_lang(language_name, STATE.get("lang", "en"))
            elif event == "shutdown":
                if STATE["project_path"] and STATE["project_dirty"]:
                    _save_project(STATE["project_path"], STATE.get("last_context", {}))
                break
            elif event == "onHostUndo":
                action_text = str(payload.get("action_text", "") or "")
                if _is_curve_checkpoint_action(action_text):
                    _undo_history_from_host(STATE.get("last_context", {}))
            elif event == "onHostRedo":
                action_text = str(payload.get("action_text", "") or "")
                if _is_curve_checkpoint_action(action_text):
                    _redo_history_from_host(STATE.get("last_context", {}))
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
    _configure_stdio_utf8()
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
