# EditHere

**An annotation whiteboard for visual feedback to AI.**

[简体中文](README.md)

This local variant is based on EditHere 0.11.1. Launching opens an empty workspace: paste, drop, or open your own image. System screenshots, OCR, scrolling capture, pinned images, and system-wide shortcuts have been removed.

## Workflow

On macOS, open the DMG and drag EditHere on the left to Applications on the right. Launch it from Applications, then eject the disk image. Quit the old version completely before replacing it during an upgrade.

1. Launch and paste with **Ctrl / ⌘ + V**, drop an image, or choose Import.
2. Add smart, point, rectangle, or global annotations.
3. Use **Explode** to move and resize components, recording original and destination coordinates.
4. Save an `.edithere` project, export JSON or an annotated image, or submit feedback to AI.

Launching again or clicking the tray restores the current workspace and preserves edits. New whiteboard prompts before replacing unsaved work.

## Features

Image import, clipboard and drag-and-drop; annotations; layout adjustment; undo/redo; zoom/pan; project persistence; JSON and annotated-image export; CLI, MCP, and skills; light/dark themes and Chinese/English UI. Existing video annotation and project loading remain available.

The application does not call a model. AI can open an input and wait for explicit submission:

```text
edithere-cli annotate <absolute-image-path> --output <new-feedback-json> --timeout 1800
```

Feedback retains `annotationSpace / objects / source / movements / annotations`. Cancel and timeout never submit unfinished edits. CLI and MCP screenshot tools are removed.

## Build and documentation

C++20 + Qt 6 Widgets for Windows, macOS, and Linux. Download the whiteboard from [this repository Releases](https://github.com/yyyzl/EditHere/releases). GitHub Actions builds Windows, macOS, Linux and Web from the same source.

- [User guide](docs/USER-GUIDE.en.md)
- [Development](docs/DEVELOPMENT.en.md)
- [Design](DESIGN.md)
- [Agent CLI](docs/AGENT-CLI.en.md)
- [Video annotations](docs/VIDEO-ANNOTATION.en.md)
- [Linux](docs/LINUX.en.md)
- [Web edition](web/README.en.md)
- [Distribution](docs/DISTRIBUTION.en.md)
- [Agent setup](docs/AI-SETUP.en.md)

Imported media is processed locally. Sending feedback to AI uses the chosen AI tool's data policy. Licensing remains [PolyForm Noncommercial 1.0.0](LICENSE); see [LICENSING.md](LICENSING.md), [NOTICE](NOTICE), and [TRADEMARK-POLICY.md](TRADEMARK-POLICY.md).
