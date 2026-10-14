# Third-party components

EditHere uses dynamically linked Qt 6.8 (Qt Core, Gui, Widgets,
Concurrent, Network, Qt SVG, Qt Image Formats and Qt Multimedia), copyright The Qt Company Ltd. and
contributors, under LGPL version 3 and the components' applicable licenses.

Qt is not part of EditHere's application code. The shared libraries
and image plugins remain separately replaceable by compatible modified versions.
No EditHere restriction prohibits reverse engineering for debugging modifications
to these LGPL libraries. The license texts, copyright notices and upstream
attributions are included in the licenses directory. Some notices cover source-tree
components beyond the subset present in this runtime.

Windows release builds use MinGW-w64/winpthreads and GCC 13.1.0 runtime
libraries. See licenses/mingw for their notices, GPL text and GCC Runtime Library Exception.
Local MSVC preview builds instead use the Microsoft Visual C++ runtime deployed
by Qt's windeployqt. The exact Qt and FFmpeg versions and FFmpeg binary hashes
are recorded in dependency-versions.json; SDK SPDX records, when provided by Qt,
are included under licenses/qt-sbom.

Video playback uses FFmpeg shared libraries obtained with the official Qt
Multimedia binaries, copyright the FFmpeg developers and respective contributors,
under LGPL-2.1-or-later and the applicable permissive licenses listed in
licenses/ffmpeg/LICENSE.md and licenses/qtmultimedia/third-party. Qt's supplied
FFmpeg build does not enable the optional GPL or nonfree components. The FFmpeg
libraries remain separately replaceable; the application does not prohibit
reverse engineering for debugging modifications to these LGPL libraries.
Qt 6.8.0's Windows preview uses FFmpeg 7.0.2; the pinned Qt 6.8.3 CI release uses
FFmpeg 7.1. Source-tree attributions may describe components beyond the shipped
runtime subset. Source code and the matching Qt build recipes are available at:
- https://ffmpeg.org/releases/ffmpeg-7.0.2.tar.xz
- https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz
- https://code.qt.io/cgit/qt/qt5.git/tree/coin/provisioning?h=v6.8.0
- https://code.qt.io/cgit/qt/qt5.git/tree/coin/provisioning?h=v6.8.3

Corresponding unmodified Qt source archives, including third-party sources and
Qt build configuration, are available at no charge from the upstream references
below. Select the version recorded in dependency-versions.json for the Windows
package (the release CI pins 6.8.3; the local MSVC preview uses 6.8.0). Qt binaries
were obtained from the official Qt online repository through aqtinstall; the
application dynamically links them. The application license does not cover these
libraries.

Upstream source references:
- https://download.qt.io/archive/qt/6.8/6.8.3/submodules/
- https://download.qt.io/archive/qt/6.8/6.8.0/submodules/
- https://code.qt.io/cgit/qt/qtbase.git/tag/?h=v6.8.3
- https://code.qt.io/cgit/qt/qtsvg.git/tag/?h=v6.8.3
- https://code.qt.io/cgit/qt/qtimageformats.git/tag/?h=v6.8.3
- https://code.qt.io/cgit/qt/qtmultimedia.git/tag/?h=v6.8.3
- https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html

System fonts are loaded from the operating system and are not bundled.
No Snipaste, PixPin or Agentation source code or assets are included.

The Linux builds use Qt's platform and multimedia runtime plugins. Applicable notices
for bundled system libraries are included under licenses/linux. Qt Wayland and
SVG sources are available from the Qt submodules references above. Libraries can
be replaced in an extracted AppImage. No modified third-party library is distributed.
linuxdeploy and its Qt plugin are build tools; their binaries are pinned and
SHA-256 verified by scripts/fetch-linuxdeploy.py.

Interface action icons use a pinned subset of Lucide (ISC, with the retained
Feather MIT notice), plus EditHere-specific derivatives recorded in
assets/ui-icons/manifest.json in the source tree. The complete upstream notices
ship in licenses/ui-icons/LICENSE. Application branding is separate and unchanged.
