# Linux whiteboard edition

[简体中文](LINUX.md) · **English**

The target is Ubuntu 24.04 x86_64 with Qt 6.8.3. Launch to an empty workspace and import, paste or drop images, annotate, adjust layouts, save and export. System screenshots, OCR, global shortcuts and screen/accessibility probing dependencies are removed.

## Running

Keep the AppImage and its CLI together:

```bash
chmod +x EditHere-linux-x86_64.AppImage edithere-cli
./EditHere-linux-x86_64.AppImage
./edithere-cli --version
```

Without FUSE, set `APPIMAGE_EXTRACT_AND_RUN=1`. The package includes Qt; fonts-noto-cjk provides Chinese fonts. The AppImage also accepts `--cli <arguments>`. Login launch uses XDG Autostart and remains configurable.

## Build and package

Use GCC, CMake, Ninja and Qt 6.8.3 gcc_64. Platform and multimedia runtime requirements are listed in `.github/workflows/native-linux.yml`.

```bash
export QT_ROOT=/path/to/Qt/6.8.3/gcc_64
bash scripts/build-linux.sh
python3 scripts/fetch-linuxdeploy.py
export LINUXDEPLOY="$PWD/.tools/linuxdeploy/linuxdeploy-x86_64.AppImage"
export LINUXDEPLOY_PLUGIN_QT="$PWD/.tools/linuxdeploy/linuxdeploy-plugin-qt-x86_64.AppImage"
bash scripts/package-linux.sh
```

GitHub Actions builds the AppImage from the same whiteboard source. Release publication requires successful Windows, macOS, Linux and Web packages. Real desktop interactions still require verification on the target Linux system.
