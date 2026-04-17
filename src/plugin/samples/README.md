# Process Plugin Examples

This folder contains minimal examples for multi-language process plugins.
These samples are pre-included in this repository as reference implementations.

## Python example

Files:
- `python/example.plugin.json`
- `python/python_plugin.py`
- `python/beat_normalizer/beat_normalizer.plugin.json`
- `python/beat_normalizer/malody_catch_colour_changer.py`

## Node.js example

Files:
- `node/example.plugin.json`
- `node/node_plugin.js`

## How to test quickly

1. Copy one `example.plugin.json` into your runtime `plugins` directory.
2. Keep script path in `args` relative to the manifest location.
3. Ensure executable exists in PATH (`python` or `node`).
4. Start editor and check logs for plugin load result.
