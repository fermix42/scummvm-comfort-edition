#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the actual MI4 PS2 engine in isolated save directories.

Provide a built CE executable, your PS2 game directory, and a compatible
existing PS2 save. No game data or original saves are modified. The test
supplies temporary Lua metadata callbacks, not a replacement game script
for normal play. No server credentials or synchronization are used.
"""

import argparse
import gzip
import hashlib
import json
import pathlib
import re
import shutil
import struct
import subprocess
import tempfile
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def original_system_script(game):
    local = next(p for p in game.iterdir() if p.name.lower() == "local.m4b")
    data = local.read_bytes()
    assert data[:4] == b"LABN", "Expected the PS2 LOCAL.M4B archive"
    count, size, offset = struct.unpack_from("<III", data, 8)
    # Same M4B filename-table bias and obfuscation used in grim/lab.cpp.
    names = bytes(b ^ 0x96 if b else 0 for b in data[offset - 0x13D0F:offset - 0x13D0F + size])
    for index in range(count):
        name, start, length, _ = struct.unpack_from("<IIII", data, 20 + index * 16)
        if names[name:].split(b"\0")[0].lower() == b"_system.lua":
            return data[start:start + length]
    raise RuntimeError("PS2 system script not found")


def run_engine(executable, config, *options):
    startup = None
    if hasattr(subprocess, "STARTUPINFO"):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    stdout_path, stderr_path = config.parent / "stdout.log", config.parent / "stderr.log"
    needs_marker = options == ("monkey4-ps2",)
    with stdout_path.open("w") as stdout, stderr_path.open("w") as stderr:
        process = subprocess.Popen(
            [str(executable), f"--config={config}", "--debuglevel=2", *options],
            stdout=stdout, stderr=stderr, startupinfo=startup,
        )
        deadline = time.monotonic() + 60
        completed = False
        try:
            while time.monotonic() < deadline:
                output = stdout_path.read_text(errors="replace")
                if needs_marker and "CE_TEST_DONE" in output:
                    completed = True
                    break
                if process.poll() is not None:
                    completed = process.returncode == 0 and not needs_marker
                    break
                time.sleep(0.1)
        finally:
            # All test writes are finalized before the marker. Some SDL/audio
            # runtimes stall during exit; never leave a test game running.
            if process.poll() is None:
                process.kill()
            process.wait()
    output = stdout_path.read_text(errors="replace")
    assert completed, output[-12000:] + stderr_path.read_text(errors="replace")[-12000:]
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=pathlib.Path, required=True)
    parser.add_argument("--game", type=pathlib.Path, required=True)
    parser.add_argument("--save", type=pathlib.Path, required=True)
    args = parser.parse_args()
    executable, game, seed = args.exe.resolve(), args.game.resolve(), args.save.resolve()
    original_hash = digest(seed)
    system_script = original_system_script(game)

    with tempfile.TemporaryDirectory(prefix="ce-mi4-banks-test-") as directory:
        root = pathlib.Path(directory).resolve()
        assert root.parent == pathlib.Path(tempfile.gettempdir()).resolve()
        saves = root / "saves"
        saves.mkdir()
        for slot in range(4):
            shutil.copy2(seed, saves / f"efmi{slot:03}.ps2")
        legacy = {p.name: digest(p) for p in saves.glob("*.ps2")}
        runs = ("smoke-A", "smoke-B")
        records = [{"target": "monkey4-ps2", "client_playthrough_id": run,
                    "title": run, "status": "active", "sync_eligible": False}
                   for run in runs]
        (saves / "ce-playthroughs.json").write_text(json.dumps({"playthroughs": records}))

        def config_for(run, body, save_directory=saves):
            extra = root / run
            extra.mkdir(exist_ok=True)
            (extra / "_ce_original_system.lua").write_bytes(system_script)
            patch = executable.parent / "extras" / "monkey4-patch.m4b"
            if patch.exists():
                shutil.copy2(patch, extra / patch.name)
            (extra / "_system.lua").write_text(
                'dofile("_ce_original_system.lua")\nce_original_boot=BOOT\n'
                'BOOT=function(a,b)\n ce_original_boot(a,b)\n start_script(function()\n'
                ' sleep_for(4000)\n' + body + '\n print("CE_TEST_DONE")\n exit(0)\n end)\nend\n')
            config = extra / "test.ini"
            config.write_text(f"""[scummvm]
enable_unsupported_game_warning=false
fullscreen=false
confirm_exit=false
joystick_num=-1
savepath={save_directory}
extrapath={extra}
ce_active_client_playthrough_id={run}
ce_active_playthrough_target=monkey4-ps2
ce_active_playthrough_status=active
ce_active_playthrough_casual=false
ce_active_playthrough_sync_eligible=false

[monkey4-ps2]
engineid=grim
gameid=monkey4
platform=playstation2
language=en
renderer=opengl
check_gamedata=false
path={game}
""")
            return config

        owned = {}
        writer_bodies = {}
        for run in runs:
            body = f'''local i
i=0
while i<4 do
 system.saveGameCallback=function() SubmitSaveGameData({{[0]="{run}"}}) end
 ScreenshotForSavegame()
 Save("efmi00"..i)
 sleep_for(1500)
 i=i+1
end'''
            writer_bodies[run] = body
            config = config_for(run, body)
            run_engine(executable, config, "monkey4-ps2")
            for name, checksum in legacy.items():
                assert digest(saves / name) == checksum, "Legacy save overwritten"
            for name, checksum in owned.items():
                assert digest(saves / name) == checksum, "Another session's save overwritten"
            files = []
            for path in saves.glob("efmi-ce-*.ps2"):
                sidecar = json.loads((saves / (path.name + ".ce-achievements.json")).read_text())
                if sidecar["client_playthrough_id"] == run:
                    files.append(path)
            assert len(files) == 4, f"{run} must have four independent slots"
            owned.update({p.name: digest(p) for p in files})
            print(f"PASS: {run} saved four slots; earlier files preserved")

        for run in runs:
            body = '''local i
local d
i=0
while i<4 do
 d=GetSaveGameData("efmi00"..i)
 print("CE_BANK:"..i..":"..tostring(d[0]))
 i=i+1
end'''
            config = config_for(run, body)
            output = run_engine(executable, config, "monkey4-ps2")
            for slot in range(4):
                assert f"CE_BANK:{slot}:{run}" in output, output
            listing = run_engine(executable, config, "--list-saves")
            assert listing.count(" - smoke-A") == 4 and listing.count(" - smoke-B") == 4, listing
            for bank in runs:
                assert re.findall(r"Slot (\d) - " + bank, listing) == ["1", "2", "3", "4"], listing
            print(f"PASS: {run} reads its own slots; chooser lists both banks")

        # Reproduce a bank with an owned legacy slot 1 and newer slots 2-4.
        # Only the temporary fixture is changed; the input save stays intact.
        mixed = next(saves / name for name in owned
                     if name.endswith("-slot1.ps2") and
                     json.loads((saves / (name + ".ce-achievements.json")).read_text())["client_playthrough_id"] == runs[0])
        mixed.replace(saves / "efmi000.ps2")
        (saves / (mixed.name + ".ce-achievements.json")).replace(saves / "efmi000.ps2.ce-achievements.json")
        listing = run_engine(executable, config, "--list-saves")
        for bank in runs:
            assert re.findall(r"Slot (\d) - " + bank, listing) == ["1", "2", "3", "4"], listing
        config = config_for(runs[0], body)
        output = run_engine(executable, config, "monkey4-ps2")
        for slot in range(4):
            assert f"CE_BANK:{slot}:{runs[0]}" in output, output
        print("PASS: mixed legacy/session bank ordered 1-4; native mapping preserved")

        # Gallery metadata uses the save's existing screenshot section.
        # Missing and unexpected image sizes must still permit listing.
        raw = gzip.decompress((saves / "efmi000.ps2").read_bytes())
        offset = 12
        while raw[offset:offset + 4] != b"SIMG":
            assert raw[offset:offset + 4] != b"ESAV", "Fixture has no screenshot"
            offset += 8 + struct.unpack_from(">I", raw, offset + 4)[0]
        length = struct.unpack_from(">I", raw, offset + 4)[0]
        assert length == 256 * 128 * 2
        for image in (b"", b"\0\0"):
            metadata_saves = root / f"image-{len(image)}"
            metadata_saves.mkdir()
            section = b"SIMG" + struct.pack(">I", len(image)) + image if image else b""
            modified = raw[:offset] + section + raw[offset + 8 + length:]
            (metadata_saves / "efmi000.ps2").write_bytes(gzip.compress(modified))
            config = config_for(runs[0], body, metadata_saves)
            listing = run_engine(executable, config, "--list-saves")
            assert "Slot 1 - smoke-A" in listing, listing
        print("PASS: missing/malformed screenshots retain save listing")

        # A filename that looks like this session's file is not proof of
        # ownership. Simulate a lost sidecar and verify that it survives.
        collision = root / "lost-sidecar"
        collision.mkdir()
        shadow_name = mixed.name
        shutil.copy2(saves / "efmi000.ps2", collision / shadow_name)
        checksum = digest(collision / shadow_name)
        config = config_for(runs[0], writer_bodies[runs[0]], collision)
        run_engine(executable, config, "monkey4-ps2")
        assert digest(collision / shadow_name) == checksum, "Untracked namespaced save overwritten"
        assert len(list(collision.glob("*.ps2"))) == 5, "Expected preserved file plus four owned slots"
        print("PASS: lost-sidecar save preserved")

    assert digest(seed) == original_hash, "Original fixture changed"
    print("PASS: original save unchanged")


if __name__ == "__main__":
    main()
