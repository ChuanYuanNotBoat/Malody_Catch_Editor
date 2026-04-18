# Testing Guide

本文档说明 Malody Catch Editor 的测试策略、执行方式与当前覆盖范围。

## 1. 测试目标

- 保证核心时间换算逻辑稳定（`MathUtils`）
- 保证谱面数据关键行为不回归（`Chart`）
- 保证基础文件扫描/难度读取流程可用（`ProjectIO`）
- 保证每次改动后可快速执行最小回归

## 2. 当前自动化测试

当前项目已接入一个最小测试可执行程序：`CatchChartEditorTests`。

覆盖内容：
- `MathUtils`：
  - `beatToMs/msToBeat` 基本往返
  - `buildBpmTimeCache` 与列表计算一致性
- `Chart`：
  - `removeNote(const Note&)` 按 `id` 优先删除
  - BPM 插入后排序
- `ProjectIO`：
  - `getDifficultyFromMc`
  - `findChartsInDirectory`

测试入口文件：
- `tests/minimal_tests.cpp`

## 3. 本地执行

在仓库根目录执行：

```powershell
cmake -S . -B build_codex -DBUILD_TESTING=ON
cmake --build build_codex --config Release --target CatchChartEditorTests
ctest --test-dir build_codex -C Release --output-on-failure
```

如果需要同时验证主程序可编译：

```powershell
cmake --build build_codex --config Release --target CatchChartEditor
```

## 4. 手工回归建议

以下场景建议每次涉及编辑器交互、插件或 IO 修改后执行：

- `ChartCanvas` 交互：
  - 放置/移动/删除音符
  - 框选、右键菜单、粘贴预览与确认
  - 播放时自动滚动、暂停吸附
- 插件链路：
  - 插件发现与启动
  - `runToolAction` 成功与失败日志
- MCZ 导入导出：
  - 普通路径
  - 含空格路径
  - 含特殊字符路径（Windows）

说明：
- `ProjectIO::exportToMcz/extractMcz` 的“特殊字符路径 + 真压缩解压”端到端自动化在当前环境仍建议手工复核。

## 5. 失败排查

- `ctest` 报 `0xc0000135`（Windows 常见）
  - 通常是 Qt 运行时 DLL 路径问题
  - 当前 CMake 已为测试注入 Qt Core 目录到 PATH
- 用例失败但无输出
  - 可直接运行测试程序查看明细：
  - `.\build_codex\Release\CatchChartEditorTests.exe`

## 6. 新增测试规范

- 优先扩展 `tests/minimal_tests.cpp`，保持依赖轻量
- 每新增功能至少补一个“正常路径”与一个“边界路径”
- 涉及时间/精度逻辑时，统一使用小误差比较（避免浮点误判）
