# Malody Catch Chart Editor / Malody Catch 谱面编辑器

A desktop chart editor for Malody Catch mode, developed to fill the feature gaps of the official charting tool.
为 Malody Catch 模式开发的谱面编辑器，弥补官方制谱器功能的缺失。

当前版本 / Current Version: **Beta v1.9.1**

> ⚠️ **Early Stage Notice / 早期状态说明**
> This project is in a very early stage of development. It is not yet ready for production use. Features may be incomplete, unstable, or subject to change.
> 本项目处于极早期开发阶段，尚未具备实际使用价值。功能可能不完整、不稳定或随时变更。
> Use at your own risk / 使用风险自负。

---

## 📥 下载

点击下载最新版本：[CatchChartEditor Beta v1.9.1 安装包](https://github.com/ChuanYuanNotBoat/Malody_Catch_Editor/releases/latest)

文档导航：
- 帮助文档：[docs/help.md](docs/help.md)
- 版本信息：[docs/version.md](docs/version.md)
- 测试说明：[TESTING.md](TESTING.md)
- 插件说明：[src/plugin/README.md](src/plugin/README.md)

---

## Features / 功能特性

- Load / save `.mc/mcz` charts (JSON based) / 读取与保存 `.mc/mcz` 谱面（基于 JSON）
- Place, move, delete, copy, and paste notes / rain notes / 放置、移动、删除、复制粘贴普通音符与雨音符
- Snap to grid and time division / 网格吸附与时间轴分度吸附
- Colorful notes based on beat division (toggleable) / 按拍型显示彩色音符（可开关）
- Hyperfruit detection with red outline / Hyperfruit 自动判定与红色描边
- Skin support (custom note images) / 皮肤支持（自定义音符图片）
- Audio playback with speed control / 音频播放与速度控制
- BPM table editing / BPM 表编辑
- Undo / redo / 撤销与重做
- Plugin system (experimental)
  插件系统（实验性）

---

## Recent Updates / 近期特性

- `Beta v1.9.1`
  - Fixed `.mcz` export structure compatibility (top-level `0/`), so Malody can import reliably.
  - 修复 `.mcz` 导出目录结构兼容性（顶层 `0/`），避免 Malody 无法导入。
  - Filtered export payload to required assets only (`.mc` / 音频 / 背景 / 引用资源) and switched Meta edits to auto-save.
  - 导出仅打包必要资源（`.mc` / 音频 / 背景 / 引用资源），Meta 编辑改为自动保存。
- `Beta v1.9.0`
  - Curve plugin now supports per-segment density (between two anchors), with better selection and density interaction.
  - 曲线插件支持“按分段设置密度”（两节点间），并修复节点/音符选择联动与密度交互。
- `Beta v1.8.x`
  - Added checkable plugin actions, anchor placement toggle, and sidecar auto sync (`.mcce-plugin/*.curve_tbd.json`).
  - 新增可勾选插件动作、锚点放置开关，以及曲线 sidecar 自动同步（`.mcce-plugin/*.curve_tbd.json`）。
  - Unified UTF-8 process-plugin environment and improved plugin toolbar/panel theme consistency.
  - 统一进程插件 UTF-8 运行环境，并优化插件工具栏/面板主题一致性。
- `Beta v1.7.x`
  - Added mirror flip workflow (custom axis, preview, center-line quick action) and timeline division color presets.
  - 新增镜像翻转流程（自定义轴、预览、中心线快速翻转）与时间分度颜色预设系统。
- `Beta v1.6.x ~ v1.5.x`
  - Added shortcut customization, imported chart quick entry, and selection mode.
  - 新增快捷键自定义、已导入谱面快速入口与选择模式。
  - Plugin system upgraded with panel/overlay/batch-edit extension points and process-plugin protocol.
  - 插件系统升级为面板/叠加层/批量编辑三类扩展点，并完善进程插件协议。

For full changelog / 完整更新历史：
- [docs/history.md](docs/history.md)

---

## Screenshots / 截图

(coming soon)

---

## Build Requirements / 构建要求

- C++17 compatible compiler / 支持 C++17 的编译器
- CMake 3.16+
- Qt 6 (Core, Widgets, Multimedia, Quick, QuickWidgets, Qml)
  Qt 6 组件：Core、Widgets、Multimedia、Quick、QuickWidgets、Qml

### Build steps / 构建步骤

```bash
git clone https://github.com/ChuanYuanNotBoat/Malody_Catch_Editor.git
cd Malody_Catch_Editor
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Windows 开发构建（含最小测试）：

```powershell
cmake -S . -B build_test -DBUILD_TESTING=ON
cmake --build build_test --config Release --target CatchChartEditorTests
ctest --test-dir build_test -C Release --output-on-failure
```

---

## Usage / 使用说明

1. Launch the editor / 启动编辑器
2. Open a chart (`.mc`) or create a new one / 打开或新建谱面
3. Use the right panel to select mode (Note / Rain / Delete / Select)右侧栏选择模式（普通音符 / 雨音符 / 删除 / 选择）
4. Left-click to place / delete notes; Ctrl+drag to select左键放置/删除音符；Ctrl+拖动框选
5. Use timeline to navigate and adjust speed / 使用时间轴导航和调整速度
6. Save your chart (`.mc`) or export as `.mcz`
   保存谱面（.mc）或导出为 .mcz 压缩包

---

## File Format / 文件格式

The chart format is compatible with Malody Catch `.mc` files (JSON based).
谱面格式兼容 Malody Catch 的 `.mc` 文件（基于 JSON）。

Example note:
音符示例：

```json
{
    "beat": [0, 1, 4],
    "x": 256
}
```

Rain note (type 3):
雨音符（type 3）：

```json
{
    "beat": [0, 1, 4],
    "x": 256,
    "type": 3,
    "endbeat": [0, 3, 4]
}
```

---

## Skin Credits / 皮肤致谢

Special thanks to **myhome** for the skin used in this editor.特别感谢 **myhome** 授权使用其制作的皮肤。

- Skin link / 皮肤链接: [https://m.mugzone.net/store/skin/detail/5982](https://m.mugzone.net/store/skin/detail/5982)

The skin is included under permission and is used for demonstration purposes only.
皮肤文件已获授权包含在本项目中，仅用于演示目的。

---

## License / 许可协议

This project is licensed under the GPL-3.0 License — see the [LICENSE](LICENSE) file for details.
本项目基于 GPL-3.0 许可证发布，详见 [LICENSE](LICENSE) 文件。

---

## Acknowledgements / 致谢

- Malody community for the game and file format
- Qt team for the cross-platform framework
- All testers and contributors

---

## Runtime Layout / 运行目录说明

- Runtime plugin directory is resolved as `<appDir>/plugins`.
- 构建后会复制默认皮肤、插件与文档到可执行文件目录，便于开箱即用。
