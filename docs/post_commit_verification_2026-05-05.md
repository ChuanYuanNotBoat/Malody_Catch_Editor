# 提交后验证报告（2026-05-05）

## 提交信息
- Commit: `270ae7e`
- Title: `desktop: finish TODO set (async working-copy open, plugin self-heal, log utf8/filter, boundary tests)`

## 自动化验证

### 1) CTest
命令：
- `ctest --test-dir build_codex -C Release --output-on-failure`

结果：
- `1/1` 通过
- `100% tests passed, 0 tests failed out of 1`

### 2) CatchChartEditorTests 直跑
命令：
- `build_codex/Release/CatchChartEditorTests.exe`

关键结果：
- `PASSED: MathUtils empty BPM boundary`
- `PASSED: MathUtils zero BPM boundary`
- `PASSED: MathUtils extreme offset boundary`
- `PASSED: MathUtils cross-segment round-trip boundary`
- `PASSED: KEDAMONO render baseline`

KEDAMONO 基线输出（本次复跑）：
- `KEDAMONO_BASELINE total=9605 notes, sample5k=5000, sample10k=9605`
- `KEDAMONO_BASELINE 5k elapsed_ms=12.023 avg_visible=58.72 max_visible=265`
- `KEDAMONO_BASELINE 10k elapsed_ms=13.197 avg_visible=64.08 max_visible=265`

## 结论
- 提交 `270ae7e` 自动化测试通过。
- KEDAMONO 基线复跑仍在 16ms 预算内（5k/10k 均 < 16ms），与“本轮不进入虚拟化重构”的结论一致。

## 待手工验收项（醒来后）
1. 打开 KEDAMONO 时进度窗与 UI 响应性。
2. 复制取消后的工作副本清理。
3. 音频失败弹窗与播放按钮禁用/恢复。
4. PowerShell/CMD 中文日志显示无乱码。
5. 插件超时后自动恢复且无僵尸进程。
