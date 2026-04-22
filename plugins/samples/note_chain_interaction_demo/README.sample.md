# Note Chain Interaction Demo (Process Plugin)

A runnable sample for Host API v3 interactive canvas flow:

- capabilities: `tool_actions`, `canvas_overlay`, `canvas_interaction`, `panel_workspace`
- methods demonstrated:
  - `listToolActions`
  - `runToolAction`
  - `listCanvasOverlays`
  - `handleCanvasInput`
  - `getPanelWorkspaceConfig`

## Files

- `note_chain_interaction_demo.plugin.json`
- `note_chain_interaction_demo.py`

## Install

1. Copy folder to `{appDir}/plugins/note_chain_interaction_demo/`
2. Ensure `python` is available in PATH
3. Restart editor

## Usage

- Open `Tools -> Plugin Enhanced Tool Mode`
- Hover/drag control points in canvas to see interactive overlay updates
- Use `Tools -> Plugin Actions -> Reset Demo Points` to reset sample state
