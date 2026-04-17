# Plugin System

This folder is the source-side plugin SDK and host integration code.

## Structure

- `PluginInterface.h`: host plugin interface contract
- `PluginManager.h/.cpp`: plugin lifecycle and dispatch
- `ExternalProcessPlugin.h/.cpp`: process-plugin adapter (multi-language)
- `docs/`: protocol and capability documentation

## Supported plugin types

- Native plugin (`.dll/.so/.dylib`)
- Process plugin (`*.plugin.json` + stdin/stdout JSON line protocol)

## UI extension points

Plugin tool actions can be mounted to:

- `tools_menu`
- `top_toolbar`
- `left_sidebar`

## References

- `docs/PROCESS_PLUGIN_PROTOCOL.md`
- `docs/ADVANCED_COLOR_EDITOR_PLUGIN.md`
- `docs/PLUGIN_TEMPLATE.md`
- `plugins/samples/beat_normalizer/`
