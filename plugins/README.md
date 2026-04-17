# Runtime Plugins Directory

This directory is for runtime plugin deployment (non-source).

## Purpose

- Put plugin binaries/manifests/scripts here for local runs.
- The app resolves plugin directory from executable path as `<appDir>/plugins` only.
- Build step copies this repo folder into the executable output directory automatically.
- Install/package step also ships this folder into release output.

## Suggested layout

```text
plugins/
  samples/
    beat_normalizer/
      beat_normalizer.plugin.json
      malody_catch_colour_changer.py
```

## Notes

- Keep `*.plugin.json` and script relative paths aligned.
- For Python process plugins, ensure `python` is available in PATH.
