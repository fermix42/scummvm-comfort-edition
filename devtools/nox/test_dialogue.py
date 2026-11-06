#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real NPC response capture with explicitly assisted authored-exit/adjacency setup.

This is a controlled dialogue-boundary test, never an earned campaign route.
No journal replies/topics, NPC eligibility, roster or quest flags are injected.
Original HDVs stay read-only; all save data and captured prose stay local.
"""
import argparse
import configparser
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import urllib.parse
from enhancement_oracle import read_save_enhancements


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--private-config", type=Path, help="Dedicated hermesbox/loopback runtime config, kept outside reports")
    parser.add_argument("--classic", action="store_true")
    parser.add_argument("--legacy-assistance", action="store_true", help="Also test a separate controlled v1 registry copy with mixed global reasons")
    parser.add_argument("--lifecycle", action="store_true", help="Before/after dialogue, fresh session B and older/newer cold restoration with permanent-history isolation")
    args = parser.parse_args()
    assert not (args.private_config and args.legacy_assistance), "Legacy configuration copy is offline only"
    assert not args.private_config or args.lifecycle, "Online diagnostic requires lifecycle"
    assert not args.legacy_assistance or args.lifecycle, "Legacy assistance fixture requires lifecycle"
    exe, game = args.exe.resolve(strict=True), args.game.resolve(strict=True)
    originals = {str(p): sha(p) for p in game.glob("*.HDV")}
    assert originals
    root = args.artifacts.resolve() / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    saves = root / "saves"; saves.mkdir(parents=True, exist_ok=False)
    config = args.private_config.resolve(strict=True) if args.private_config else root / "dialogue.ini"
    if args.private_config:
        assert not config.is_relative_to(root), "Credentials must stay outside reports"
        ini = configparser.ConfigParser(interpolation=None); ini.read(config, encoding="utf-8")
        endpoint = urllib.parse.urlsplit(ini["scummvm"]["ce_achievements_api_base_url"])
        assert endpoint.hostname in ("hermesbox", "127.0.0.1") and not endpoint.username and not endpoint.query
        assert ini["scummvm"].get("ce_link_token")
        ini["nox-gameplay-test"]["nox_online_validation"] = "true"
    else:
        ini = configparser.ConfigParser(interpolation=None)
        ini["scummvm"] = {"gui_return_to_launcher":"false", "updates_check":"0"}
        ini["nox-gameplay-test"] = {"engineid":"nox", "gameid":"nox", "platform":"apple2", "path":str(game)}
    domain = ini["nox-gameplay-test"]
    domain["path"] = str(game)
    for key in ("nox_enhancement_fixture", "nox_interface_validation", "nox_continue_validation", "nox_metadata_validation", "nox_caller_validation", "nox_grid_validation", "nox_host_validation", "nox_chooser_validation", "nox_assistance_legacy"):
        domain[key] = "false"
    domain["nox_gameplay_validation"] = domain["nox_dialogue_validation"] = "true"
    domain["nox_dialogue_reload"] = "false"
    domain["nox_snapshot_validation"] = "false"
    domain["nox_classic_saves"] = "true" if args.classic else "false"
    domain["nox_dialogue_lifecycle"] = "true" if args.lifecycle else "false"
    with config.open("w", encoding="utf-8") as out: ini.write(out)
    command = [str(exe), "--gfx-mode=surfacesdl", f"--config={config}", f"--savepath={saves}", "nox-gameplay-test"]
    env = dict(os.environ, SDL_VIDEODRIVER="windows", SDL_AUDIODRIVER="dummy")
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
    invocations = []

    def run(name):
        if name != "warm":
            target_saves = Path(next(arg.split("=", 1)[1] for arg in command if arg.startswith("--savepath=")))
            (target_saves / ("nox-dialogue-lifecycle-cold.tsv" if args.lifecycle else "nox-dialogue-cold.tsv")).unlink(missing_ok=True)
        argv = [command[0], f"--logfile={root / (name + '-engine.log')}", *command[1:]]
        result = subprocess.run(argv, env=env, startupinfo=startup, capture_output=True, timeout=90)
        (root / (name + "-stdout.txt")).write_bytes(result.stdout)
        (root / (name + "-stderr.txt")).write_bytes(result.stderr)
        invocation = {"argv": argv, "exit": result.returncode}
        invocations.append(invocation)
        (root / (name + "-invocation.json")).write_text(json.dumps(invocation, indent=2) + "\n")
        assert all(sha(Path(path)) == digest for path, digest in originals.items()), "Original HDV changed during diagnostic"
        assert result.returncode == 0, f"Dialogue process failed: {root}"

    run("warm")
    report = (saves / "nox-dialogue.tsv").read_text().strip()
    assert report.startswith("PASS\tprovenance=authored-exit-adjacency-fixture")
    if args.lifecycle:
        assert (saves / "nox-dialogue-lifecycle.tsv").read_text().startswith("PASS\t"), "Full warm lifecycle did not finish"
    generations = sorted(saves.glob("nox-ce-*.g[01]"))
    assert len(generations) == (6 if args.lifecycle else 2)
    parsed = [read_save_enhancements(path) for path in generations]
    groups = {}
    for path, state in zip(generations, parsed):
        groups.setdefault(state["description"], []).append(state)
        sidecar = json.loads(path.with_name(path.name + ".ce-achievements.json").read_text())
        assert sidecar["client_playthrough_id"] == state["session"]
        assert sidecar["tainted"] == (state["description"] == "Observed dialogue fixture")
        if args.lifecycle:
            assert sidecar["progress"]["nox_m2_state_owner_probe"] == {"Before dialogue": 11, "Observed dialogue fixture": 22, "Fresh session B": 33}[state["description"]]
    for pair in groups.values():
        assert len(pair) == 2 and pair[0] == pair[1], "Enhancement state changed between two immediate saves"
    state = groups["Observed dialogue fixture"][0]
    if args.lifecycle:
        assert set(groups) == {"Before dialogue", "Observed dialogue fixture", "Fresh session B"}
        before, fresh = groups["Before dialogue"][0], groups["Fresh session B"][0]
        assert before["session"] == state["session"] != fresh["session"]
        assert before["sequence"] == fresh["sequence"] == 0 and not before["characters"] and not fresh["characters"]
        assert before["cells"] == fresh["cells"] and len(before["cells"]) == 2
        assert {(cell["x"], cell["y"]) for cell in before["cells"]} == {(24, 38), (25, 38)}
        assert all(cell["resource"] == "NA/DATA.MAP.KEEP" and cell["map_type"] == 8 and cell["map_id"] == 7 and cell["observed"] and not cell["revealed"] for cell in before["cells"])
        assert state["cells"] != before["cells"] and any(cell["resource"] == "NA/DATA.MAP.TOWN2" and cell["map_id"] == 46 for cell in state["cells"])
        assert before["saved_mode"] == state["saved_mode"] == ("Classic" if args.classic else "Modern")
        assert fresh["saved_mode"] == ("Modern" if args.classic else "Classic")
        assert before["slot"] == fresh["slot"] == 1 and state["slot"] == 2
        assert (saves / "nox-dialogue-lifecycle.tsv").read_text().startswith("PASS\t")
    assert state["description"] == "Observed dialogue fixture" and state["sequence"] == 9
    assert len(state["characters"]) == 1
    character = state["characters"][0]
    assert character["name"] == "Cutter"
    replies = character["replies"]
    assert [reply["input"] for reply in replies] == ["", "NAME", "ZZZZ", "JOB", "WORK", "DOCKS", "THIEVES", "OPERATION", "JOIN"]
    assert [reply["keyword"] for reply in replies] == ["", "NAME", "", "JOB", "JOB", "DOCK", "THIE", "OPER", "JOIN"]
    assert replies[-1]["response"] == "Let's go!" and not replies[2]["recognized"]
    assert len(character["topics"]) == 6 and all(topic["used"] for topic in character["topics"])
    knowledge = {}
    for row in (saves / "nox-dialogue-knowledge.tsv").read_text().splitlines():
        sequence, resource, record, key, used = row.split("\t")
        assert resource == character["resource"] and int(record) == character["record"]
        knowledge.setdefault(int(sequence), {})[key] = used == "1"
    assert "ZZZZ" not in knowledge[3]
    assert knowledge[4]["DOCK"] is False
    assert knowledge[6]["DOCK"] is True and knowledge[6]["THIE"] is False
    assert knowledge[7]["JOIN"] is False and knowledge[9]["JOIN"] is True
    # No topic may become visible merely because it was authored in a packet.
    spoken = {word.upper()[:4] for reply in replies for word in re.findall(r"[A-Za-z0-9]+", reply["response"])}
    resolved = {reply["keyword"] for reply in replies if reply["recognized"]}
    assert all(topic["key"] in spoken | resolved for topic in character["topics"])
    assert all(cell["observed"] and not cell["revealed"] for cell in state["cells"])
    (root / "observed-journal-private.json").write_text(json.dumps(state, indent=2) + "\n")
    ini = configparser.ConfigParser(interpolation=None); ini.read(config, encoding="utf-8")
    ini["nox-gameplay-test"]["nox_dialogue_reload"] = "true"
    ini["nox-gameplay-test"]["nox_classic_saves"] = "false" if args.classic else "true"
    with config.open("w", encoding="utf-8") as out: ini.write(out)
    hashes = {path.name: sha(path) for path in saves.glob("nox-ce-*")}
    run("cold")
    cold_report = (saves / ("nox-dialogue-lifecycle-cold.tsv" if args.lifecycle else "nox-dialogue-cold.tsv")).read_text().strip()
    assert cold_report.startswith("PASS\t") and "runtime_bytes=identical" in cold_report
    if args.lifecycle: assert "ce_progress_isolated=1" in cold_report and "assistance_reasons_isolated=1" in cold_report
    assert (saves / "nox-dialogue-saved.png").read_bytes() == (saves / ("nox-lifecycle-cold-after.png" if args.lifecycle else "nox-dialogue-cold.png")).read_bytes()
    if args.lifecycle:
        assert (saves / "nox-lifecycle-before.png").read_bytes() == (saves / "nox-lifecycle-cold-before.png").read_bytes()
        assert (saves / "nox-lifecycle-b.png").read_bytes() == (saves / "nox-lifecycle-cold-b.png").read_bytes()
        registry = json.loads((saves / "ce-playthroughs.json").read_text())["playthroughs"]
        assert len(registry) == 2
        statuses = {record["client_playthrough_id"]: record["casual"] for record in registry}
        assert statuses == {state["session"]: True, fresh["session"]: False}
        reason_owners = {record["client_playthrough_id"]: record["assistance_flags"] for record in registry}
        assert reason_owners[fresh["session"]] == []
        assert set(reason_owners[state["session"]]) == {"nox_controlled_dialogue_fixture", "tainted_save_loaded"}
        assert len(reason_owners[state["session"]]) == 2

    assert {path.name: sha(path) for path in saves.glob("nox-ce-*")} == hashes
    replay_events = 0
    if args.private_config:
        queue_file = saves / "ce-achievements-queue.json"
        acknowledged = json.loads(queue_file.read_text())
        assert acknowledged and all(row["state"] == "synced" for row in acknowledged)
        assert all(row["request_type"] != "achievement" for row in acknowledged), "No Nox achievements exist"
        for row in acknowledged: row["state"] = "pending"
        queue_file.write_text(json.dumps(acknowledged) + "\n")
        run("replay-cold")
        replayed = json.loads(queue_file.read_text())
        assert all(row["state"] == "synced" for row in replayed)
        events = {(row["request_type"], row["event_id"]):row for row in replayed}
        transport = {"state", "attempt_count", "last_attempt_at", "last_error"}
        for row in acknowledged:
            actual = events[(row["request_type"], row["event_id"])]
            assert all(actual[key] == value for key,value in row.items() if key not in transport), "Immutable event changed"
            assert actual["attempt_count"] > row["attempt_count"]
        replay_events = len(acknowledged)
        registry = json.loads((saves / "ce-playthroughs.json").read_text())["playthroughs"]
        assert {row["client_playthrough_id"]:row["casual"] for row in registry} == {state["session"]:True, fresh["session"]:False}
        reason_owners = {row["client_playthrough_id"]:row["assistance_flags"] for row in registry}
        assert set(reason_owners[state["session"]]) == {"nox_controlled_dialogue_fixture", "tainted_save_loaded"} and reason_owners[fresh["session"]] == []
        assert {path.name:sha(path) for path in saves.glob("nox-ce-*")} == hashes
    legacy_report = None
    if args.legacy_assistance:
        original_registry_hash = sha(saves / "ce-playthroughs.json")
        legacy_saves = root / "legacy-saves"
        shutil.copytree(saves, legacy_saves)
        legacy_registry = json.loads((legacy_saves / "ce-playthroughs.json").read_text())
        legacy_registry["version"] = 1
        for record in legacy_registry["playthroughs"]:
            record.pop("assistance_flags")
        (legacy_saves / "ce-playthroughs.json").write_text(json.dumps(legacy_registry) + "\n")
        # Cold runs advance immutable event IDs; copy their latest counters,
        # not the original warm configuration held in this Python object.
        ini.read(config, encoding="utf-8")
        ini["nox-gameplay-test"]["nox_assistance_legacy"] = "true"
        ini["scummvm"]["ce_achievements_assistance_flags"] = "another_session_only,mixed_legacy_reason"
        legacy_config = root / "legacy.ini"
        with legacy_config.open("w", encoding="utf-8") as out: ini.write(out)
        command = [str(exe), "--gfx-mode=surfacesdl", f"--config={legacy_config}", f"--savepath={legacy_saves}", "nox-gameplay-test"]
        original_events = json.loads((saves / "ce-achievements-queue.json").read_text())
        run("legacy-cold")
        assert "event id reused" not in (root / "legacy-cold-stderr.txt").read_text(), "Legacy fixture reused immutable event IDs"
        recovered_events = {row["event_id"]: row for row in json.loads((legacy_saves / "ce-achievements-queue.json").read_text())}
        assert all(recovered_events[row["event_id"]] == row for row in original_events), "Legacy repair changed an existing signed event"
        legacy_report = (legacy_saves / "nox-dialogue-lifecycle-cold.tsv").read_text().strip()
        assert "assistance_reasons_isolated=1" in legacy_report
        migrated = json.loads((legacy_saves / "ce-playthroughs.json").read_text())
        assert migrated["version"] == 2
        owners = {record["client_playthrough_id"]: record for record in migrated["playthroughs"]}
        assert set(owners[state["session"]]["assistance_flags"]) == {"legacy_assistance_history", "nox_controlled_dialogue_fixture", "tainted_save_loaded"}
        assert owners[state["session"]]["casual"] is True
        assert owners[fresh["session"]]["assistance_flags"] == [] and owners[fresh["session"]]["casual"] is False
        assert sha(saves / "ce-playthroughs.json") == original_registry_hash
        assert {path.name: sha(path) for path in legacy_saves.glob("nox-ce-*")} == hashes
    assert all(sha(Path(path)) == digest for path, digest in originals.items())
    evidence = {"status": "VERIFIED", "scope": "actual resolved NPC responses/keywords/recruitment and Engine cold persistence using assisted authored-exit/adjacency fixture; no earned campaign claim", "executable_sha256": sha(exe), "original_sha256": originals, "saved_mode": "Classic" if args.classic else "Modern", "provenance": "authored-exit-adjacency-fixture", "warm": report, "cold": cold_report, "replies": len(replies), "used_topics": len(character["topics"]), "physical_generations": len(generations), "invocations": invocations, "artifacts": str(root)}
    evidence["online_transport"] = bool(args.private_config)
    evidence["immutable_replayed_events"] = replay_events
    evidence["permanent_assistance_reasons"] = reason_owners if args.lifecycle else None
    evidence["legacy_assistance_migration"] = legacy_report
    evidence["older_save_session_lifecycle"] = bool(args.lifecycle)
    if args.lifecycle:
        evidence["scope"] = "older/newer observed journal and party restoration, two sessions with distinct modes, permanent Casual history versus older clean sidecar, exact cold worlds; assisted authored-exit NPC setup, no earned campaign claim"
    (root / "verification.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    main()
