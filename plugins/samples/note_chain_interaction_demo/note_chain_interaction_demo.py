import json
import math
import sys
import time

STATE = {
    "points": [
        {"x": 220.0, "y": 220.0},
        {"x": 420.0, "y": 340.0},
        {"x": 620.0, "y": 260.0},
    ],
    "drag_index": -1,
}


def _write(msg):
    line = json.dumps(msg, ensure_ascii=False) + "\n"
    sys.stdout.write(line)
    sys.stdout.flush()


def _respond(req_id, result):
    _write({"type": "response", "id": req_id, "result": result})


def _distance(x1, y1, x2, y2):
    return math.hypot(x1 - x2, y1 - y2)


def _find_hit(x, y, threshold=18.0):
    best = -1
    best_dist = 1e9
    for i, p in enumerate(STATE["points"]):
        d = _distance(x, y, p["x"], p["y"])
        if d < threshold and d < best_dist:
            best = i
            best_dist = d
    return best


def _build_overlay(context):
    toggles = context.get("overlay_toggles", {}) if isinstance(context, dict) else {}
    overlay_enabled = bool(toggles.get("overlay_enabled", True))
    if not overlay_enabled:
        return []

    show_points = bool(toggles.get("control_points", True))
    show_preview = bool(toggles.get("preview", True))
    show_labels = bool(toggles.get("labels", True))

    items = []
    points = STATE["points"]

    if show_preview and len(points) >= 2:
        for i in range(len(points) - 1):
            a = points[i]
            b = points[i + 1]
            items.append(
                {
                    "kind": "line",
                    "x1": a["x"],
                    "y1": a["y"],
                    "x2": b["x"],
                    "y2": b["y"],
                    "color": "#33CCFF",
                    "width": 2.0,
                }
            )

    if show_points:
        for i, p in enumerate(points):
            items.append(
                {
                    "kind": "rect",
                    "x": p["x"] - 6,
                    "y": p["y"] - 6,
                    "w": 12,
                    "h": 12,
                    "color": "#FFFFFF",
                    "fill_color": "#AA0077FF" if i == STATE["drag_index"] else "#AA00A3FF",
                    "width": 1.5,
                }
            )
            if show_labels:
                items.append(
                    {
                        "kind": "text",
                        "x1": p["x"] + 8,
                        "y1": p["y"] - 8,
                        "text": f"P{i}",
                        "color": "#FFFFFF",
                        "font_px": 12,
                    }
                )

    return items


def _handle_canvas_input(payload):
    context = payload.get("context", {}) if isinstance(payload, dict) else {}
    event = payload.get("event", {}) if isinstance(payload, dict) else {}

    et = str(event.get("type", ""))
    x = float(event.get("x", 0.0))
    y = float(event.get("y", 0.0))

    consumed = False
    status = ""
    cursor = "arrow"

    if et == "mouse_down":
        idx = _find_hit(x, y)
        if idx >= 0:
            STATE["drag_index"] = idx
            consumed = True
            cursor = "size_all"
            status = f"Dragging control point #{idx}"
    elif et == "mouse_move":
        if STATE["drag_index"] >= 0:
            idx = STATE["drag_index"]
            STATE["points"][idx]["x"] = x
            STATE["points"][idx]["y"] = y
            consumed = True
            cursor = "size_all"
            status = f"Dragging control point #{idx}"
        elif _find_hit(x, y) >= 0:
            cursor = "pointing_hand"
    elif et == "mouse_up":
        if STATE["drag_index"] >= 0:
            idx = STATE["drag_index"]
            STATE["drag_index"] = -1
            consumed = True
            status = f"Control point #{idx} moved"
    elif et == "cancel":
        STATE["drag_index"] = -1
        consumed = True
        status = "Interaction cancelled"

    return {
        "consumed": consumed,
        "overlay": _build_overlay(context),
        "cursor": cursor,
        "status_text": status,
        "preview_batch_edit": {"add": [], "remove": [], "move": []},
    }


def _list_tool_actions():
    return [
        {
            "action_id": "reset_demo_points",
            "title": "Reset Demo Points",
            "description": "Reset interactive demo control points",
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        }
    ]


def _run_tool_action(payload):
    action_id = str((payload or {}).get("action_id", ""))
    if action_id == "reset_demo_points":
        STATE["points"] = [
            {"x": 220.0, "y": 220.0},
            {"x": 420.0, "y": 340.0},
            {"x": 620.0, "y": 260.0},
        ]
        STATE["drag_index"] = -1
        return True
    return False


def _workspace_config(_payload):
    return {
        "workspace_id": "note_chain_workspace",
        "docking_supported": True,
        "tab_merge_supported": True,
        "default_layout": "advanced",
        "window_group": "note_chain",
    }


def run_one_shot(action_id):
    if action_id == "reset_demo_points":
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
            _respond(req_id, {"add": [], "remove": [], "move": []})
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
