#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise MI4 Windows session banks in the actual engine, using temporary saves.

The input game data and save are read-only. Temporary Lua callbacks drive the
engine's real save, metadata, enumeration and removal paths; nothing is sent
to the achievement server.
"""
import argparse
import gzip
import hashlib
import json
import pathlib
import re
import shutil
import struct
import tempfile

from test_mi4_save_banks import original_system_script, run_engine


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save_metadata(path):
    raw = path.read_bytes()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    offset = 12
    while raw[offset:offset + 4] != b"ESAV":
        size = struct.unpack_from(">I", raw, offset + 4)[0]
        if raw[offset:offset + 4] == b"SUBS":
            metadata = []
            end = offset + 8 + size
            offset += 8
            while offset < end:
                length = struct.unpack_from("<i", raw, offset)[0]
                offset += 4
                metadata.append(raw[offset:offset + length - 1].decode("utf-8"))
                offset += length
            return metadata
        offset += 8 + size
    raise ValueError("Input save has no native metadata")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=pathlib.Path, required=True)
    parser.add_argument("--game", type=pathlib.Path, required=True)
    parser.add_argument("--save", type=pathlib.Path, required=True)
    args = parser.parse_args()
    exe, game, seed = args.exe.resolve(), args.game.resolve(), args.save.resolve()
    seed_hash = digest(seed)
    original = original_system_script(game)
    # Preserve the native CD/set metadata so startup menus can inspect saves.
    metadata_tail = "".join(f",[{i}]=" + json.dumps(value) for i, value in enumerate(save_metadata(seed)) if i)
    with tempfile.TemporaryDirectory(prefix="ce-mi4-windows-banks-") as directory:
        root = pathlib.Path(directory)
        saves = root / "saves"
        saves.mkdir()
        shutil.copy2(seed, saves / "efmi001.gsv")
        legacy_hash = digest(saves / "efmi001.gsv")
        runs = ("windows-bank-A", "windows-bank-B")
        records = [{"target": "monkey4-windows-test", "client_playthrough_id": run,
                    "title": run, "status": "active", "sync_eligible": False} for run in runs]
        (saves / "ce-playthroughs.json").write_text(json.dumps({"playthroughs": records}))
        counter = 0

        def config_for(session, body, boot_game=False, save_directory=saves):
            nonlocal counter
            counter += 1
            extra = root / f"run-{counter}"
            extra.mkdir()
            (extra / "_ce_original_system.lua").write_bytes(original)
            # The native BOOT can wait for title-menu input when saves exist.
            # Only writers need a fresh scene; reads/erases use the engine APIs
            # directly, and loads restore the scene from their saved state.
            boot = "ce_original_boot(a,b)" if boot_game else '''system.buttonHandler=function() end
system.charHandler=function() end'''
            (extra / "_system.lua").write_text('''dofile("_ce_original_system.lua")
ce_original_boot=BOOT
BOOT=function(a,b)
 start_script(function()
  sleep_for(4000)
''' + body + '''
  print("CE_TEST_DONE")
  exit(0)
 end)
 ''' + boot + '''
end
''')
            cfg = extra / "test.ini"
            cfg.write_text(f"""[scummvm]
enable_unsupported_game_warning=false
confirm_exit=false
gui_theme=RESIDUALVM
fullscreen=false
joystick_num=-1
savepath={save_directory}
extrapath={extra}
ce_active_client_playthrough_id={session}
ce_active_playthrough_target=monkey4-windows-test
ce_active_playthrough_status=active
ce_active_playthrough_casual=false
ce_active_playthrough_sync_eligible=false

[monkey4-windows-test]
engineid=grim
gameid=monkey4
platform=windows
language=en
renderer=opengl
path={game}
""")
            return cfg

        def run(session, body, boot_game=False, save_directory=saves):
            cfg = config_for(session, body, boot_game, save_directory)
            return run_engine(exe, cfg, "monkey4-windows-test", expect_marker=True)

        owned = {}
        for session in runs:
            output = run(session, '''ce_bank_owner="''' + session + '''"
local f=FileFindFirst("Saves/*.gsv")
print("CE_INITIAL:"..tostring(f))
FileFindDispose()
local i=0
while i<160 do
 ce_bank_slot=i+1
 system.saveGameCallback=function() SubmitSaveGameData({[0]="''' + session + '''-"..format("%03d",ce_bank_slot)''' + metadata_tail + '''}) end
 ScreenshotForSavegame()
 Save(format("Saves/efmi%03d.gsv",i+1))
 sleep_for(150)
 i=i+1
end
print("CE_RESTORED:"..ce_bank_owner)''', boot_game=True)
            assert "CE_INITIAL:nil" in output, output[-5000:]
            assert digest(saves / "efmi001.gsv") == legacy_hash, "Legacy save overwritten"
            for name, checksum in owned.items():
                assert digest(saves / name) == checksum, "Another session's save overwritten"
            names = []
            for p in saves.glob("efmi-win-ce-*.gsv"):
                owner = json.loads((saves / (p.name + ".ce-achievements.json")).read_text())
                if owner["client_playthrough_id"] == session:
                    names.append(p.name)
            assert len(names) == 160, f"{session}: expected 160 saves, got {len(names)}"
            owned.update({name: digest(saves / name) for name in names})
            print(f"PASS: {session} saved 160 slots; prior saves preserved", flush=True)

        def read_body():
            return '''local f=FileFindFirst("Saves/*.gsv")
while f do
 print("CE_FILE:"..f)
 f=FileFindNext()
end
local i=0
while i<160 do
 local f=FileFindFirst(format("Saves/efmi%03d.gsv",i+1),"./")
 print("CE_EXACT:"..i..":"..tostring(f))
 FileFindDispose()
 local d=GetSaveGameData(format("Saves/efmi%03d.gsv",i+1))
 print("CE_DATA:"..i..":"..tostring(d[0]))
 i=i+1
end'''

        for session in runs:
            output = run(session, read_body())
            assert re.findall(r"CE_FILE:(efmi\d{3}\.gsv)", output) == [f"efmi{i+1:03}.gsv" for i in range(160)]
            for i in range(160):
                assert f"CE_DATA:{i}:{session}-{i+1:03}" in output
                assert f"CE_EXACT:{i}:efmi{i+1:03}.gsv" in output
            cfg = config_for(session, "")
            listing = run_engine(exe, cfg, "--list-saves")
            for bank in runs:
                assert re.findall(r"^\s*\d+\s+" + bank + r"-(\d{3})$", listing, re.M) == [f"{i+1:03}" for i in range(160)]
            print(f"PASS: {session} native menu sees only its bank; chooser keeps slot order", flush=True)

        for session in runs:
            output = run(session, 'Load("Saves/efmi160.gsv")\nsleep_for(1000)')
            assert f"CE_RESTORED:{session}" in output, output[-5000:]
        listing = run_engine(exe, config_for(runs[1], ""), "--list-saves")
        chooser_slot = re.search(r"^\s*(\d+)\s+windows-bank-A-160$", listing, re.M)[1]
        output = run_engine(exe, config_for(runs[1], ""), "--save-slot=" + chooser_slot,
                            "monkey4-windows-test", expect_marker=True)
        assert "CE_RESTORED:windows-bank-A" in output, output[-5000:]
        print("PASS: native loads resolve their bank; launcher loads its selected file", flush=True)

        # Owned legacy slots must mix correctly with newer banked filenames.
        first = next(p for p in saves.glob("efmi-win-ce-*.gsv") if p.name.endswith("-slot009.gsv") and
                     json.loads((saves / (p.name + ".ce-achievements.json")).read_text())["client_playthrough_id"] == runs[0])
        first.replace(saves / "efmi009.gsv")
        (saves / (first.name + ".ce-achievements.json")).replace(saves / "efmi009.gsv.ce-achievements.json")
        output = run(runs[0], read_body())
        assert "CE_DATA:8:windows-bank-A-009" in output
        listing = run_engine(exe, config_for(runs[0], ""), "--list-saves")
        assert re.findall(r"^\s*\d+\s+windows-bank-A-(\d{3})$", listing, re.M) == [f"{i+1:03}" for i in range(160)]
        print("PASS: owned legacy slot retains its native ordering", flush=True)

        # A lost sidecar must not allow its slot to be overwritten by this run.
        lost = next(p for p in saves.glob("efmi-win-ce-*.gsv") if p.name.endswith("-slot100.gsv") and
                    json.loads((saves / (p.name + ".ce-achievements.json")).read_text())["client_playthrough_id"] == runs[0])
        lost_hash = digest(lost)
        (saves / (lost.name + ".ce-achievements.json")).unlink()
        collision = root / "collision"
        collision.mkdir()
        shutil.copy2(lost, collision / lost.name)
        shutil.copy2(saves / "ce-playthroughs.json", collision / "ce-playthroughs.json")
        run(runs[0], '''system.saveGameCallback=function() SubmitSaveGameData({[0]="replacement-100"''' + metadata_tail + '''}) end
ScreenshotForSavegame()
Save("Saves/efmi100.gsv")
sleep_for(500)''', boot_game=True, save_directory=collision)
        assert digest(lost) == lost_hash
        assert digest(collision / lost.name) == lost_hash
        assert len(list(collision.glob("*copy1-slot100.gsv"))) == 1
        print("PASS: missing-sidecar collision preserves the old file", flush=True)

        other_names = [p for p in saves.glob("efmi-win-ce-*.gsv") if
                       (saves / (p.name + ".ce-achievements.json")).exists() and
                       json.loads((saves / (p.name + ".ce-achievements.json")).read_text())["client_playthrough_id"] == runs[1]]
        other_hashes = {p.name: digest(p) for p in other_names}
        output = run(runs[0], '''local result=remove("Saves/efmi160.gsv")
print("CE_REMOVE:"..tostring(result))
local f=FileFindFirst("Saves/*.gsv")
while f do
 print("CE_FILE:"..f)
 f=FileFindNext()
end''')
        assert "CE_FILE:efmi160.gsv" not in output
        for name, checksum in other_hashes.items():
            assert digest(saves / name) == checksum
        assert digest(saves / "efmi001.gsv") == legacy_hash
        print("PASS: native erase affects only the active session", flush=True)

    assert digest(seed) == seed_hash
    print("PASS: original input save unchanged", flush=True)


if __name__ == "__main__":
    main()
