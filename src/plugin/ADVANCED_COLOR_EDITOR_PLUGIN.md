# Advanced Color Editor Plugin Interface (Reserved)

## Purpose
This document reserves a stable extension point for future advanced note color editing plugins.

The built-in editor currently supports:
- Right-click note(s) -> `Edit Color (By Division)`
- Only denominator options that preserve exact note time are shown
- Multi-select uses intersection of exactly-convertible denominator options

## Reserved Interface
`src/plugin/PluginInterface.h` now includes optional methods:

- `bool supportsAdvancedColorEditor() const`
- `bool openAdvancedColorEditor(const QVariantMap &context)`

Default behavior returns `false`, so existing plugins are not broken.

## Context Contract (Draft)
When advanced plugin integration is wired in future versions, `context` is expected to include:

- `feature`: string, fixed value `note_color_editor`
- `chart_path`: string, current chart absolute path if available
- `target_note_ids`: string list, selected or target notes
- `safe_denominators`: int list, denominator options that keep exact timing
- `time_division`: int, editor time division

Plugins should:
- Never change note timing unless explicitly requested by user
- Prefer preserving denominators when not needed for color change
- Return `true` if plugin handled the request; `false` to fallback to built-in editor

## Compatibility Notes
- This is a reserved API draft; keys may be extended but not removed without deprecation.
- Existing plugins can ignore this feature safely.
