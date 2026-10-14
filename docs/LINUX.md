# Linux 白板版

**简体中文** · [English](LINUX.en.md)

目标环境为 Ubuntu 24.04 x86_64、Qt 6.8.3。启动后进入空白工作台，支持图片导入、粘贴、拖放、批注、布局调整和保存导出。截图、OCR、全局快捷键、屏幕权限及辅助功能探测依赖已删除。

## 运行

将 AppImage 与配套 CLI 放在同一目录：

```bash
chmod +x EditHere-linux-x86_64.AppImage edithere-cli
./EditHere-linux-x86_64.AppImage
./edithere-cli --version
```

没有 FUSE 时使用 `APPIMAGE_EXTRACT_AND_RUN=1`。AppImage 自带 Qt；中文字体可使用 fonts-noto-cjk。CLI 也可通过 AppImage 的 `--cli <参数>` 调用。登录启动仍使用 XDG Autostart，可在设置中管理。

## 构建与打包

使用 GCC、CMake、Ninja 和 Qt 6.8.3 gcc_64。窗口系统与多媒体插件的系统依赖见 `.github/workflows/native-linux.yml`。

```bash
export QT_ROOT=/path/to/Qt/6.8.3/gcc_64
bash scripts/build-linux.sh
python3 scripts/fetch-linuxdeploy.py
export LINUXDEPLOY="$PWD/.tools/linuxdeploy/linuxdeploy-x86_64.AppImage"
export LINUXDEPLOY_PLUGIN_QT="$PWD/.tools/linuxdeploy/linuxdeploy-plugin-qt-x86_64.AppImage"
bash scripts/package-linux.sh
```

GitHub Actions 按相同源码构建 Linux AppImage。发行工作流会在 Windows、macOS、Linux 和 Web 全部打包成功后发布 Release。实际桌面交互仍需在目标 Linux 环境验收。
