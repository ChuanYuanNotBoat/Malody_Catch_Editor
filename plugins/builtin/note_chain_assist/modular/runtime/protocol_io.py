
def write_message(msg, *, json_module, sys_module):
    line = json_module.dumps(msg, ensure_ascii=False) + "\n"
    if hasattr(sys_module.stdout, "buffer"):
        sys_module.stdout.buffer.write(line.encode("utf-8"))
        sys_module.stdout.buffer.flush()
    else:
        sys_module.stdout.write(line)
        sys_module.stdout.flush()


def respond(req_id, result, *, write_fn):
    write_fn({"type": "response", "id": req_id, "result": result})
