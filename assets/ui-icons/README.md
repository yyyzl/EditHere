# 白板共享操作图标

桌面与 Web 共用 `icons/` 中的 SVG，唯一清单为 `manifest.json`。当前保留图片输入、批注、布局、媒体、视图与保存导出所需图标，截图、OCR、贴图、长截图及其候选图形已删除。

`crop` 用于白板中手动框出布局组件；它不执行屏幕截图。`candidateGroups` 仅保留组件拆分的方案。运行 `node tools/icon-review/build.cjs` 生成图标评审页，运行 `node web/build.cjs` 同步 Web 内联资源。

图形来源、许可、SHA-256 与已退役 ID 记录在清单中。Qt 将 SVG 嵌入资源，不依赖外部服务。品牌素材由 `assets/brand/` 独立管理。
