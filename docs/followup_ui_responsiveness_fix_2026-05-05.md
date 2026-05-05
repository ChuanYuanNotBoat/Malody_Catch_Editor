# 跟进修复：打开谱面“进度条闪过后短暂无响应”

日期：2026-05-05

## 问题复盘
在“异步复制 + 进度窗”落地后，复制阶段已不阻塞 UI；但在复制完成后，`ChartController::loadChart` 仍在主线程同步执行 `ChartIO::load`（大 JSON 解析），因此会出现“进度条闪过后短暂无响应”。

## 修复内容
1. 新增 `ChartController::loadChartFromData(const QString&, Chart)`
- 职责：仅在主线程接管已解析好的 `Chart` 数据（赋值、清空 undo、发 `chartChanged/chartLoaded` 信号）。
- 位置：
  - `src/controller/ChartController.h`
  - `src/controller/ChartController.cpp`

2. `MainWindow::loadChartFile` 改为“两阶段异步”
- 阶段 A：后台复制工作副本（已有）。
- 阶段 B：后台执行 `ChartIO::load` 解析 working chart，主线程显示“Loading chart data...”进度窗（busy），期间持续事件循环。
- 解析完成后主线程调用 `loadChartFromData` 接管。
- 位置：
  - `src/app/MainWindow.cpp`

## 结果
- 复制后不再立即落入主线程重解析，明显减少“进度条消失后窗口短暂假死”的概率与时长。
- 自动化验证通过：
  - `cmake --build build_codex --config Release`
  - `ctest --test-dir build_codex -C Release --output-on-failure`

## 备注
- 该修复不改变“工作副本隔离编辑”模型。
- 若仍有个别机器出现轻微卡顿，下一步建议对 `loadChartFromData` 后续 UI 组件刷新链路做分段 profiling（canvas rebuild、panel refresh、audio load）。
