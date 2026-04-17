import json
import locale
import math
import os
import shutil
import sys
import tempfile
import zipfile
from datetime import datetime


TRANSLATIONS = {
    "processed": {
        "zh": "处理成功: {file} (备份: {backup})",
        "en": "Processed: {file} (backup: {backup})",
    },
    "failed": {
        "zh": "处理失败: {file} | 错误: {error}",
        "en": "Failed: {file} | Error: {error}",
    },
    "no_targets": {
        "zh": "未找到可处理的 .mc 或 .mcz 文件。",
        "en": "No .mc or .mcz files found.",
    },
}


def detect_lang(default="zh"):
    sys_locale = locale.getlocale()[0]
    if sys_locale and sys_locale.lower().startswith("zh"):
        return "zh"
    if sys_locale and sys_locale.lower().startswith("en"):
        return "en"
    return default


def tr(lang, key, **kwargs):
    text = TRANSLATIONS.get(key, {}).get(lang, TRANSLATIONS.get(key, {}).get("en", key))
    return text.format(**kwargs)


def adjust_denominator(numerator, denominator):
    gcd_val = math.gcd(numerator, denominator)
    return numerator // gcd_val, denominator // gcd_val


def process_beat(beat):
    if not isinstance(beat, list) or len(beat) != 3:
        return beat
    measure, num, den = beat
    if not isinstance(num, int) or not isinstance(den, int) or den == 0:
        return beat
    new_num, new_den = adjust_denominator(num, den)
    return [measure, new_num, new_den]


def process_mc_file(mc_path, lang):
    with open(mc_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    changed = False
    for note in data.get("note", []):
        beat = note.get("beat")
        if beat is None:
            continue
        new_beat = process_beat(beat)
        if new_beat != beat:
            note["beat"] = new_beat
            changed = True

    backup_path = mc_path + ".bak"
    shutil.copyfile(mc_path, backup_path)

    if changed:
        with open(mc_path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)

    print(tr(lang, "processed", file=os.path.basename(mc_path), backup=os.path.basename(backup_path)))
    return True


def process_mcz_file(mcz_path, lang):
    output_dir = os.path.dirname(os.path.abspath(mcz_path))
    with tempfile.TemporaryDirectory() as tmpdir:
        with zipfile.ZipFile(mcz_path, "r") as zip_ref:
            zip_ref.extractall(tmpdir)

        found_mc = False
        for root, _, files in os.walk(tmpdir):
            for file_name in files:
                if not file_name.lower().endswith(".mc"):
                    continue
                found_mc = True
                process_mc_file(os.path.join(root, file_name), lang)

        if not found_mc:
            return False

        backup = mcz_path + ".bak"
        shutil.copyfile(mcz_path, backup)
        with zipfile.ZipFile(mcz_path, "w", zipfile.ZIP_DEFLATED) as zip_write:
            for folder, _, files in os.walk(tmpdir):
                for file_name in files:
                    if file_name.lower().endswith(".bak"):
                        continue
                    full_path = os.path.join(folder, file_name)
                    arcname = os.path.relpath(full_path, tmpdir)
                    zip_write.write(full_path, arcname)
    return True


def normalize_path(path, lang):
    try:
        if path.lower().endswith(".mc"):
            return process_mc_file(path, lang)
        if path.lower().endswith(".mcz"):
            return process_mcz_file(path, lang)
        return False
    except Exception as exc:
        print(tr(lang, "failed", file=os.path.basename(path), error=str(exc)), file=sys.stderr)
        return False


def run_standalone():
    lang = detect_lang()
    cwd = os.getcwd()
    targets = [os.path.join(cwd, n) for n in os.listdir(cwd) if n.lower().endswith((".mc", ".mcz"))]
    if not targets:
        print(tr(lang, "no_targets"))
        return 1
    for p in targets:
        normalize_path(p, lang)
    return 0


def _send_response(req_id, result):
    payload = {"type": "response", "id": req_id, "result": bool(result)}
    sys.stdout.write(json.dumps(payload, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def run_process_plugin():
    lang = "zh"

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
            payload = msg.get("payload", {}) or {}
            if event == "initialize":
                locale_name = str(payload.get("locale", "zh_CN"))
                lang = "zh" if locale_name.lower().startswith("zh") else "en"
            elif event in ("onChartSaved", "onChartLoaded"):
                chart_path = payload.get("chart_path")
                if isinstance(chart_path, str) and chart_path:
                    normalize_path(chart_path, lang)
            elif event == "shutdown":
                break
            continue

        if msg_type == "request":
            req_id = msg.get("id", "")
            method = msg.get("method", "")
            payload = msg.get("payload", {}) or {}

            if method in ("openAdvancedColorEditor", "normalizeCurrentChart"):
                chart_path = payload.get("chart_path")
                if isinstance(chart_path, str) and chart_path:
                    _send_response(req_id, normalize_path(chart_path, lang))
                else:
                    _send_response(req_id, False)
            else:
                _send_response(req_id, False)

    return 0


if __name__ == "__main__":
    if "--plugin" in sys.argv:
        sys.exit(run_process_plugin())
    sys.exit(run_standalone())

