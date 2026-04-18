# Process Plugin Minimal Example (Host API v2)

This page provides a minimal runnable process plugin example, plus error code and logging conventions.

## 1. Manifest (`plugins/echo.plugin.json`)

```json
{
  "pluginId": "demo.process.echo",
  "displayName": "Demo Process Echo",
  "version": "0.1.0",
  "description": "Minimal process plugin example",
  "author": "Example Author",
  "pluginApiVersion": 2,
  "executable": "python",
  "args": ["./echo_plugin.py"],
  "capabilities": ["chart_observer"]
}
```

## 2. Minimal Python Plugin (`plugins/echo_plugin.py`)

```python
import json
import sys
import traceback


def log(level, code, message, **fields):
    payload = {
        "level": level,
        "code": code,
        "message": message,
        "fields": fields,
    }
    # Host-side debug logs can read stderr safely.
    sys.stderr.write("[plugin] " + json.dumps(payload, ensure_ascii=False) + "\n")
    sys.stderr.flush()


def write_response(request_id, result):
    msg = {"type": "response", "id": request_id, "result": result}
    sys.stdout.write(json.dumps(msg, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def list_tool_actions():
    return [
        {
            "action_id": "echo.hello",
            "title": "Echo Hello",
            "description": "Example action from process plugin",
            "placement": "tools_menu",
            "requires_undo_snapshot": False,
        }
    ]


def handle_request(message):
    method = message.get("method", "")
    request_id = message.get("id")
    payload = message.get("payload", {}) or {}

    if method == "listToolActions":
        write_response(request_id, list_tool_actions())
        return

    if method == "runToolAction":
        action_id = payload.get("action_id")
        if action_id == "echo.hello":
            log("info", "I1001", "tool action invoked", action_id=action_id)
            write_response(request_id, True)
            return
        log("warn", "W1404", "unknown action id", action_id=action_id)
        write_response(request_id, False)
        return

    # Optional APIs (new extension points)
    if method == "buildBatchEdit":
        # Return empty edit if unsupported in this minimal example.
        write_response(request_id, {"add": [], "remove": [], "move": []})
        return

    if method == "listCanvasOverlays":
        write_response(request_id, [])
        return

    log("warn", "W1400", "unknown method", method=method)
    write_response(request_id, False)


def main():
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            message = json.loads(raw)
        except Exception:
            log("error", "E1200", "invalid json line", raw=raw)
            continue

        msg_type = message.get("type")
        if msg_type == "notify":
            event = message.get("event", "")
            log("info", "I1000", "notify", event=event)
            if event == "shutdown":
                return
            continue

        if msg_type == "request":
            try:
                handle_request(message)
            except Exception:
                log("error", "E1500", "request handler crashed", traceback=traceback.format_exc())
                req_id = message.get("id")
                if req_id is not None:
                    write_response(req_id, False)
            continue

        log("warn", "W1401", "unknown message type", type=msg_type)


if __name__ == "__main__":
    main()
```

## 3. Error Code Convention (Recommended)

- `I1xxx`: informational logs (normal lifecycle, action called)
- `W14xx`: recoverable warnings (unknown method/action, ignored input)
- `E12xx`: protocol/transport errors (invalid JSON, malformed message)
- `E15xx`: runtime errors (uncaught exceptions in request handling)

Plugins should not crash on unknown methods/events. Return `result=false` and log warning/error context.

## 4. Logging Convention (Recommended)

- Write structured logs to `stderr` in single-line JSON.
- Include at least `level`, `code`, `message`.
- Attach context fields like `action_id`, `chart_path`, `method`, and exception summary.
- Do not print non-JSON noise to `stdout`; reserve `stdout` for protocol response lines.

## 5. Batch Undo Compatibility Tip

If your action edits multiple notes at once, prefer implementing `buildBatchEdit`.  
Host applies that payload as one atomic undo step.
