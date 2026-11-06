#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate the CE resource/record slice against an independent ProDOS reader.

Original images are read-only. Resource bytes and altered test images stay in
the specified local artifact directory, never in the repository. This does not
claim playable-engine, save/session or campaign acceptance.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time


def run_validation_process(command, root, env, startup=None, timeout=90):
    """Stop only this explicitly failed test child; never wait on its error UI."""
    failure=root/'saves/nox-native-validation-failure.txt'
    process=subprocess.Popen(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,startupinfo=startup)
    deadline=time.monotonic()+timeout
    reason=None
    action='process_exited'
    try:
        while True:
            if failure.exists() and failure.stat().st_size:
                reason=failure.read_text(errors='replace')
                action='terminated_from_native_failure'
                process.terminate()
                stdout,stderr=process.communicate(timeout=10)
                break
            try:
                stdout,stderr=process.communicate(timeout=0.1)
                break
            except subprocess.TimeoutExpired:
                if time.monotonic()>=deadline:
                    process.kill();stdout,stderr=process.communicate(timeout=10)
                    action='killed_on_timeout'
                    reason='Validation timed out after '+str(timeout)+' seconds without a completed report'
                    break
        if failure.exists() and reason is None:reason=failure.read_text(errors='replace') or 'Empty native failure receipt'
        if process.returncode and reason is None:
            reason='Validation process exited with code 0x'+format(process.returncode&0xffffffff,'08X')+' without a native validation failure receipt'
        if reason is not None:
            (root/'native-validation-rejected.stdout.txt').write_bytes(stdout)
            (root/'native-validation-rejected.stderr.txt').write_bytes(stderr)
            (root/'native-validation-rejected.json').write_text(json.dumps(dict(status='IN PROGRESS',argv=command,
                 exit=process.returncode,reason=reason,action=action,scope='Rejected test invocation; never a gameplay pass or inferred engine defect'),indent=2)+'\n')
            raise AssertionError(reason)
        return subprocess.CompletedProcess(command,process.returncode,stdout,stderr)
    finally:
        if process.poll() is None:
            process.kill();process.communicate(timeout=10)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def files(image):
    """Independent small ProDOS oracle; no CE or conversion parser imports."""
    result = {}

    def block(n):
        assert 0 <= n < len(image) // 512
        return image[n * 512:(n + 1) * 512]

    def pointers(n):
        b = block(n) if n else bytes(512)
        return [b[i] | b[i + 256] << 8 for i in range(256)]

    visited = set()

    def directory(key, prefix):
        assert key not in visited
        visited.add(key)
        n = key
        chain = set()
        while n:
            assert n not in chain
            chain.add(n)
            b = block(n)
            for slot in range(13):
                if n == key and slot == 0:
                    continue
                e = b[4 + slot * 39:43 + slot * 39]
                kind = e[0] >> 4
                if not kind:
                    continue
                name = prefix + e[1:1 + (e[0] & 15)].decode("ascii")
                ptr = int.from_bytes(e[17:19], "little")
                size = int.from_bytes(e[21:24], "little")
                if kind == 13:
                    directory(ptr, name + "/")
                else:
                    assert 1 <= kind <= 3
                    ids = ([ptr] if kind == 1 else pointers(ptr) if kind == 2
                           else [j for i in pointers(ptr) for j in pointers(i)])
                    data = b"".join(block(i) if i else bytes(512) for i in ids[:(size + 511) // 512])[:size]
                    assert len(data) == size and name not in result
                    result[name] = (size, hashlib.md5(data).hexdigest())
            n = int.from_bytes(b[2:4], "little")
    directory(2, "")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--expansion", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--native-video", action="store_true", help="Use native Windows SDL instead of dummy video")
    parser.add_argument("--native-dialogue-oracle", type=Path, help="Local-only native metadata comparison vectors")
    parser.add_argument("--native-inventory-oracle", type=Path, help="Local-only actual M3 inventory handoff comparison vectors")
    parser.add_argument("--native-dialogue-selection", type=Path, help="Local-only first-eligible response selection vectors")
    parser.add_argument("--native-dialogue-openings", type=Path, help="Local-only opening response selection vectors")
    parser.add_argument("--native-cannon", type=Path, help="Local original complete cannon command cases")
    parser.add_argument("--native-transport-control", type=Path, help="Local source B/X record/control cases")
    parser.add_argument("--native-music", type=Path, help="Whole original music sequencer traces")
    parser.add_argument("--native-ending", type=Path, help="Original authored special-entry and ending branch contracts")
    parser.add_argument("--native-vendor", type=Path, help="Local complete vendor transaction and authored catalog source cases")
    parser.add_argument("--native-transport-motion", type=Path, help="Local source complete transport movement-gate cases")
    parser.add_argument("--native-party-selector", type=Path, help="Local original shared party-selector cases")
    parser.add_argument("--native-inn-membership", type=Path, help="Local original full-owned inn membership chains")
    parser.add_argument("--native-inn", type=Path, help="Local original inn rest/sleep oracle")
    parser.add_argument("--native-temple", type=Path, help="Local original temple pricing/payment reference")
    parser.add_argument("--native-conversation", type=Path, help="Local-only actual NPC transaction comparison vectors")
    parser.add_argument("--native-dialogue-input", type=Path, help="Local-only input normalization vectors")
    parser.add_argument("--native-pages-chain", type=Path, help="Local-only actual Axea/Valess/Nystul chain comparison vectors")
    parser.add_argument("--native-search-reward", type=Path)
    parser.add_argument("--native-lock", type=Path)
    parser.add_argument("--native-lock-party", type=Path)
    parser.add_argument("--native-page-search", type=Path)
    parser.add_argument("--native-book", type=Path)
    parser.add_argument("--native-cast-preparation", type=Path)
    parser.add_argument("--native-vortex-targets", type=Path)
    parser.add_argument("--native-spell-power", type=Path)
    parser.add_argument("--native-vortex-effect", type=Path)
    parser.add_argument("--native-vortex-cast", type=Path)
    parser.add_argument("--native-summon", type=Path)
    parser.add_argument("--native-battle-records", type=Path)
    parser.add_argument("--native-equipment-ownership", type=Path)
    parser.add_argument("--native-armor-ready", type=Path)
    parser.add_argument("--native-equipment-derived", type=Path)
    parser.add_argument("--native-owned-armor", type=Path)
    parser.add_argument("--native-weapon-ready", type=Path)
    parser.add_argument("--native-npc-schedule", type=Path)
    parser.add_argument("--native-owned-path", type=Path)
    parser.add_argument("--native-npc-route-store", type=Path)
    parser.add_argument("--native-npc-route-follow", type=Path)
    parser.add_argument("--native-npc-navigation", type=Path)
    parser.add_argument("--native-npc-clock-pipeline", type=Path)
    parser.add_argument("--native-world-large-geometry", type=Path)
    parser.add_argument("--native-surface-offscreen", type=Path)
    parser.add_argument("--native-world-actor-phase", type=Path)
    parser.add_argument("--native-world-actor-turn", type=Path)
    parser.add_argument("--native-compact-world", type=Path)
    parser.add_argument("--native-text-window", type=Path)
    parser.add_argument("--native-text-glyph", type=Path)
    parser.add_argument("--native-message-strip", type=Path)
    parser.add_argument("--native-npc-route-recovery", type=Path)
    parser.add_argument("--native-actor-pursuit", type=Path)
    parser.add_argument("--native-npc-action-dispatch", type=Path)
    parser.add_argument("--native-npc-path", type=Path)
    parser.add_argument("--native-npc-prefetch", type=Path)
    parser.add_argument("--native-npc-reentry", type=Path)
    parser.add_argument("--native-npc-clock", type=Path)
    parser.add_argument("--native-npc-actor-actions", type=Path)
    parser.add_argument("--native-actor-motion", type=Path)
    parser.add_argument("--native-actor-intent", type=Path)
    parser.add_argument("--native-actor-step", type=Path)
    parser.add_argument("--native-actor-spatial", type=Path)
    parser.add_argument("--native-actor-outside", type=Path)
    parser.add_argument("--native-actor-movement-effects", type=Path)
    parser.add_argument("--native-npc-contact", type=Path)
    parser.add_argument("--native-npc-world-action", type=Path)
    parser.add_argument("--native-npc-world-ranged", type=Path)
    parser.add_argument("--native-npc-world-effects", type=Path)
    parser.add_argument("--native-npc-world-free", type=Path)
    parser.add_argument("--native-npc-large-contact", type=Path)
    parser.add_argument("--native-actor-contact", type=Path)
    parser.add_argument("--native-viewport-maps", type=Path)
    parser.add_argument("--native-world-viewport", type=Path)
    parser.add_argument("--native-occlusion", type=Path)
    parser.add_argument("--native-world-command", type=Path)
    parser.add_argument("--native-world-scene", type=Path)
    parser.add_argument("--native-world-story", type=Path)
    parser.add_argument("--native-story-patches", type=Path)
    parser.add_argument("--native-new-game", type=Path)
    parser.add_argument("--native-owned-weapon", type=Path)
    parser.add_argument("--native-rng", type=Path)
    parser.add_argument("--native-world-presentation", type=Path)
    parser.add_argument("--native-tile-paint", type=Path)
    parser.add_argument("--native-actors", type=Path)
    parser.add_argument("--native-actor-paint", type=Path)
    parser.add_argument("--native-lighting-chain", type=Path)
    parser.add_argument("--native-personal-light", type=Path)
    parser.add_argument("--native-terrain-light", type=Path)
    parser.add_argument("--native-light", type=Path)
    parser.add_argument("--native-visibility", type=Path)
    parser.add_argument("--native-objects", type=Path)
    parser.add_argument("--native-view", type=Path)
    parser.add_argument("--native-party", type=Path)
    parser.add_argument("--native-traversal", type=Path)
    parser.add_argument("--native-graphics", type=Path, help="Local-only actual graphic data and pixel comparison vectors")
    args = parser.parse_args()
    exe, base, expansion = (p.resolve(strict=True) for p in (args.exe, args.base, args.expansion))
    root = args.artifacts.resolve() / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    root.mkdir(parents=True, exist_ok=False)
    hashes = {str(p): digest(p) for p in (base, expansion)}
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    if args.native_video:
        assert os.name == "nt"
        env["SDL_VIDEODRIVER"] = "windows"
    commands = []

    def run(name, parameters):
        command = [str(exe), "--gfx-mode=surfacesdl", *map(str, parameters)]
        startup = None
        if os.name == "nt":
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        completed = run_validation_process(command, root, env, startup)
        (root / f"{name}.stdout.txt").write_bytes(completed.stdout)
        (root / f"{name}.stderr.txt").write_bytes(completed.stderr)
        commands.append({"name": name, "argv": command, "exit": completed.returncode})
        return (completed.stdout + completed.stderr).decode("utf-8", errors="replace")

    # Keep all fixtures outside the Git checkout. Renamed and save-sector-modified
    # inputs ensure detection does not depend on the distribution filename/hash.
    game = root / "base-renamed"
    game.mkdir()
    data = bytearray(base.read_bytes())
    expected = files(data)
    shutil.copy2(base, game / "renamed.HDV")
    saves = root / "saves"
    saves.mkdir()
    config = root / "test.ini"
    config.write_text("[scummvm]\ngui_return_to_launcher=false\nupdates_check=0\n\n[nox-resource-test]\nengineid=nox\ngameid=nox\nplatform=apple2\npath=" + str(game) + "\nnox_resource_validation=true\n", encoding="utf-8")
    if args.native_dialogue_oracle:
        with config.open('a',encoding='utf-8') as output:
            output.write('nox_native_dialogue_oracle='+str(args.native_dialogue_oracle.resolve())+'\n')
    if args.native_music:
        with config.open('a',encoding='utf-8') as output:
            output.write('nox_native_music='+str(args.native_music.resolve())+'\n')
    if args.native_ending:
        with config.open('a',encoding='utf-8') as output:
            output.write('nox_native_ending='+str(args.native_ending.resolve())+'\n')
    if args.native_inventory_oracle:
        with config.open('a',encoding='utf-8') as output:
            output.write('nox_native_inventory_oracle='+str(args.native_inventory_oracle.resolve())+'\n')
    if args.native_dialogue_selection:
        with config.open('a',encoding='utf-8') as output:
            output.write('nox_native_dialogue_selection='+str(args.native_dialogue_selection.resolve())+'\n')
    for name in ['native_vendor','native_transport_motion','native_transport_control','native_cannon','native_party_selector','native_inn_membership','native_inn','native_temple','native_dialogue_openings','native_conversation','native_dialogue_input','native_pages_chain','native_graphics','native_traversal','native_party','native_view','native_objects','native_visibility','native_light','native_terrain_light','native_personal_light','native_lighting_chain','native_actor_paint','native_actors','native_tile_paint','native_world_presentation','native_rng','native_search_reward','native_lock','native_lock_party','native_page_search','native_book','native_cast_preparation','native_vortex_targets','native_spell_power','native_vortex_effect','native_vortex_cast','native_summon','native_battle_records','native_equipment_ownership','native_armor_ready','native_equipment_derived','native_owned_armor','native_weapon_ready','native_owned_weapon','native_new_game','native_story_patches','native_world_story','native_world_scene','native_world_command','native_occlusion','native_world_viewport','native_viewport_maps','native_actor_step','native_actor_spatial','native_actor_outside','native_actor_movement_effects','native_npc_contact','native_npc_world_action','native_npc_world_ranged','native_npc_world_effects','native_npc_world_free','native_npc_large_contact','native_actor_contact','native_actor_intent','native_npc_schedule','native_actor_motion','native_npc_actor_actions','native_npc_clock','native_npc_reentry','native_npc_prefetch','native_npc_path','native_owned_path','native_npc_route_store','native_npc_route_follow','native_npc_navigation','native_npc_clock_pipeline','native_world_large_geometry','native_surface_offscreen','native_world_actor_phase','native_world_actor_turn','native_compact_world','native_text_window','native_text_glyph','native_message_strip','native_npc_route_recovery','native_actor_pursuit','native_npc_action_dispatch']:
        value=getattr(args,name)
        if value:
            with config.open('a',encoding='utf-8') as output:
                output.write('nox_'+name+'='+str(value.resolve())+'\n')
    run("resources", ["--debuglevel=1", f"--config={config}", f"--savepath={saves}", f"--logfile={root / 'engine.log'}", "nox-resource-test"])
    assert "Surface SDL: deferring redraw before initial graphics size" in (root / "engine.log").read_text(errors="replace"), "Pre-init redraw path was not exercised"
    report = saves / "nox-resource-validation.tsv"
    assert report.is_file(), f"Engine did not produce validation report; inspect {root}"
    actual, summary = {}, None
    for line in report.read_text().splitlines():
        fields = line.split("\t")
        if fields[0] == "resource":
            assert fields[1] not in actual
            actual[fields[1]] = (int(fields[2]), fields[3])
        elif fields[0] == "PASS":
            summary = dict(field.split("=", 1) for field in fields[1:])
    assert actual == expected, "CE resource data differs from independent oracle"
    assert summary and int(summary["files"]) == len(expected) and int(summary["negative"]) == 8
    assert int(summary.get("storage_cases", 0)) == 3, "Generation storage checks missing"
    assert int(summary.get("schema_cases", 0)) == 7, "Version migration/completion checks missing"
    assert int(summary.get("metadata_cases", 0)) == 22, "Resource/mode/date/preview metadata checks missing"
    assert int(summary.get("enhancement_cases", 0)) == 23, "Enhancement semantics/codec/rollback contracts missing"
    engines = run("engines", ["--list-engines"])
    assert "nox" in engines.lower() and "Nox Archaist" in engines
    detected = run("detect-base", ["--detect", f"--path={game}"])
    assert re.search(r"^nox:nox\s+.*v119", detected, re.MULTILINE)
    exp = root / "expansion-only"
    exp.mkdir()
    shutil.copy2(expansion, exp / "renamed.HDV")
    rejected = run("detect-expansion", ["--detect", f"--path={exp}"])
    assert not re.search(r"^nox:nox\s", rejected, re.MULTILINE), "Expansion incorrectly detected as supported base"
    # Modify only a genuine GAME1 data block selected from its ProDOS directory.
    # Locate the raw block by matching its first full logical block. All stable
    # detection resources must still match the independent expected values.
    def find_entry(name):
        for offset in range(4, len(data) - 39, 512):
            for slot in range(13):
                p = offset + slot * 39
                e = data[p:p + 39]
                if e[0] & 15 == len(name) and e[1:1 + len(name)] == name.encode():
                    return e
        raise AssertionError(name)
    save_entry = find_entry("DATA.SAVE.GAME1")
    key = int.from_bytes(save_entry[17:19], "little")
    if save_entry[0] >> 4 == 2:
        key = data[key * 512] | data[key * 512 + 256] << 8
    assert key
    data[key * 512] ^= 1
    altered = root / "saved-image"
    altered.mkdir()
    (altered / "saved.HDV").write_bytes(data)
    altered_detection = run("detect-saved-image", ["--detect", f"--path={altered}"])
    assert re.search(r"^nox:nox\s+.*v119", altered_detection, re.MULTILINE)
    session_config = root / "sessions.ini"
    session_config.write_text("[scummvm]\ngui_return_to_launcher=false\nupdates_check=0\n\n[nox-session-test]\nengineid=nox\ngameid=nox\nplatform=apple2\npath=" + str(game) + "\nnox_session_validation=true\n", encoding="utf-8")
    session_args = [f"--config={session_config}", f"--savepath={saves}", f"--logfile={root / 'sessions.log'}", "nox-session-test"]
    run("sessions-create", session_args)
    assert (saves / "nox-session-create.tsv").is_file(), "Offline CE session create checks failed"
    registry = saves / "ce-playthroughs.json"
    registry_before = registry.read_bytes()
    run("sessions-cold", session_args)
    assert (saves / "nox-session-cold.tsv").is_file(), "Fresh-process CE session checks failed"
    assert registry.read_bytes() == registry_before, "Cold loads unexpectedly changed session registry"
    queue = json.loads((saves / "ce-achievements-queue.json").read_text())
    assert len(queue) == 8 and all(row["state"] == "pending" and row["attempt_count"] == 0 for row in queue), "Unlinked metadata must remain durable without transmission attempts"
    assert all(row["request_type"] != "achievement" for row in queue), "No fixture achievement definitions or events"
    owners = {row["client_playthrough_id"] for row in json.loads(registry.read_text())["playthroughs"]}
    assert len(owners) == 2 and {row["client_playthrough_id"] for row in queue} == owners
    assert sum(row["request_type"] == "playthrough_start" for row in queue) == 2
    assert sum(row["request_type"] == "playthrough_save" for row in queue) == 6
    assert all(digest(Path(p)) == sha for p, sha in hashes.items()), "Original HDV changed"
    assert all(c["exit"] == 0 for c in commands if c["name"] != "detect-expansion"), commands
    evidence = {"status": "VERIFIED", "scope": "M1.1, M1.2 registration/detection, M1.5 offline metadata only; no gameplay or online acceptance", "executable_sha256": digest(exe), "source_sha256": hashes, "summary": summary, "resource_files_exact": len(expected), "offline_sessions": 2, "commands": commands, "artifacts": str(root)}
    (root / "verification.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    main()
