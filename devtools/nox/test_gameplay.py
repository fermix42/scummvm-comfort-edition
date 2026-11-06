#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise real new-game/movement/modal input through CE's event loop.

No coordinates, inventory, statistics or quest state are injected. The original
HDVs are read-only. This is bounded M1 gameplay evidence, not a campaign test or
durable gameplay-save acceptance.
"""
import argparse
import configparser
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import urllib.parse
if __package__:
    from .enhancement_oracle import read_save_enhancements
else:
    from enhancement_oracle import read_save_enhancements


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_observed_enhancements(saves):
    inspected, sessions = 0, set()
    for path in saves.glob("nox-ce-*.g[01]"):
        state = read_save_enhancements(path)
        assert state["description"] in ("Session A slot1", "Session B slot1")
        sessions.add(state["session"])
        cells = state["cells"]
        assert {(cell["x"], cell["y"]) for cell in cells} == {(24, 38), (25, 38)}, "Normal opening visits not recorded"
        assert len(cells) == 2 and all(cell["resource"] == "NA/DATA.MAP.KEEP" and cell["map_type"] == 8 and cell["map_id"] == 7 for cell in cells), "Discovery lacks the original loaded map identity"
        assert all(cell["observed"] and not cell["revealed"] and not cell["annotation"] for cell in cells), "Normal exploration became a reveal or synthetic annotation"
        inspected += 1
    assert inspected == 4 and len(sessions) == 2
    return inspected


def validate_saved_enhancements(saves):
    """Independent NXEH binary oracle for labeled controlled-owner fixtures."""
    inspected = 0
    for path in saves.glob("nox-ce-*.g[01]"):
        raw = path.read_bytes()
        length = struct.unpack_from("<I", raw, 12)[0]
        payload = raw[32:32 + length]
        metadata_size = struct.unpack_from("<I", payload, 4)[0]
        metadata = payload[8:8 + metadata_size]
        offset, texts = 0, []
        for _ in range(3):
            size = struct.unpack_from("<H", metadata, offset)[0]
            texts.append(metadata[offset + 2:offset + 2 + size].decode("utf-8"))
            offset += 2 + size
        assert texts[2] in ("Session A slot1", "Session B slot1"), "Unlabeled enhancement fixture"
        session_a = texts[2] == "Session A slot1"
        suffix = "A" if session_a else "B"
        runtime = payload[8 + metadata_size:]
        assert struct.unpack_from("<I", runtime)[0] == 2, "New runtime omitted explicit enhancement version"
        candidates, cursor = [], 0
        while True:
            cursor = runtime.find(b"NXEH", cursor)
            if cursor < 0:
                break
            if cursor + 12 <= len(runtime):
                version, size = struct.unpack_from("<II", runtime, cursor + 4)
                if version == 1 and size <= 16 * 1024 * 1024 and cursor + 12 + size == len(runtime):
                    candidates.append(runtime[cursor + 12:])
            cursor += 4
        assert len(candidates) == 1, "Mandatory terminal enhancement chunk missing/ambiguous"
        body, position = candidates[0], 0

        def number(fmt):
            nonlocal position
            value = struct.unpack_from(fmt, body, position)[0]
            position += struct.calcsize(fmt)
            return value

        def text_field():
            nonlocal position
            size = number("<I")
            assert size <= len(body) - position
            value = body[position:position + size].decode("utf-8")
            position += size
            return value

        assert number("<Q") == 2 and number("<I") == 1
        assert text_field() == "NA/DIALOG.FIX" + suffix
        assert number("<H") == (7 if session_a else 8) and number("<H") == (2 if session_a else 3)
        assert text_field() == "Fixture " + suffix
        assert text_field() == ("Cave ahead." if session_a else "River ahead.")
        assert number("<I") == 1
        assert text_field() == ("CAVE" if session_a else "RIVE")
        assert text_field() == ("CAVE" if session_a else "RIVER") and number("<B") == 1
        assert number("<I") == 2
        for index in range(2):
            assert number("<Q") == index + 1
            assert text_field() == ("" if index == 0 else ("cave" if session_a else "river"))
            assert text_field() == ("" if index == 0 else ("CAVE" if session_a else "RIVE"))
            assert text_field() == (("Cave ahead." if session_a else "River ahead.") if index == 0 else "Resolved fixture reply.")
            assert number("<B") == (1 if index == 0 else 0)
            assert number("<B") == index
        assert number("<I") == 2
        for index in range(2):
            assert text_field() == "NA/DATA.MAP.FIX" + suffix
            assert number("<B") == 5 and number("<B") == (1 if session_a else 2)
            assert number("<B") == (24 if session_a else 10) + index
            assert number("<B") == (38 if session_a else 11)
            assert number("<B") == (1 if index == 0 else 0)
            assert number("<B") == index
            assert text_field() == ("Observed fixture " + suffix if index == 0 else "")
        assert position == len(body) and b"SECRETS" not in body
        inspected += 1
    assert inspected == 4, "Expected two physical generations for each controlled session"
    return inspected


def validate_saved_modes(saves, classic, metadata=False, mixed_modes=False):
    """Independent envelope/NSM2 inspection; do not infer mode from the CLI flag."""
    generations = list(saves.glob("nox-ce-*.g[01]"))
    assert len(generations) >= 4, "Two session generation pairs missing"
    for path in generations:
        raw = path.read_bytes()
        assert raw[:4] == b"NXSV"
        version, sequence, length = struct.unpack_from("<III", raw, 4)
        footer = raw[32 + length:]
        assert version in (1, 2) and sequence > 0
        assert footer == (b"NXOK" if version == 2 else b"")
        payload = raw[32:32 + length]
        assert hashlib.md5(raw[:16] + payload + footer).digest() == raw[16:32]
        assert payload[:4] == b"NSM2", "New saves did not record explicit identity/mode"
        size = struct.unpack_from("<I", payload, 4)[0]
        body = payload[8:8 + size]
        assert len(body) == size
        offset = 0
        texts = []
        for _ in range(3):
            count = struct.unpack_from("<H", body, offset)[0]
            texts.append(body[offset + 2:offset + 2 + count].decode("utf-8"))
            offset += 2 + count
        slot, playtime = struct.unpack_from("<II", body, offset)
        offset += 8
        chunks = {}
        while offset < len(body):
            tag, flags = struct.unpack_from("4sB", body, offset)
            # Chunk lengths are little endian; tags alone are byte strings.
            count = struct.unpack_from("<I", body, offset + 5)[0]
            offset += 9
            assert flags in (0, 1) and count <= len(body) - offset
            chunks[tag] = body[offset:offset + count]
            offset += count
        expected_classic = not classic if mixed_modes and texts[2] == "Session B slot1" else classic
        assert chunks[b"MODE"] == bytes([1 if expected_classic else 0]), "Actual saved mode differs from requested new-session mode"
        assert chunks[b"RSRC"].startswith(b"v119:") and len(chunks[b"RSRC"]) == 103
        if metadata:
            year, month, day, hour, minute, second = struct.unpack("<HBBBBB", chunks[b"DATE"])
            datetime.datetime(year, month, day, hour, minute, second)
            assert len(chunks[b"P565"]) == 160 * 120 * 2
            from PIL import Image
            frame = {"Session A slot1": "nox-gameplay.png", "Session B slot1": "nox-engine-saved-b.png"}.get(texts[2], "nox-grid-saved.png")
            source = Image.open(saves / frame).convert("RGB")
            # Independent file oracle: actual separately captured gameplay PNG,
            # canonical aspect-preserving nearest-neighbor P565 specification.
            width, height = source.size
            scaled_height = 160 * height // width
            top = (120 - scaled_height) // 2
            expected = bytearray(160 * 120 * 2)
            for row in range(scaled_height):
                for column in range(160):
                    red, green, blue = source.getpixel((column * width // 160, row * height // scaled_height))
                    value = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
                    struct.pack_into("<H", expected, ((row + top) * 160 + column) * 2, value)
            assert chunks[b"P565"] == expected, "Saved preview differs from independently captured actual gameplay"
            assert len(set(struct.unpack("<19200H", chunks[b"P565"]))) > 1, "Actual gameplay preview is uniform"
        assert (slot in (1, 2)) if classic else (0 <= slot <= 999)
    return len(generations)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--private-config", type=Path, help="Dedicated hermesbox/loopback config outside artifacts; never copied to reports")
    parser.add_argument("--host-lifecycle", action="store_true", help="Cold process also exercises normal pacing, pause, global menu and quit")
    parser.add_argument("--native-video", action="store_true", help="Use Windows SDL video rather than the headless dummy backend")
    parser.add_argument("--chooser-ui", action="store_true", help="Cold process selects actual Engine save/load dialog entries")
    parser.add_argument("--launcher-ui", action="store_true", help="First process uses actual launcher Start and session-name dialog")
    parser.add_argument("--classic", action="store_true", help="Create Classic-mode snapshots; cold startup uses the opposite default to verify restoration")
    parser.add_argument("--interfaces", action="store_true", help="Actual Q save/cancel and third-process opening Continue load/cancel")
    parser.add_argument("--metadata", action="store_true", help="CE metadata/preview queries and isolated paired-generation deletion matrix")
    parser.add_argument("--callers", action="store_true", help="Elapsed-due host autosave and manual-slot policy, plus actual host exit file hashes")
    parser.add_argument("--grid", action="store_true", help="Separate offline grid/list write, overwrite and fresh-process UI load scenario")
    parser.add_argument("--mode-defaults", action="store_true", help="Separate two-session fixture with an actual default change between session creations")
    parser.add_argument("--enhancement-fixture", action="store_true", help="Explicit controlled nonempty journal/discovery owners in actual Engine session saves; not earned capture")
    parser.add_argument("--observed-enhancements", action="store_true", help="Verify actual normally visited opening cells and map-resource identity in both session saves")
    parser.add_argument("--world-command-reference", action="store_true", help="Read-only actual original first-east tutorial and Open-return milestones")
    parser.add_argument("--opening-reference", action="store_true", help="Offline read-only original creation/story keyboard milestones; compatibility oracle, not native acceptance")
    args = parser.parse_args()
    assert not (args.world_command_reference and args.private_config), "World command reference is an isolated offline observer"
    assert not (args.opening_reference and args.private_config), "Original opening reference is an isolated offline observer"
    assert not (args.observed_enhancements and (args.enhancement_fixture or args.grid)), "Observed-state acceptance must not use synthetic owners or the separate grid fixture"
    assert not (args.enhancement_fixture and (args.grid or args.private_config)), "Controlled enhancement owners use offline two-slot fixtures"
    assert not (args.mode_defaults and (args.grid or args.callers or args.interfaces or args.metadata or args.chooser_ui or args.launcher_ui or args.private_config)), "Mixed-mode lifecycle uses a separate offline fixture"
    assert not (args.grid and (args.callers or args.interfaces or args.private_config)), "Grid uses a separate retained-extra-slot fixture; run caller/interface/online scenarios separately"
    if args.grid:
        args.metadata = args.host_lifecycle = True
    if args.callers:
        args.host_lifecycle = True
    assert not (args.launcher_ui and args.private_config), "Launcher acceptance uses its own offline config; online transport is tested separately"
    exe, game = args.exe.resolve(strict=True), args.game.resolve(strict=True)
    originals = {str(p): sha(p) for p in game.glob("*.HDV")}
    assert originals, "No original HDVs supplied"
    root = args.artifacts.resolve() / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    root.mkdir(parents=True, exist_ok=False)
    saves = root / "saves"
    saves.mkdir()
    config = args.private_config.resolve(strict=True) if args.private_config else root / "gameplay.ini"
    if args.private_config:
        assert not config.is_relative_to(root), "Private credential config must stay outside artifact reports"
        ini = configparser.ConfigParser(interpolation=None)
        ini.read(config, encoding="utf-8")
        endpoint = urllib.parse.urlsplit(ini["scummvm"]["ce_achievements_api_base_url"])
        assert endpoint.hostname in ("hermesbox", "127.0.0.1", "localhost") and not endpoint.username and not endpoint.query
        assert ini["scummvm"].get("ce_link_token"), "Dedicated token missing"
        ini["nox-gameplay-test"]["nox_snapshot_validation"] = "false"
        ini["nox-gameplay-test"]["nox_gameplay_validation"] = "true"
        ini["nox-gameplay-test"]["nox_online_validation"] = "true"
        ini["nox-gameplay-test"]["path"] = str(game)
        with config.open("w", encoding="utf-8") as stream:
            ini.write(stream)
    else:
        config.write_text("[scummvm]\ngui_return_to_launcher=false\nupdates_check=0\n\n[nox-gameplay-test]\nengineid=nox\ngameid=nox\nplatform=apple2\npath=" + str(game) + "\nnox_gameplay_validation=true\n", encoding="utf-8")
    ini = configparser.ConfigParser(interpolation=None);ini.read(config,encoding="utf-8")
    ini["nox-gameplay-test"]["nox_classic_saves"] = "true" if args.classic else "false"
    ini["nox-gameplay-test"]["nox_interface_validation"] = "true" if args.interfaces else "false"
    ini["nox-gameplay-test"]["nox_metadata_validation"] = "true" if args.metadata else "false"
    ini["nox-gameplay-test"]["nox_caller_validation"] = "true" if args.callers else "false"
    ini["nox-gameplay-test"]["nox_grid_validation"] = "true" if args.grid else "false"
    ini["nox-gameplay-test"]["nox_mode_defaults_validation"] = "true" if args.mode_defaults else "false"
    ini["nox-gameplay-test"]["nox_enhancement_fixture"] = "true" if args.enhancement_fixture else "false"
    ini["nox-gameplay-test"]["nox_world_command_reference"] = "true" if args.world_command_reference else "false"
    ini["nox-gameplay-test"]["nox_opening_reference"] = "true" if args.opening_reference else "false"
    if args.callers:
        ini["nox-gameplay-test"]["autosave_period"] = "1"
    with config.open("w",encoding="utf-8") as stream:ini.write(stream)
    if args.launcher_ui:
        ini = configparser.ConfigParser(interpolation=None);ini.read(config,encoding="utf-8")
        ini["scummvm"]["nox_launcher_validation"] = "true"
        ini["scummvm"]["lastselectedgame"] = "nox-gameplay-test"
        ini["scummvm"]["gui_launcher_chooser"] = "list"
        with config.open("w",encoding="utf-8") as stream:ini.write(stream)
    command = [str(exe), "--gfx-mode=surfacesdl", f"--config={config}", f"--savepath={saves}", f"--logfile={root / 'engine.log'}", "nox-gameplay-test"]
    if args.launcher_ui: command.pop()
    startup = None
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    if args.native_video:
        assert os.name == "nt", "Native video driver currently targets Windows"
        env["SDL_VIDEODRIVER"] = "windows"
    try:
        result = subprocess.run(command, env=env, capture_output=True, timeout=90, startupinfo=startup)
        (root / "stdout.txt").write_bytes(result.stdout)
        (root / "stderr.txt").write_bytes(result.stderr)
        (root / "invocation.json").write_text(json.dumps({"argv": command, "exit": result.returncode, "exe_sha256": sha(exe)}, indent=2) + "\n")
        assert result.returncode == 0, f"CE gameplay failed; inspect {root}"
        report = saves / "nox-gameplay.tsv"
        assert report.is_file(), f"Gameplay report missing; inspect {root}"
        fields = report.read_text().strip().split("\t")
        assert fields[0] == "PASS"
        summary = dict(field.split("=", 1) for field in fields[1:])
        assert summary["new_game"] == summary["movement"] == summary["modal_open"] == "1"
        assert int(summary["inputs"]) >= 10 and int(summary["audio_samples"]) > 0
        assert int(summary["audio_nonzero"]) > 0, "Only silent PCM generated"
        assert (saves / "nox-gameplay.png").is_file()
        owner_report = (saves / "nox-owner-contracts.tsv").read_text().strip()
        assert owner_report.startswith("PASS\t")
        owner_fields = dict(field.split("=", 1) for field in owner_report.split("\t")[1:])
        assert owner_fields["opaque_domains"] == "5" and owner_fields["rng_calls"] == "64" and owner_fields["clock_calls"] == "124"
        assert owner_fields["live_unchanged"] == "1" and owner_fields["provenance"] == "controlled_clones"

        assert (saves / "nox-engine-saves.tsv").read_text().startswith("PASS\t"), "Engine session saves missing"
        reset = (saves / "nox-session-reset.tsv").read_text().strip()
        reset_fields = dict(item.split("=", 1) for item in reset.split("\t")[1:])
        assert reset.startswith("PASS\t") and int(reset_fields["prior_playtime"]) > 0 and int(reset_fields["prior_audio_queued"]) > 0
        assert reset_fields["new_playtime"] == reset_fields["new_audio_queued"] == "0"
        journey_trace = [row.split("\t") for row in (saves / "nox-gameplay-trace.tsv").read_text().splitlines()]
        fresh_boundaries = {}
        for phase, address in (("0", "a06b"), ("1", "1324"), ("2", "1371")):
            count = sum(len(row) >= 3 and row[1] == phase and row[2].lower() == address for row in journey_trace)
            assert count == 2, f"Expected two independent new-game journeys at {address}, found {count}"
            fresh_boundaries[address] = count
        if args.interfaces:
            assert (saves / "nox-interface-save.tsv").read_text().startswith("PASS\t"), "Original Q save/cancel checks missing"
        metadata_generations = validate_saved_modes(saves, args.classic, args.metadata, args.mode_defaults)
        enhancement_generations = validate_saved_enhancements(saves) if args.enhancement_fixture else 0
        observed_generations = validate_observed_enhancements(saves) if args.observed_enhancements else 0
        if args.enhancement_fixture:
            assert (saves / "nox-enhancement-fixture.tsv").read_text().startswith("PASS\tprovenance=controlled-owner-fixture")
        if args.mode_defaults:
            mode_report = (saves / "nox-mode-defaults.tsv").read_text()
            assert mode_report.startswith("PASS\t") and "active_default_changes=2" in mode_report
            assert f"initial_mode={'Classic' if args.classic else 'Modern'}" in mode_report
            assert f"new_session_mode={'Modern' if args.classic else 'Classic'}" in mode_report
        if args.launcher_ui:
            assert (saves / "nox-launcher.tsv").read_text().startswith("PASS\t"), "Actual launcher flow missing"
        ini = configparser.ConfigParser(interpolation=None)
        ini.read(config, encoding="utf-8")
        ini["nox-gameplay-test"]["nox_snapshot_validation"] = "true"
        ini["nox-gameplay-test"]["nox_classic_saves"] = "false" if args.classic else "true"
        ini["nox-gameplay-test"]["nox_host_validation"] = "true" if args.host_lifecycle else "false"
        ini["nox-gameplay-test"]["nox_chooser_validation"] = "true" if args.chooser_ui else "false"
        ini["scummvm"]["confirm_exit"] = "false"
        with config.open("w", encoding="utf-8") as stream:
            ini.write(stream)
        cold_command = [f"--logfile={root / 'cold-engine.log'}" if arg.startswith("--logfile=") else arg for arg in command]
        if args.launcher_ui: cold_command.append("nox-gameplay-test")
        cold_result = subprocess.run(cold_command, env=env, capture_output=True, timeout=90, startupinfo=startup)
        (root / "cold-stdout.txt").write_bytes(cold_result.stdout)
        (root / "cold-stderr.txt").write_bytes(cold_result.stderr)
        (root / "cold-invocation.json").write_text(json.dumps({"argv": cold_command, "exit": cold_result.returncode, "exe_sha256": sha(exe)}, indent=2) + "\n")
        assert cold_result.returncode == 0 and (saves / "nox-cold.tsv").is_file(), f"Cold gameplay failed; inspect {root}"
        cold_fields = (saves / "nox-cold.tsv").read_text().strip().split("\t")
        assert cold_fields[0] == "PASS"
        cold_summary = dict(field.split("=", 1) for field in cold_fields[1:])
        assert cold_summary["cold_load"] == cold_summary["continuation"] == "1"
        assert (cold_summary["restored_x"], cold_summary["restored_y"]) == (summary["x"], summary["y"])
        assert (cold_summary["x"], cold_summary["y"]) != (summary["x"], summary["y"])
        assert (saves / "nox-cold-restored.png").read_bytes() == (saves / "nox-gameplay.png").read_bytes(), "Cold restored frame differs"
        assert (saves / "nox-engine-cold.tsv").read_text().startswith("PASS\t"), "Cold Engine session checks missing"
        assert "saved_mode_restored=1" in (saves / "nox-engine-cold.tsv").read_text(), "Saved mode restoration missing"
        if args.metadata:
            assert (saves / "nox-metadata.tsv").read_text().startswith("PASS\t"), "Metadata/deletion matrix missing"
            metadata_ui = (saves / "nox-metadata-ui.tsv").read_text()
            assert metadata_ui.startswith("PASS\t") and f"actual={'classic-list' if args.classic else 'grid'}" in metadata_ui
            assert (saves / "nox-metadata-chooser-ui.png").is_file(), "Actual metadata/grid preview missing"
            for sidecar in saves.glob("nox-ce-*.ce-achievements.json"):
                assert sidecar.with_name(sidecar.name.removesuffix(".ce-achievements.json")).is_file(), "Deletion left an orphan generation sidecar after process exit"
            validate_saved_modes(saves, args.classic, True)
        if args.host_lifecycle:
            assert (saves / "nox-host.tsv").read_text().startswith("PASS\t"), "Normal host lifecycle checks missing"
        if args.callers:
            assert (saves / "nox-callers.tsv").read_text().startswith("PASS\t"), "Actual caller policy checks missing"
            fingerprints = dict(line.split("\t") for line in (saves / "nox-callers-files.tsv").read_text().splitlines())
            assert set(fingerprints) == {p.name for p in saves.glob("nox-ce-*")}, "Load/exit autosave changed save set"
            assert all(hashlib.md5((saves / name).read_bytes()).hexdigest() == value for name, value in fingerprints.items()), "Actual host exit changed a generation/sidecar"
        if args.chooser_ui:
            chooser = (saves / "nox-chooser.tsv").read_text()
            assert chooser.startswith("PASS\t"), "Actual Engine chooser checks missing"
            assert f"allowed_slots={2 if args.classic else 1000}" in chooser and "policy_guards=1" in chooser
            assert (saves / "nox-save-chooser-ui.png").is_file(), "Actual save chooser screenshot missing"
        assert (saves / "nox-engine-cold-a.png").read_bytes() == (saves / "nox-gameplay.png").read_bytes(), "Cold session A frame differs"
        assert (saves / "nox-engine-cold-b.png").read_bytes() == (saves / "nox-engine-saved-b.png").read_bytes(), "Cold session B frame differs"
        if args.enhancement_fixture:
            validate_saved_enhancements(saves)
        if args.observed_enhancements:
            validate_observed_enhancements(saves)
        if args.grid:
            assert (saves / "nox-grid.tsv").read_text().startswith("PASS\t"), "Actual grid/list write and overwrite missing"
            baseline = dict(line.split("\t") for line in (saves / "nox-grid-base-files.tsv").read_text().splitlines())
            assert len({p.name for p in saves.glob("nox-ce-*")} - set(baseline)) == 4, "Grid fixture did not create one paired logical save"
            assert all(hashlib.md5((saves / name).read_bytes()).hexdigest() == value for name, value in baseline.items()), "Grid overwrite changed another save/session"
            ini.read(config, encoding="utf-8")
            ini["nox-gameplay-test"].update(nox_snapshot_validation="false", nox_grid_validation="false",
                nox_grid_reload_validation="true", nox_metadata_validation="false", nox_host_validation="false",
                nox_chooser_validation="false", nox_classic_saves="true" if args.classic else "false")
            with config.open("w", encoding="utf-8") as stream:
                ini.write(stream)
            grid_command = [f"--logfile={root / 'grid-cold-engine.log'}" if arg.startswith("--logfile=") else arg for arg in cold_command]
            grid_cold = subprocess.run(grid_command, env=env, capture_output=True, timeout=90, startupinfo=startup)
            (root / "grid-cold-stdout.txt").write_bytes(grid_cold.stdout)
            (root / "grid-cold-stderr.txt").write_bytes(grid_cold.stderr)
            (root / "grid-cold-invocation.json").write_text(json.dumps({"argv": grid_command, "exit": grid_cold.returncode}, indent=2) + "\n")
            assert grid_cold.returncode == 0 and (saves / "nox-grid-cold.tsv").read_text().startswith("PASS\t"), "Actual fresh-process grid/list load failed"
            assert (saves / "nox-grid-cold.png").read_bytes() == (saves / "nox-grid-saved.png").read_bytes(), "Grid cold frame differs"
            assert set(baseline) == {p.name for p in saves.glob("nox-ce-*")}, "Grid cleanup left a generation or sidecar"
            assert all(hashlib.md5((saves / name).read_bytes()).hexdigest() == value for name, value in baseline.items()), "Grid load/cleanup changed original saves"
        if args.interfaces:
            ini.read(config, encoding="utf-8")
            ini["nox-gameplay-test"]["nox_snapshot_validation"] = "false"
            ini["nox-gameplay-test"]["nox_interface_validation"] = "false"
            ini["nox-gameplay-test"]["nox_continue_validation"] = "true"
            ini["nox-gameplay-test"]["nox_host_validation"] = "false"
            ini["nox-gameplay-test"]["nox_chooser_validation"] = "false"
            with config.open("w", encoding="utf-8") as stream:
                ini.write(stream)
            continue_command = [f"--logfile={root / 'continue-engine.log'}" if arg.startswith("--logfile=") else arg for arg in cold_command]
            resumed = subprocess.run(continue_command, env=env, capture_output=True, timeout=90, startupinfo=startup)
            (root / "continue-stdout.txt").write_bytes(resumed.stdout)
            (root / "continue-stderr.txt").write_bytes(resumed.stderr)
            (root / "continue-invocation.json").write_text(json.dumps({"argv": continue_command, "exit": resumed.returncode}, indent=2) + "\n")
            assert resumed.returncode == 0, "Original Continue process failed"
            continued = (saves / "nox-interface-continue.tsv").read_text()
            assert continued.startswith("PASS\t"), "Original Continue load/cancel checks missing"
            expected = "nox-gameplay.png" if "label=Session A slot1" in continued else "nox-engine-saved-b.png"
            assert (saves / "nox-interface-continue.png").read_bytes() == (saves / expected).read_bytes(), "Original Continue frame differs"
        if args.private_config:
            # Simulate loss of delivery acknowledgement. Preserve every signed
            # event identity/payload; only the local transport status is reset.
            queue_file = saves / "ce-achievements-queue.json"
            acknowledged = json.loads(queue_file.read_text())
            assert acknowledged and all(row["state"] == "synced" for row in acknowledged)
            for row in acknowledged:
                row["state"] = "pending"
            queue_file.write_text(json.dumps(acknowledged) + "\n", encoding="utf-8")
            replay_command = [f"--logfile={root / 'replay-engine.log'}" if arg.startswith("--logfile=") else arg for arg in cold_command]
            replay = subprocess.run(replay_command, env=env, capture_output=True, timeout=90, startupinfo=startup)
            (root / "replay-stdout.txt").write_bytes(replay.stdout)
            (root / "replay-stderr.txt").write_bytes(replay.stderr)
            (root / "replay-invocation.json").write_text(json.dumps({"argv": replay_command, "exit": replay.returncode}, indent=2) + "\n")
            assert replay.returncode == 0, "CE transport replay failed"
            records = json.loads((saves / "ce-playthroughs.json").read_text())["playthroughs"]
            assert len(records) == 2 and all(row["playthrough"] for row in records), "Missing online session identity"
            assert len({row["playthrough"] for row in records}) == 2, "Online session identities collapsed"
            events = json.loads((saves / "ce-achievements-queue.json").read_text())
            assert events and all(row["state"] == "synced" for row in events), "Online queue includes unsynchronized/permanently failed events"
            by_client = {row["client_playthrough_id"]: row["playthrough"] for row in records}
            starts = [row for row in events if row["request_type"] == "playthrough_start"]
            assert len(starts) == 2 and all(by_client.get(row["client_playthrough_id"]) == row["playthrough"] for row in starts), "Start response and registry identities disagree"
            by_event = {(row["request_type"], row["event_id"]): row for row in events}
            transport_fields = {"state", "attempt_count", "last_attempt_at", "last_error"}
            for original in acknowledged:
                replayed = by_event[(original["request_type"], original["event_id"])]
                assert all(replayed[key] == value for key, value in original.items() if key not in transport_fields), "Replay changed immutable event payload"
                assert replayed["attempt_count"] > original["attempt_count"], "Original event was not retried"
        evidence = {"status": "VERIFIED", "scope": "CE opening gameplay/audio, complete runtime codec, two session-qualified Engine slot1 saves, cold identical frames/identities and audio reset; no full campaign or lifecycle failure matrix", "online_transport": bool(args.private_config), "executable_sha256": sha(exe), "original_sha256": originals, "summary": summary, "cold_summary": cold_summary, "engine_saves": (saves / "nox-engine-saves.tsv").read_text().strip(), "engine_cold": (saves / "nox-engine-cold.tsv").read_text().strip(), "argv": command, "cold_argv": cold_command, "artifacts": str(root)}
        evidence["immutable_transport_replay"] = bool(args.private_config)
        evidence["sdl_video_driver"] = env["SDL_VIDEODRIVER"]
        evidence["host_lifecycle"] = (saves / "nox-host.tsv").read_text().strip() if args.host_lifecycle else "NOT STARTED"
        evidence["chooser_ui"] = (saves / "nox-chooser.tsv").read_text().strip() if args.chooser_ui else "NOT STARTED"
        evidence["launcher_ui"] = (saves / "nox-launcher.tsv").read_text().strip() if args.launcher_ui else "NOT STARTED"
        evidence["owner_contracts"] = owner_report
        evidence["saved_mode"] = "Classic" if args.classic else "Modern"
        evidence["fresh_session_new_game_boundaries"] = fresh_boundaries
        evidence["new_session_reset"] = reset
        if args.world_command_reference:
            captured={}
            for name in ['first-east','first-east-idle','open-return']:
                prefix=saves/('nox-original-world-'+name)
                raw=prefix.with_suffix('.bin').read_bytes();assert len(raw)==131072 and raw[0x267d]==8 and raw[0x2af9]==7
                assert raw[0x6cec:0x6cee]==bytes([25,38])
                if name=='open-return':assert raw[0xbe03]==40 and raw[0x2bf8]==1
                if name=='first-east-idle':assert raw[0xbe03]==16 and raw[0x2bf8]==0 and raw[0xbd07]==6 and raw[0x257e]==1
                for suffix in ('.bin','.tsv','.png'):
                    q=prefix.with_suffix(suffix);assert q.exists();captured[q.name]=sha(q)
            evidence['original_world_command_reference']=dict(status='VERIFIED',scope='Actual original first east/tutorial and Open return through ordinary CE keys; compatibility reference only',artifacts=captured)
        if args.opening_reference:
            boundaries=sorted(saves.glob('nox-original-opening-*.tsv'))
            rows=[q.read_text() for q in boundaries]
            story_boundaries=[]
            for row in rows:
                fields=dict(item.split('=',1) for item in row.strip().split('\t')[1:])
                if any(0xb38f<=int(fields[key],16)<=0xb65a for key in ('main_text_pointer','aux_text_pointer')):story_boundaries.append(fields)
            assert boundaries and story_boundaries, "Actual original story text keyboard milestone not observed"
            captured={q.name:sha(q) for q in saves.glob('nox-original-opening-*')}
            for q in boundaries:assert q.with_suffix('.bin').stat().st_size==131072
            evidence['original_opening_reference']=dict(status='VERIFIED',scope='Read-only actual compatibility creation/story input milestones followed by actual world input; no native opening acceptance',boundaries=rows,story_boundaries=story_boundaries,artifacts=captured)
        evidence["mode_defaults"] = (saves / "nox-mode-defaults.tsv").read_text().strip() if args.mode_defaults else "NOT STARTED"
        evidence["controlled_enhancement_generations"] = enhancement_generations
        evidence["observed_enhancement_generations"] = observed_generations
        evidence["metadata_management"] = (saves / "nox-metadata.tsv").read_text().strip() if args.metadata else "NOT STARTED"
        evidence["metadata_ui"] = (saves / "nox-metadata-ui.tsv").read_text().strip() if args.metadata else "NOT STARTED"
        evidence["save_callers"] = (saves / "nox-callers.tsv").read_text().strip() if args.callers else "NOT STARTED"
        evidence["grid_writes"] = (saves / "nox-grid.tsv").read_text().strip() if args.grid else "NOT STARTED"
        evidence["grid_cold"] = (saves / "nox-grid-cold.tsv").read_text().strip() if args.grid else "NOT STARTED"
        evidence["original_interfaces"] = {"save": (saves / "nox-interface-save.tsv").read_text().strip(),
                                            "continue": (saves / "nox-interface-continue.tsv").read_text().strip()} if args.interfaces else "NOT STARTED"
        evidence["independently_verified_mode_generations"] = metadata_generations
        (root / "verification.json").write_text(json.dumps(evidence, indent=2) + "\n")
        print(json.dumps(evidence, indent=2))
    except subprocess.TimeoutExpired as error:
        (root / "stdout.txt").write_bytes(error.stdout or b"")
        (root / "stderr.txt").write_bytes(error.stderr or b"")
        raise AssertionError(f"CE gameplay timed out; inspect {root}") from error
    finally:
        assert all(sha(Path(p)) == value for p, value in originals.items()), "Original HDV changed"


if __name__ == "__main__":
    main()
