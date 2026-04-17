# Process Plugin Examples

This folder contains minimal examples for multi-language process plugins.

## Python example

Files:
- `python/example.plugin.json`
- `python/python_plugin.py`

## Node.js example

Files:
- `node/example.plugin.json`
- `node/node_plugin.js`

## How to test quickly

1. Copy one `example.plugin.json` into your runtime `plugins` directory.
2. Keep script path in `args` relative to the manifest location.
3. Ensure executable exists in PATH (`python` or `node`).
4. Start editor and check logs for plugin load result.
