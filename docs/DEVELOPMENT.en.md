# EditHere · Development notes

> This local variant is an annotation whiteboard. Screenshot, OCR, scrolling capture and pinning modules are removed. Annotation, layout, project and Agent formats are preserved. See [DESIGN.md](../DESIGN.md). Unit tests have a 60-second limit.


[简体中文](DEVELOPMENT.md) · **English**

[Back to the product overview](../README.en.md) · [User guide](USER-GUIDE.en.md) · [AI integration and CLI](AGENT-CLI.en.md) · [Changelog (Chinese)](../CHANGELOG.md)

Run all the commands below from the repository root; file paths are also relative to the repository root.

## Branches and technology stack

The current branch is `codex/annotation-whiteboard`, built with C++20 + Qt 6 Widgets. The retained `desktop` branch is Windows WPF 0.2 (`cf9c364`), and the `web` branch is Web 0.3 (`66548c5`).

## Version management

Routine fixes and small improvements only increase the last patch number, for example `0.8.0 → 0.8.1 → 0.8.2`. The middle number increases only when a larger feature stage is completed in one go and the release is explicit; it is no longer incremented for every round of development. The first number is reserved for explicit major releases, and existing version numbers and historical packages stay unchanged. A rollback applies only to the current implementation and does not freeze later versions; withdrawn numbers are not reused.

The single source of truth for the product version is `project(... VERSION ...)` in `CMakeLists.txt`, currently **0.11.1**. The runtime version and the Mac app information use that value automatically; after a successful link, `build/version.txt` is generated (on Mac, `build-macos/version.txt`), and the packaging scripts for both platforms name packages from it and ship `version.txt` with the package. After changing the version you must rebuild, so that an old binary is not labelled as a new version. The product version and the JSON formats are managed independently: the current format for minimal image feedback is defined by `feedback-minimal.schema.json` (an `objects` object structure since `0.9.0`; the parallel `annotations` / `changes` arrays of `0.8.21` are in `feedback-v0.7.schema.json` and can still be imported); `feedback-v1` / `v1.1` / `v2` describe the early **project document** formats, while a full image project uses `project-v3.schema.json`. Video feedback and video projects use `video-feedback-v1.schema.json` (`video-feedback-1`) and `video-project-v1.schema.json` (`video-project-1`) respectively. The file name represents the data format, not the app version.

## Release process

Pushing a `vX.Y.Z` tag triggers `.github/workflows/release.yml`: the Windows, macOS and Linux build jobs each produce a package, the `build-web` job packages the two web ZIPs, and the release job aggregates them and creates a GitHub Release. Updating the source version or building a local package does not mean the public latest release has been updated; GitHub's `/releases/latest` is decided by its stable Release metadata. In-app updates only check stable releases from `yyyzl/EditHere` on GitHub. Packages and SHA-256 files come from the same release; upstream product mirrors are not used.

- **A release must ship complete packages.** Before publishing, the release job checks that `EditHere-win-x64-setup.exe`, `EditHere-win-x64.zip`, `EditHere-macos-universal.dmg`, `EditHere-linux-x86_64.AppImage` and both web ZIPs (`EditHere-web.zip`, `EditHere-web-agent.zip`) all exist and are non-empty; if any is missing, the workflow fails. A failed build job never produces a source-only release. The web ZIPs come from the `build-web` job running `python scripts/prepare-distribution.py --web-only --channel release`; the line endings of embedded assets and of the shipped legal texts are pinned in `.gitattributes`, so every platform and CI rebuild the same `web/EditHere.html` and the same two ZIPs. Before publishing, check both web ZIPs against the bytes that will be handed to users: `node scripts/web-package-check.cjs <ZIP path>`.
- Asset names contain no version number (the docs and the update check use `/releases/latest/download/<stable name>`), and each package ships with `SHA256SUMS.txt` and a per-file `.sha256`; the web ZIPs follow the same rule as `EditHere-web.zip` and `EditHere-web-agent.zip`.
- Where the packages land after the release job downloads them is decided by `upload-artifact`, so do not predict it: only the path after the fixed prefix preceding the first wildcard survives, which puts `dist/linux-*/<file>` under `linux-<version>/` rather than `dist/linux-<version>/`, and the web ZIPs under `web/<version>-<short-hash>/`. The release job moves the native and web packages to the archive root by name, at any depth; do not go back to moving them by directory (REG-157).
- Release notes are taken first from `docs/releases/<version>.md` (a hand-written, user-facing description); when that file is missing, it falls back to the matching section of `CHANGELOG.md`. When preparing the notes, write a draft for the user to confirm before tagging.
- The notes are maintained in both languages, `docs/releases/<version>.md` and `docs/releases/<version>.en.md`, and release.yml assembles them into one body: a language switcher at the top, the Chinese notes, then the English notes under `## English`. A release page is a single field with no language switch, and the English half is a real heading rather than a collapsed block, because an anchor pointing into a closed block lands on nothing. `check-packaging.py` fails when only the Chinese page exists.

## Documentation languages

Interface text goes through Qt translations (see [Internationalisation and translations](i18n.md)); **the repository documentation does not**. It keeps one file per language, with `.en` on the English copy:

- Seven user-facing documents have an English version: `USER-GUIDE`, `LONG-CAPTURE`, `VIDEO-ANNOTATION`, `AGENT-CLI`, `AI-SETUP`, `LINUX` and `DEVELOPMENT`. Each pair carries a language switcher under its title pointing at the other half, and `README.en.md` links every English page. All three rules are checked by `check-packaging.py`'s `translated_docs`, so a new page has to be registered in `TRANSLATED_DOCS`.
- `CHANGELOG.md` stays Chinese-only: it is the single source of history, and keeping one history twice guarantees drift. What users see for a given version is the release page instead — the bilingual release notes above.
- `docs/releases/*.md` are release-page body fragments with no title and no switcher line: a relative link inside them resolves against the release URL, so the two languages are joined by release.yml, and `translated_docs` only guarantees that both halves exist.
- In the English pages, JSON field names, commands, file paths and platform identifiers stay as they are. Chinese text that is genuinely on screen — the subtitles in the demo animation, for instance — stays Chinese too and is not translated.
- Chinese only: `i18n.md`, `REGRESSIONS.md`, `PRERELEASE-0.9.9.md` and other maintainer records, plus historical release pages such as `docs/releases/0.9.x.md`.
- The Windows portable package still ships only the Chinese `AGENT-CLI.md` and `VIDEO-ANNOTATION.md`; `package-windows.ps1` rewrites their relative links to repository URLs, the switcher line included.
## Build and verification

For the Linux preview's system dependencies, build, AppImage packaging and X11/Wayland behaviour, see the [Linux guide](LINUX.en.md).

Dependencies: Qt **6.8.3** (the qtbase, qtsvg, qtimageformats and qtmultimedia shared libraries, plus qttranslations for the Qt-supplied translations the interface languages use), CMake 3.24+, Ninja and a C++20 compiler. Windows uses MinGW GCC 13.1.0; Mac CI is pinned to macOS 15 + Xcode 16.4 to match Qt 6.8.3; the Xcode 26 SDK removed the AGL framework that this Qt version links against. Video playback needs the Multimedia plugin and the matching decoder dependencies deployed with the package; users are not asked to install Qt separately.

Windows PowerShell 7, with CMake on PATH:

```powershell
./scripts/build-windows.ps1 -QtRoot C:/Qt/6.8.3/mingw_64 -Compiler C:/Qt/Tools/mingw1310_64/bin/g++.exe -Ninja C:/Tools/ninja.exe
./scripts/package-windows.ps1 -QtRoot C:/Qt/6.8.3/mingw_64 -CompilerBin C:/Qt/Tools/mingw1310_64/bin
```

The build script runs tests covering detection, core data, layout, interface, inline text, canvas interaction, settings, localisation, the guide, autostart, the startup flow, updates and the CLI; Windows additionally includes Agent launch tests, and the current CTest output is authoritative. Export samples and window rendering screenshots are stored in `artifacts/native-ui/`. After installing `jsonschema==4.26.0`, run `python scripts/validate-exports.py` to validate exports independently; the macOS build (`.github/workflows/native-macos.yml`) runs it automatically on the same artifacts, and the build fails when a schema does not match a real export. After changing interface text, also run `python scripts/check-translations.py --qt <Qt installation directory>`; see [internationalisation and translation (Chinese)](i18n.md) for details. Packaging only writes to a new directory; to package again, pass a new `-OutputDirectory`.

Besides the Qt tests, CTest registers two toolchain-independent checks: `packaging` (`python scripts/check-packaging.py`) verifies the installer script encoding, the NSIS macros and an actual compile, the portable archive structure, the release asset names and checksum files, the single version source, the Qt translation deployment, the one-to-one correspondence between `.ts` files and the precompiled `.qm` files, and the consistency of the feedback structure and export fields in the AI and command-line documentation and in the skill; `translations` (`scripts/check-translations.py`) is registered only when Qt Linguist can be found. For how these checks map to the project's past issues, see the [issue archive and regression tests (Chinese)](REGRESSIONS.md); **when you fix a new problem, record it on that page and add a check that will catch a recurrence**.

Contributor consent is verified by `.github/workflows/cla.yml`, implemented in `scripts/cla-check.cjs` with configuration in `.github/cla/config.json`: the workflow checks out only the trusted base and never runs code from the pull request, requires the confirming account to be the contributor's own, and records the agreement version and the SHA-256 digest of `cla/v2.md` on a separate `cla-signatures` branch. After changing any CLA-related file, run `node --test tests/cla_check_test.cjs` locally; the read-only `.github/workflows/cla-tests.yml` runs the same test on pull requests. See the [contributor licence agreement (Chinese)](../CLA.md) for the agreement and the enablement steps.

Action icon geometry lives only in `assets/ui-icons/icons/*.svg`. The manifest records semantics and provenance; [the visual specification](icon-visual-spec.md) defines the shared style. Qt embeds the SVGs and the Web build inlines the same geometry. Run `node tools/icon-review/build.cjs` to regenerate the standalone review HTML. Rebuild both clients after changing a shared SVG; retain the existing brand assets separately.
