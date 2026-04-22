# Malody Catch Chart Editor / Malody Catch 谱面编辑器

A desktop chart editor for Malody Catch mode, developed to fill the feature gaps of the official charting tool.
为 Malody Catch 模式开发的谱面编辑器，弥补官方制谱器功能的缺失。

当前版本 / Current Version: **Beta v1.7.3**

> ⚠️ **Early Stage Notice / 早期状态说明**
> This project is in a very early stage of development. It is not yet ready for production use. Features may be incomplete, unstable, or subject to change.
> 本项目处于极早期开发阶段，尚未具备实际使用价值。功能可能不完整、不稳定或随时变更。
> Use at your own risk / 使用风险自负。

---

## 📥 下载

点击下载最新版本：[CatchChartEditor Beta v1.7.3 安装包](https://github.com/ChuanYuanNotBoat/Malody_Catch_Editor/releases/latest)

---

## Features / 功能特性

- Load / save `.mc/mcz` charts (JSON based)读取/保存 `.mc/mcz` 谱面（基于 JSON）
- Place, move, delete, copy, and paste notes / rain notes放置、移动、删除、复制粘贴普通音符与雨音符
- Snap to grid and time division网格吸附与时间轴分度吸附
- Colorful notes based on beat division (toggleable)按拍型显示彩色音符（可开关）
- Hyperfruit detection with red outlineHyperfruit 自动判定与红色描边
- Skin support (custom note images)皮肤支持（自定义音符图片）
- Audio playback with speed control音频播放与速度控制
- BPM table editingBPM 表编辑
- Undo / redo撤销/重做
- Plugin system (experimental)
  插件系统（实验性）

---

## Recent Updates / 近期特性

- Safer note removal by note `id` first (avoid wrong delete on same-content notes)音符删除优先按 `id` 匹配，避免同内容误删
- Temporary audio files are cleaned up automatically after loading/destruction临时音频文件会在加载切换/析构时自动清理
- Playback snap now follows current time division setting播放头吸附使用当前时间分度设置
- Paste preview now matches 288-division conversion behavior粘贴预览与 288 分度转换行为一致
- Unified skin discovery from both `skins` and `resources/default_skin`皮肤扫描统一支持 `skins` 与 `resources/default_skin`
- Added diagnostics/export and log settings dialog integration
  新增诊断导出与日志设置入口

---

## Screenshots / 截图

(coming soon)

---

## Build Requirements / 构建要求

- C++17 compatible compiler支持 C++17 的编译器
- CMake 3.16+
- Qt 6 (Core, Widgets, Multimedia)
  Qt 6 组件：Core, Widgets, Multimedia

### Build steps / 构建步骤

```bash
git clone https://github.com/ChuanYuanNotBoat/Malody_Catch_Editor.git
cd Malody_Catch_Editor
mkdir build && cd build
cmake ..
cmake --build . --config Release
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
