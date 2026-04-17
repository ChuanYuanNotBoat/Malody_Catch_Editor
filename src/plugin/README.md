# Plugin System (Beta v1.2.0)

当前主程序支持两类插件：
- `Native Plugin`：C++ 动态库（`.dll/.so/.dylib`）
- `Process Plugin`：外部进程插件（任意语言，通过 JSON 行协议通信）

这意味着 Python / Node.js / Rust / Go 都可以做插件，不要求与宿主同 ABI。

## 1. 支持范围

已实现：
- 从 `{appDir}/plugins` 自动发现并加载插件
- API 版本强校验（`PluginInterface::kHostApiVersion`）
- 生命周期：`initialize -> events -> shutdown`
- 主程序内置插件管理页面（Tools -> Plugin Manager）
  - 查看插件元信息、来源路径、能力、加载状态
  - 启用/禁用插件（持久化）并手动重载
- 事件回调：
  - `onChartChanged`
  - `onChartLoaded(chartPath)`
  - `onChartSaved(chartPath)`
- 高级配色扩展点：`openAdvancedColorEditor(context)`
- 元信息多语言字段（插件名/描述）

暂未实现：
- 插件开关 UI / 配置页
- 权限沙箱与隔离策略

## 2. Native Plugin（C++）

必须导出 3 个符号：
- `int pluginApiVersion()`
- `PluginInterface* createPlugin()`
- `void destroyPlugin(PluginInterface*)`

任一缺失会被拒绝加载。

参考：`PLUGIN_TEMPLATE.md`

## 3. Process Plugin（多编程语言）

通过 `*.plugin.json` 清单定义插件，宿主启动外部可执行程序，通过 `stdin/stdout` 发送 JSON 行。

最小清单字段：
- `pluginId`
- `displayName`
- `version`
- `description`
- `author`
- `pluginApiVersion`
- `executable`
- `args`（可选）
- `capabilities`（可选）

可选多语言字段：
- `localizedDisplayName`
- `localizedDescription`

协议细节：`PROCESS_PLUGIN_PROTOCOL.md`

## 4. 版本规则

宿主当前 API 版本：`2`

要求：
- Native 插件导出的 `pluginApiVersion()` 必须等于 `2`
- Native 插件实例方法 `pluginApiVersion()` 必须等于 `2`
- Process 插件清单 `pluginApiVersion` 必须等于 `2`

## 5. 能力声明（capabilities）

- `chart_observer`
- `advanced_color_editor`

## 6. 目录建议

`plugins/` 下建议结构：

```text
plugins/
  my_native_plugin.dll
  py_color_tool.plugin.json
  py_color_tool.py
  node_tool.plugin.json
  node_tool.js
```

仓库内可参考：`samples/process/`。

其中 `samples/process/python/beat_normalizer/` 是从原始 Python 脚本迁移的完整插件示例。

## 7. 宿主端实现位置

- `PluginInterface.h`：统一插件契约
- `ExternalProcessPlugin.h/.cpp`：进程插件适配器
- `PluginManager.h/.cpp`：生命周期与事件分发
- `../file/PluginLoader.h/.cpp`：插件发现与加载
