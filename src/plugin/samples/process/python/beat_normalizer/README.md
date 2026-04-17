# Beat Normalizer Process Plugin (Python)

This sample ports `malody_catch_colour_changer.py` to the new process-plugin runtime.

Files:
- `beat_normalizer.plugin.json`
- `malody_catch_colour_changer.py`

## Runtime behavior

- On `onChartSaved` / `onChartLoaded`, it normalizes beat fractions in the target chart file.
- Supports `.mc` and `.mcz`.
- Creates backup files with `.bak` suffix.

## Install (manual)

1. Copy both files into `{appDir}/plugins/beat_normalizer/`.
2. Ensure `python` is in PATH.
3. Keep manifest/script relative paths unchanged.

## Notes

- Running script directly (without `--plugin`) works as standalone batch tool in current directory.
- In plugin mode, protocol messages are read from stdin and responses are written to stdout.
