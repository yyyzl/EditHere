#!/usr/bin/env python3
"""挂载真实 DMG，检查安装入口，并在推出磁盘后验证单独复制的应用。"""
import argparse
import json
import plistlib
import subprocess
import sys
import tempfile
from pathlib import Path


def check_bundle(application: Path, version: str) -> None:
    contents = application / "Contents"
    with (contents / "Info.plist").open("rb") as file:
        info = plistlib.load(file)
    if info.get("CFBundleExecutable") != "EditHere" or info.get("CFBundleShortVersionString") != version:
        raise ValueError("The app bundle executable or version is incorrect.")
    resources = contents / "Resources"
    required = (
        "version.txt", "AGENT-CLI.md", "AGENT-CLI.en.md", "使用说明.txt", "LICENSE", "NOTICE",
        "LICENSING.md", "COMMERCIAL-LICENSE.md", "TRADEMARK-POLICY.md", "THIRD-PARTY-NOTICES.md",
        "skills/edithere/SKILL.md", "schema/feedback-minimal.schema.json", "schema/project-v3.schema.json",
        "licenses/qtbase/LGPL-3.0-only.txt", "licenses/qtmultimedia/LGPL-3.0-only.txt",
        "licenses/ffmpeg/COPYING.LGPLv2.1", "licenses/ui-icons/LICENSE", "licenses/dmgbuild/LICENSE",
    )
    for name in required:
        file = resources / name
        if not file.is_file() or not file.stat().st_size:
            raise ValueError(f"Missing bundled resource: {name}")
    if (resources / "version.txt").read_text(encoding="utf-8").strip() != version:
        raise ValueError("The bundled version stamp does not match the app.")
    for name in ("EditHere", "edithere-cli"):
        if not (contents / "MacOS" / name).is_file():
            raise ValueError(f"Missing bundled executable: {name}")


def check_volume(volume: Path, version: str) -> None:
    from ds_store import DSStore

    visible = sorted(file.name for file in volume.iterdir() if not file.name.startswith("."))
    if visible != ["Applications", "EditHere.app"]:
        raise ValueError(f"Unexpected visible DMG contents: {visible}")
    link = volume / "Applications"
    if not link.is_symlink() or link.readlink() != Path("/Applications"):
        raise ValueError("The Applications installation shortcut is missing or incorrect.")
    check_bundle(volume / "EditHere.app", version)
    with DSStore.open(str(volume / ".DS_Store"), "r") as store:
        if store["."]["icvl"] != (b"type", b"icnv"):
            raise ValueError("The installer must open in icon view.")
        app_position = store["EditHere.app"]["Iloc"]
        target_position = store["Applications"]["Iloc"]
        if app_position[0] >= target_position[0] or app_position[1] != target_position[1]:
            raise ValueError("The application and destination must appear from left to right.")


def run(*command: str) -> str:
    return subprocess.check_output(command, text=True, timeout=60).strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("Verifying a mounted DMG requires macOS.")
    with tempfile.TemporaryDirectory(prefix="edithere-dmg-check-") as directory:
        root = Path(directory)
        volume = root / "volume"
        volume.mkdir()
        installed = root / "installed/EditHere.app"
        installed.parent.mkdir()
        run("hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", str(volume), str(args.image.resolve()))
        try:
            check_volume(volume, args.version)
            run("ditto", str(volume / "EditHere.app"), str(installed))
        finally:
            try:
                run("hdiutil", "detach", str(volume))
            except subprocess.CalledProcessError:
                run("hdiutil", "detach", "-force", str(volume))
        # 推出 DMG 后再运行，确认拖入应用程序时带走了完整依赖。
        run("codesign", "--verify", "--deep", "--strict", str(installed))
        cli_version = run(str(installed / "Contents/MacOS/edithere-cli"), "--version")
        if cli_version != f"EditHere {args.version}":
            raise ValueError(f"The copied app has an unexpected CLI version: {cli_version}")
    print(json.dumps({"macOSPackage": "passed", "visibleEntries": ["EditHere.app", "Applications"],
                      "finderIconView": True, "copiedAppWorksAfterEject": True, "version": args.version}))


if __name__ == "__main__":
    main()
