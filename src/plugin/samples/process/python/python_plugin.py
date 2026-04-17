import json
import sys


def send(obj):
    sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
    sys.stdout.flush()


for raw in sys.stdin:
    raw = raw.strip()
    if not raw:
        continue

    try:
        msg = json.loads(raw)
    except Exception:
        continue

    msg_type = msg.get("type")
    if msg_type == "notify":
        event = msg.get("event")
        if event == "shutdown":
            break
        continue

    if msg_type == "request":
        req_id = msg.get("id", "")
        method = msg.get("method", "")

        if method == "openAdvancedColorEditor":
            send({"type": "response", "id": req_id, "result": True})
        else:
            send({"type": "response", "id": req_id, "result": False})
