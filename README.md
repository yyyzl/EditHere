# EditHere · 改这里

**图片批注白板：把修改意见放回画面，再交给 AI。**

[English](README.en.md)

这是基于 EditHere 0.11.1 改造的本地精简版。打开应用即进入空白工作台，图片由你粘贴、拖入或选择。截图、OCR、长截图、置顶贴图及系统全局快捷键已经移除。

## 使用流程

1. 打开应用，按 **Ctrl / ⌘ + V** 粘贴图片、拖入图片，或点击「导入」选择图片。
2. 智能选块、点批注、框批注或全局批注，直接写下修改意见。
3. 点击「大爆炸」，移动或缩放组件，表达你希望的布局。
4. 保存 `.edithere` 项目继续编辑，或导出 JSON、带批注图片交给 AI。

再次打开应用或点击托盘会唤回当前白板，保留正在编辑的图片和批注。「新建批注白板」用于主动新建，已有修改会先询问保存。

## 保留的能力

- PNG / JPEG / WebP / BMP 图片导入、剪贴板粘贴、拖放。
- 智能选块、点／框／全局批注，撤销、重做、画布缩放与平移。
- 组件移动、缩放、精确坐标调整，记录原位置与目标位置。
- 项目保存、JSON 导出、复制 JSON、带批注图片导出。
- `edithere-cli`、MCP 连接器和配套 skill：AI 打开素材，等待你提交，再读取反馈。
- 浅色／深色主题、中英文界面、应用内快捷键和工具栏定制。
- 已有的视频批注与项目读取能力继续保留；系统截屏功能不再存在。

## AI 接入

EditHere 负责整理反馈，不内置模型调用。

```text
edithere-cli annotate <图片绝对路径> --output <新的反馈JSON路径> --timeout 1800
```

在白板中完成批注并点击「完成并返回 AI」。反馈保留 `annotationSpace / objects / source / movements / annotations` 格式，取消或超时不提交未完成意见。

MCP 连接器提供 `edithere_status`、`edithere_open`、`edithere_annotate_start`、`edithere_annotate_poll`、`edithere_annotate`、`edithere_export`。CLI 和 MCP 的截图命令已移除。

## 开发与文档

桌面端使用 C++20 + Qt 6 Widgets，支持 Windows、macOS、Linux；Web 版见 [web/README.md](web/README.md)。白板发行包由 [本仓库 Releases](https://github.com/yyyzl/EditHere/releases) 提供，Windows、macOS、Linux 和 Web 均通过 GitHub Actions 从同一源码构建。

- [使用指南](docs/USER-GUIDE.md)
- [构建与开发](docs/DEVELOPMENT.md)
- [白板改造设计](DESIGN.md)
- [AI 与命令行](docs/AGENT-CLI.md)
- [视频批注](docs/VIDEO-ANNOTATION.md)
- [平台与分发](docs/DISTRIBUTION.md)
- [更新记录](CHANGELOG.md)

本地截图和图片不再由系统采集；导入素材、分析、批注和导出在本机完成。将反馈交给 AI 时，数据处理由所接入的 AI 工具决定。

许可沿用 [PolyForm Noncommercial 1.0.0](LICENSE)。商业许可、第三方许可和品牌规则见 [LICENSING.md](LICENSING.md)、[NOTICE](NOTICE)、[TRADEMARK-POLICY.md](TRADEMARK-POLICY.md)。
