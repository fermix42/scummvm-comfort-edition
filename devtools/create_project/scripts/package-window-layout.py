#!/usr/bin/env python3
"""Package and check this private Windows x64 test build on a Windows runner."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("root", "build", "vcpkg", "vs", "output"):
        parser.add_argument("--" + key, required=True, type=Path)
    parser.add_argument("--sha", required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    sha = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if sha != args.sha:
        raise RuntimeError("Checkout does not match requested build SHA")
    dest = args.output.resolve() / "scummvm-window-layout-windows-x64"
    dest.mkdir(parents=True, exist_ok=False)

    def copy(source, target=None):
        source = Path(source)
        target = dest / (target or source.name)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    copy(args.build / "scummvm.exe")
    binary = (dest / "scummvm.exe").read_bytes()
    pe_offset = struct.unpack_from("<I", binary, 0x3C)[0]
    if binary[pe_offset:pe_offset + 4] != b"PE\0\0" or struct.unpack_from("<H", binary, pe_offset + 4)[0] != 0x8664:
        raise RuntimeError("Expected an AMD64 Windows PE executable")
    dlls = list((args.vcpkg / "bin").glob("*.dll"))
    if not dlls:
        raise RuntimeError("No Release dependency DLLs found")
    for path in dlls:
        copy(path)
    crt_dirs = sorted((args.vs / "VC/Redist/MSVC").glob("*/x64/Microsoft.VC*.CRT"))
    if not crt_dirs:
        raise RuntimeError("No redistributable x64 Microsoft CRT found")
    for path in crt_dirs[-1].glob("*.dll"):
        copy(path)
    for name in ("AUTHORS", "COPYING", "COPYRIGHT", "README.md", "NEWS.md"):
        copy(root / name)
    shutil.copytree(root / "LICENSES", dest / "LICENSES")
    for copyright_file in (args.vcpkg / "share").glob("*/copyright"):
        copy(copyright_file, Path("LICENSES/dependencies") / (copyright_file.parent.name + ".txt"))
    # Include every distributed engine data file, including the optional large
    # assets, so the portable test does not depend on another installation.
    for makefile in (root / "dists/engine-data").glob("engine_data*.mk"):
        for name in re.findall(r"dists/engine-data/[^\s\\]+", makefile.read_text()):
            copy(root / name)
    for name in ("scummmodern.zip", "scummclassic.zip", "scummremastered.zip", "residualvm.zip",
                 "gui-icons.dat", "shaders.dat", "translations.dat"):
        copy(root / "gui/themes" / name)
    copy(root / "dists/networking/wwwroot.zip")
    for path in (root / "backends/vkeybd/packs").glob("*.zip"):
        copy(path)
    copy(root / "doc/docportal/advanced_topics/configuration_file.rst", "window-layout-reference.rst")
    (dest / "window-layout-test.ini").write_text(
        "[scummvm]\nfullscreen=false\nstretch_mode=fit\naspect_ratio=true\n"
        "window_layout=1\nwindow_layout_display=0\nwindow_layout_borderless=true\n", encoding="utf-8")
    (dest / "Start window layout test.cmd").write_text(
        '@echo off\ncd /d "%~dp0"\nstart "ScummVM window layout test" "%~dp0scummvm.exe" --config="%~dp0window-layout-test.ini"\n',
        encoding="utf-8")
    (dest / "WINDOW-LAYOUT-TEST.txt").write_text(
        "Private Windows x64 Release test build\nSource commit: " + sha + "\n"
        "Source: https://github.com/fermix42/scummvm-private/tree/" + sha + "\n\n"
        "Extract the entire ZIP to a writable folder. Run Start window layout test.cmd.\n"
        "This uses the included separate configuration, leaving your normal ScummVM config alone.\n"
        "Global Options > Backend: choose your monitor and Left two-thirds, Right two-thirds, or Custom.\n"
        "Borderless window checked = borderless; unchecked = title bar and borders.\n"
        "Keep fullscreen OFF, Fit to window selected, and aspect correction enabled.\n"
        "The preset leaves the right third for your browser and uses the OS work area.\n"
        "Custom size includes the frame; height 0 fills the usable height.\n"
        "Game data is not included. Add your own supported games.\n\n"
        "The build runs CLI smoke checks on Windows. Native GUI/gameplay, taskbar and DPI\n"
        "behavior on your PC still require testing. See window-layout-reference.rst.\n"
        "No installer, auto-update, release publication, or code-signing is performed.\n"
        "All bundled libraries retain their licenses under LICENSES.\n", encoding="utf-8")

    # Audit import closure: every non-system dependency must ship beside the EXE.
    dumpbins = sorted((args.vs / "VC/Tools/MSVC").glob("*/bin/Hostx64/x64/dumpbin.exe"))
    if not dumpbins:
        raise RuntimeError("dumpbin not found")
    shipped = {p.name.lower() for p in dest.glob("*.dll")}
    system = Path(os.environ["SystemRoot"]) / "System32"
    for path in [dest / "scummvm.exe", *dest.glob("*.dll")]:
        output = subprocess.check_output([str(dumpbins[-1]), "/DEPENDENTS", str(path)], text=True)
        for dependency in re.findall(r"^\s+([\w.-]+\.dll)\s*$", output, re.MULTILINE | re.IGNORECASE):
            name = dependency.lower()
            if name not in shipped and not name.startswith(("api-ms-", "ext-ms-")) and not (system / dependency).is_file():
                raise RuntimeError(f"Missing dependency: {path.name} -> {dependency}")
    for option, filename in (("--version", "VERSION.txt"), ("--list-engines", "ENGINES.txt")):
        result = subprocess.run([str(dest / "scummvm.exe"), "--config=" + str(dest / "window-layout-test.ini"), option],
                                cwd=dest, text=True, capture_output=True, timeout=30, check=True)
        (dest / filename).write_text(result.stdout + result.stderr, encoding="utf-8")
        if not result.stdout.strip():
            raise RuntimeError("CLI smoke test produced no output")
    manifest = {"source_sha": sha, "architecture": "x64", "configuration": "Release",
                "windows_cli_checked": True, "windows_gui_checked": False,
                "files": {str(p.relative_to(dest)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(dest.rglob("*")) if p.is_file()}}
    (dest / "BUILD-MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Verified Windows x64 package:", sha, len(manifest["files"]), "files")


if __name__ == "__main__":
    main()
