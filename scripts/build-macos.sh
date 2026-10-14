#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
: "${QT_ROOT:?Set QT_ROOT to the Qt 6.8 macos SDK directory}"
build_path="$project_root/build-macos"
# Resolve the same SDK for preflight and CMake; a cached/default SDK must not
# silently override the Qt-compatible Xcode selected by CI or a local build.
sdk_path="$(xcrun --sdk macosx --show-sdk-path)"
if [[ ! -d "$sdk_path/System/Library/Frameworks/AGL.framework" ]]; then
  printf '%s\n' "Qt 6.8.3 requires AGL.framework, which is absent from the selected SDK: $sdk_path" \
    "Select Xcode 16.4 with DEVELOPER_DIR before building; do not use the Xcode 26 SDK for this Qt release." >&2
  exit 1
fi
mkdir -p "$build_path"
{
  xcodebuild -version
  printf 'SDK: %s\nQt: %s\nTMPDIR: %s\n' "$sdk_path" "$QT_ROOT" "${TMPDIR:-/tmp}"
  "$QT_ROOT/bin/qmake" -query QT_VERSION
  cmake --version
} | tee "$build_path/build-environment.txt"
cmake -S "$project_root" -B "$build_path" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT" -DCMAKE_OSX_SYSROOT="$sdk_path" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build "$build_path" --parallel
export H2D_TEST_ARTIFACTS="$project_root/artifacts/native-ui"
ctest --test-dir "$build_path" --output-on-failure --output-junit "$build_path/test-results.xml"
if [[ ! -f "$project_root/schema/feedback-minimal.schema.json" ]]; then printf '%s\n' "The current feedback schema is missing." >&2; exit 1; fi
if [[ ! -f "$project_root/schema/project-v3.schema.json" ]]; then printf '%s\n' "The project schema is missing." >&2; exit 1; fi
build_version="$(cat "$build_path/version.txt")"
if [[ ! "$build_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then printf '%s\n' "Invalid build version." >&2; exit 1; fi
output="$project_root/dist/EditHere-$build_version-macos-universal"
if [[ -e "$output" || -e "$output.dmg" ]]; then printf '%s\n' "Output folder or DMG already exists: $output" >&2; exit 1; fi
mkdir -p "$output"
cp -R "$build_path/EditHere.app" "$output/"
application="$output/EditHere.app"
resources="$application/Contents/Resources"
mkdir -p "$resources"
cp "$build_path/version.txt" "$resources/"
cp "$build_path/edithere-cli" "$application/Contents/MacOS/edithere-cli"
"$QT_ROOT/bin/macdeployqt" "$application" -always-overwrite -executable="$application/Contents/MacOS/edithere-cli"
cp -R "$project_root/skills" "$project_root/schema" "$project_root/packaging/licenses" "$resources/"
mkdir -p "$resources/licenses/ui-icons" "$resources/licenses/dmgbuild"
cp "$project_root/assets/ui-icons/LICENSE" "$resources/licenses/ui-icons/"
cp "$project_root/packaging/dmgbuild-LICENSE.txt" "$resources/licenses/dmgbuild/LICENSE"
for guide in AGENT-CLI.md AGENT-CLI.en.md; do
  sed -e 's|../skills/|skills/|g' -e 's|../schema/|schema/|g' \
    -e 's|../README.en.md|https://github.com/yyyzl/EditHere/blob/codex/annotation-whiteboard/README.en.md|g' \
    -e 's|../README.md|https://github.com/yyyzl/EditHere|g' \
    -e 's|USER-GUIDE.en.md|https://github.com/yyyzl/EditHere/blob/codex/annotation-whiteboard/docs/USER-GUIDE.en.md|g' \
    -e 's|USER-GUIDE.md|https://github.com/yyyzl/EditHere/blob/codex/annotation-whiteboard/docs/USER-GUIDE.md|g' \
    "$project_root/docs/$guide" > "$resources/$guide"
done
cp "$project_root/packaging/使用说明.txt" "$project_root/packaging/THIRD-PARTY-NOTICES.md" "$resources/"
cp "$project_root/LICENSE" "$project_root/COMMERCIAL-LICENSE.md" "$project_root/NOTICE" "$project_root/TRADEMARK-POLICY.md" "$resources/"
sed 's|packaging/THIRD-PARTY-NOTICES.md|THIRD-PARTY-NOTICES.md|g' "$project_root/LICENSING.md" > "$resources/LICENSING.md"
# Local test signing only. Distribution signing/notarization needs the product owner's Apple identity.
codesign --force --deep --sign - "$application"
python3 -m dmgbuild -s "$project_root/packaging/macos-dmg.py" -D "app=$application" EditHere "$output.dmg"
python3 "$project_root/scripts/check-macos-package.py" "$output.dmg" --version "$build_version"
printf '%s\n' "Local test build: $output.dmg"
