# EditHere · 改这里：开发说明

> 本地精简版已改为纯批注白板。截图、OCR、长截图和贴图模块已删除；核心批注、布局、项目和 Agent 会话保持原有格式。设计见 [DESIGN.md](../DESIGN.md)。后台单元测试最长 60 秒。


**简体中文** · [English](DEVELOPMENT.en.md)

[返回产品介绍](../README.md) · [使用指南](USER-GUIDE.md) · [AI 与命令行](AGENT-CLI.md) · [更新记录](../CHANGELOG.md)

以下命令均在仓库根目录执行，文件路径也相对于仓库根目录。

## 分支与技术栈

当前分支为 `codex/annotation-whiteboard`，使用 C++20 + Qt 6 Widgets。已保留的 `desktop` 分支为 Windows WPF 0.2（`cf9c364`），`web` 分支为 Web 0.3（`66548c5`）。

## 版本管理

日常修复和小幅优化只增加末位补丁号，例如 `0.8.0 → 0.8.1 → 0.8.2`。中间位只在集中完成较大功能阶段、明确发布时增加；不再为每轮开发递增。第一位保留给明确的大版本发布，已有版本号和历史包保持不变。回退仅针对当次实现，不冻结后续版本；撤回的编号不复用。

产品版本唯一来源为 `CMakeLists.txt` 的 `project(... VERSION ...)`，以该声明为准。运行时版本和 Mac 应用信息自动使用该值；成功链接后生成 `build/version.txt`（Mac 为 `build-macos/version.txt`），打包脚本据此命名；Windows 将 `version.txt` 放在程序目录，macOS 放在应用包 `Contents/Resources/`。修改版本后必须重新构建，避免将旧程序标记为新版本。产品版本与 JSON 格式独立管理：精简图片反馈的当前格式由 `feedback-minimal.schema.json` 定义（`0.9.0` 起为 `objects` 对象结构，`0.8.21` 的 `annotations` / `changes` 并行数组见 `feedback-v0.7.schema.json`，仍可导入）；`feedback-v1` / `v1.1` / `v2` 描述早期**项目文档**格式，完整图片项目使用 `project-v3.schema.json`。视频反馈与项目分别使用 `video-feedback-v1.schema.json`（`video-feedback-1`）及 `video-project-v1.schema.json`（`video-project-1`）。文件名代表数据格式，不代表应用版本。

## 发布流程

打 `vX.Y.Z` 标签并推送即触发 `.github/workflows/release.yml`：Windows、macOS 与 Linux 构建任务分别产出程序包，`build-web` 作业打包两种 Web 包，release 任务汇总后创建 GitHub Release。手动从分支运行发布工作流时，新标签指向本次实际构建的源码提交，所有检查通过后才发布。源码版本或本地包更新不等于公开最新版本已更新；GitHub 的 `/releases/latest` 依其正式 Release 元数据决定。应用内更新仅检查 GitHub `yyyzl/EditHere` 的正式发行版。安装包和 SHA-256 来自同一发行版，不再查询上游产品镜像。

- **发布必须带完整程序包**。release 任务在发布前检查 `EditHere-win-x64-setup.exe`、`EditHere-win-x64.zip`、`EditHere-macos-universal.dmg`、`EditHere-linux-x86_64.AppImage` 与两种 Web 包（`EditHere-web.zip`、`EditHere-web-agent.zip`）均存在且非空，任一缺失即令工作流失败；构建任务失败时不会产出只有源码的发布。Web 包由 `build-web` 作业调用 `python scripts/prepare-distribution.py --web-only --channel release` 生成；内嵌素材与随包法律文本的行尾由 `.gitattributes` 固定，各平台与 CI 重建出同样的 `web/EditHere.html` 与同样的两种 ZIP。发布前按下发给用户的字节复核两种 Web 包：`node scripts/web-package-check.cjs <ZIP 路径>`。
- 资产名不含版本号（文档与更新检查使用 `/releases/latest/download/<固定名>`），随包附 `SHA256SUMS.txt` 与逐文件 `.sha256`；Web 包同样按无版本名发布为 `EditHere-web.zip` 与 `EditHere-web-agent.zip`。
- 包下载到 release 任务后所在目录由 `upload-artifact` 决定，不要预测：它只保留第一个通配符之前那段固定前缀之后的路径，`dist/linux-*/<文件>` 因此落在 `linux-<版本>/` 下而不是 `dist/linux-<版本>/`，Web 包落在 `web/<版本>-<短哈希>/` 下。release 任务按文件名在任意深度把原生包与 Web 包搬到归档根目录，改收集方式时不要退回"按目录搬"（REG-157）。
- 发布说明优先取 `docs/releases/<版本>.md`（人工整理的面向用户说明）；该文件缺失时回落到 `CHANGELOG.md` 对应章节。整理说明时先写草稿交用户确认，再打标签。
- 发布说明同时维护中英两份：`docs/releases/<版本>.md` 与 `docs/releases/<版本>.en.md`，由 release.yml 拼成「顶部语言切换 + 中文正文 + `## English` 英文正文」（发布页是单字段，没有语言切换；英文用真正的标题而不是折叠块，否则指向它的锚点点不开）。只写中文时 `check-packaging.py` 的检查会失败。

## 文档语言

界面文案走 Qt 翻译（见[多语言与翻译](i18n.md)）；**仓库里的说明文档不走翻译，中英各存一份文件**，文件名加 `.en` 后缀：

- 面向使用者的七份文档各有英文版：`USER-GUIDE`、`LONG-CAPTURE`、`VIDEO-ANNOTATION`、`AGENT-CLI`、`AI-SETUP`、`LINUX`、`DEVELOPMENT`。两份文件各自在标题下放一行语言切换互相指向，`README.en.md` 也要链接到每一份英文页；这三件事都由 `check-packaging.py` 的 `translated_docs` 检查，新增中文文档时要在 `TRANSLATED_DOCS` 里登记。
- `CHANGELOG.md` 保持中文单语，它是历史的唯一来源——同一段历史维护两份必然漂移。对外可见的「本版变了什么」由发布页承担，即上一条的双语发布说明。
- `docs/releases/*.md` 只是发布页正文片段，没有标题也没有语言切换行：相对链接在发布页上会按发布地址解析，所以语言切换靠 release.yml 拼接完成，`translated_docs` 负责两侧都在。
- 英文文档里的 JSON 字段名、命令、路径与平台标识保持原样；源视频或截图里实际显示的中文（例如演示动画的字幕）也保持中文，不译。
- 只维护中文：`i18n.md`、`REGRESSIONS.md`、`PRERELEASE-0.9.9.md` 这类维护者记录，以及 `docs/releases/0.9.x.md` 等历史发布页。
- Windows 免安装包随包附带的仍只有中文版 `AGENT-CLI.md` 与 `VIDEO-ANNOTATION.md`；`package-windows.ps1` 会把它们里面的相对链接改写成仓库地址，语言切换行也在改写范围内。

## 构建与验证

Linux 预览版的系统依赖、构建、AppImage 打包与 X11/Wayland 行为见 [Linux 说明](LINUX.md)。

依赖：Qt **6.8.3**（qtbase、qtsvg、qtimageformats、qtmultimedia 动态库，外加 qttranslations 提供界面语言用到的 Qt 自带翻译）、CMake 3.24+、Ninja、C++20 编译器。Windows 使用 MinGW GCC 13.1.0；Mac CI 固定 macOS 15 + Xcode 16.4，以匹配 Qt 6.8.3；Xcode 26 SDK 已移除该版本 Qt 链接的 AGL framework。视频播放需随包部署 Multimedia 插件和对应解码依赖，不要求用户另行安装 Qt。

Windows PowerShell 7，CMake 在 PATH 中：

```powershell
./scripts/build-windows.ps1 -QtRoot C:/Qt/6.8.3/mingw_64 -Compiler C:/Qt/Tools/mingw1310_64/bin/g++.exe -Ninja C:/Tools/ninja.exe
./scripts/package-windows.ps1 -QtRoot C:/Qt/6.8.3/mingw_64 -CompilerBin C:/Qt/Tools/mingw1310_64/bin
```

macOS 安装盘使用 `dmgbuild==1.6.7`，与 Qt 下载工具一起装在项目内的 `.tools/python` 虚拟环境，构建时将其 `bin` 加入当前进程 PATH。`scripts/build-macos.sh` 打包后调用 `scripts/check-macos-package.py`，检查真实 DMG 的双入口、Finder 布局、应用资源和复制推出后的签名及 CLI。

构建脚本运行检测、核心数据、布局、界面、行内文本、画布交互、设置、多语言、引导、自启、启动流程、更新和 CLI 等测试；Windows 另含 Agent 启动测试，以当前 CTest 输出为准。导出样本及窗口渲染截图存于 `artifacts/native-ui/`。安装 `jsonschema==4.26.0` 后，可运行 `python scripts/validate-exports.py` 独立校验导出；macOS 构建（`.github/workflows/native-macos.yml`）会为同一批产物自动运行它，schema 与真实导出不符时构建即失败。改动界面文案后另跑 `python scripts/check-translations.py --qt <Qt 安装目录>`，详见 [多语言与翻译](i18n.md)。打包只写新目录；重复打包请传入新的 `-OutputDirectory`。

除 Qt 测试外，CTest 还注册两条与工具链解耦的检查：`packaging`（`python scripts/check-packaging.py`）校验安装器脚本编码、NSIS 宏与实编译、便携压缩包结构、发布资产命名与校验文件、版本号唯一来源、Qt 翻译部署、`.ts` 与预编译 `.qm` 的逐条对应，以及 AI 与命令行文档、skill 里的反馈结构与导出字段的一致性；`translations`（`scripts/check-translations.py`）在能找到 Qt Linguist 时才注册。这些检查与项目历史问题的对应关系见[问题归档与回归测试](REGRESSIONS.md)，**新增问题修复时要在该页登记并补一条会复发的检查**。

贡献者授权由 `.github/workflows/cla.yml` 核验，实现见 `scripts/cla-check.cjs`、配置见 `.github/cla/config.json`：工作流只检出可信基线、绝不执行 PR 中的代码，确认账号必须是贡献者本人，记录协议版本与 `cla/v2.md` 的 SHA-256 摘要到独立的 `cla-signatures` 分支。改动 CLA 相关文件后本地跑 `node --test tests/cla_check_test.cjs`；只读的 `.github/workflows/cla-tests.yml` 在 PR 中自动运行同一测试。协议内容与启用步骤见[贡献者许可协议](../CLA.md)。

操作图标的唯一几何源为 `assets/ui-icons/icons/*.svg`，语义与来源见 `manifest.json`，视觉规则见[图标视觉规范](icon-visual-spec.md)。Qt 将 SVG 嵌入资源，Web 构建内联同一图形；运行 `node tools/icon-review/build.cjs` 生成独立 HTML 方案页。替换共享 SVG 后重新构建两端，不在各端手绘另一份图标。原有品牌素材独立保留。
