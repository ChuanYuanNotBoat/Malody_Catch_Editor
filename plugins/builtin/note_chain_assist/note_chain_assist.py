import json
import locale
import math
import os
import shutil
import sys
import time
import uuid
from fractions import Fraction

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from modular.core.state import (
    CTRL_MODIFIER_MASK,
    CURVE_CHECKPOINT_PREFIX,
    CURVE_SIDECAR_FORMAT_VERSION,
    DEFAULT_CURVE_GROUP_ID,
    DEFAULT_NODE_GROUP_ID,
    KEY_A,
    KEY_BACKSPACE,
    KEY_DELETE,
    LEFT_BUTTON,
    MAX_HISTORY,
    RIGHT_BUTTON,
    SERIALIZE_DEN,
    SHIFT_MODIFIER_MASK,
    STATE,
    STYLE_PRESETS,
)

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
    "action_commit_context_segments": {
        "en": "Place Notes for Segment(s)",
        "zh": "放置曲线段 Note",
        "ja": "セグメントのノートを配置",
    },
    "action_commit_context_segments_desc": {
        "en": "Generate normal notes from the right-clicked segment, or all selected segments",
        "zh": "根据右键曲线段生成普通 note；多选曲线段时生成所有选中段",
        "ja": "右クリックしたセグメント、または選択中のセグメントから通常ノートを生成",
    },
    "action_anchor_place": {"en": "Anchor Place", "zh": "放置锚点", "ja": "アンカー配置"},
    "action_anchor_place_desc": {
        "en": "Toggle anchor placement mode to prevent misclick additions",
        "zh": "切换锚点放置模式，避免误触新增锚点",
        "ja": "アンカー配置モードを切り替えて誤操作での追加を防ぎます",
    },
    "action_curve_visible": {"en": "Show Curve (with Nodes)", "zh": "显示曲线（含节点）", "ja": "カーブ表示（ノード含む）"},
    "action_curve_visible_desc": {"en": "Toggle curve and node overlay visibility", "zh": "切换曲线与节点叠加层显示", "ja": "カーブとノードのオーバーレイ表示を切替"},
    "action_polyline_mode": {"en": "Polyline Link", "zh": "折线连接", "ja": "ポリライン接続"},
    "action_polyline_mode_desc": {
        "en": "Toggle selected connection lines between Bezier curves and straight segments; with no selection this sets the shape for new links",
        "zh": "切换选中连接线的曲线/折线状态；未选中连接线时设置新连接线的默认状态",
        "ja": "選択した接続線のベジェ/直線状態を切り替えます。未選択時は新規接続線の既定状態を設定します",
    },
    "action_polyline_context_desc": {
        "en": "Toggle the right-clicked connection line, or all selected connection lines, between curve and polyline",
        "zh": "切换右键连接线或所有选中连接线的曲线/折线状态",
        "ja": "右クリックした接続線、または選択中の接続線のカーブ/ポリライン状態を切り替えます",
    },
    "action_toggle_context_shape": {"en": "Toggle Curve/Polyline", "zh": "切换曲线/折线", "ja": "カーブ/ポリライン切替"},
    "action_note_curve_snap": {"en": "Snap Notes to Curve", "zh": "Note 吸附到曲线", "ja": "ノートを曲線に吸着"},
    "action_note_curve_snap_desc": {
        "en": "Use the curve as a note drag snap reference; curve and anchor selection is disabled while enabled",
        "zh": "拖拽 note 时以曲线作为吸附参考；启用时不可选中曲线和节点",
        "ja": "ノートドラッグ時に曲線を吸着基準にします。有効時は曲線とノードを選択できません",
    },
    "status_curve_shape_changed": {"en": "Curve shape: {shape}", "zh": "曲线形态：{shape}", "ja": "曲線形状: {shape}"},
    "shape_curve": {"en": "Curve", "zh": "曲线", "ja": "カーブ"},
    "shape_polyline": {"en": "Polyline", "zh": "折线", "ja": "ポリライン"},
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

TRANSLATIONS.update(
    {
        "status_segment_selected": {"en": "Segment selected", "zh": "已选中曲线段", "ja": "セグメントを選択しました"},
        "status_box_selecting": {"en": "Box selecting", "zh": "正在框选", "ja": "範囲選択中"},
        "status_selection_cleared": {"en": "Selection cleared", "zh": "已清除选择", "ja": "選択を解除しました"},
        "status_box_selection_applied": {"en": "Box selection applied", "zh": "框选已应用", "ja": "範囲選択を適用しました"},
        "status_box_selection_cleared": {"en": "Box selection cleared", "zh": "框选已清除", "ja": "範囲選択をクリアしました"},
        "status_selection_deleted": {"en": "Selection deleted", "zh": "已删除所选对象", "ja": "選択対象を削除しました"},
        "action_toggle_select_anchors": {"en": "Selectable: Anchors", "zh": "可选中：节点", "ja": "選択可能: ノード"},
        "action_toggle_select_anchors_desc": {"en": "Enable/disable anchor selection", "zh": "启用/禁用节点选择", "ja": "ノード選択の有効/無効を切替"},
        "action_toggle_select_segments": {"en": "Selectable: Segments", "zh": "可选中：曲线段", "ja": "選択可能: セグメント"},
        "action_toggle_select_segments_desc": {"en": "Enable/disable segment selection", "zh": "启用/禁用曲线段选择", "ja": "セグメント選択の有効/無効を切替"},
        "action_toggle_select_notes": {"en": "Selectable: Notes", "zh": "可选中：音符", "ja": "選択可能: ノーツ"},
        "action_toggle_select_notes_desc": {
            "en": "Allow selecting host notes (disable for curve-only selection)",
            "zh": "允许选择主程序音符（关闭后为仅曲线选择）",
            "ja": "ホストノーツ選択を許可（無効でカーブのみ選択）",
        },
        "action_connect_selected": {"en": "Connect Selected", "zh": "连接选中节点", "ja": "選択ノードを接続"},
        "action_connect_selected_desc": {"en": "Connect selected anchors by time order", "zh": "按时间顺序连接选中节点", "ja": "時間順に選択ノードを接続"},
        "action_disconnect_selected_segments": {"en": "Delete Selected Segment", "zh": "删除选中曲线段", "ja": "選択セグメントを削除"},
        "action_disconnect_selected_segments_desc": {"en": "Disconnect selected curve segments", "zh": "断开选中的曲线段连接", "ja": "選択セグメントの接続を解除"},
        "status_link_dragging": {"en": "Shift-drag to another anchor to connect", "zh": "按住 Shift 拖到另一个节点即可连接", "ja": "Shiftドラッグで別ノードに接続"},
        "status_link_drag_target": {"en": "Release to connect A{from_idx} -> A{to_idx}", "zh": "松开即可连接 A{from_idx} -> A{to_idx}", "ja": "離して接続 A{from_idx} -> A{to_idx}"},
        "status_link_connected": {"en": "Connected A{from_idx} -> A{to_idx}", "zh": "已连接 A{from_idx} -> A{to_idx}", "ja": "接続済み A{from_idx} -> A{to_idx}"},
        "status_link_already_connected": {"en": "A{from_idx} and A{to_idx} are already connected", "zh": "A{from_idx} 与 A{to_idx} 已连接", "ja": "A{from_idx} と A{to_idx} は既に接続済み"},
        "status_link_cancelled": {"en": "Connect drag cancelled", "zh": "连接拖拽已取消", "ja": "接続ドラッグをキャンセル"},
    }
)


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


def _mods_has_shift(mods):
    try:
        value = int(mods)
    except Exception:
        return False
    # Prefer Qt::ShiftModifier; keep low-bit fallback for host/tooling variance.
    return (value & SHIFT_MODIFIER_MASK) != 0 or (value & 0x1) != 0


def _event_has_shift(event):
    if isinstance(event, dict):
        if "shift_down" in event:
            return bool(event.get("shift_down", False))
        if "shiftDown" in event:
            return bool(event.get("shiftDown", False))
    mods = event.get("modifiers", 0) if isinstance(event, dict) else 0
    return _mods_has_shift(mods)


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
        "segment_denominators": _clone(STATE.get("segment_denominators", {})),
        "segment_shapes": _clone(STATE.get("segment_shapes", {})),
        "anchor_group_ids": _clone(STATE.get("anchor_group_ids", {})),
        "anchor_reserved": _clone(STATE.get("anchor_reserved", {})),
        "anchor_compat_handles": _clone(STATE.get("anchor_compat_handles", {})),
        "curve_id_by_link": _clone(STATE.get("curve_id_by_link", {})),
        "curve_no_by_link": _clone(STATE.get("curve_no_by_link", {})),
        "curve_group_ids_by_link": _clone(STATE.get("curve_group_ids_by_link", {})),
        "curve_density_mode_by_link": _clone(STATE.get("curve_density_mode_by_link", {})),
        "curve_reserved_by_link": _clone(STATE.get("curve_reserved_by_link", {})),
        "curve_special_joystick_by_link": _clone(STATE.get("curve_special_joystick_by_link", {})),
        "node_groups": _clone(STATE.get("node_groups", [])),
        "curve_groups": _clone(STATE.get("curve_groups", [])),
        "selection_targets": _clone(STATE.get("selection_targets", {"anchors": True, "segments": True, "notes": False})),
        "curve_visible": bool(STATE.get("curve_visible", True)),
        "active_link_shape": _active_link_shape(),
        "note_curve_snap_enabled": bool(STATE.get("note_curve_snap_enabled", False)),
        "anchor_placement_enabled": bool(STATE.get("anchor_placement_enabled", False)),
        "next_anchor_id": int(STATE.get("next_anchor_id", 1)),
        "next_curve_id": int(STATE.get("next_curve_id", 1)),
        "next_group_id": int(STATE.get("next_group_id", 2)),
        "pending_connect_anchor_id": int(STATE.get("pending_connect_anchor_id", -1)),
    }


def _restore_snapshot(snapshot):
    if not isinstance(snapshot, dict):
        return
    STATE["anchors"] = _clone(snapshot.get("anchors", [])) if isinstance(snapshot.get("anchors"), list) else []
    STATE["links"] = _clone(snapshot.get("links", [])) if isinstance(snapshot.get("links"), list) else []
    style = snapshot.get("style")
    STATE["style"] = _clone(style) if isinstance(style, dict) else {"denominators": [4, 8, 12, 16], "style_name": "balanced"}
    seg_dens = snapshot.get("segment_denominators")
    STATE["segment_denominators"] = _clone(seg_dens) if isinstance(seg_dens, dict) else {}
    seg_shapes = snapshot.get("segment_shapes")
    STATE["segment_shapes"] = _clone(seg_shapes) if isinstance(seg_shapes, dict) else {}
    anchor_group_ids = snapshot.get("anchor_group_ids")
    STATE["anchor_group_ids"] = _clone(anchor_group_ids) if isinstance(anchor_group_ids, dict) else {}
    anchor_reserved = snapshot.get("anchor_reserved")
    STATE["anchor_reserved"] = _clone(anchor_reserved) if isinstance(anchor_reserved, dict) else {}
    anchor_compat = snapshot.get("anchor_compat_handles")
    STATE["anchor_compat_handles"] = _clone(anchor_compat) if isinstance(anchor_compat, dict) else {}
    curve_id_by_link = snapshot.get("curve_id_by_link")
    STATE["curve_id_by_link"] = _clone(curve_id_by_link) if isinstance(curve_id_by_link, dict) else {}
    curve_no_by_link = snapshot.get("curve_no_by_link")
    STATE["curve_no_by_link"] = _clone(curve_no_by_link) if isinstance(curve_no_by_link, dict) else {}
    curve_group_ids_by_link = snapshot.get("curve_group_ids_by_link")
    STATE["curve_group_ids_by_link"] = _clone(curve_group_ids_by_link) if isinstance(curve_group_ids_by_link, dict) else {}
    curve_density_mode_by_link = snapshot.get("curve_density_mode_by_link")
    STATE["curve_density_mode_by_link"] = _clone(curve_density_mode_by_link) if isinstance(curve_density_mode_by_link, dict) else {}
    curve_reserved_by_link = snapshot.get("curve_reserved_by_link")
    STATE["curve_reserved_by_link"] = _clone(curve_reserved_by_link) if isinstance(curve_reserved_by_link, dict) else {}
    curve_special_js = snapshot.get("curve_special_joystick_by_link")
    STATE["curve_special_joystick_by_link"] = _clone(curve_special_js) if isinstance(curve_special_js, dict) else {}
    node_groups = snapshot.get("node_groups")
    STATE["node_groups"] = _clone(node_groups) if isinstance(node_groups, list) else [{"group_id": DEFAULT_NODE_GROUP_ID, "group_name": "base", "reserved": {}}]
    curve_groups = snapshot.get("curve_groups")
    STATE["curve_groups"] = _clone(curve_groups) if isinstance(curve_groups, list) else [{"group_id": DEFAULT_CURVE_GROUP_ID, "group_name": "base", "reserved": {}}]
    targets = snapshot.get("selection_targets")
    if isinstance(targets, dict):
        STATE["selection_targets"] = {
            "anchors": bool(targets.get("anchors", True)),
            "segments": bool(targets.get("segments", True)),
            "notes": bool(targets.get("notes", False)),
        }
    else:
        STATE["selection_targets"] = {"anchors": True, "segments": True, "notes": False}
    STATE["curve_visible"] = bool(snapshot.get("curve_visible", True))
    active_shape = str(snapshot.get("active_link_shape", snapshot.get("curve_shape", "curve")) or "curve").strip().lower()
    STATE["active_link_shape"] = "polyline" if active_shape == "polyline" else "curve"
    STATE["note_curve_snap_enabled"] = bool(snapshot.get("note_curve_snap_enabled", False))
    STATE["anchor_placement_enabled"] = bool(snapshot.get("anchor_placement_enabled", False))
    STATE["drag"] = {"mode": "", "index": -1}
    STATE["selected_anchor_ids"] = []
    STATE["selected_links"] = []
    STATE["box_select"] = {"active": False, "start": [0.0, 0.0], "end": [0.0, 0.0], "append": False}
    STATE["link_drag"] = {"active": False, "source_anchor_id": -1, "hover_anchor_id": -1, "x": 0.0, "y": 0.0}
    STATE["shift_down"] = False
    STATE["pending_connect_anchor_id"] = int(snapshot.get("pending_connect_anchor_id", -1))
    STATE["next_anchor_id"] = max(1, int(snapshot.get("next_anchor_id", 1)))
    STATE["next_curve_id"] = max(1, int(snapshot.get("next_curve_id", 1)))
    STATE["next_group_id"] = max(2, int(snapshot.get("next_group_id", 2)))
    _ensure_anchor_ids()
    _cleanup_links_and_selection()
    _seed_missing_segment_denominators()
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


def _link_key(id_a, id_b):
    norm = _normalize_link(id_a, id_b)
    if norm is None:
        return ""
    return f"{norm[0]}:{norm[1]}"


def _segment_denominator_for_link(id_a, id_b, fallback_den=4):
    key = _link_key(id_a, id_b)
    if not key:
        return max(1, int(fallback_den))
    density_mode = STATE.get("curve_density_mode_by_link", {})
    if isinstance(density_mode, dict):
        mode = str(density_mode.get(key, "") or "").strip().lower()
        if mode == "follow":
            return max(1, int(fallback_den))
    seg_map = STATE.get("segment_denominators", {})
    if isinstance(seg_map, dict):
        try:
            den = int(seg_map.get(key, 0))
            if den > 0:
                return den
        except Exception:
            pass
    return max(1, int(fallback_den))


def _set_segment_denominator(id_a, id_b, den):
    key = _link_key(id_a, id_b)
    if not key:
        return False
    try:
        val = int(den)
    except Exception:
        val = 0
    seg_map = STATE.get("segment_denominators", {})
    if not isinstance(seg_map, dict):
        seg_map = {}
    density_mode = STATE.get("curve_density_mode_by_link", {})
    if not isinstance(density_mode, dict):
        density_mode = {}
    prev = int(seg_map.get(key, 0) or 0)
    if val <= 0:
        density_mode[key] = "follow"
        STATE["curve_density_mode_by_link"] = density_mode
        if key in seg_map:
            seg_map.pop(key, None)
            STATE["segment_denominators"] = seg_map
            return True
        return False
    seg_map[key] = val
    density_mode[key] = "fixed"
    STATE["curve_density_mode_by_link"] = density_mode
    STATE["segment_denominators"] = seg_map
    return prev != val


def _normalize_shape_name(shape):
    return "polyline" if str(shape or "").strip().lower() == "polyline" else "curve"


def _active_link_shape():
    return _normalize_shape_name(STATE.get("active_link_shape", STATE.get("curve_shape", "curve")))


def _active_link_shape_is_polyline():
    return _active_link_shape() == "polyline"


def _segment_shape_for_link(id_a, id_b, fallback_shape="curve"):
    key = _link_key(id_a, id_b)
    if not key:
        return _normalize_shape_name(fallback_shape)
    seg_map = STATE.get("segment_shapes", {})
    if isinstance(seg_map, dict) and key in seg_map:
        return _normalize_shape_name(seg_map.get(key))
    return _normalize_shape_name(fallback_shape)


def _set_segment_shape(id_a, id_b, shape):
    key = _link_key(id_a, id_b)
    if not key:
        return False
    val = _normalize_shape_name(shape)
    seg_map = STATE.get("segment_shapes", {})
    if not isinstance(seg_map, dict):
        seg_map = {}
    prev = _normalize_shape_name(seg_map.get(key, "curve"))
    if val == "curve":
        changed = key in seg_map and prev != "curve"
        seg_map.pop(key, None)
    else:
        changed = prev != val
        seg_map[key] = val
    STATE["segment_shapes"] = seg_map
    return changed


def _context_default_segment_denominator(context=None):
    ctx = context if isinstance(context, dict) else {}
    if not ctx:
        ctx = STATE.get("last_context", {}) if isinstance(STATE.get("last_context", {}), dict) else {}
    override_den = int(ctx.get("plugin_time_division_override", 0) or 0) if isinstance(ctx, dict) else 0
    if override_den > 0:
        return max(1, override_den)
    if isinstance(ctx, dict):
        try:
            time_div = int(ctx.get("time_division", 0) or 0)
            if time_div > 0:
                return time_div
        except Exception:
            pass
    dens = _sanitize_denominators(STATE.get("style", {}).get("denominators", [4]), ctx)
    if dens:
        return max(1, int(dens[0]))
    return 4


def _seed_missing_segment_denominators(context=None):
    links = STATE.get("links", [])
    seg_map = STATE.get("segment_denominators", {})
    if not isinstance(seg_map, dict):
        seg_map = {}
    default_den = _context_default_segment_denominator(context)
    density_mode = STATE.get("curve_density_mode_by_link", {})
    if not isinstance(density_mode, dict):
        density_mode = {}
    changed = False
    for raw in links:
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        key = f"{norm[0]}:{norm[1]}"
        if str(density_mode.get(key, "") or "").strip().lower() == "follow":
            continue
        try:
            cur = int(seg_map.get(key, 0))
        except Exception:
            cur = 0
        if cur > 0:
            continue
        seg_map[key] = default_den
        changed = True
    if changed or not isinstance(STATE.get("segment_denominators"), dict):
        STATE["segment_denominators"] = seg_map
    return changed


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
    _set_segment_denominator(norm[0], norm[1], _context_default_segment_denominator())
    _set_segment_shape(norm[0], norm[1], _active_link_shape())
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
        _set_segment_denominator(norm[0], norm[1], 0)
        _set_segment_shape(norm[0], norm[1], "curve")
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
    valid_link_keys = set(f"{a}:{b}" for (a, b) in dedup)
    seg_map_raw = STATE.get("segment_denominators", {})
    seg_cleaned = {}
    if isinstance(seg_map_raw, dict):
        for key, raw_val in seg_map_raw.items():
            if key not in valid_link_keys:
                continue
            try:
                den = int(raw_val)
            except Exception:
                continue
            if den > 0:
                seg_cleaned[key] = den
    STATE["segment_denominators"] = seg_cleaned

    seg_shapes_raw = STATE.get("segment_shapes", {})
    seg_shapes_cleaned = {}
    if isinstance(seg_shapes_raw, dict):
        for key, raw_val in seg_shapes_raw.items():
            if key not in valid_link_keys:
                continue
            shape = _normalize_shape_name(raw_val)
            if shape == "polyline":
                seg_shapes_cleaned[key] = shape
    STATE["segment_shapes"] = seg_shapes_cleaned
    density_mode_raw = STATE.get("curve_density_mode_by_link", {})
    density_mode_cleaned = {}
    if isinstance(density_mode_raw, dict):
        for key, raw_val in density_mode_raw.items():
            if key not in valid_link_keys:
                continue
            mode = str(raw_val or "").strip().lower()
            if mode in ("fixed", "follow"):
                density_mode_cleaned[key] = mode
    STATE["curve_density_mode_by_link"] = density_mode_cleaned

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


def _add_selected_anchor(anchor_id):
    aid = int(anchor_id)
    if aid <= 0:
        return
    selected = [int(v) for v in STATE.get("selected_anchor_ids", []) if int(v) > 0]
    if aid not in selected:
        selected.append(aid)
    STATE["selected_anchor_ids"] = selected


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


def _find_segment_hit(context, cx, cy, threshold=14.0):
    best = None
    best_d = 1e9
    for _i0, _i1, id0, id1, a0, a1 in _connected_anchor_segments():
        sampled = _sample_segment_chart(a0, a1, 24, id0, id1)
        if len(sampled) < 2:
            continue
        prev = _chart_to_canvas(context, sampled[0][0], sampled[0][1])
        for lane_x, beat in sampled[1:]:
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
    if bool(STATE.get("note_curve_snap_enabled", False)) and kind in ("anchors", "segments"):
        return False
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
        hit = False
        for lane_x, beat in _sample_segment_chart(a0, a1, 24, id0, id1):
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
        _mark_dirty(context, flush=True)
    return changed


def _undo_history_from_host(context):
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))
    if not hist or idx <= 0:
        return False
    idx -= 1
    STATE["history_index"] = idx
    _restore_snapshot(hist[idx])
    _mark_dirty(context, flush=True)
    return True


def _redo_history_from_host(context):
    hist = STATE.get("history", [])
    idx = int(STATE.get("history_index", -1))
    if not hist or idx >= len(hist) - 1:
        return False
    idx += 1
    STATE["history_index"] = idx
    _restore_snapshot(hist[idx])
    _mark_dirty(context, flush=True)
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


def _note_curve_snap_enabled():
    return bool(STATE.get("note_curve_snap_enabled", False))


def _segment_point_chart(a0, a1, t, id0=None, id1=None):
    p0 = (a0["lane_x"], a0["beat"])
    p3 = (a1["lane_x"], a1["beat"])
    t = _clamp(float(t), 0.0, 1.0)
    if id0 is not None and id1 is not None:
        shape = _segment_shape_for_link(id0, id1)
    else:
        shape = _active_link_shape()
    if shape == "polyline":
        return p0[0] + (p3[0] - p0[0]) * t, p0[1] + (p3[1] - p0[1]) * t

    p1 = _anchor_out_abs_chart(a0)
    p2 = _anchor_in_abs_chart(a1)
    return _cubic_point(p0, p1, p2, p3, t)


def _sample_segment_chart(a0, a1, samples_per_segment=24, id0=None, id1=None):
    pts = []
    samples = max(1, int(samples_per_segment))
    for j in range(samples + 1):
        pts.append(_segment_point_chart(a0, a1, j / float(samples), id0, id1))
    return pts


def _sample_curve_chart(samples_per_segment=24):
    segments = _connected_anchor_segments()
    if not segments:
        return []

    pts = []
    for _i0, _i1, id0, id1, a0, a1 in segments:
        sampled = _sample_segment_chart(a0, a1, samples_per_segment, id0, id1)
        if pts and sampled:
            sampled = sampled[1:]
        pts.extend(sampled)
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


def _parse_int(value, fallback=0):
    try:
        return int(value)
    except Exception:
        return int(fallback)


def _parse_float(value, fallback=0.0):
    try:
        return float(value)
    except Exception:
        return float(fallback)


def _triplet_from_any(value, fallback_float=0.0):
    beat = _triplet_to_float(value)
    if beat is None:
        beat = _parse_float(value, fallback_float)
    return _float_to_triplet(float(beat), SERIALIZE_DEN)


def _beat_from_any(value, fallback_float=0.0):
    beat = _triplet_to_float(value)
    if beat is None:
        beat = _parse_float(value, fallback_float)
    return float(beat)


def _clone_dict_or_empty(value):
    return _clone(value) if isinstance(value, dict) else {}


def _unique_positive_int_list(value, default_id):
    out = []
    used = set()
    if isinstance(value, list):
        for raw in value:
            iv = _parse_int(raw, 0)
            if iv <= 0 or iv in used:
                continue
            used.add(iv)
            out.append(iv)
    if not out and default_id > 0:
        out = [int(default_id)]
    return out


def _next_available_positive(used, start=1):
    value = max(1, int(start))
    while value in used:
        value += 1
    return value


def _default_group_entry(group_id, name):
    return {"group_id": int(group_id), "group_name": str(name), "reserved": {}}


def _dedupe_group_names(groups):
    used_names = set()
    for g in groups:
        base = str(g.get("group_name", "") or "").strip()
        if not base:
            base = f"group_{int(g.get('group_id', 0))}"
        name = base
        suffix = 2
        while name.casefold() in used_names:
            name = f"{base}_{suffix}"
            suffix += 1
        used_names.add(name.casefold())
        g["group_name"] = name


def _normalize_group_entries(entries, default_group_id, default_name):
    raw_entries = entries if isinstance(entries, list) else []
    normalized = []
    used_ids = set()
    auto_id = max(2, int(STATE.get("next_group_id", 2)))

    for raw in raw_entries:
        if not isinstance(raw, dict):
            continue
        gid = _parse_int(raw.get("group_id", 0), 0)
        if gid <= 0 or gid in used_ids:
            gid = _next_available_positive(used_ids, auto_id)
            auto_id = gid + 1
        used_ids.add(gid)
        normalized.append({
            "group_id": gid,
            "group_name": str(raw.get("group_name", "") or "").strip(),
            "reserved": _clone_dict_or_empty(raw.get("reserved")),
        })

    if default_group_id not in used_ids:
        normalized.insert(0, _default_group_entry(default_group_id, default_name))
        used_ids.add(default_group_id)

    _dedupe_group_names(normalized)
    max_id = max(used_ids) if used_ids else 1
    STATE["next_group_id"] = max(int(STATE.get("next_group_id", 2)), max_id + 1)
    return normalized


def _ensure_groups_contain_ids(groups_key, required_ids, default_prefix):
    groups = STATE.get(groups_key, [])
    if not isinstance(groups, list):
        groups = []
    used_ids = set()
    for g in groups:
        if not isinstance(g, dict):
            continue
        gid = _parse_int(g.get("group_id", 0), 0)
        if gid > 0:
            used_ids.add(gid)
    for gid in required_ids:
        if gid in used_ids or gid <= 0:
            continue
        groups.append(_default_group_entry(gid, f"{default_prefix}_{gid}"))
        used_ids.add(gid)
    _dedupe_group_names(groups)
    STATE[groups_key] = groups
    if used_ids:
        STATE["next_group_id"] = max(int(STATE.get("next_group_id", 2)), max(used_ids) + 1)


def _cleanup_v3_metadata():
    valid_link_keys = set()
    for raw in STATE.get("links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        valid_link_keys.add(f"{norm[0]}:{norm[1]}")

    valid_anchor_ids = set(int(a.get("id", 0)) for a in STATE.get("anchors", []) if isinstance(a, dict))

    for key in ("anchor_group_ids", "anchor_reserved", "anchor_compat_handles"):
        raw_map = STATE.get(key, {})
        cleaned = {}
        if isinstance(raw_map, dict):
            for raw_id, raw_val in raw_map.items():
                aid = _parse_int(raw_id, 0)
                if aid <= 0 or aid not in valid_anchor_ids:
                    continue
                cleaned[str(aid)] = _clone(raw_val)
        STATE[key] = cleaned

    for key in (
        "curve_id_by_link",
        "curve_no_by_link",
        "curve_group_ids_by_link",
        "curve_density_mode_by_link",
        "curve_reserved_by_link",
        "curve_special_joystick_by_link",
    ):
        raw_map = STATE.get(key, {})
        cleaned = {}
        if isinstance(raw_map, dict):
            for raw_link_key, raw_val in raw_map.items():
                if raw_link_key not in valid_link_keys:
                    continue
                cleaned[raw_link_key] = _clone(raw_val)
        STATE[key] = cleaned

    required_node_group_ids = set()
    for aid in valid_anchor_ids:
        key = str(aid)
        gids = _unique_positive_int_list(STATE.get("anchor_group_ids", {}).get(key, []), DEFAULT_NODE_GROUP_ID)
        STATE["anchor_group_ids"][key] = gids
        required_node_group_ids.update(gids)
        STATE["anchor_reserved"][key] = _clone_dict_or_empty(STATE.get("anchor_reserved", {}).get(key))

    required_curve_group_ids = set()
    density_mode = STATE.get("curve_density_mode_by_link", {})
    if not isinstance(density_mode, dict):
        density_mode = {}
    for link_key in valid_link_keys:
        gids = _unique_positive_int_list(STATE.get("curve_group_ids_by_link", {}).get(link_key, []), DEFAULT_CURVE_GROUP_ID)
        STATE["curve_group_ids_by_link"][link_key] = gids
        required_curve_group_ids.update(gids)
        mode = str(density_mode.get(link_key, "") or "").strip().lower()
        if mode not in ("fixed", "follow"):
            den = _parse_int(STATE.get("segment_denominators", {}).get(link_key, 0), 0)
            density_mode[link_key] = "fixed" if den > 0 else "follow"
        STATE["curve_reserved_by_link"][link_key] = _clone_dict_or_empty(STATE.get("curve_reserved_by_link", {}).get(link_key))
        STATE["curve_special_joystick_by_link"][link_key] = _clone_dict_or_empty(
            STATE.get("curve_special_joystick_by_link", {}).get(link_key)
        )
    STATE["curve_density_mode_by_link"] = density_mode

    STATE["node_groups"] = _normalize_group_entries(STATE.get("node_groups", []), DEFAULT_NODE_GROUP_ID, "base")
    STATE["curve_groups"] = _normalize_group_entries(STATE.get("curve_groups", []), DEFAULT_CURVE_GROUP_ID, "base")
    _ensure_groups_contain_ids("node_groups", required_node_group_ids, "node_group")
    _ensure_groups_contain_ids("curve_groups", required_curve_group_ids, "curve_group")


def _ensure_curve_identity_and_numbers():
    _cleanup_links_and_selection()
    _cleanup_v3_metadata()

    ordered_keys = []
    for raw in STATE.get("links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        ordered_keys.append(f"{norm[0]}:{norm[1]}")

    existing_id_map = _clone_dict_or_empty(STATE.get("curve_id_by_link", {}))
    existing_no_map = _clone_dict_or_empty(STATE.get("curve_no_by_link", {}))
    id_out = {}
    no_out = {}

    used_ids = set()
    next_curve_id = max(1, _parse_int(STATE.get("next_curve_id", 1), 1))
    for key in ordered_keys:
        cid = _parse_int(existing_id_map.get(key, 0), 0)
        if cid <= 0 or cid in used_ids:
            cid = _next_available_positive(used_ids, next_curve_id)
        used_ids.add(cid)
        id_out[key] = cid
        if cid >= next_curve_id:
            next_curve_id = cid + 1

    used_numbers = set()
    for key in ordered_keys:
        cno = _parse_int(existing_no_map.get(key, 0), 0)
        if cno <= 0 or cno in used_numbers:
            cno = _next_available_positive(used_numbers, 1)
        used_numbers.add(cno)
        no_out[key] = cno

    STATE["curve_id_by_link"] = id_out
    STATE["curve_no_by_link"] = no_out
    STATE["next_curve_id"] = max(1, next_curve_id)


def _serialize_node_for_v3(anchor):
    aid = int(anchor.get("id", 0))
    key = str(aid)
    out_handle = anchor.get("out", [0.0, 0.0])
    out_dx = _parse_float(out_handle[0] if isinstance(out_handle, list) and len(out_handle) >= 1 else 0.0, 0.0)
    out_db = _parse_float(out_handle[1] if isinstance(out_handle, list) and len(out_handle) >= 2 else 0.0, 0.0)
    compat_raw = STATE.get("anchor_compat_handles", {}).get(key, {})
    compat = _clone_dict_or_empty(compat_raw)
    if not compat:
        compat = {
            "in": {
                "lane_dx": _parse_float(anchor.get("in", [0.0, 0.0])[0] if isinstance(anchor.get("in"), list) else 0.0, 0.0),
                "beat_delta": _triplet_from_any(anchor.get("in", [0.0, 0.0])[1] if isinstance(anchor.get("in"), list) else 0.0, 0.0),
            },
            "out": {
                "lane_dx": out_dx,
                "beat_delta": _triplet_from_any(out_db, out_db),
            },
        }
    return {
        "node_id": aid,
        "lane_x": _parse_float(anchor.get("lane_x", 0.0), 0.0),
        "beat": _triplet_from_any(anchor.get("beat", 0.0), 0.0),
        "joystick": {"lane_dx": out_dx, "beat_delta": _triplet_from_any(out_db, out_db)},
        "group_ids": _unique_positive_int_list(STATE.get("anchor_group_ids", {}).get(key, []), DEFAULT_NODE_GROUP_ID),
        "reserved": _clone_dict_or_empty(STATE.get("anchor_reserved", {}).get(key)),
        "compat_handles": compat,
        "smooth": bool(anchor.get("smooth", True)),
    }


def _serialize_curve_for_v3(norm):
    key = f"{norm[0]}:{norm[1]}"
    den = _parse_int(STATE.get("segment_denominators", {}).get(key, 0), 0)
    density_mode = str(STATE.get("curve_density_mode_by_link", {}).get(key, "") or "").strip().lower()
    if density_mode == "follow":
        density = {"mode": "follow"}
    elif den > 0:
        density = {"mode": "fixed", "denominator": den}
    else:
        density = {"mode": "follow"}
    shape = _normalize_shape_name(STATE.get("segment_shapes", {}).get(key, "curve"))
    return {
        "curve_id": _parse_int(STATE.get("curve_id_by_link", {}).get(key, 0), 0),
        "curve_no": _parse_int(STATE.get("curve_no_by_link", {}).get(key, 0), 0),
        "node_ids": [int(norm[0]), int(norm[1])],
        "density": density,
        "style_category": shape,
        "group_ids": _unique_positive_int_list(STATE.get("curve_group_ids_by_link", {}).get(key, []), DEFAULT_CURVE_GROUP_ID),
        "special_joystick_reserved": _clone_dict_or_empty(STATE.get("curve_special_joystick_by_link", {}).get(key)),
        "reserved": _clone_dict_or_empty(STATE.get("curve_reserved_by_link", {}).get(key)),
    }


def _build_v3_payload():
    _ensure_curve_identity_and_numbers()
    anchors = STATE.get("anchors", [])
    nodes = [_serialize_node_for_v3(a) for a in anchors]

    curves = []
    for raw in STATE.get("links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        curves.append(_serialize_curve_for_v3(norm))

    revision = max(0, _parse_int(STATE.get("project_revision", 0), 0))
    file_uuid = str(STATE.get("project_file_uuid", "") or "").strip()
    if not file_uuid:
        file_uuid = uuid.uuid4().hex
        STATE["project_file_uuid"] = file_uuid
    return {
        "format_version": CURVE_SIDECAR_FORMAT_VERSION,
        "coordinate_space": "chart",
        "revision": revision,
        "file_uuid": file_uuid,
        "updated_at": int(time.time() * 1000),
        "last_writer_instance": str(STATE.get("instance_id", "") or ""),
        "nodes": nodes,
        "curves": curves,
        "node_groups": _clone(STATE.get("node_groups", [])),
        "curve_groups": _clone(STATE.get("curve_groups", [])),
        "style": STATE.get("style", {"denominators": [4, 8, 12, 16], "style_name": "balanced"}),
        "active_link_shape": _active_link_shape(),
        "note_curve_snap_enabled": _note_curve_snap_enabled(),
    }


def _set_save_error(code, detail=""):
    STATE["last_save_error"] = str(code or "")
    STATE["last_save_error_detail"] = str(detail or "")
    if code:
        try:
            sys.stderr.write(f"[note_chain_assist] save failed: {code} {detail}\n")
            sys.stderr.flush()
        except Exception:
            pass


def _read_disk_payload(path):
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _save_project(path, context=None):
    if not isinstance(path, str) or not path.strip():
        _set_save_error("invalid_path")
        return False
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)

        disk_payload = None
        if os.path.exists(path):
            try:
                disk_payload = _read_disk_payload(path)
            except Exception as ex:
                _set_save_error("read_existing_failed", str(ex))
                return False

        disk_revision = 0
        if isinstance(disk_payload, dict):
            disk_revision = max(0, _parse_int(disk_payload.get("revision", 0), 0))
        state_revision = max(0, _parse_int(STATE.get("project_revision", 0), 0))
        if disk_revision != state_revision:
            _set_save_error("revision_conflict", "file updated by another instance, please refresh")
            return False

        payload = _build_v3_payload()
        payload["revision"] = disk_revision + 1
        payload["updated_at"] = int(time.time() * 1000)
        payload["last_writer_instance"] = str(STATE.get("instance_id", "") or "")

        tmp_path = f"{path}.tmp.{os.getpid()}.{int(time.time() * 1000)}"
        with open(tmp_path, "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)
        os.replace(tmp_path, path)

        STATE["project_dirty"] = False
        STATE["project_revision"] = int(payload.get("revision", 0))
        STATE["project_file_uuid"] = str(payload.get("file_uuid", "") or "")
        STATE["project_last_writer_instance"] = str(payload.get("last_writer_instance", "") or "")
        _set_save_error("", "")
        return True
    except Exception as ex:
        _set_save_error("write_failed", str(ex))
        return False


def _load_project_v2_payload(payload, context):
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
    seg_dens = payload.get("segment_denominators")
    STATE["segment_denominators"] = _clone(seg_dens) if isinstance(seg_dens, dict) else {}
    seg_shapes = payload.get("segment_shapes")
    if isinstance(seg_shapes, dict):
        STATE["segment_shapes"] = _clone(seg_shapes)
    else:
        legacy_shape = _normalize_shape_name(payload.get("curve_shape", "curve"))
        STATE["segment_shapes"] = {}
        if legacy_shape == "polyline":
            for raw in STATE.get("links", []):
                if isinstance(raw, list) and len(raw) == 2:
                    norm = _normalize_link(raw[0], raw[1])
                    if norm is not None:
                        STATE["segment_shapes"][f"{norm[0]}:{norm[1]}"] = "polyline"

    STATE["anchor_group_ids"] = {}
    STATE["anchor_reserved"] = {}
    STATE["anchor_compat_handles"] = {}
    for a in STATE.get("anchors", []):
        aid = int(a.get("id", 0))
        if aid <= 0:
            continue
        key = str(aid)
        STATE["anchor_group_ids"][key] = [DEFAULT_NODE_GROUP_ID]
        STATE["anchor_reserved"][key] = {}
        STATE["anchor_compat_handles"][key] = {
            "in": {
                "lane_dx": _parse_float(a.get("in", [0.0, 0.0])[0] if isinstance(a.get("in"), list) else 0.0, 0.0),
                "beat_delta": _triplet_from_any(a.get("in", [0.0, 0.0])[1] if isinstance(a.get("in"), list) else 0.0, 0.0),
            },
            "out": {
                "lane_dx": _parse_float(a.get("out", [0.0, 0.0])[0] if isinstance(a.get("out"), list) else 0.0, 0.0),
                "beat_delta": _triplet_from_any(a.get("out", [0.0, 0.0])[1] if isinstance(a.get("out"), list) else 0.0, 0.0),
            },
        }

    STATE["curve_id_by_link"] = {}
    STATE["curve_no_by_link"] = {}
    STATE["curve_group_ids_by_link"] = {}
    STATE["curve_density_mode_by_link"] = {}
    STATE["curve_reserved_by_link"] = {}
    STATE["curve_special_joystick_by_link"] = {}
    STATE["node_groups"] = [_default_group_entry(DEFAULT_NODE_GROUP_ID, "base")]
    STATE["curve_groups"] = [_default_group_entry(DEFAULT_CURVE_GROUP_ID, "base")]

    style = payload.get("style")
    if isinstance(style, dict):
        STATE["style"] = {
            "style_name": str(style.get("style_name", "loaded")),
            "denominators": _sanitize_denominators(style.get("denominators"), context),
        }
    active_shape = payload.get("active_link_shape", payload.get("curve_shape", "curve"))
    STATE["active_link_shape"] = _normalize_shape_name(active_shape)
    STATE["note_curve_snap_enabled"] = bool(payload.get("note_curve_snap_enabled", False))
    STATE["project_revision"] = 0
    STATE["project_file_uuid"] = str(STATE.get("project_file_uuid", "") or "") or uuid.uuid4().hex
    STATE["project_last_writer_instance"] = str(payload.get("last_writer_instance", "") or "")
    _cleanup_links_and_selection()
    _seed_missing_segment_denominators(context)
    _ensure_curve_identity_and_numbers()


def _load_project_v3_payload(payload, context):
    nodes = payload.get("nodes")
    parsed_anchors = []
    anchor_group_ids = {}
    anchor_reserved = {}
    anchor_compat = {}
    if isinstance(nodes, list):
        for raw in nodes:
            if not isinstance(raw, dict):
                continue
            aid = _parse_int(raw.get("node_id", raw.get("id", 0)), 0)
            if aid <= 0:
                continue
            lane_x = _parse_float(raw.get("lane_x", 0.0), 0.0)
            beat = _beat_from_any(raw.get("beat"), 0.0)
            joystick_raw = raw.get("joystick")
            if isinstance(joystick_raw, dict):
                joy_dx = _parse_float(joystick_raw.get("lane_dx", 0.0), 0.0)
                joy_db = _beat_from_any(joystick_raw.get("beat_delta"), 0.0)
            else:
                compat_raw = raw.get("compat_handles", {})
                out_raw = compat_raw.get("out", {}) if isinstance(compat_raw, dict) else {}
                joy_dx = _parse_float(out_raw.get("lane_dx", 0.0), 0.0)
                joy_db = _beat_from_any(out_raw.get("beat_delta"), 0.0)

            lane_x, beat = _snap_chart_point(context, lane_x, beat)
            parsed_anchors.append({
                "id": aid,
                "lane_x": lane_x,
                "beat": beat,
                "in": [-joy_dx, -joy_db],
                "out": [joy_dx, joy_db],
                "smooth": bool(raw.get("smooth", True)),
            })
            key = str(aid)
            anchor_group_ids[key] = _unique_positive_int_list(raw.get("group_ids"), DEFAULT_NODE_GROUP_ID)
            anchor_reserved[key] = _clone_dict_or_empty(raw.get("reserved"))
            compat_payload = raw.get("compat_handles")
            if isinstance(compat_payload, dict) and compat_payload:
                anchor_compat[key] = _clone_dict_or_empty(compat_payload)
            else:
                anchor_compat[key] = {
                    "in": {"lane_dx": -joy_dx, "beat_delta": _triplet_from_any(-joy_db, -joy_db)},
                    "out": {"lane_dx": joy_dx, "beat_delta": _triplet_from_any(joy_db, joy_db)},
                }

    parsed_anchors.sort(key=lambda a: (float(a.get("beat", 0.0)), float(a.get("lane_x", 0.0)), int(a.get("id", 0))))
    STATE["anchors"] = parsed_anchors
    _ensure_anchor_ids()

    curve_id_by_link = {}
    curve_no_by_link = {}
    curve_group_ids_by_link = {}
    curve_density_mode_by_link = {}
    curve_reserved_by_link = {}
    curve_special_joystick_by_link = {}
    links = []
    dedup_links = set()
    curves = payload.get("curves")
    if isinstance(curves, list):
        for raw in curves:
            if not isinstance(raw, dict):
                continue
            node_ids = raw.get("node_ids")
            if not isinstance(node_ids, list) or len(node_ids) != 2:
                continue
            norm = _normalize_link(node_ids[0], node_ids[1])
            if norm is None or norm in dedup_links:
                continue
            dedup_links.add(norm)
            links.append([norm[0], norm[1]])
            key = f"{norm[0]}:{norm[1]}"
            curve_id_by_link[key] = _parse_int(raw.get("curve_id", 0), 0)
            curve_no_by_link[key] = _parse_int(raw.get("curve_no", 0), 0)
            curve_group_ids_by_link[key] = _unique_positive_int_list(raw.get("group_ids"), DEFAULT_CURVE_GROUP_ID)
            curve_reserved_by_link[key] = _clone_dict_or_empty(raw.get("reserved"))
            curve_special_joystick_by_link[key] = _clone_dict_or_empty(raw.get("special_joystick_reserved"))

            density = raw.get("density", {})
            mode = str(density.get("mode", "follow") if isinstance(density, dict) else "follow").strip().lower()
            if mode == "fixed":
                den = _parse_int(density.get("denominator", 0), 0) if isinstance(density, dict) else 0
                if den > 0:
                    STATE["segment_denominators"][key] = den
                    curve_density_mode_by_link[key] = "fixed"
            else:
                curve_density_mode_by_link[key] = "follow"
            style_category = raw.get("style_category", "curve")
            if _normalize_shape_name(style_category) == "polyline":
                STATE["segment_shapes"][key] = "polyline"

    STATE["links"] = links if links else _default_links_for_anchors()
    STATE["anchor_group_ids"] = anchor_group_ids
    STATE["anchor_reserved"] = anchor_reserved
    STATE["anchor_compat_handles"] = anchor_compat
    STATE["curve_id_by_link"] = curve_id_by_link
    STATE["curve_no_by_link"] = curve_no_by_link
    STATE["curve_group_ids_by_link"] = curve_group_ids_by_link
    STATE["curve_density_mode_by_link"] = curve_density_mode_by_link
    STATE["curve_reserved_by_link"] = curve_reserved_by_link
    STATE["curve_special_joystick_by_link"] = curve_special_joystick_by_link
    STATE["node_groups"] = _normalize_group_entries(payload.get("node_groups", []), DEFAULT_NODE_GROUP_ID, "base")
    STATE["curve_groups"] = _normalize_group_entries(payload.get("curve_groups", []), DEFAULT_CURVE_GROUP_ID, "base")

    style = payload.get("style")
    if isinstance(style, dict):
        STATE["style"] = {
            "style_name": str(style.get("style_name", "loaded")),
            "denominators": _sanitize_denominators(style.get("denominators"), context),
        }
    active_shape = payload.get("active_link_shape", payload.get("curve_shape", "curve"))
    STATE["active_link_shape"] = _normalize_shape_name(active_shape)
    STATE["note_curve_snap_enabled"] = bool(payload.get("note_curve_snap_enabled", False))
    STATE["project_revision"] = max(0, _parse_int(payload.get("revision", 0), 0))
    STATE["project_file_uuid"] = str(payload.get("file_uuid", "") or "").strip() or uuid.uuid4().hex
    STATE["project_last_writer_instance"] = str(payload.get("last_writer_instance", "") or "")
    _cleanup_links_and_selection()
    _seed_missing_segment_denominators(context)
    _ensure_curve_identity_and_numbers()


def _load_project(path, context):
    if not isinstance(path, str) or not path.strip() or not os.path.exists(path):
        return False
    try:
        with open(path, "r", encoding="utf-8") as f:
            payload = json.load(f)

        STATE["segment_denominators"] = {}
        STATE["segment_shapes"] = {}
        format_version = _parse_int(payload.get("format_version", 0), 0)
        if format_version >= CURVE_SIDECAR_FORMAT_VERSION or ("nodes" in payload and "curves" in payload):
            _load_project_v3_payload(payload, context)
        else:
            _load_project_v2_payload(payload, context)

        _invalidate_curve_cache()
        STATE["project_dirty"] = False
        _set_save_error("", "")
        return True
    except Exception as ex:
        _set_save_error("load_failed", str(ex))
        return False


def _ensure_project_context(context):
    path = ""
    if isinstance(context, dict):
        path = str(context.get("curve_project_path", "") or "").strip()
    if not path:
        return

    if STATE["project_path"] != path:
        if STATE["project_path"] and STATE["project_dirty"]:
            if not bool(STATE.get("suppress_persist_once", False)):
                _save_project(STATE["project_path"], context)
            else:
                STATE["project_dirty"] = False
        STATE["project_path"] = path
        STATE["suppress_persist_once"] = False
        _try_seed_curve_project_from_source(context, path)
        if not _load_project(path, context):
            STATE["anchors"] = _default_anchors()
            _ensure_anchor_ids()
            STATE["links"] = _default_links_for_anchors()
            STATE["segment_denominators"] = {}
            STATE["segment_shapes"] = {}
            STATE["anchor_group_ids"] = {}
            STATE["anchor_reserved"] = {}
            STATE["anchor_compat_handles"] = {}
            STATE["curve_id_by_link"] = {}
            STATE["curve_no_by_link"] = {}
            STATE["curve_group_ids_by_link"] = {}
            STATE["curve_density_mode_by_link"] = {}
            STATE["curve_reserved_by_link"] = {}
            STATE["curve_special_joystick_by_link"] = {}
            STATE["node_groups"] = [_default_group_entry(DEFAULT_NODE_GROUP_ID, "base")]
            STATE["curve_groups"] = [_default_group_entry(DEFAULT_CURVE_GROUP_ID, "base")]
            STATE["next_curve_id"] = 1
            STATE["next_group_id"] = 2
            STATE["project_revision"] = 0
            STATE["project_file_uuid"] = uuid.uuid4().hex
            STATE["project_last_writer_instance"] = ""
            _set_save_error("", "")
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


def _mark_dirty(context, flush=False):
    STATE["project_dirty"] = True
    if not flush:
        return
    if bool(STATE.get("suppress_persist_once", False)):
        return
    if isinstance(context, dict):
        _ensure_project_context(context)
    if STATE["project_path"]:
        _save_project(STATE["project_path"], context)


def _build_overlay(context):
    _ensure_project_context(context)
    if not bool(STATE.get("curve_visible", True)):
        return []
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
            is_selected_segment = (id0, id1) in selected_link_set
            seg_color = "#FFD66B" if is_selected_segment else "#33CCFF"
            seg_width = 3.0 if is_selected_segment else 2.0
            sampled = _sample_segment_chart(a0, a1, 24, id0, id1)
            for j in range(len(sampled) - 1):
                line_item = {
                    "kind": "line",
                    "coord_space": "chart",
                    "lane_x1": sampled[j][0],
                    "beat1": sampled[j][1],
                    "lane_x2": sampled[j + 1][0],
                    "beat2": sampled[j + 1][1],
                    "color": seg_color,
                    "width": seg_width,
                }
                if _note_curve_snap_enabled():
                    line_item["note_snap_reference"] = True
                items.append(line_item)
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

    link_drag = STATE.get("link_drag", {})
    if bool(link_drag.get("active", False)):
        idx_map = _anchor_index_map()
        source_id = int(link_drag.get("source_anchor_id", -1))
        source_idx = idx_map.get(source_id, -1)
        if 0 <= source_idx < len(anchors):
            src = anchors[source_idx]
            sx, sy = _chart_to_canvas(context, src["lane_x"], src["beat"])
            tx = float(link_drag.get("x", sx))
            ty = float(link_drag.get("y", sy))
            hover_id = int(link_drag.get("hover_anchor_id", -1))
            hover_idx = idx_map.get(hover_id, -1)
            if 0 <= hover_idx < len(anchors):
                dst = anchors[hover_idx]
                tx, ty = _chart_to_canvas(context, dst["lane_x"], dst["beat"])
                items.append({
                    "kind": "rect",
                    "coord_space": "chart",
                    "lane_x": dst["lane_x"],
                    "beat": dst["beat"],
                    "w": 16,
                    "h": 16,
                    "rect_anchor": "center",
                    "color": "#FFE08A",
                    "fill_color": "#33FFE08A",
                    "width": 2.0,
                })
            items.append({
                "kind": "line",
                "x1": sx,
                "y1": sy,
                "x2": tx,
                "y2": ty,
                "color": "#FFE08A",
                "width": 2.0,
            })

    return items


def _reset_anchors(context):
    STATE["anchors"] = _default_anchors()
    STATE["links"] = []
    STATE["segment_denominators"] = {}
    STATE["segment_shapes"] = {}
    STATE["anchor_group_ids"] = {}
    STATE["anchor_reserved"] = {}
    STATE["anchor_compat_handles"] = {}
    STATE["curve_id_by_link"] = {}
    STATE["curve_no_by_link"] = {}
    STATE["curve_group_ids_by_link"] = {}
    STATE["curve_density_mode_by_link"] = {}
    STATE["curve_reserved_by_link"] = {}
    STATE["curve_special_joystick_by_link"] = {}
    STATE["node_groups"] = [_default_group_entry(DEFAULT_NODE_GROUP_ID, "base")]
    STATE["curve_groups"] = [_default_group_entry(DEFAULT_CURVE_GROUP_ID, "base")]
    STATE["next_curve_id"] = 1
    STATE["next_group_id"] = 2
    STATE["drag"] = {"mode": "", "index": -1}
    STATE["selected_anchor_ids"] = []
    STATE["selected_links"] = []
    STATE["context_menu_links"] = []
    STATE["link_drag"] = {"active": False, "source_anchor_id": -1, "hover_anchor_id": -1, "x": 0.0, "y": 0.0}
    STATE["pending_connect_anchor_id"] = -1
    _invalidate_curve_cache()
    _record_history_state(context)


def _append_anchor(context, cx, cy):
    lane_x, beat = _canvas_to_chart(context, cx, cy)
    lane_x, beat = _snap_chart_point(context, lane_x, beat)
    anchors = STATE["anchors"]

    if not anchors:
        anchors.append({"id": _new_anchor_id(), "lane_x": lane_x, "beat": beat, "in": [-16.0, 0.0], "out": [16.0, 0.0], "smooth": True})
        return len(anchors) - 1

    idx = len(anchors)
    for i, a in enumerate(anchors):
        if beat < float(a.get("beat", 0.0)):
            idx = i
            break

    ref = None
    if idx > 0 and idx < len(anchors):
        prev = anchors[idx - 1]
        nxt = anchors[idx]
        prev_dist = abs(beat - float(prev.get("beat", beat)))
        next_dist = abs(float(nxt.get("beat", beat)) - beat)
        ref = prev if prev_dist <= next_dist else nxt
    elif idx > 0:
        ref = anchors[idx - 1]
    elif idx < len(anchors):
        ref = anchors[idx]

    if ref is None:
        ol, ob = 16.0, 0.0
    else:
        if beat <= float(ref.get("beat", beat)):
            dl = float(ref.get("lane_x", lane_x)) - lane_x
            db = float(ref.get("beat", beat)) - beat
        else:
            dl = lane_x - float(ref.get("lane_x", lane_x))
            db = beat - float(ref.get("beat", beat))
        # Split axis scaling to avoid overly weak handles on lane axis.
        ol = _clamp(dl * 0.25, -96.0, 96.0)
        ob = _clamp(db * 0.25, -2.0, 2.0)

    anchors.insert(idx, {"id": _new_anchor_id(), "lane_x": lane_x, "beat": beat, "in": [-ol, -ob], "out": [ol, ob], "smooth": True})
    _enforce_anchor_time_order(idx, context)
    _enforce_handle_time_constraints(idx, context)
    _enforce_handle_time_constraints(idx - 1, context)
    _enforce_handle_time_constraints(idx + 1, context)
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


def _beat_fraction_from_triplet(triplet):
    if not isinstance(triplet, list) or len(triplet) != 3:
        return None
    try:
        beat_num = int(triplet[0])
        numerator = int(triplet[1])
        denominator = int(triplet[2])
    except Exception:
        return None
    if denominator <= 0:
        return None
    return Fraction(beat_num, 1) + Fraction(numerator, denominator)


def _normal_note_position_key(note_obj):
    if not isinstance(note_obj, dict):
        return None
    try:
        note_type = int(note_obj.get("type", 0) or 0)
    except Exception:
        note_type = 0
    if note_type != 0:
        return None

    beat = _beat_fraction_from_triplet(note_obj.get("beat"))
    if beat is None:
        return None
    try:
        x = int(round(float(note_obj.get("x", note_obj.get("lane_x", 0)))))
    except Exception:
        return None
    return beat, x


def _existing_normal_note_position_keys(context):
    keys = set()
    if not isinstance(context, dict):
        return keys

    raw_positions = context.get("existing_note_positions")
    if isinstance(raw_positions, list):
        for raw in raw_positions:
            key = _normal_note_position_key(raw)
            if key is not None:
                keys.add(key)

    if keys:
        return keys

    chart_path = str(context.get("chart_path", "") or "").strip()
    if not chart_path or not os.path.exists(chart_path):
        return keys
    try:
        with open(chart_path, "r", encoding="utf-8") as f:
            payload = json.load(f)
    except Exception:
        return keys

    notes = payload.get("note") if isinstance(payload, dict) else None
    if not isinstance(notes, list):
        return keys
    for raw in notes:
        key = _normal_note_position_key(raw)
        if key is not None:
            keys.add(key)
    return keys


def _sample_single_segment_chart(a0, a1, samples_per_segment=24, id0=None, id1=None):
    return _sample_segment_chart(a0, a1, samples_per_segment, id0, id1)


def _connected_curve_segment_groups():
    segments = _connected_anchor_segments()
    if not segments:
        return []

    by_anchor = {}
    for seg in segments:
        id0 = int(seg[2])
        id1 = int(seg[3])
        by_anchor.setdefault(id0, []).append(seg)
        by_anchor.setdefault(id1, []).append(seg)

    visited_anchor_ids = set()
    grouped = []
    for anchor_id in by_anchor.keys():
        if anchor_id in visited_anchor_ids:
            continue
        queue = [anchor_id]
        comp_anchor_ids = set()
        comp_segments = []
        seen_seg_keys = set()
        while queue:
            cur = int(queue.pop())
            if cur in comp_anchor_ids:
                continue
            comp_anchor_ids.add(cur)
            for seg in by_anchor.get(cur, []):
                id0 = int(seg[2])
                id1 = int(seg[3])
                key = (id0, id1)
                if key not in seen_seg_keys:
                    seen_seg_keys.add(key)
                    comp_segments.append(seg)
                if id0 not in comp_anchor_ids:
                    queue.append(id0)
                if id1 not in comp_anchor_ids:
                    queue.append(id1)
        visited_anchor_ids.update(comp_anchor_ids)
        comp_segments.sort(key=lambda s: (min(float(s[4]["beat"]), float(s[5]["beat"])), max(float(s[4]["beat"]), float(s[5]["beat"]))))
        grouped.append(comp_segments)
    return grouped


def _build_batch_from_curve(context, target_links=None):
    if not isinstance(context, dict):
        return {"add": [], "remove": [], "move": []}

    segments = _connected_anchor_segments()
    if target_links is not None:
        target_set = set()
        for raw in target_links:
            if not isinstance(raw, (list, tuple)) or len(raw) != 2:
                continue
            norm = _normalize_link(raw[0], raw[1])
            if norm is not None:
                target_set.add(norm)
        if not target_set:
            return {"add": [], "remove": [], "move": []}
        segments = [seg for seg in segments if _normalize_link(seg[2], seg[3]) in target_set]

    if not segments:
        return {"add": [], "remove": [], "move": []}

    override_den = int(context.get("plugin_time_division_override", 0) or 0)
    default_den = max(1, override_den if override_den > 0 else int(context.get("time_division", 4)))

    out = []
    existing = _existing_normal_note_position_keys(context)
    seen = set()
    for _i0, _i1, id0, id1, a0, a1 in segments:
        seg_den = _segment_denominator_for_link(id0, id1, default_den)
        sampled_segment = _sample_single_segment_chart(a0, a1, 32, id0, id1)
        samples_by_beat = _normalize_samples_by_beat(sampled_segment)
        if len(samples_by_beat) < 2:
            continue

        seg_start = float(samples_by_beat[0][1])
        seg_end = float(samples_by_beat[-1][1])
        lo = min(seg_start, seg_end)
        hi = max(seg_start, seg_end)

        start_tick = int(round(float(lo) * seg_den))
        end_tick = int(round(float(hi) * seg_den))
        if end_tick < start_tick:
            start_tick, end_tick = end_tick, start_tick

        for tick in range(start_tick, end_tick + 1):
            beat = float(tick) / float(seg_den)
            if beat < lo or beat > hi:
                continue
            lane_x = _lane_x_at_beat(samples_by_beat, beat)
            note = _chart_to_note(context, lane_x, beat, seg_den)
            key = _normal_note_position_key(note)
            if key is None or key in existing or key in seen:
                continue
            seen.add(key)
            out.append(note)

    out.sort(key=lambda n: (int((n.get("beat") or [0, 0, 1])[0]),
                            int((n.get("beat") or [0, 0, 1])[1]),
                            int((n.get("beat") or [0, 0, 1])[2]),
                            int(n.get("x", 0))))

    return {"add": out, "remove": [], "move": []}


def _sync_anchor_placement_with_host_mode(context):
    if not isinstance(context, dict):
        return
    host_sel = context.get("host_selection_tool", {})
    if not isinstance(host_sel, dict):
        return
    mode = str(host_sel.get("mode", "")).strip().lower()
    if mode == "anchor_place":
        STATE["anchor_placement_enabled"] = True
    elif mode in ("place_note", "place_rain", "delete", "select"):
        STATE["anchor_placement_enabled"] = False


def _host_controls_anchor_mode(context):
    if not isinstance(context, dict):
        return False
    host_sel = context.get("host_selection_tool", {})
    if not isinstance(host_sel, dict):
        return False
    mode = str(host_sel.get("mode", "")).strip().lower()
    return mode in ("anchor_place", "place_note", "place_rain", "delete", "select")


def _selected_target_links():
    selected_links = []
    for raw in STATE.get("selected_links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is None:
            continue
        selected_links.append(norm)

    if selected_links:
        return selected_links

    selected_anchor_ids = [int(v) for v in STATE.get("selected_anchor_ids", []) if int(v) > 0]
    if len(selected_anchor_ids) == 2:
        norm = _normalize_link(selected_anchor_ids[0], selected_anchor_ids[1])
        if norm is not None and _link_exists(norm[0], norm[1]):
            return [norm]
    return []


def _selected_links_all_polyline():
    links = _selected_target_links()
    if not links:
        return _active_link_shape_is_polyline()
    return all(_segment_shape_for_link(id0, id1) == "polyline" for id0, id1 in links)


def _toggle_polyline_for_active_or_selected(context):
    links = _selected_target_links()
    target = "polyline"
    if links and all(_segment_shape_for_link(id0, id1) == "polyline" for id0, id1 in links):
        target = "curve"
    elif not links and _active_link_shape_is_polyline():
        target = "curve"

    STATE["active_link_shape"] = target
    changed = False
    for id0, id1 in links:
        changed = _set_segment_shape(id0, id1, target) or changed
    if links and changed:
        _invalidate_curve_cache()
    _record_history_state(context)
    return True


def _set_context_menu_links_for_hit(context, x, y):
    hit = _find_segment_hit(context, x, y)
    selected = _selected_target_links()
    selected_set = set(selected)

    target = []
    if hit is not None:
        norm = _normalize_link(hit[0], hit[1])
        if norm is not None:
            if norm in selected_set and len(selected) > 1:
                target = selected
            else:
                target = [norm]
    elif selected:
        target = selected

    STATE["context_menu_links"] = [[int(a), int(b)] for a, b in target]
    return STATE["context_menu_links"]


def _context_menu_target_links():
    target = []
    for raw in STATE.get("context_menu_links", []):
        if not isinstance(raw, list) or len(raw) != 2:
            continue
        norm = _normalize_link(raw[0], raw[1])
        if norm is not None and _link_exists(norm[0], norm[1]):
            target.append(norm)
    if target:
        return target
    return _selected_target_links()


def _context_links_all_polyline():
    links = _context_menu_target_links()
    return bool(links) and all(_segment_shape_for_link(id0, id1) == "polyline" for id0, id1 in links)


def _toggle_polyline_for_context_links(context):
    links = _context_menu_target_links()
    if not links:
        return False

    target = "curve" if all(_segment_shape_for_link(id0, id1) == "polyline" for id0, id1 in links) else "polyline"
    changed = False
    for id0, id1 in links:
        changed = _set_segment_shape(id0, id1, target) or changed
    if changed:
        _invalidate_curve_cache()
        _record_history_state(context)
    return True


def _set_density_for_selected_segments(context, target_den):
    links = _selected_target_links()
    if not links:
        return False
    den = int(target_den)
    if den <= 0:
        den = _context_default_segment_denominator(context)
    changed = False
    for id0, id1 in links:
        changed = _set_segment_denominator(id0, id1, den) or changed
    if changed:
        _record_history_state(context)
    return changed


def _sync_anchor_selection_from_host_notes(context):
    if not isinstance(context, dict):
        return
    if not _selection_enabled("notes") or not _selection_enabled("anchors"):
        STATE["last_host_selected_note_ids"] = []
        return
    if STATE.get("drag", {}).get("mode") or bool(STATE.get("box_select", {}).get("active", False)):
        return

    raw_ids = context.get("selected_note_ids")
    selected_ids = [str(v) for v in raw_ids if isinstance(v, str) and v] if isinstance(raw_ids, list) else []
    if selected_ids == list(STATE.get("last_host_selected_note_ids", [])):
        return
    STATE["last_host_selected_note_ids"] = list(selected_ids)

    selected_notes = context.get("selected_notes")
    if not isinstance(selected_notes, list) or not selected_notes:
        return

    idx_map = _anchor_index_map()
    anchors = STATE.get("anchors", [])
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
    STATE["selected_anchor_ids"] = picked


def _handle_canvas_input(payload):
    context = payload.get("context", {}) if isinstance(payload, dict) else {}
    event = payload.get("event", {}) if isinstance(payload, dict) else {}
    STATE["last_context"] = context if isinstance(context, dict) else {}
    _ensure_project_context(STATE["last_context"])
    _seed_missing_segment_denominators(STATE["last_context"])
    _sync_anchor_placement_with_host_mode(STATE["last_context"])
    _sync_anchor_selection_from_host_notes(STATE["last_context"])

    et = str(event.get("type", ""))
    x = float(event.get("x", 0.0))
    y = float(event.get("y", 0.0))
    button = int(event.get("button", 0))
    ts = int(event.get("timestamp_ms", int(time.time() * 1000)))

    consumed = False
    status = ""
    cursor = "arrow"
    request_checkpoint = False

    if _note_curve_snap_enabled() and et in ("mouse_down", "mouse_move", "mouse_up"):
        buttons = int(event.get("buttons", 0))
        if button == LEFT_BUTTON or (buttons & LEFT_BUTTON):
            return {
                "consumed": False,
                "overlay": _build_overlay(STATE["last_context"]),
                "cursor": "arrow",
                "status_text": "",
                "preview_batch_edit": {"add": [], "remove": [], "move": []},
                "request_undo_checkpoint": False,
                "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
            }

    if et == "mouse_down":
        hkind, hidx = _find_handle_hit(STATE["last_context"], x, y)
        aidx = _find_anchor_hit(STATE["last_context"], x, y) if _selection_enabled("anchors") else -1
        seg_hit = _find_segment_hit(STATE["last_context"], x, y) if _selection_enabled("segments") else None
        mods = int(event.get("modifiers", 0))
        ctrl = (mods & CTRL_MODIFIER_MASK) != 0
        shift = _event_has_shift(event) or bool(STATE.get("shift_down", False))
        host_sel = STATE["last_context"].get("host_selection_tool", {}) if isinstance(STATE["last_context"], dict) else {}
        is_select_mode = bool(host_sel.get("is_select_mode", False)) if isinstance(host_sel, dict) else False

        if button == RIGHT_BUTTON:
            _set_context_menu_links_for_hit(STATE["last_context"], x, y)
            consumed = False
        elif button == LEFT_BUTTON:
            notes_selectable = _selection_enabled("notes")
            anchor_placement_enabled = bool(STATE.get("anchor_placement_enabled", False))
            host_select_passthrough = bool(is_select_mode and notes_selectable)
            blank_hit = hidx < 0 and aidx < 0 and seg_hit is None
            had_selection = bool(STATE.get("selected_anchor_ids")) or bool(STATE.get("selected_links"))
            if blank_hit and had_selection:
                STATE["selected_anchor_ids"] = []
                STATE["selected_links"] = []
                STATE["pending_connect_anchor_id"] = -1
                STATE["drag"] = {"mode": "", "index": -1}
                status = tr(STATE["last_context"], "status_selection_cleared")
                consumed = True
            elif host_select_passthrough:
                consumed = False
            elif hidx >= 0:
                STATE["drag"] = {"mode": hkind, "index": hidx}
                consumed = True
                cursor = "crosshair"
                status = tr(STATE["last_context"], "dragging_handle", handle_kind=tr(STATE["last_context"], f"handle_kind_{hkind}"), index=hidx)
            elif aidx >= 0:
                anchor_id = int(STATE["anchors"][aidx].get("id", 0))
                if shift:
                    STATE["drag"] = {"mode": "", "index": -1}
                    STATE["link_drag"] = {
                        "active": True,
                        "source_anchor_id": anchor_id,
                        "hover_anchor_id": -1,
                        "x": x,
                        "y": y,
                    }
                    consumed = True
                    cursor = "pointing_hand"
                    status = tr(STATE["last_context"], "status_link_dragging")
                    return {
                        "consumed": consumed,
                        "overlay": _build_overlay(STATE["last_context"]),
                        "cursor": cursor,
                        "status_text": status,
                        "preview_batch_edit": {"add": [], "remove": [], "move": []},
                        "request_undo_checkpoint": request_checkpoint,
                        "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
                    }
                if ctrl:
                    _toggle_selected_anchor(anchor_id)
                else:
                    _add_selected_anchor(anchor_id)
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
                if _selection_enabled("anchors"):
                    seg_anchor_ids = []
                    for aid in (int(seg_hit[0]), int(seg_hit[1])):
                        if aid > 0 and aid not in seg_anchor_ids:
                            seg_anchor_ids.append(aid)
                    if ctrl:
                        merged = [int(v) for v in STATE.get("selected_anchor_ids", []) if int(v) > 0]
                        for aid in seg_anchor_ids:
                            if aid not in merged:
                                merged.append(aid)
                        STATE["selected_anchor_ids"] = merged
                    else:
                        STATE["selected_anchor_ids"] = seg_anchor_ids
                else:
                    STATE["selected_anchor_ids"] = []
                consumed = True
                cursor = "pointing_hand"
                status = tr(STATE["last_context"], "status_segment_selected")
            elif (ctrl or is_select_mode) and not notes_selectable:
                STATE["box_select"] = {"active": True, "start": [x, y], "end": [x, y], "append": bool(ctrl)}
                consumed = True
                status = tr(STATE["last_context"], "status_box_selecting")
            else:
                if notes_selectable and not anchor_placement_enabled:
                    consumed = False
                elif not anchor_placement_enabled:
                    consumed = True
                    status = tr(STATE["last_context"], "anchor_place_off_hint")
                else:
                    consumed = True
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
        link_drag = STATE.get("link_drag", {})
        if bool(link_drag.get("active", False)):
            link_drag["x"] = x
            link_drag["y"] = y
            hidx = _find_anchor_hit(STATE["last_context"], x, y) if _selection_enabled("anchors") else -1
            hover_id = -1
            if hidx >= 0:
                hit_id = int(STATE["anchors"][hidx].get("id", 0))
                if hit_id > 0 and hit_id != int(link_drag.get("source_anchor_id", -1)):
                    hover_id = hit_id
            link_drag["hover_anchor_id"] = hover_id
            STATE["link_drag"] = link_drag
            consumed = True
            cursor = "pointing_hand"
            if hover_id > 0:
                idx_map = _anchor_index_map()
                src_i = idx_map.get(int(link_drag.get("source_anchor_id", -1)), -1)
                dst_i = idx_map.get(hover_id, -1)
                status = tr(STATE["last_context"], "status_link_drag_target", from_idx=src_i, to_idx=dst_i)
            else:
                status = tr(STATE["last_context"], "status_link_dragging")
            return {
                "consumed": consumed,
                "overlay": _build_overlay(STATE["last_context"]),
                "cursor": cursor,
                "status_text": status,
                "preview_batch_edit": {"add": [], "remove": [], "move": []},
                "request_undo_checkpoint": request_checkpoint,
                "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
            }

        shift_now = _event_has_shift(event) or bool(STATE.get("shift_down", False))
        if shift_now and STATE.get("drag", {}).get("mode") == "anchor":
            drag_idx = int(STATE.get("drag", {}).get("index", -1))
            if 0 <= drag_idx < len(STATE.get("anchors", [])):
                source_id = int(STATE["anchors"][drag_idx].get("id", 0))
                if source_id > 0:
                    STATE["drag"] = {"mode": "", "index": -1}
                    STATE["link_drag"] = {
                        "active": True,
                        "source_anchor_id": source_id,
                        "hover_anchor_id": -1,
                        "x": x,
                        "y": y,
                    }
                    consumed = True
                    cursor = "pointing_hand"
                    status = tr(STATE["last_context"], "status_link_dragging")
                    return {
                        "consumed": consumed,
                        "overlay": _build_overlay(STATE["last_context"]),
                        "cursor": cursor,
                        "status_text": status,
                        "preview_batch_edit": {"add": [], "remove": [], "move": []},
                        "request_undo_checkpoint": request_checkpoint,
                        "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
                    }

        box = STATE.get("box_select", {})
        if bool(box.get("active", False)):
            box["end"] = [x, y]
            STATE["box_select"] = box
            consumed = True
            cursor = "crosshair"
            status = tr(STATE["last_context"], "status_box_selecting")
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
        link_drag = STATE.get("link_drag", {})
        if bool(link_drag.get("active", False)):
            consumed = True
            source_id = int(link_drag.get("source_anchor_id", -1))
            target_id = int(link_drag.get("hover_anchor_id", -1))
            STATE["link_drag"] = {"active": False, "source_anchor_id": -1, "hover_anchor_id": -1, "x": 0.0, "y": 0.0}
            if source_id > 0 and target_id > 0 and source_id != target_id:
                added = _add_link(source_id, target_id)
                idx_map = _anchor_index_map()
                src_i = idx_map.get(source_id, -1)
                dst_i = idx_map.get(target_id, -1)
                if added:
                    _cleanup_links_and_selection()
                    _invalidate_curve_cache()
                    _record_history_state(STATE["last_context"])
                    request_checkpoint = True
                    status = tr(STATE["last_context"], "status_link_connected", from_idx=src_i, to_idx=dst_i)
                else:
                    status = tr(STATE["last_context"], "status_link_already_connected", from_idx=src_i, to_idx=dst_i)
            else:
                status = tr(STATE["last_context"], "status_link_cancelled")
            return {
                "consumed": consumed,
                "overlay": _build_overlay(STATE["last_context"]),
                "cursor": cursor,
                "status_text": status,
                "preview_batch_edit": {"add": [], "remove": [], "move": []},
                "request_undo_checkpoint": request_checkpoint,
                "undo_checkpoint_label": CURVE_CHECKPOINT_PREFIX,
            }

        if bool(STATE.get("box_select", {}).get("active", False)):
            changed = _apply_box_selection(STATE["last_context"])
            consumed = True
            status = tr(STATE["last_context"], "status_box_selection_applied") if changed else tr(STATE["last_context"], "status_box_selection_cleared")
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
        STATE["link_drag"] = {"active": False, "source_anchor_id": -1, "hover_anchor_id": -1, "x": 0.0, "y": 0.0}
        consumed = True
        status = tr(STATE["last_context"], "interaction_cancelled")

    elif et == "key_down":
        key = int(event.get("key", 0))
        mods = int(event.get("modifiers", 0))
        ctrl = (mods & CTRL_MODIFIER_MASK) != 0
        if key == 0x01000020 or _event_has_shift(event):  # Qt::Key_Shift
            STATE["shift_down"] = True
        if not ctrl and key == KEY_A and not _host_controls_anchor_mode(STATE["last_context"]):
            STATE["anchor_placement_enabled"] = not bool(STATE.get("anchor_placement_enabled", False))
            consumed = True
            status = tr(STATE["last_context"], "anchor_enabled") if STATE["anchor_placement_enabled"] else tr(STATE["last_context"], "anchor_disabled")
            _record_history_state(STATE["last_context"])
            request_checkpoint = True
        elif key in (KEY_DELETE, KEY_BACKSPACE):
            changed_segments = _disconnect_selected_segments(STATE["last_context"])
            changed_anchors = _delete_selected_anchors(STATE["last_context"])
            if changed_segments or changed_anchors:
                consumed = True
                request_checkpoint = True
                status = tr(STATE["last_context"], "status_selection_deleted")
        elif key == 0x01000000:  # Esc
            if STATE.get("selected_anchor_ids") or STATE.get("selected_links"):
                STATE["selected_anchor_ids"] = []
                STATE["selected_links"] = []
                consumed = True
                status = tr(STATE["last_context"], "status_selection_cleared")

    # In tool mode, left-button canvas operations belong to plugin.
    if et == "key_up":
        key = int(event.get("key", 0))
        if key == 0x01000020 or not _event_has_shift(event):
            STATE["shift_down"] = False

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
    actions = [
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
            "action_id": "toggle_curve_visible",
            "title": tr(STATE.get("last_context", {}), "action_curve_visible"),
            "description": tr(STATE.get("last_context", {}), "action_curve_visible_desc"),
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("curve_visible", True)),
        },
        {
            "action_id": "toggle_polyline_mode",
            "title": tr(STATE.get("last_context", {}), "action_polyline_mode"),
            "description": tr(STATE.get("last_context", {}), "action_polyline_mode_desc"),
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": _selected_links_all_polyline(),
        },
        {
            "action_id": "toggle_note_curve_snap",
            "title": tr(STATE.get("last_context", {}), "action_note_curve_snap"),
            "description": tr(STATE.get("last_context", {}), "action_note_curve_snap_desc"),
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": _note_curve_snap_enabled(),
        },
        {
            "action_id": "toggle_select_anchors",
            "title": tr(STATE.get("last_context", {}), "action_toggle_select_anchors"),
            "description": tr(STATE.get("last_context", {}), "action_toggle_select_anchors_desc"),
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("selection_targets", {}).get("anchors", True)),
        },
        {
            "action_id": "toggle_select_segments",
            "title": tr(STATE.get("last_context", {}), "action_toggle_select_segments"),
            "description": tr(STATE.get("last_context", {}), "action_toggle_select_segments_desc"),
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("selection_targets", {}).get("segments", True)),
        },
        {
            "action_id": "toggle_select_notes",
            "title": tr(STATE.get("last_context", {}), "action_toggle_select_notes"),
            "description": tr(STATE.get("last_context", {}), "action_toggle_select_notes_desc"),
            "placement": "right_note_panel",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": bool(STATE.get("selection_targets", {}).get("notes", False)),
        },
        {
            "action_id": "commit_curve_to_notes_sidebar",
            "title": tr(STATE.get("last_context", {}), "action_commit_curve"),
            "description": tr(STATE.get("last_context", {}), "action_commit_curve_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": True,
        },
        {
            "action_id": "commit_context_segments_to_notes",
            "title": tr(STATE.get("last_context", {}), "action_commit_context_segments"),
            "description": tr(STATE.get("last_context", {}), "action_commit_context_segments_desc"),
            "placement": "plugin_context_menu",
            "requires_undo_snapshot": True,
        },
        {
            "action_id": "toggle_context_polyline_mode",
            "title": tr(STATE.get("last_context", {}), "action_toggle_context_shape"),
            "description": tr(STATE.get("last_context", {}), "action_polyline_context_desc"),
            "placement": "plugin_context_menu",
            "requires_undo_snapshot": False,
            "checkable": True,
            "checked": _context_links_all_polyline(),
        },
        {
            "action_id": "reset_curve",
            "title": tr(STATE.get("last_context", {}), "action_reset_curve"),
            "description": tr(STATE.get("last_context", {}), "action_reset_curve_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "connect_selected_nodes",
            "title": tr(STATE.get("last_context", {}), "action_connect_selected"),
            "description": tr(STATE.get("last_context", {}), "action_connect_selected_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "disconnect_selected_segments",
            "title": tr(STATE.get("last_context", {}), "action_disconnect_selected_segments"),
            "description": tr(STATE.get("last_context", {}), "action_disconnect_selected_segments_desc"),
            "placement": "left_sidebar",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "connect_selected_nodes_ctx",
            "title": tr(STATE.get("last_context", {}), "action_connect_selected"),
            "description": tr(STATE.get("last_context", {}), "action_connect_selected_desc"),
            "placement": "plugin_context_menu",
            "requires_undo_snapshot": False,
        },
        {
            "action_id": "disconnect_selected_segments_ctx",
            "title": tr(STATE.get("last_context", {}), "status_selection_deleted"),
            "description": tr(STATE.get("last_context", {}), "status_selection_deleted"),
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

    return actions


def _run_tool_action(payload):
    action_id = str((payload or {}).get("action_id", ""))
    context = (payload or {}).get("context", {}) or {}
    _ensure_project_context(context)

    if action_id == "reset_curve":
        _reset_anchors(context)
        return True
    if action_id in ("commit_curve_to_notes", "commit_curve_to_notes_sidebar", "commit_context_segments_to_notes"):
        if STATE["project_path"] and STATE["project_dirty"]:
            if not _save_project(STATE["project_path"], context):
                return False
        return True
    if action_id == "toggle_anchor_placement":
        STATE["anchor_placement_enabled"] = not bool(STATE.get("anchor_placement_enabled", False))
        _record_history_state(context)
        return True
    if action_id == "toggle_curve_visible":
        STATE["curve_visible"] = not bool(STATE.get("curve_visible", True))
        _record_history_state(context)
        return True
    if action_id == "toggle_polyline_mode":
        return _toggle_polyline_for_active_or_selected(context)
    if action_id == "toggle_context_polyline_mode":
        return _toggle_polyline_for_context_links(context)
    if action_id == "toggle_note_curve_snap":
        STATE["note_curve_snap_enabled"] = not _note_curve_snap_enabled()
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
    if action_id == "set_segment_density_follow":
        return _set_density_for_selected_segments(context, 0)
    if action_id.startswith("set_segment_density_"):
        suffix = action_id[len("set_segment_density_") :]
        try:
            den = int(suffix)
        except Exception:
            return False
        return _set_density_for_selected_segments(context, den)
    if action_id in ("connect_selected_nodes", "connect_selected_nodes_ctx"):
        return _connect_selected_anchors(context)
    if action_id == "disconnect_selected_segments":
        return _disconnect_selected_segments(context)
    if action_id == "disconnect_selected_segments_ctx":
        changed_segments = _disconnect_selected_segments(context)
        changed_anchors = _delete_selected_anchors(context)
        return changed_segments or changed_anchors
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
    _seed_missing_segment_denominators(context)
    if action_id not in ("commit_curve_to_notes", "commit_curve_to_notes_sidebar", "commit_context_segments_to_notes"):
        return {"add": [], "remove": [], "move": []}
    if action_id == "commit_context_segments_to_notes":
        return _build_batch_from_curve(context, _context_menu_target_links())
    return _build_batch_from_curve(context)


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
            elif event == "onHostDiscardChanges":
                STATE["suppress_persist_once"] = True
                STATE["project_dirty"] = False
            elif event == "onChartSaved":
                STATE["suppress_persist_once"] = False
                if STATE["project_path"] and STATE["project_dirty"]:
                    _save_project(STATE["project_path"], STATE.get("last_context", {}))
            elif event == "shutdown":
                if (not bool(STATE.get("suppress_persist_once", False)) and
                        STATE["project_path"] and STATE["project_dirty"]):
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
