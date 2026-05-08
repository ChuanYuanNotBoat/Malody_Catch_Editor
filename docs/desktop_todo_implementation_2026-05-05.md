# 桌面 TODO 实施与验收文档（2026-05-05）

## 1. 本轮目标
- 全量完成 `TODO.md` 桌面项（P0/P1/P2）。
- 新增并完成：Qt 控制台中文乱码修复。
- 打开大谱面卡顿专项：采用“异步复制 + 进度窗”，保证 UI 不出现“未响应”。
- 性能验收基准固定为：
  - `C:\Users\boatnotcy\AppData\Local\CatchEditor\Malody Catch Chart Editor\beatmap\KEDAMONO Drop-out\0\1737904376.mc`

## 2. 主要实现变更

### 2.1 打开谱面卡顿专项（异步复制 + 进度窗）
- 将工作副本整目录复制从 UI 线程迁移到后台线程。
- 主线程显示可取消模态进度窗，并持续处理事件，避免窗口假死。
- 复制完成后再进入 `ChartController::loadChart`，保留工作副本隔离语义。
- 增加结构化日志字段：
  - `elapsed_ms`
  - `files_total`
  - `files_copied`
  - `bytes_total`
  - `bytes_copied`
  - `max_file_ms`
  - `success`

涉及文件：
- `src/app/MainWindow.cpp`

### 2.2 P0 稳定性修复
- `ExternalProcessPlugin` 请求超时后增加健康探测，探测失败则强制重启子进程并清理状态；后续请求可自动拉起。
- `ChartCanvas::endMoveSelection` 提交阶段按 note 身份重建变更（优先 id），降低对旧 index 的依赖。
- 音频失败时主窗体显式提示（弹窗 + 状态栏），并禁用播放入口；加载成功后恢复。

涉及文件：
- `src/plugin/ExternalProcessPlugin.h`
- `src/plugin/ExternalProcessPlugin.cpp`
- `src/ui/CustomWidgets/ChartCanvas/ChartCanvasMouse.cpp`
- `src/app/MainWindow.h`
- `src/app/MainWindow.cpp`
- `src/app/MainWindowPrivate.h`

### 2.3 P1 质量与防护
- 插件批量编辑解析加入硬限制：`id/sound` 长度上限、关键数值范围上限、异常输入拒绝。
- Qt 噪音日志过滤：支持按 category / 前缀配置过滤，且不屏蔽 critical/fatal。
- 扩展数学边界测试：空 BPM、BPM=0、极端 offset、跨段 round-trip。

涉及文件：
- `src/plugin/ExternalProcessPlugin.cpp`
- `src/utils/Logger.h`
- `src/utils/Logger.cpp`
- `src/utils/Settings.h`
- `src/utils/Settings.cpp`
- `src/app/Application.cpp`
- `src/app/MainWindowDialogs.cpp`
- `tests/minimal_tests.cpp`

### 2.4 P2 维护性与基线
- 删除 `ChartCanvas::invalidateCache`、`updateNotePosCacheIfNeeded` 空声明与空实现。
- 通过自动化用例记录 KEDAMONO 基线统计，并更新 TODO 结论。

涉及文件：
- `src/ui/CustomWidgets/ChartCanvas/ChartCanvas.h`
- `src/ui/CustomWidgets/ChartCanvas/ChartCanvasPlayback.cpp`
- `tests/minimal_tests.cpp`

## 3. 代码审核发现与追加修复
本轮在“实现完成后”额外做了代码审核，并修复 2 个高风险点：

1. `Logger` Qt 过滤配置与回调链存在并发/死锁风险
- 问题：过滤配置在消息处理与设置更新之间存在非受控读取；且在持锁状态调用上一层 Qt handler 有潜在锁互斥风险。
- 修复：`qtMessageHandler` 在锁内完成过滤与文件写入，释放锁后再调用上一层 handler；过滤配置 getter 增加互斥保护。

2. `endMoveSelection` 对无 id note 的提交可能丢变更
- 问题：仅依赖“当前内容匹配 original”会在拖拽预览已改写时失配。
- 修复：提交阶段构建 `original -> moved` 目标，优先按 id 定位，fallback 增强（含 source index / moved 内容匹配），并统一回滚后走 `moveNotes`。

涉及文件：
- `src/utils/Logger.cpp`
- `src/ui/CustomWidgets/ChartCanvas/ChartCanvasMouse.cpp`

## 4. 验证结果

### 4.1 自动化
执行命令：
- `cmake --build build_codex --config Release`
- `ctest --test-dir build_codex -C Release --output-on-failure`

结果：
- 构建成功。
- 测试通过（100%）。

### 4.2 KEDAMONO 基线（自动化统计）
来源：`CatchChartEditorTests` 中 `KEDAMONO render baseline` 用例。

固定谱面：
- `C:\Users\boatnotcy\AppData\Local\CatchEditor\Malody Catch Chart Editor\beatmap\KEDAMONO Drop-out\0\1737904376.mc`
- note 总数：9605

结果：
- 5k 样本：`elapsed_ms=0.156`，`avg_visible=58.70`，`max_visible=264`
- 10k 样本（实际 9605）：`elapsed_ms=0.173`，`avg_visible=64.06`，`max_visible=264`

结论：
- 当前“可见区裁剪 + 索引检索”成本显著低于 16ms 预算，本轮不进入虚拟化重构。

## 5. 你醒来后建议手工验收
1. 打开上述 KEDAMONO 谱面：观察主窗口在复制期间是否始终可响应、无“未响应”标题。
2. 在进度窗中点取消：确认无半成品工作副本残留，且可继续正常打开其他谱面。
3. 构造插件超时/卡死场景：确认后续请求可恢复，且无僵尸子进程。
4. 构造音频失败场景（缺失文件或不支持格式）：确认有弹窗提示，播放入口禁用，重载成功后恢复。
5. 在 PowerShell/CMD 下观察中文日志输出：确认无乱码。

## 6. 变更文件清单
- `src/app/Application.cpp`
- `src/app/MainWindow.cpp`
- `src/app/MainWindow.h`
- `src/app/MainWindowDialogs.cpp`
- `src/app/MainWindowPrivate.h`
- `src/plugin/ExternalProcessPlugin.h`
- `src/plugin/ExternalProcessPlugin.cpp`
- `src/ui/CustomWidgets/ChartCanvas/ChartCanvas.h`
- `src/ui/CustomWidgets/ChartCanvas/ChartCanvasMouse.cpp`
- `src/ui/CustomWidgets/ChartCanvas/ChartCanvasPlayback.cpp`
- `src/utils/Logger.h`
- `src/utils/Logger.cpp`
- `src/utils/Settings.h`
- `src/utils/Settings.cpp`
- `tests/minimal_tests.cpp`


