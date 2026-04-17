# Process Plugin Protocol (Host API v2)

适用于 Python / Node.js / Rust / Go 等任意语言插件。

## 1. Discovery

宿主会在 `{appDir}/plugins` 扫描 `*.plugin.json` 文件作为插件清单。

## 2. Manifest Schema

示例：

```json
{
  "pluginId": "demo.py.echo",
  "displayName": "Python Echo Plugin",
  "version": "0.1.0",
  "description": "Process plugin example",
  "author": "Your Name",
  "pluginApiVersion": 2,
  "executable": "python",
  "args": ["./py_echo_plugin.py"],
  "capabilities": ["chart_observer"],
  "localizedDisplayName": {
    "zh_CN": "Python 回显插件",
    "en_US": "Python Echo Plugin"
  },
  "localizedDescription": {
    "zh_CN": "多语言进程插件示例",
    "en_US": "Example process plugin"
  }
}
```

必填字段：
- `pluginId`
- `displayName`
- `version`
- `description`
- `author`
- `pluginApiVersion`
- `executable`

选填字段：
- `args`（字符串数组）
- `capabilities`（字符串数组）
- `localizedDisplayName`（对象）
- `localizedDescription`（对象）

## 3. Transport

- 宿主启动外部进程
- 宿主向插件 `stdin` 写入一行 JSON（UTF-8）
- 插件从 `stdin` 逐行读取
- 插件向 `stdout` 逐行输出 JSON（仅用于 request-response）

每条消息必须单行，以 `\n` 结束。

## 4. Message Types

### 4.1 Notification (Host -> Plugin)

```json
{"type":"notify","event":"initialize","payload":{"plugin_id":"demo.py.echo","locale":"zh_CN","host_api_version":2}}
{"type":"notify","event":"onChartChanged"}
{"type":"notify","event":"onChartLoaded","payload":{"chart_path":"D:/beatmap/test.mc"}}
{"type":"notify","event":"onChartSaved","payload":{"chart_path":"D:/beatmap/test.mc"}}
{"type":"notify","event":"shutdown"}
```

### 4.2 Request (Host -> Plugin)

```json
{"type":"request","id":"1713512345678","method":"openAdvancedColorEditor","payload":{"feature":"note_color_editor"}}
```

```json
{"type":"request","id":"1713512345680","method":"listToolActions","payload":{}}
```

```json
{"type":"request","id":"1713512345681","method":"runToolAction","payload":{"action_id":"standardize_all_colors","context":{"chart_path":"D:/beatmap/test.mc","locale":"zh_CN","language":"zh"}}}
```

### 4.3 Response (Plugin -> Host)

```json
{"type":"response","id":"1713512345678","result":true}
```

`result` 默认按 `bool` 处理；对于 `listToolActions` 返回数组。

对于 `listToolActions`，`result` 应为数组，元素结构建议：

```json
{
  "action_id": "standardize_all_colors",
  "title": "标准化所有颜色",
  "description": "可选描述",
  "confirm_message": "执行前确认文案",
  "placement": "left_sidebar",
  "requires_undo_snapshot": true
}
```

`placement` 可选值：
- `tools_menu`
- `top_toolbar`
- `left_sidebar`

## 5. i18n Metadata Fallback

宿主读取本地化字段时按顺序匹配：
1. 完整 locale（如 `zh_CN`）
2. 语言码（如 `zh`）
3. `default`
4. 非本地化字段（`displayName` / `description`）

## 6. Error Guidance

- 插件应忽略未知 `event` / `method`
- 插件内部异常不应导致进程崩溃
- 对无法处理的 `request` 返回 `result=false`


## 7. Optional Locale Context

- Host may include locale (e.g. zh_CN) and language (e.g. zh) in request context.
- Plugins can ignore these fields safely if not needed.
- For one-shot mode (--run-tool-action), host also exports:
  - MALODY_LOCALE
  - MALODY_LANGUAGE
