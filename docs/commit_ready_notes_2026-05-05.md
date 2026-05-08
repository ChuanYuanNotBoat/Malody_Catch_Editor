# 提交准备说明（2026-05-05）

## 1) 变更范围确认
本次变更仅涉及桌面主线与测试：
- `src/app/*`
- `src/plugin/*`
- `src/ui/CustomWidgets/ChartCanvas/*`
- `src/utils/*`
- `tests/minimal_tests.cpp`
- `docs/desktop_todo_implementation_2026-05-05.md`



## 2) 建议提交标题
`desktop: finish TODO set (async working-copy open, plugin self-heal, log utf8/filter, boundary tests)`

## 3) 建议提交说明（可直接用）
- open: move working-copy directory clone off UI thread and show cancelable progress dialog
- open: keep working-copy isolation semantics and add structured copy timing metrics
- plugin: add timeout health probe and process restart self-heal for ExternalProcessPlugin
- plugin: harden batch-edit note json parsing limits (id/sound length, beat/denominator/offset/vol bounds)
- canvas: harden endMoveSelection identity-based commit/rollback, reduce stale index coupling
- audio: surface load failures in MainWindow and disable play action until audio is ready
- logging: add Qt message noise filter settings (category/prefix), wire app startup & dialog
- logging: fix Windows console Chinese mojibake via UTF-8 codepage/output
- cleanup: remove deprecated empty ChartCanvas cache stubs
- tests: add math edge boundaries + KEDAMONO render baseline case
- docs: add full implementation + review + acceptance guide

## 4) 提交前状态
- Build: `cmake --build build_codex --config Release` 通过
- Test: `ctest --test-dir build_codex -C Release --output-on-failure` 通过

## 5) 提交命令（示例）
```powershell
git add src/app/Application.cpp `
        src/app/MainWindow.cpp `
        src/app/MainWindow.h `
        src/app/MainWindowDialogs.cpp `
        src/app/MainWindowPrivate.h `
        src/plugin/ExternalProcessPlugin.cpp `
        src/plugin/ExternalProcessPlugin.h `
        src/ui/CustomWidgets/ChartCanvas/ChartCanvas.h `
        src/ui/CustomWidgets/ChartCanvas/ChartCanvasMouse.cpp `
        src/ui/CustomWidgets/ChartCanvas/ChartCanvasPlayback.cpp `
        src/utils/Logger.cpp `
        src/utils/Logger.h `
        src/utils/Settings.cpp `
        src/utils/Settings.h `
        tests/minimal_tests.cpp `
        docs/desktop_todo_implementation_2026-05-05.md

git commit -m "desktop: finish TODO set (async working-copy open, plugin self-heal, log utf8/filter, boundary tests)"
```

## 6) 备注
- `TODO.md` 已在本地更新，但该文件当前不受 git 跟踪（仓库配置导致），不会进入提交。
- 若希望把 TODO 文档也纳入版本控制，需要先调整仓库忽略规则。

