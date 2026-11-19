#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the standard CE detector and native Nox player against a base v119 HDV."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ce"))
from test_cli_client import Client


IMAGE_SHA256 = "c42a95c7a356548c78957f98693e92d33c9bae3ff5126d9887c2980e5a490c3c"
WORLD_FIELDS = ("map_type", "map", "x", "y", "quest_fields", "party", "inventory")


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def press(client, key, ascii_code=0):
    for phase in ("down", "up"):
        client.command("input", kind="key", key=key, ascii=ascii_code, phase=phase)
        client.command("advance", frames=1)


def launch(client, game):
    client.launch(game, "apple2", game="nox", engine="nox")
    client.command("wait", field="adapter.stage", equals="title", timeout_ms=10000)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True,
                        help="Executable in a disposable portable installation with its DLLs")
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True,
                        help="New directory outside the game-data and installation directories")
    args = parser.parse_args()
    exe, image, output = (item.resolve() for item in (args.exe, args.image, args.artifacts))
    if sha256(image) != IMAGE_SHA256:
        parser.error("expected the original base v119 HDV")
    output.mkdir(parents=True, exist_ok=False)

    detection = subprocess.run(
        [str(exe), "--detect", f"--path={image.parent}"],
        cwd=exe.parent, text=True, capture_output=True, timeout=30, check=True,
    )
    (output / "detection.txt").write_text(detection.stdout, encoding="utf-8")
    assert "Nox Archaist" in detection.stdout, detection.stdout

    root = output / "profile"
    client = Client(exe, root, exe.parent)
    try:
        launch(client, image.parent)
        client.command("wait", field="adapter.music_ready", equals=True, timeout_ms=5000)
        client.command("wait", field="adapter.effects_played", equals=1, timeout_ms=5000)
        client.command("advance", frames=20)
        title = client.command("inspect")["adapter"]
        assert title["native_only"] and title["music_handle_active"], title
        assert title["music_nonzero_samples"] > 0 and title["effect_samples"] > 0, title

        press(client, 32, 32)
        press(client, 274)  # Down selects Start a New Game.
        press(client, 13, 13)
        client.command("assert", field="adapter.stage", equals="creation")
        client.type_text("CE SMOKE")
        for _ in range(4):
            press(client, 13, 13)
        for _ in range(20):
            state = client.command("inspect")["adapter"]
            if state["stage"] == "world":
                break
            assert state["stage"] == "story", state
            press(client, 13, 13)
        else:
            raise AssertionError("Opening story did not reach the world")
        assert state["name"] == "CE SMOKE", state
        assert tuple(state[field] for field in ("map_type", "map", "x", "y")) == (8, 7, 24, 38), state

        press(client, 32, 32)  # World Wait advances the original game clock.
        saved_state = client.command("inspect")["adapter"]
        assert (saved_state["hour"], saved_state["minute"]) != (state["hour"], state["minute"])
        client.command("screenshot")
        assert list((root / "screenshots").glob("*.png")), "Missing gameplay frame"
        client.command("save", slot=0, description="Nox CE integration smoke")
        assert any("slot000." in record["file"] for record in client.command("saves")["saves"])
    finally:
        client.close()

    client = Client(exe, root, exe.parent, transcript=output / "cold.jsonl")
    try:
        launch(client, image.parent)
        record = next(record for record in client.command("saves")["saves"]
                      if "slot000." in record["file"])
        client.command("load", chooser_slot=record["chooser_slot"])
        restored = client.command("inspect")["adapter"]
        assert restored["stage"] == "world", restored
        assert all(restored[field] == saved_state[field] for field in WORLD_FIELDS)
    finally:
        client.close()

    assert sha256(image) == IMAGE_SHA256
    receipt = {
        "binary_sha256": sha256(exe),
        "image_sha256": IMAGE_SHA256,
        "checks": ["detection", "native title", "title music and effects", "New Game",
                   "world Wait", "world frame", "save", "quit", "cold restore"],
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("PASS " + ", ".join(receipt["checks"]))


if __name__ == "__main__":
    main()
