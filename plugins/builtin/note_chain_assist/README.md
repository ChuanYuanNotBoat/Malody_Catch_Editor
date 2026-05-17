# Note Chain Assist (Builtin)

This plugin provides direct pen-curve editing on the main chart canvas.

Core interactions:
- Left drag: move anchor/handle
- Left click empty space: append anchor (only when `Anchor Place` is enabled)
- Right click anchor: delete anchor
- Double click anchor: smooth/corner toggle
- Mouse wheel: rotate denominator sequence style

Main actions:
- Commit Curve
- Right Note panel toggle: Anchor Place (checkable)
- Side quick actions: Undo Curve / Redo Curve / Reset Curve / Cycle Density
- Press `A` to toggle Anchor Place quickly
- Export Style / Import Style

## Sidecar Format (V3)

Curve project data is persisted to `.mcce-plugin/*.curve_tbd.json` with `format_version: 3`.

- Core entities: `nodes[]`, `curves[]`, `node_groups[]`, `curve_groups[]`
- Time fields use chart-like triplets: `[beatNum, numerator, denominator]`
- Node control uses `joystick` as the single source in auto-symmetric mode; `compat_handles` is retained for compatibility/future dual-handle editing
- Curve identity uses stable `curve_id`; unique business numbering uses `curve_no`
- Save uses CAS via `revision` metadata; when conflict is detected (another instance updated the file), save is rejected and the user should refresh first
- Custom group names are de-duplicated case-insensitively (no duplicate names inside node groups or curve groups)
