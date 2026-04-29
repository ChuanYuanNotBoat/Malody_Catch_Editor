# Core 拆库与 Flutter FFI 移动端迁移计划

更新日期：2026-04-29

## 背景与结论

当前 Qt/C++ 桌面端已经基本完善，继续把同一套 Qt Widgets/QML UI 强行适配移动端，会持续遇到 GUI 出界、卡顿、布局抽搐、触摸交互不自然等问题。后续方向调整为：

- 冻结现有 Qt 桌面端 UI，保持桌面端布局、样式、菜单、插件入口稳定。
- 从当前项目中拆出纯 C++ 核心逻辑库，作为桌面端和移动端共享的唯一业务核心。
- Android 移动端首版使用 Flutter + `dart:ffi` 调用同一套 C++ core。
- 移动端首版不支持现有插件系统，先完成低延迟编辑闭环。

新仓库与当前项目并列，位于当前项目根目录的上一级目录：

```text
f:/projects/tools/working/malody_tools/
  Malody_catch_editor/      # 当前 Qt 桌面端仓库
  Malody_catch_core/        # 新建纯 C++ 核心库仓库
  Malody_catch_mobile/      # 新建 Flutter 移动端仓库
```

当前桌面端后续通过 git submodule 引用核心库，建议路径为：

```text
Malody_catch_editor/external/malody-catch-core
```

## Core 仓库目标

新建 `Malody_catch_core`，使用 `git-filter-repo` 从当前仓库提取核心逻辑并保留历史。初始提取范围建议包括：

- `src/model`
- `src/file/ChartIO*`
- `src/file/ProjectIO*`
- `src/utils/MathUtils*`
- `src/controller/ChartController*`
- `tests/minimal_tests.cpp`

拆出后立即进行纯 C++ 化：

- 替换 `QString` 为 `std::string`。
- 替换 `QVector/QList/QPair/QSet/QHash` 为标准库容器。
- 替换 `QUuid` 为 core 自有 ID 生成器。
- 替换 `QJsonDocument/QJsonObject` 为 `nlohmann/json`。
- 替换 `QFile/QDir/QProcess` 为标准文件 API 和内置 zip 库。
- 替换 `QUndoStack` 为 core 自有命令栈。

Core 不包含：

- Qt Widgets/QML。
- `QPainter/QPixmap` 渲染。
- 音频播放。
- 桌面插件系统。
- 桌面设置、菜单、对话框、快捷键。

## Core API 设计

核心 C++ API 以 `mce::EditorSession` 为中心：

- 加载 `.mc` / `.mcz`。
- 保存 `.mc` / 导出 `.mcz`。
- 查询当前 `Chart` 快照。
- 添加、删除、移动、批量编辑音符。
- 修改 BPM 和 metadata。
- 撤销/重做。
- beat/ms 换算。
- 编辑数据校验。

核心数据类型：

- `mce::Note`
- `mce::BpmEntry`
- `mce::MetaData`
- `mce::Chart`
- `mce::BatchEdit`
- `mce::TimeMapper`
- `mce::EditorSession`

FFI 层使用 C ABI，避免把 C++ ABI 暴露给 Dart：

- `mce_session_create`
- `mce_session_destroy`
- `mce_session_load_chart`
- `mce_session_load_project`
- `mce_session_save_chart`
- `mce_session_export_mcz`
- `mce_session_apply_edit_json`
- `mce_session_undo`
- `mce_session_redo`
- `mce_session_can_undo`
- `mce_session_can_redo`
- `mce_session_note_count`
- `mce_session_get_note_snapshot`
- `mce_session_last_error`
- `mce_free_string`

热路径使用固定布局 POD 结构输出音符快照；JSON 只用于编辑命令、配置、导入导出元数据，不用于逐帧渲染。

## 桌面端改造原则

当前 `Malody_catch_editor` 的 Qt UI 不重写，只把业务核心替换为 submodule 中的 core：

- 添加 `external/malody-catch-core` submodule。
- CMake 引入 core target。
- 保留现有 `MainWindow`、`ChartCanvas`、面板、菜单、QSS、插件 UI。
- 将现有 `ChartController` 改为 Qt 适配器：对外仍使用 Qt signals/slots，对内调用 `mce::EditorSession`。
- 桌面端保留 Qt 侧 `Chart/Note` 兼容快照或转换层，避免一次性大规模改 UI。
- 插件系统继续作为桌面端能力，不进入 core，也不进入移动端首版。

桌面端验收标准：

- 现有桌面布局和样式不变化。
- 现有插件入口不变化。
- `.mc/.mcz` 读写结果与迁移前兼容。
- 现有最小测试继续通过。

## 移动端方案

新建 `Malody_catch_mobile`，首发 Android，技术栈为 Flutter + `dart:ffi`。

移动端职责：

- Flutter Canvas/CustomPainter 或后续更高性能渲染方案。
- 触摸手势：点击放置、拖动移动、框选/多选、双指缩放、长按菜单。
- 移动端布局与页面导航。
- Android 文件选择、导入、保存、分享。
- 音频播放与播放进度同步。

Core 职责：

- 谱面数据。
- 编辑规则。
- 撤销/重做。
- `.mc/.mcz` 导入导出。
- BPM/time 计算。
- 数据校验。

移动端首版功能闭环：

- 打开 `.mc/.mcz`。
- 放置、删除、移动普通音符和 rain 音符。
- 选择、复制、粘贴。
- 播放同步。
- 保存 `.mc`。
- 导出 `.mcz`。

移动端首版明确不做：

- 现有桌面插件系统。
- QWidget/QML 移动布局延续。
- 与桌面端完全相同的菜单结构。

## 实施顺序

1. 在上一级目录创建 `Malody_catch_core`。
2. 用 `git-filter-repo` 从当前仓库抽取核心代码并保留历史。
3. 在 core 仓库完成第一轮纯 C++ 化和 CMake 独立构建。
4. 建立 core 单元测试，覆盖现有 `minimal_tests.cpp` 中的核心场景。
5. 设计并实现 C ABI FFI 层。
6. 在当前桌面仓库添加 core submodule。
7. 将桌面端 `ChartController` 接到 core，保持 UI 层不变。
8. 新建 `Malody_catch_mobile` Flutter 仓库。
9. Android 端接入 core shared library。
10. 实现移动端最小编辑闭环。

## 测试计划

Core 单元测试：

- Note 校验。
- Rain 起止时间校验。
- BPM 排序。
- beat/ms 双向换算。
- 批量编辑校验。
- 撤销/重做。
- `.mc` load/save parity。
- `.mcz` import/export 目录结构兼容性。

桌面端回归：

- 打开、编辑、保存、导出。
- 皮肤加载。
- 播放同步。
- 插件菜单和面板入口。
- 现有 `CatchChartEditorTests`。

FFI 测试：

- session 反复 create/destroy。
- load chart 后读取 note snapshot。
- apply edit 后 undo/redo。
- 错误输入返回稳定错误信息且不污染 session。

移动端验收：

- 中端 Android 设备上画布交互稳定，不出现明显卡顿和布局抽搐。
- 能完成打开、编辑、保存、重开验证。
- 无插件能力时 UI 不展示插件入口。

## 默认决策

- Core 仓库名：`Malody_catch_core`。
- 移动端仓库名：`Malody_catch_mobile`。
- 两个新仓库均与当前 `Malody_catch_editor` 并列，位于上一级目录。
- 桌面端 submodule 路径：`external/malody-catch-core`。
- Core 必须是纯 C++，不保留 QtCore 依赖。
- 移动端首版 Android 优先。
- 移动端首版使用 Flutter + `dart:ffi`。
- 移动端首版不支持插件。
- 桌面端稳定性优先于一次性删除所有 Qt 兼容模型。
