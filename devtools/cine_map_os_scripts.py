#!/usr/bin/env python3
"""Build a static Operation Stealth script atlas from decompiled Cine dumps.

This helper reads the text files produced by devtools/cine_extract_os_scripts.py
and extracts the structural facts that matter for achievement design:
loaded resources, room transitions, messages, script calls, and object/global
state changes. It intentionally stays descriptive; it does not infer achievement
hooks on its own.
"""

from __future__ import annotations

import argparse
import builtins
import struct
import json
import re
from collections import defaultdict
from pathlib import Path


SCRIPT_RE = re.compile(r"^(?P<base>.+)_(?P<index>\d{3})\.txt$")
REL_PARAMS_RE = re.compile(r"// REL params: p1=(?P<p1>-?\d+) p2=(?P<p2>-?\d+) p3=(?P<p3>-?\d+)")
OP_RE = re.compile(r"^\s*(?P<offset>[0-9A-Fa-f]{4}):\s*(?P<op>.*)$")
LOAD_RE = re.compile(r"\b(?P<op>loadBg|loadCt|loadABS|loadPart|loadPrc|changeDataDisk)\((?P<args>[^)]*)\)")
MESSAGE_RE = re.compile(r"\bmessage\((?P<id>-?\d+),(?P<args>[^)]*)\)")
GLOBAL_WRITE_RE = re.compile(r"\bglobalVar\[(?P<var>\d+)\]\s*=\s*(?P<value>-?\d+|var\[\d+\]|globalVar\[\d+\])")
OBJECT_STATUS_RE = re.compile(r"\bobj\[(?P<object>\d+)\]\.status\s*=\s*(?P<value>-?\d+|var\[\d+\]|globalVar\[\d+\])")
GLOBAL_ACCESS_RE = re.compile(r"\bglobalVar\[(?P<var>\d+)\]")
OBJECT_STATUS_ACCESS_RE = re.compile(r"\bobj\[(?P<object>\d+)\]\.status\b")
OBJECT_ACCESS_RE = re.compile(r"\bobj\[(?P<object>\d+)\]\.(?P<field>[A-Za-z_][A-Za-z0-9_]*)\b")
START_SCRIPT_RE = re.compile(r"\bstartGlobalScript\((?P<script>\d+)\)")
END_SCRIPT_RE = re.compile(r"\bendGlobalScript\((?P<script>\d+)\)")
COLLISION_RE = re.compile(r"\bcheckCollision\((?P<args>[^)]*)\)")
BG_RE = re.compile(r"\bloadBg\((?P<bg>[^)]*)\)")
MSG_LINE_RE = re.compile(r"^\[(?P<id>\d+)\]\s*(?P<text>.*)$")
OBJ_ENTRY_SIZE = 32
OBJECT_GROUP_FALLBACKS = {
    "PROCS03": ["PROCS02"],
    "PROCS04": ["PROCS02"],
    "PROCS06": ["PROCS02"],
    "PROCS07": ["PROCS02"],
}
ACTION_LABELS = {
    0: "examine/look",
    1: "take",
    3: "use/apply",
    4: "operate",
    5: "speak",
    6: "special/on-screen action",
}
REPEATED_GROUP_NOTES = [
    {
        "groups": ["PROCS03", "PROCS04", "PROCS07"],
        "canonical": "PROCS03",
        "note": "These groups all load VILLE.REL / VILLE.MSG town interaction scripts. Treat PROCS03 as the first static pass, then check PROCS04/PROCS07 when a watcher must be phase-aware.",
    },
    {
        "groups": ["PROCS06"],
        "canonical": None,
        "note": "GROTTES.REL reuses several town-style object IDs and message numbers after the ambush. Cross-table message/object candidates from VILLE are leads, not proof.",
    },
]
BEAT_DEFINITIONS = [
    {
        "id": "airport_customs",
        "title": "Airport Customs",
        "status": "mapped",
        "description": "The opening airport passport and customs acceptance/failure branches.",
        "script_refs": [
            ("rel", "PROCS02", "AEROPORT.REL", 55),
            ("rel", "PROCS02", "AEROPORT.REL", 56),
            ("rel", "PROCS02", "AEROPORT.REL", 57),
            ("rel", "PROCS02", "AEROPORT.REL", 68),
            ("rel", "PROCS02", "AEROPORT.REL", 69),
            ("rel", "PROCS02", "AEROPORT.REL", 70),
            ("prc", "PROCS02", "AIRPORT.PRC", 46),
        ],
        "object_ids": [5, 10, 41, 42],
        "notes": [
            "Acceptance text is centered on message 37; nationality branches use French, English, and German passport scripts.",
            "Baggage follow-up scripts 68/69/70 include the secondary acceptance and Martinez bag branches.",
        ],
    },
    {
        "id": "razor_briefing",
        "title": "Razor Briefing",
        "status": "mapped",
        "description": "The electric razor recording that directs John to Las Mimosas Park.",
        "script_refs": [
            ("rel", "PROCS02", "AEROPORT.REL", 95),
        ],
        "object_ids": [46, 47, 23],
        "notes": [
            "AEROPORT.REL_095 plays messages 143-154 and sets globalVar[16] = 1.",
        ],
    },
    {
        "id": "park_contact",
        "title": "Park Contact And Escape",
        "status": "mapped-runtime-validated",
        "description": "The red-carnation park contact, card/key handoff, and traced left/base escape path.",
        "script_refs": [
            ("rel", "PROCS02", "AEROPORT.REL", 122),
            ("rel", "PROCS03", "VILLE.REL", 121),
            ("prc", "PROCS02", "AIRPORT.PRC", 13),
            ("prc", "PROCS02", "AIRPORT.PRC", 19),
            ("rel", "PROCS03", "VILLE.REL", 133),
            ("prc", "PROCS02", "AIRPORT.PRC", 24),
            ("prc", "PROCS02", "AIRPORT.PRC", 22),
        ],
        "object_ids": [57, 64, 65, 66, 67, 88],
        "notes": [
            "AIRPORT.PRC_019 waits on globalVar[16], globalVar[11], and globalVar[10], then sets obj[65].status = -3.",
            "Runtime trace showed the successful escape after the drive-by uses AIRPORT.PRC_013 line 271, writing globalVar[240] = 43 / globalVar[241] = 1 from park background 22.PI1 to 43J.PI1.",
            "The validated achievement guard state at that exit is globalVar[11] = 1, obj[88].status = 3, and obj[65].status = -3.",
            "AIRPORT.PRC_024 exits screen 34 to 371:3 when obj[88].status == 3, but a live trace of the successful drive-by escape did not execute that transition; keep it as an adjacent static lead, not this achievement hook.",
            "Validated production watcher: AIRPORT.PRC_013 line 271, globalVar[240] = 43, gated by the post-contact state above.",
        ],
    },
    {
        "id": "bank_ambush",
        "title": "Bank Device And Ambush",
        "status": "mapped-runtime-validated",
        "description": "The bank/card branch that retrieves the device/envelope and reaches the ambush transition.",
        "script_refs": [
            ("rel", "PROCS03", "VILLE.REL", 114),
            ("rel", "PROCS03", "VILLE.REL", 154),
            ("rel", "PROCS03", "VILLE.REL", 157),
            ("rel", "PROCS03", "VILLE.REL", 158),
            ("rel", "PROCS03", "VILLE.REL", 159),
            ("rel", "PROCS03", "VILLE.REL", 160),
            ("rel", "PROCS03", "VILLE.REL", 161),
            ("rel", "PROCS03", "VILLE.REL", 162),
            ("rel", "PROCS03", "VILLE.REL", 163),
            ("rel", "PROCS03", "VILLE.REL", 164),
            ("prc", "PROCS02", "AIRPORT.PRC", 20),
            ("rel", "PROCS03", "VILLE.REL", 188),
        ],
        "object_ids": [6, 60, 61, 62, 65, 66, 67, 77, 78, 79, 98],
        "notes": [
            "VILLE.REL_114 is the bank-teller money exchange; it gives coins by setting obj[61].status or obj[62].status to -3 and can exhaust the bank notes with obj[6].status = 100.",
            "VILLE.REL_154 turns the park-contact information into the key state: it sets globalVar[12] = 1, obj[65].status = 100, obj[66].status = 100, and obj[67].status = -3.",
            "VILLE.REL_157 uses key object 67 on briefcase object 76, opens the agent briefcase, sets obj[67].status = 100, and creates briefcase object 77, envelope object 78, and little-box/device object 79.",
            "VILLE.REL_161 is only the device pickup: taking object 79 displays message 238 and sets obj[79].status = -3.",
            "VILLE.REL_163 is the envelope-take branch: message 236, globalVar[241] = 101, globalVar[240] = 26, fade, and startGlobalScript(2).",
            "VILLE.REL_164 is the envelope-open branch: message 241, message 242, then the same 26:101 ambush transition.",
            "AIRPORT.PRC_020 is the post-ambush scene at room 26: it displays the Colonel/Karpov/Ostrovitch dialogue, resets early inventory objects to status 100, and proceeds toward the mine/cave part.",
            "Validated production watcher: VILLE.REL_163 line 16 or VILLE.REL_164 line 27 writing globalVar[240] = 26 with globalVar[241] = 101, gated by obj[79].status = -3 so the little-box/device has been retrieved. Avoid firing on bank money exchange, key creation, briefcase opening, envelope examine, envelope-only transition, or little-box pickup alone.",
        ],
    },
    {
        "id": "escape_clause",
        "title": "Escape Clause",
        "status": "mapped-runtime-validated-candidate",
        "description": "The mine/cave escape chain: free John, recover the pickaxe, open the exit, and walk through to the flooded-water sequence.",
        "script_refs": [
            ("prc", "PROCS02", "AIRPORT.PRC", 18),
            ("rel", "PROCS06", "GROTTES.REL", 165),
            ("rel", "PROCS06", "GROTTES.REL", 166),
            ("rel", "PROCS06", "GROTTES.REL", 171),
            ("rel", "PROCS06", "GROTTES.REL", 0),
            ("rel", "PROCS06", "GROTTES.REL", 1),
            ("rel", "PROCS06", "GROTTES.REL", 2),
            ("rel", "PROCS06", "GROTTES.REL", 3),
            ("rel", "PROCS06", "GROTTES.REL", 4),
            ("rel", "PROCS06", "GROTTES.REL", 188),
            ("rel", "PROCS06", "GROTTES.REL", 210),
        ],
        "object_ids": [80, 81, 82, 83, 88, 98, 174, 176],
        "notes": [
            "GROTTES.REL_166 exposes the metal/pickaxe progression: obj[81].status 100 then 200, and obj[82].status -3 for the obtained pickaxe.",
            "GROTTES.REL_171 frees John from the ropes and sets obj[83].status = 100.",
            "GROTTES.REL_179 can set obj[88].status = 3 while moving through the cave/floor-depth path and can transition to encoded destination 371:1; this is positioning, not the escape completion.",
            "GROTTES.REL_188 is the final operate branch for door/exit object 98. It requires obj[88].status == 3 and obj[8].status != -3; otherwise it displays message 213, 'I don't see anything special.'",
            "On the success path, GROTTES.REL_188 displays message 36, 'IT'S OPENING, JOHN!', sets obj[98].frame = 103 / costume = 9, preserves carried inventory in globals, clears old inventory statuses including obj[82].status = 100, closes the current part, changes to disk 3, loads SD02, and loads the next captive-room / flooded-water resource set: CHAMBRE.PRC plus the internally named BATEAU.REL / BATEAU.OBJ / BATEAU.MSG.",
            "Runtime trace of the player walking through the finished opening showed the actual achievement boundary in AIRPORT.PRC_018: line 996 writes globalVar[240] = 28 while globalVar[241] is still 2, line 1001 sets globalVar[241] = 1, and line 1006 starts dispatcher script 2. This moves from cave background 27.PI1 to the flooded sequence 28:1.",
            "A production watcher should use AIRPORT.PRC_018 line 996 with GROTTES.REL / GROTTES.MSG active and globalVar[241] still 2. Treat GROTTES.REL_188 message 36 as the opening action, not the completion boundary. Avoid firing on freeing the ropes, pickaxe discovery/take, blocked-rock messages 208-210, breeze/rock-wall examine messages 211-212, floor-depth movement obj[88].status changes, fallback message 213, or generic room-28 references in other parts.",
        ],
    },
    {
        "id": "two_for_the_surface",
        "title": "Two for the Surface",
        "status": "mapped-static-production-hook",
        "description": "The flooded-water escape, zodiac pickup, and post-rescue scene where John and Julia are safely together.",
        "script_refs": [
            ("prc", "PROCS08", "BATEAU7.PRC", 2),
            ("prc", "PROCS08", "BATEAU7.PRC", 3),
            ("prc", "PROCS08", "BATEAU7.PRC", 9),
            ("prc", "PROCS08", "BATEAU7.PRC", 10),
            ("prc", "PROCS08", "BATEAU7.PRC", 11),
            ("prc", "PROCS08", "BATEAU7.PRC", 12),
            ("prc", "PROCS08", "BATEAU7.PRC", 13),
            ("prc", "PROCS08", "BATEAU7.PRC", 18),
            ("prc", "PROCS08", "BATEAU7.PRC", 19),
            ("prc", "PROCS08", "BATEAU7.PRC", 20),
            ("prc", "PROCS08", "BATEAU7.PRC", 21),
            ("prc", "PROCS08", "BATEAU7.PRC", 22),
            ("prc", "PROCS08", "BATEAU7.PRC", 23),
            ("prc", "PROCS08", "BATEAU7.PRC", 27),
            ("prc", "PROCS08", "BATEAU7.PRC", 28),
            ("prc", "PROCS08", "BATEAU7.PRC", 29),
            ("prc", "PROCS08", "BATEAU7.PRC", 45),
        ],
        "object_ids": [1, 2, 7, 8, 9, 10, 11, 20, 21, 22, 23, 24, 51, 99, 100, 201, 202],
        "notes": [
            "BATEAU7.PRC_002 dispatches room 39 to script 009, room 41 to script 027, and room 42 to script 045.",
            "BATEAU7.PRC_009 is the underwater sequence: it loads 39.PI1 / 40.PI1, starts swim and hazard helpers, starts drowning failure script 013, and only reaches the success handoff when John's frame becomes -41; then it exits to room 41:1.",
            "BATEAU7.PRC_013 is the drowning/failure loop: it displays messages 8, 9, and then repeats message 10 forever after ending the underwater helper scripts.",
            "BATEAU7.PRC_027 is the rescue/surface pickup: it loads 41_1.PI1, starts helper scripts 28 and 29 to move John, Julia, and the Zodiac together, displays message 20, boards John and Julia onto overlay objects 201 and 202, then exits to room 42:1.",
            "BATEAU7.PRC_045 is the safe post-rescue briefing: message 21 directly addresses Julia's dangerous solo action, messages 22-29 set up the palace infiltration, then the scene proceeds to room 43:1.",
            "Implemented production watcher: BATEAU7.PRC_027 line 623 writing globalVar[240] = 42 while globalVar[241] = 1, after message 20 and after John and Julia board Zodiac overlay objects 201 and 202. Avoid firing on underwater entry, underwater survival before the Zodiac arrives, room 41 setup, or the drowning failure script 013.",
        ],
    },
    {
        "id": "palace_intrigue",
        "title": "Palace Intrigue",
        "status": "mapped-static-production-hook",
        "description": "The palace infiltration path through the labyrinth and chase sequence into the office/safe room.",
        "script_refs": [
            ("prc", "PROCS08", "BATEAU7.PRC", 46),
            ("prc", "PROCLABY", "LABY.PRC", 1),
            ("prc", "PROCLABY", "LABY.PRC", 9),
            ("prc", "PROCLABY", "LABY.PRC", 45),
            ("prc", "PROCS10", "PALAIS1.PRC", 1),
            ("prc", "PROCS10", "PALAIS1.PRC", 2),
            ("prc", "PROCS10", "PALAIS1.PRC", 3),
            ("prc", "PROCS10", "PALAIS1.PRC", 13),
            ("prc", "PROCS10", "PALAIS1.PRC", 15),
            ("rel", "PROCS10", "PALAIS.REL", 0),
            ("rel", "PROCS10", "PALAIS.REL", 4),
            ("prc", "PROCS10", "PALAIS1.PRC", 40),
            ("prc", "PROCS10", "PALAIS1.PRC", 42),
            ("prc", "PROCS10", "PALAIS1.PRC", 18),
            ("prc", "PROCS10", "PALAIS1.PRC", 19),
            ("prc", "PROCS10", "PALAIS1.PRC", 20),
            ("prc", "PROCS10", "PALAIS1.PRC", 21),
            ("prc", "PROCS10", "PALAIS1.PRC", 22),
        ],
        "object_ids": [1, 120, 129, 130, 133, 140],
        "notes": [
            "BATEAU7.PRC_046 brings the party to the palace edge and loads PALAIS1.PRC / PALAIS.REL.",
            "LABY.PRC_009 completes labyrinth level 3 when globalVar[229] becomes 9, reloads SD02, sets globalVar[240] = 46, and loads the palace resources; this is palace entry, not office arrival.",
            "PALAIS1.PRC_013 and PALAIS1.PRC_015 cover the hall/door rooms 46 and 47; PALAIS.REL_000 opens the hall door and PALAIS.REL_004 manipulates the statue arm into the safe/confrontation branch.",
            "PALAIS1.PRC_040 / 042 stage the confrontation and move into room 48; PALAIS1.PRC_018 advances to room 49 and PALAIS1.PRC_021 sets globalVar[240] = 50, globalVar[241] = 2, and dispatches script 2.",
            "PALAIS1.PRC_022 loads 50.PI1, the office/safe room. Its normal first-entry branch runs when globalVar[243] != -50 and initializes the room before any safe-envelope recovery.",
            "PALAIS1.PRC_031 also returns to room 50, but sets globalVar[243] = -50; the matching PALAIS1.PRC_022 branch displays messages 29/30, sets obj[164].status = 100, and belongs to A Safe Bet / the next submarine handoff.",
            "Trace-validated production watcher: LABY.PRC_009 line 482 writes globalVar[240] = 46 after LABY.PRC_045 line 176 set globalVar[229] = 9. That branch loads PALAIS1.PRC / PALAIS.REL / PALAIS.OBJ / PALAIS.MSG; PALAIS1.PRC_001 then routes to 46:2 with globalVar[243] = 44 and PALAIS1.PRC_013 loads 46.PI1. Avoid PALAIS.REL_000 message 12 (IT'S OPENING!!!), which is an earlier hotel/hall-door action, and avoid PALAIS1.PRC_022 / globalVar[243] == -50 envelope branch.",
        ],
    },
    {
        "id": "a_safe_bet",
        "title": "A Safe Bet",
        "status": "mapped-runtime-production-hook",
        "description": "The palace safe puzzle: place and arm the decoder box, set the four safe digits, open the safe, and recover the envelope/documents.",
        "script_refs": [
            ("prc", "PROCS10", "PALAIS1.PRC", 22),
            ("prc", "PROCS10", "PALAIS1.PRC", 32),
            ("prc", "PROCS10", "PALAIS1.PRC", 39),
            ("rel", "PROCS10", "PALAIS.REL", 5),
            ("rel", "PROCS10", "PALAIS.REL", 6),
            ("rel", "PROCS10", "PALAIS.REL", 7),
            ("rel", "PROCS10", "PALAIS.REL", 8),
            ("rel", "PROCS10", "PALAIS.REL", 9),
            ("rel", "PROCS10", "PALAIS.REL", 10),
            ("rel", "PROCS10", "PALAIS.REL", 11),
            ("rel", "PROCS10", "PALAIS.REL", 23),
            ("rel", "PROCS10", "PALAIS.REL", 24),
            ("rel", "PROCS10", "PALAIS.REL", 25),
            ("rel", "PROCS10", "PALAIS.REL", 26),
        ],
        "object_ids": [121, 122, 123, 124, 126, 127, 150, 151, 152, 153, 159, 164],
        "notes": [
            "PALAIS.REL_005 places the little box on the safe door, creates the validation button object, and sets obj[124].status = -1.",
            "PALAIS.REL_009 and PALAIS.REL_026 toggle the decoder box; the armed/on state is obj[124].status = -2, and toggling off returns it to obj[124].status = -1 while revealing envelope object 164.",
            "PALAIS.REL_006 / 023 and PALAIS.REL_007 / 024 are the up/down button scripts for objects 121 and 122, adjusting digit objects 150-153 according to the selected slot in globalVar[123].",
            "PALAIS1.PRC_039 chooses randomized target frames for the four digits in globalVar[150-153]; PALAIS1.PRC_032 watches displayed digit frames and sets globalVar[124] = -99 when all four match.",
            "PALAIS.REL_010 removes the box; removing it while still armed goes to the explosion/failure branch, otherwise it sets obj[124].status = -3, which is required before opening the safe.",
            "PALAIS.REL_008 and PALAIS.REL_025 open the safe only when globalVar[123] == -1, globalVar[124] == -99, and obj[124].status == -3; success displays message 17, while the failure path counts down through messages 33-38 and ends with message 32.",
            "PALAIS.REL_011 marks the envelope taken with obj[164].status = -3 and sets globalVar[243] = -1.",
            "PALAIS1.PRC_031 is the fight/escape interlude after the envelope, displaying messages 54/55 and returning to room 50 with globalVar[243] = -50.",
            "PALAIS1.PRC_022 then takes the globalVar[243] == -50 branch, displays messages 29/30, sets obj[164].status = 100, stores prior inventory states in globals 1-10, and loads SOUSMAR2.PRC / SOUSMARI resources.",
            "Trace-validated production watcher: PALAIS.REL_011 line 2 changes envelope object 164 status from -1 to -3 when the player takes it from the safe in COFFRE.PI1; line 8 then sets globalVar[243] = -1. Avoid safe-open message 17, solved-combination state alone, countdown/explosion messages 32-38, and the later documents/submarine handoff.",
        ],
    },
    {
        "id": "making_waves",
        "title": "Making Waves",
        "status": "mapped-runtime-production-hook",
        "description": "Complete both parts of the jet-ski chase: catch the envelope thief after dodging sharks, then survive the pursuing guards until the submarine pickup.",
        "script_refs": [
            ("prc", "PROCS10", "PALAIS1.PRC", 22),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 2),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 4),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 5),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 6),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 7),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 8),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 12),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 13),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 14),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 15),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 20),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 21),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 22),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 23),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 24),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 29),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 30),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 31),
            ("rel", "PROCS12", "SOUSMARI.REL", 0),
            ("rel", "PROCS12", "SOUSMARI.REL", 2),
            ("rel", "PROCS12", "SOUSMARI.REL", 7),
        ],
        "object_ids": [1, 16, 50, 51, 65, 66, 70, 71, 72, 73, 76, 77, 78, 79, 80, 81, 82, 83, 100, 101, 102, 103, 104, 105, 110, 111, 130, 131, 132, 133, 164],
        "notes": [
            "Trace showed the user stopped when the submarine appeared; the trace was still in PALAIS1.PRC / PALAIS.REL on 50.PI1, not SOUSMAR2.PRC. PALAIS1.PRC_022 was moving submarine objects 110 and 111 upward while globalVar[240] == 50, globalVar[243] == -1, and envelope object 164 was still carried.",
            "Trace-backed production watcher: PALAIS1.PRC_022 message 29 at line 393, 'THE DOCUMENTS ARE FINALLY YOURS!', after the submarine pickup animation and before the handoff to SOUSMAR2 resources.",
            "SOUSMAR2.PRC_002 is the dispatcher for the sequence, mapping globalVar[240] values 53, 541, 542, 543, 387-393, and 55 to the relevant room/control scripts.",
            "SOUSMAR2.PRC_004 starts the jet-ski chase after the envelope is stolen, initializes the chase inventory/state, starts helpers 9 and 24, and transitions to destination 387.",
            "SOUSMAR2.PRC_005 / 006 / 007 are the later visible chase nodes 541.PI1, 542.PI1, and 543.PI1; SOUSMAR2.PRC_008 / 012 / 013 / 014 / 015 / 029 / 030 are companion chase control nodes, not a walkable room maze.",
            "The early chase makes the player dodge sharks while catching the thief; after the thief segment, the second phase has guards on jet skis pursuing the player until the submarine pickup.",
            "SOUSMARI.REL_000 opens the palm-trunk entrance and sets obj[81].status = -1; SOUSMARI.REL_002 operates the button, sets obj[81].status = -99, and sets globalVar[43] = 1 after the shark/electric diversion path completes.",
            "SOUSMAR2.PRC_007 reaches destination 55 only from the right edge of 543.PI1 when globalVar[43] == 1 and John's Y position is between 120 and 130; it then sets globalVar[240] = 55 and dispatches script 2. This appears to be the successful end of the guard-pursuit phase and submarine pickup boundary.",
            "SOUSMAR2.PRC_002 maps globalVar[240] == 55 to SOUSMAR2.PRC_020. SOUSMAR2.PRC_020 displays message 45 about the passage closing, water lowering, and the decompressor, then loads 55.PI1 / 55_CT.PI1 and allows interaction again after pickup.",
            "SOUSMARI.REL_007 operates decompressor object 130, displays message 46, and starts SOUSMAR2.PRC_031; SOUSMAR2.PRC_031 loads DOUCHE6.PRC / DOUCHE.REL / DOUCHE.OBJ / DOUCHE.MSG for the next part.",
            "Do not use SOUSMAR2.PRC_020 message 45 for Making Waves; that is after the submarine pickup and belongs to the next sequence boundary. Treat SOUSMARI.REL_007 -> SOUSMAR2.PRC_031 as the follow-on base handoff for Under New Management, and keep this separate from optional underwater objects, shark/death scripts, and the prior EU VGA jet-ski corruption workaround.",
        ],
    },
    {
        "id": "under_new_management",
        "title": "Under New Management",
        "status": "implemented-trace-informed",
        "description": "Enter the hidden underwater base and reach the opening Dr. Why / piranha-trap confrontation.",
        "script_refs": [
            ("prc", "PROCS12", "SOUSMAR2.PRC", 31),
            ("prc", "PROCS13", "DOUCHE6.PRC", 1),
            ("prc", "PROCS13", "DOUCHE6.PRC", 2),
            ("prc", "PROCS13", "DOUCHE6.PRC", 15),
            ("prc", "PROCS13", "DOUCHE6.PRC", 37),
            ("prc", "PROCS13", "DOUCHE6.PRC", 38),
            ("rel", "PROCS13", "DOUCHE.REL", 99),
            ("rel", "PROCS13", "DOUCHE.REL", 100),
            ("rel", "PROCS13", "DOUCHE.REL", 101),
            ("rel", "PROCS13", "DOUCHE.REL", 102),
            ("rel", "PROCS13", "DOUCHE.REL", 103),
            ("rel", "PROCS13", "DOUCHE.REL", 104),
            ("rel", "PROCS13", "DOUCHE.REL", 105),
            ("rel", "PROCS13", "DOUCHE.REL", 108),
            ("rel", "PROCS13", "DOUCHE.REL", 114),
            ("rel", "PROCS13", "DOUCHE.REL", 116),
        ],
        "object_ids": [1, 3, 27, 28, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 81, 83, 86, 150, 151, 152, 154, 156],
        "notes": [
            "SOUSMAR2.PRC_031 is the handoff from the decompressor sequence into the base resource set; after the 56.PI1 transition it stores carried inventory state, clears the diving/approach inventory to status 100, and loads DOUCHE6.PRC / DOUCHE.REL / DOUCHE.OBJ / DOUCHE.MSG.",
            "A trace of opening the porthole, climbing the ladder, and entering the base reached SOUSMAR2.PRC_035 message 49 ('Hello, Mr. Glames!!! How have you been enjoying our company up until now?') and then sets globalVar[200] = 1, which allows SOUSMAR2.PRC_031 to continue into the base load.",
            "DOUCHE6.PRC_001 initializes the PROCS13 state from saved inventory globals, changes to disk 3, loads SD02, sets globalVar[199] = 0, and forces globalVar[240] = 8 unless resuming room 10 or 59.",
            "DOUCHE6.PRC_002 dispatches globalVar[240] = 8 to DOUCHE6.PRC_015, making scene 8 the first normal base entry after the decompressor handoff.",
            "DOUCHE6.PRC_015 loads 08.PI1 / 08_CT.PI1, disallows player input, creates the door, remote control, and winch-control objects, resets globalVar[20] = 0, and starts scripts 37 and 38 before the player can act.",
            "DOUCHE6.PRC_037 performs the opening hidden-base confrontation: it places John, the Spyder agent, soldiers, cage, rope, lock, and automatic doors, sets globalVar[20] = 48, displays messages 97-101 from Dr. Why, then sets globalVar[20] = 1 before restoring control.",
            "DOUCHE6.PRC_038 marks the piranha-trap state, initializes the rope overlay, sets globalVar[110] = 0 for John still in the cage, and handles the piranha failure loop/message 188 if the player does not escape.",
            "The nearby DOUCHE.REL piranha/cage examine and operate scripts document the trapped state, while the escape solution belongs to the later pen_mightier_than_piranhas beat.",
            "The production watcher emits underwater_base_entered at SOUSMAR2.PRC_035 line 2251, message 49, while SOUSMARI.MSG is active and globalVar[240] == 55. This is the visible Dr. Why welcome dialogue before the PROCS13 base load. Avoid porthole-open message 46, the later DOUCHE6.PRC_037 message 97 ambush line, globalVar[20] = 20, EGOUBASE loads, later escape solution actions, or later infiltration-room actions for this beat.",
        ],
    },
    {
        "id": "pen_mightier_than_piranhas",
        "title": "The Pen Is Mightier Than the Piranhas",
        "status": "implemented-trace-validated",
        "description": "Escape the piranha cage using the pen acid and the watch cord.",
        "script_refs": [
            ("prc", "PROCS13", "DOUCHE6.PRC", 15),
            ("prc", "PROCS13", "DOUCHE6.PRC", 37),
            ("prc", "PROCS13", "DOUCHE6.PRC", 38),
            ("prc", "PROCS13", "DOUCHE6.PRC", 39),
            ("prc", "PROCS13", "DOUCHE6.PRC", 45),
            ("rel", "PROCS13", "DOUCHE.REL", 99),
            ("rel", "PROCS13", "DOUCHE.REL", 110),
            ("rel", "PROCS13", "DOUCHE.REL", 111),
            ("rel", "PROCS13", "DOUCHE.REL", 112),
            ("rel", "PROCS13", "DOUCHE.REL", 113),
            ("rel", "PROCS13", "DOUCHE.REL", 114),
        ],
        "object_ids": [1, 10, 11, 70, 71, 72, 75, 76, 77, 78, 79, 80, 83, 86, 150, 151, 152, 154, 156],
        "notes": [
            "DOUCHE6.PRC_037 / 038 create the piranha-trap state: John is in the cage, the cage/rope objects are active, globalVar[20] moves from 48 to 1, and the failure loop drops him into PYRANHAS.PI1/message 188 if he does not escape.",
            "DOUCHE.REL_110 is use/apply Pen (object 10) on Lock (object 76). It requires John to be in the cage and obj[71].frame == 83, resets globalVar[111] and globalVar[112], displays messages 189 and 91, moves the cage, changes obj[71].frame to 84, and removes the lock/door masks.",
            "DOUCHE.REL_111 is use/apply Watch (object 11) on the right wall (object 77); DOUCHE.REL_112 is use/apply Watch on the left wall (object 78). Each requires obj[71].frame == 84 and globalVar[110] == 0, so neither can happen before the pen-acid lock step.",
            "The first watch-wall action creates cord object 79 or 80 and sets globalVar[111] or globalVar[112]. The second matching action displays message 214, sets globalVar[110] = 1, cancels the sibling script, and starts DOUCHE6.PRC_039.",
            "DOUCHE6.PRC_039 animates John on the watch cord, contains both failure branches into PYRANHAS.PI1/message 188, and only starts DOUCHE6.PRC_045 after John has moved around the basin rather than falling in.",
            "DOUCHE.REL_113 operates the grill/exit object 83 only when John is on the upper-right wire position (Y <= 50 and X >= 249); it ends script 39, animates the final movement, and sets globalVar[20] = 20.",
            "DOUCHE6.PRC_015 watches globalVar[20]; when it becomes 20, it closes the PROCS13 part and loads EGOUBASE / EGOU.PRC / LABY.REL / LABY.OBJ / LABY.MSG.",
            "Trace validation for the successful cage escape showed earlier DOUCHE.REL_113 grill operations ending without state change when John was not at the upper-right exit position. The accepted run reached DOUCHE.REL_113 line 188 and wrote globalVar[20] from 1 to 20 before continuing to EGOU.PRC.",
            "The production watcher emits piranha_cage_escaped at DOUCHE.REL_113 line 188 when globalVar[20] changes from 1 to 20. The subsequent DOUCHE6.PRC_015 load of EGOUBASE is confirmation, not the primary boundary. Avoid firing on Dr. Why intro, the first pen use, the first watch cord, message 214 alone, DOUCHE6.PRC_039 start alone, failed/too-early grill operations, or piranha failure branches.",
        ],
    },
    {
        "id": "rats_all_folks",
        "title": "Rats All, Folks!",
        "status": "implemented-trace-validated",
        "description": "Clear the rat mazes inside the hidden underwater base.",
        "script_refs": [
            ("prc", "PROCEGOU", "EGOU.PRC", 1),
            ("prc", "PROCEGOU", "EGOU.PRC", 6),
            ("prc", "PROCEGOU", "EGOU.PRC", 7),
            ("prc", "PROCEGOU", "EGOU.PRC", 8),
            ("prc", "PROCEGOU", "EGOU.PRC", 9),
            ("prc", "PROCEGOU", "EGOU.PRC", 23),
            ("prc", "PROCEGOU", "EGOU.PRC", 25),
            ("prc", "PROCEGOU", "EGOU.PRC", 30),
            ("prc", "PROCEGOU", "EGOU.PRC", 31),
            ("prc", "PROCEGOU", "EGOU.PRC", 32),
            ("prc", "PROCEGOU", "EGOU.PRC", 33),
            ("prc", "PROCEGOU", "EGOU.PRC", 34),
            ("prc", "PROCEGOU", "EGOU.PRC", 35),
            ("prc", "PROCEGOU", "EGOU.PRC", 36),
            ("prc", "PROCEGOU", "EGOU.PRC", 37),
            ("prc", "PROCEGOU", "EGOU.PRC", 45),
            ("prc", "PROCEGOU", "EGOU.PRC", 46),
        ],
        "object_ids": [1, 191, 199, 200, 201, 202, 203, 204, 205, 206, 213, 215, 216, 217, 218, 219, 220, 221, 222, 245, 246],
        "notes": [
            "After the piranha-cage escape, DOUCHE6.PRC_015 loads EGOUBASE and EGOU.PRC / LABY.REL / LABY.MSG while globalVar[20] is 20.",
            "EGOU.PRC_006 through _009 are the rat maze levels. Script 9 initializes the fourth map (EGOUMAP3.PI1), starts the key/exit procedure, and places the exit object 246 plus multiple enemy rat objects.",
            "EGOU.PRC_045 is the key/exit procedure. It waits for object 245, then watches exit object 246; when the exit is reached it writes globalVar[229] = 9.",
            "EGOU.PRC_009 only follows the clear/exit branch after globalVar[229] is 9. It closes the maze part, reloads SD02, sets globalVar[199] = 1, writes globalVar[240] = 10, and loads DOUCHE6.PRC / DOUCHE.REL / DOUCHE.OBJ / DOUCHE.MSG.",
            "Trace validation of exiting rat maze 4 confirmed EGOU.PRC_045 line 176 writes globalVar[229] from 0 to 9, then EGOU.PRC_009 line 518 writes globalVar[199] from 0 to 1, and EGOU.PRC_009 line 523 writes globalVar[240] from 8 to 10.",
            "The production watcher emits rat_mazes_cleared at EGOU.PRC_009 line 523 when globalVar[240] changes from 8 to 10 with globalVar[229] still 9. Avoid firing on initial EGOUBASE load, intermediate maze-map starts, key pickup alone, rat collision/death branches, or the later DOUCHE6.PRC room load.",
        ],
    },
    {
        "id": "dressed_to_infiltrate",
        "title": "Dressed to Infiltrate",
        "status": "implemented-trace-validated",
        "description": "Acquire the soldier disguise by collecting the uniform and boots in the shower/base rooms.",
        "script_refs": [
            ("prc", "PROCS13", "DOUCHE6.PRC", 3),
            ("prc", "PROCS13", "DOUCHE6.PRC", 4),
            ("prc", "PROCS13", "DOUCHE6.PRC", 7),
            ("prc", "PROCS13", "DOUCHE6.PRC", 40),
            ("prc", "PROCS13", "DOUCHE6.PRC", 42),
            ("prc", "PROCS13", "DOUCHE6.PRC", 43),
            ("rel", "PROCS13", "DOUCHE.REL", 1),
            ("rel", "PROCS13", "DOUCHE.REL", 3),
            ("rel", "PROCS13", "DOUCHE.REL", 5),
            ("rel", "PROCS13", "DOUCHE.REL", 73),
            ("rel", "PROCS13", "DOUCHE.REL", 77),
            ("rel", "PROCS13", "DOUCHE.REL", 80),
        ],
        "object_ids": [1, 54, 55, 58, 59, 60, 61, 73, 74, 90, 138],
        "notes": [
            "DOUCHE6.PRC_003 loads the shower room 10.PI1 and initializes the usable soldier-room objects: laces object 54, army boots object 55, clothes/uniform object 58, napkin object 59, and soldier object 60.",
            "DOUCHE6.PRC_040 keeps those room-10 objects visible while their statuses are active; it specifically watches obj[54], obj[55], obj[58], obj[59], and obj[60].",
            "DOUCHE.REL_005 is Take Clothes/Uniform (object 58). If boots are not yet taken it displays message 5 and sets obj[58].status = -3; if boots are already taken it displays message 6, sets obj[58].status = -3, and starts DOUCHE6.PRC_042.",
            "DOUCHE.REL_077 is Take Army Boots (object 55). If clothes are not yet taken it displays message 78 and sets obj[55].status = -3; if clothes are already taken it displays message 77, sets obj[55].status = -3, sets tableUnk1[10] = 10, and starts DOUCHE6.PRC_042.",
            "DOUCHE6.PRC_042 is the disguise-change animation and only runs once the second required clothing item has been taken. It moves John to the changing position, sets globalVar[242] = 0, applies costume 5 / frame 36, and hides both obj[58] and obj[55] by setting their statuses to -1.",
            "Trace validation showed the clothes-first order: DOUCHE.REL_005 line 108 set obj[58].status from 10 to -3, DOUCHE.REL_077 line 106 set obj[55].status from 10 to -3, and DOUCHE6.PRC_042 line 147 set globalVar[242] from 1 to 0 before hiding both clothing objects. The static scripts also support boots-first because DOUCHE.REL_005 starts DOUCHE6.PRC_042 when obj[55].status is already -3.",
            "DOUCHE.REL_001 / 003 and DOUCHE.REL_080 / 073 cover the adjacent safe-exit setup: taking and using the napkin gags the soldier with obj[59].status = -99, while taking and using the laces ties the soldier with obj[54].status = -99.",
            "DOUCHE6.PRC_004 gates exits from room 11 on the gagged soldier state, and DOUCHE6.PRC_007 later tests the disguise plus the tied-boot state before the officer lets John continue. Those are passability checks, not the disguise acquisition boundary.",
            "The production watcher emits soldier_disguise_acquired at DOUCHE6.PRC_042 line 147 when globalVar[242] changes from 1 to 0 while both obj[58].status and obj[55].status are -3. The final hidden-object state obj[58].status = -1 and obj[55].status = -1 is confirmation. Avoid firing on only one clothing item, on gagging/tying the soldier, or on later officer approval.",
        ],
    },
    {
        "id": "fingerprint_fiction",
        "title": "Fingerprint Fiction",
        "status": "mapped-static-candidate",
        "description": "Make and use the false fingerprint, then pass the fingerprint-controlled door from room 12 into room 13.",
        "script_refs": [
            ("prc", "PROCS13", "DOUCHE6.PRC", 5),
            ("prc", "PROCS13", "DOUCHE6.PRC", 18),
            ("prc", "PROCS13", "DOUCHE6.PRC", 22),
            ("prc", "PROCS13", "DOUCHE6.PRC", 30),
            ("rel", "PROCS13", "DOUCHE.REL", 40),
            ("rel", "PROCS13", "DOUCHE.REL", 41),
            ("rel", "PROCS13", "DOUCHE.REL", 65),
            ("rel", "PROCS13", "DOUCHE.REL", 66),
            ("rel", "PROCS13", "DOUCHE.REL", 67),
            ("rel", "PROCS13", "DOUCHE.REL", 120),
        ],
        "object_ids": [46, 100, 101, 102, 103, 104, 150, 152, 154, 156, 180, 181, 182],
        "notes": [
            "DOUCHE.REL_120 examines the heavy fingerprint door and displays message 38, explicitly identifying the fingerprint-controlled door.",
            "DOUCHE.REL_065 is the successful false-fingerprint creation branch: use/apply cigarette paper object 180 on glass object 46; it consumes both objects, sets obj[102].status = -3, and displays message 129.",
            "DOUCHE.REL_066 and DOUCHE.REL_067 are decoy paper-on-glass branches. They consume objects 181 or 182 and display message 130, but do not create fingerprint object 102.",
            "DOUCHE.REL_040 examines lock object 100, starts DOUCHE6.PRC_018 for the scanner animation, and displays messages 40 or 41 depending on the lock frame; it does not pass the door.",
            "DOUCHE.REL_041 uses fingerprint object 102 on lock object 100. It animates the lock, creates helper objects 103 and 104, sets tableUnk1[2] = 2, ends scanner script 18, and restores the lock object.",
            "DOUCHE6.PRC_005 initializes room 12; when obj[102].status == -3 it sets tableUnk1[2] = 8, and all room exits are gated on the disguise flag globalVar[242] == 0.",
            "DOUCHE6.PRC_022 creates the room-12-to-room-13 door pieces and starts DOUCHE6.PRC_030, the door manager for collision zone 2. DOUCHE6.PRC_030 opens the door and sets tableUnk1[14] = 14 once the opening state reaches zero.",
            "DOUCHE6.PRC_005 routes collision zone 14 to destination room 13 by setting globalVar[240] = 13 and globalVar[241] = 1, then ending door manager scripts 22 and 30 before dispatching script 2.",
            "A production watcher candidate is the room-12 transition to globalVar[240] = 13 / globalVar[241] = 1 after DOUCHE.REL_041 accepted fingerprint object 102, with obj[102].status == -3 and globalVar[242] == 0. Avoid firing on fingerprint creation alone, lock examination/scanner animation, wrong cigarette papers, tableUnk1[2] changes alone, or unrelated room-12 movement.",
        ],
    },
    {
        "id": "officially_unofficial",
        "title": "Officially Unofficial",
        "status": "mapped-static-candidate",
        "description": "Create the forged mission order and get it accepted by the guard behind the bulletproof window.",
        "script_refs": [
            ("prc", "PROCS13", "DOUCHE6.PRC", 9),
            ("prc", "PROCS13", "DOUCHE6.PRC", 10),
            ("prc", "PROCS13", "DOUCHE6.PRC", 13),
            ("prc", "PROCS13", "DOUCHE6.PRC", 33),
            ("prc", "PROCS13", "DOUCHE6.PRC", 36),
            ("rel", "PROCS13", "DOUCHE.REL", 16),
            ("rel", "PROCS13", "DOUCHE.REL", 51),
            ("rel", "PROCS13", "DOUCHE.REL", 53),
            ("rel", "PROCS13", "DOUCHE.REL", 55),
            ("rel", "PROCS13", "DOUCHE.REL", 56),
            ("rel", "PROCS13", "DOUCHE.REL", 57),
            ("rel", "PROCS13", "DOUCHE.REL", 68),
            ("rel", "PROCS13", "DOUCHE.REL", 69),
            ("rel", "PROCS13", "DOUCHE.REL", 70),
            ("rel", "PROCS13", "DOUCHE.REL", 71),
            ("rel", "PROCS13", "DOUCHE.REL", 109),
            ("rel", "PROCS13", "DOUCHE.REL", 118),
            ("rel", "PROCS13", "DOUCHE.REL", 119),
        ],
        "object_ids": [29, 38, 39, 40, 41, 42, 93, 126, 149, 168],
        "notes": [
            "DOUCHE.REL_057 is the closet/pocket search that reveals mission instruction object 42 when it is still hidden, displaying message 57 and setting obj[42].status = 15.",
            "DOUCHE.REL_051 reads mission instruction object 42 and displays messages 51 and 52; DOUCHE.REL_053 is the joke operate branch for the same object and displays messages 53 and 54.",
            "DOUCHE.REL_109 takes mission instruction object 42, displays the generic take message 9, and stores it with obj[42].status = -3.",
            "The stamp chain starts with DOUCHE.REL_016 taking wooden stamp object 93, DOUCHE.REL_068 / 069 examining and taking ink pad object 38, and DOUCHE.REL_070 using the stamp on the ink pad to create inked stamp object 29 while consuming object 93.",
            "DOUCHE.REL_071 uses inked stamp object 29 on mission instruction object 42, creates authorized mission object 39 with obj[39].status = -3, marks the original mission instruction as stamped/hidden with obj[42].status = 99 and obj[42].frame = -99, and displays message 133.",
            "DOUCHE6.PRC_013 is room 58: it creates mailbox object 40, guard object 41, and laser object 126; entry 1 starts DOUCHE6.PRC_033, the laser manager.",
            "DOUCHE.REL_118 and DOUCHE.REL_119 are guard examine/operate decoys, displaying messages 69 and 70 without accepting orders.",
            "DOUCHE.REL_056 is the swallow/destroy-order branch for mission instruction object 42; it displays message 56 and sets obj[42].status/frame = -1, so it is not part of the success chain.",
            "DOUCHE.REL_055 is the acceptance branch: use/apply authorized mission object 39 on mailbox object 40. It waits until John reaches the opening, sets obj[39].status = -99, displays message 55, sets tableUnk1[2] = 2, ends laser script 33, and unloads laser mask object 126.",
            "DOUCHE6.PRC_013 contains the nearby laser failure movement branch that displays messages 209 and 210; the accepted-order path should avoid that death branch and should also not depend on later room exits.",
            "A production watcher candidate is DOUCHE.REL_055 reaching message 55 and setting obj[39].status = -99 after object 39 was created by DOUCHE.REL_071. Avoid firing on finding/reading/taking the mission order, stamping alone/message 133, guard examine/operate, swallowing the order, laser failure, or later movement beyond the checkpoint.",
        ],
    },
    {
        "id": "virus_successfully_installed",
        "title": "Virus Successfully Installed",
        "status": "mapped-static-candidate",
        "description": "Sabotage the Stealth by inserting the virus compact disc into the radar CD-ROM player.",
        "script_refs": [
            ("prc", "PROCS12", "SOUSMAR2.PRC", 3),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 4),
            ("prc", "PROCS12", "SOUSMAR2.PRC", 31),
            ("prc", "PROCS15", "SALLE59.PRC", 14),
            ("prc", "PROCS15", "SALLE59.PRC", 35),
            ("prc", "PROCS15", "ILE.PRC", 24),
            ("rel", "PROCS13", "DOUCHE.REL", 92),
            ("rel", "PROCS13", "DOUCHE.REL", 93),
            ("rel", "PROCS15", "SALLE59.REL", 92),
            ("rel", "PROCS15", "SALLE59.REL", 93),
        ],
        "object_ids": [7, 26, 67, 230, 235, 237],
        "notes": [
            "SOUSMAR2.PRC_003 is the briefing that defines the objective: messages 24 and 25 explain that the Stealth's radar cover depends on external brain computers and that the compact disc contains the virus to erase those programs.",
            "SOUSMAR2.PRC_004 grants the compact disc before the underwater/base approach by setting obj[235].status = -3; SOUSMAR2.PRC_031 carries that inventory forward into PROCS13 with obj[235].status = 100.",
            "The late-game SALLE59 resource set reuses inventory slots; object 237 is the carried compact disc in the CD-player interaction even though the broader generated object table can label it from an earlier part.",
            "SALLE59.PRC_014 creates the final command-room scene 59, places CD player object 230, and includes the Dr. Why confrontation. The CD player is available in this scene after the later entry path.",
            "DOUCHE.REL_092 and SALLE59.REL_092 are the same use/apply branch: compact disc object 237 on CD player object 230. The success path requires obj[7].frame == 213; otherwise it falls through without doing anything.",
            "On the success path, REL_092 moves John to the player, sets globalVar[97] = 1, displays message 167 about inserting the compact disk and removing the Stealth's radar cover, and then sets obj[237].status = -99. The later messages 195/196/197, palette transforms, explosion, and ending handoff are downstream finale effects, not the trigger.",
            "REL_092 has a timeout/help branch at label 3 that displays message 169 if John cannot reach the CD player; this is not success.",
            "SALLE59.REL_093 is only the CD player examine branch, displaying message 168 about the radar program.",
            "ILE.PRC_024 later checks obj[237].status; when it is not -3, the script proceeds to FIN2.PRC / FIN.MSG, while an unused disc keeps the island/ending sequence in the interactive waiting branch. This confirms the disc insertion is durable story state, but the achievement should fire at the insertion branch rather than waiting for the ending.",
            "A production watcher candidate is REL_092 displaying message 167 with globalVar[97] = 1 and obj[237].status = -99 after the compact disc is used on CD player object 230. Avoid firing on the briefing, acquiring/carrying the disc, the obj[7].frame != 213 no-op branch, examining the player, the unreachable-player timeout, the explosive-cigarette main-computer branch, or later finale/ending messages.",
        ],
    },
    {
        "id": "order_of_the_banana",
        "title": "Order of the Banana",
        "status": "mapped-static-candidate",
        "description": "Finish the game and receive the Santa Paragua Republic Order of the Banana.",
        "script_refs": [
            ("prc", "PROCS15", "ILE.PRC", 24),
            ("prc", "PROCS16", "FIN2.PRC", 1),
            ("prc", "PROCS16", "FIN2.PRC", 2),
            ("prc", "PROCS16", "FIN2.PRC", 3),
            ("prc", "PROCS16", "FIN2.PRC", 14),
            ("prc", "PROCS16", "FIN2.PRC", 15),
            ("prc", "PROCS16", "FIN2.PRC", 16),
            ("prc", "PROCS16", "FIN2.PRC", 17),
            ("prc", "PROCS16", "FIN2.PRC", 18),
        ],
        "object_ids": [1, 2, 20, 21, 22, 23, 24, 237],
        "notes": [
            "ILE.PRC_024 is the successful island/endgame handoff into FIN2.PRC and FIN.MSG after the final sequence loads BOND31. The same script later has a failure/waiting branch when the compact disc condition has not been satisfied, so this load is upstream context rather than the clean award boundary.",
            "FIN2.PRC_001 initializes the ending part, loads SD03, sets globalVar[240] = 99, and starts FIN2.PRC_002, the ending dispatcher.",
            "FIN2.PRC_002 dispatches the ending sequence by globalVar[240]: 99 starts FIN2.PRC_003, 100 starts FIN2.PRC_015, 101 starts FIN2.PRC_014, 102 starts FIN2.PRC_016, and 103 starts FIN2.PRC_017.",
            "FIN2.PRC_003 is the first ending scene. After the character animation, it advances to globalVar[240] = 100, globalVar[241] = 2, and returns to the dispatcher.",
            "FIN2.PRC_015 is FIN_1, the award scene. It displays FIN.MSG message 3 twice inside a loop; that message literally awards John Glames the Santa Paragua Republic Order of the Banana.",
            "FIN2.PRC_015 then displays FIN.MSG messages 4 and 7, advances to globalVar[240] = 101, globalVar[241] = 2, and returns to the dispatcher for the remaining ending scenes.",
            "FIN2.PRC_014 and FIN2.PRC_016 continue the ending sequence, then FIN2.PRC_017 runs the credits/generic sequence and starts FIN2.PRC_018 for the logo/intro part transition.",
            "A production watcher candidate is the first reach of FIN2.PRC_015 / FIN.MSG message 3, or equivalently the FIN2.PRC_002 dispatch to script 15 from globalVar[240] = 100. Deduplicate the repeated message 3 loop. Avoid firing on the earlier ILE.PRC_024 resource load alone, on the failure text in the island sequence, or on later credits/logo scripts.",
        ],
    },
    {
        "id": "well_read_intruder",
        "title": "Well-Read Intruder",
        "status": "mapped-production-hook",
        "description": "Examine the humorous book hotspot in the palace office/library.",
        "script_refs": [
            ("prc", "PROCS10", "PALAIS1.PRC", 2),
            ("prc", "PROCS10", "PALAIS1.PRC", 13),
            ("prc", "PROCS10", "PALAIS1.PRC", 15),
            ("rel", "PROCS10", "PALAIS.REL", 27),
            ("rel", "PROCS10", "PALAIS.REL", 28),
        ],
        "object_ids": [148, 149],
        "notes": [
            "PALAIS1.PRC_002 is the palace dispatcher: globalVar[240] = 47 starts PALAIS1.PRC_015, the room with the book/library objects.",
            "PALAIS1.PRC_013 covers the adjacent room 46 and can transition into room 47 when the door state globalVar[243] == 47.",
            "PALAIS1.PRC_015 loads background 47.PI1 and control table 47_CT.PI1, then creates object 149 as Library at X=82,Y=3,frame=112 and object 148 as Book at X=82,Y=6,frame=113.",
            "PALAIS.REL_028 is the generic library examine branch for object 149. It only displays message 46, 'It is well stocked,' and should not be treated as the humorous-book achievement by itself.",
            "PALAIS.REL_027 is the humorous book examine branch for object 148. It rolls rand() mod 5 and displays exactly one of PALAIS.MSG messages 41-45 on each examine; there is no script-side memory of which title was shown.",
            "The five possible humorous titles are message 41, 'DYNAMIC DICTATORSHIP WITH POWER AND WEAPONS'; message 42, 'AN ANTHROPORMOPHIC STUDY OF POWER-MAD CRIMINALS AMONG BEES'; message 43, 'THE HITCHHIKERS GUIDE TO TIME TRAVEL AND TOWELS'; message 44, 'IMPROVE YOUR COMMUNICATION SKILLS'; and message 45, 'MY LIFE STORY AS A BEE' by OTTO.",
            "Trace confirmed multiple shelf click positions still route through object 148 / PALAIS.REL_027, so title completion must be message-based rather than hotspot-based.",
            "Production watcher: record title messages 41-45 in CE-side active-domain state, then emit humorous_book_read only after all five title bits have been seen. Avoid firing on room entry, PALAIS.REL_028 / object 149's generic library message 46, other palace desk object examines, repeated reads before all five titles, or unrelated early-game reading text.",
        ],
    },
    {
        "id": "corporate_espionage",
        "title": "Corporate Espionage",
        "status": "mapped-static-candidate",
        "description": "Read the mission orders that advertise Delphine Software games.",
        "script_refs": [
            ("prc", "PROCS13", "DOUCHE6.PRC", 1),
            ("prc", "PROCS13", "DOUCHE6.PRC", 36),
            ("rel", "PROCS13", "DOUCHE.REL", 51),
            ("rel", "PROCS13", "DOUCHE.REL", 53),
            ("rel", "PROCS13", "DOUCHE.REL", 56),
            ("rel", "PROCS13", "DOUCHE.REL", 57),
            ("rel", "PROCS13", "DOUCHE.REL", 71),
            ("rel", "PROCS13", "DOUCHE.REL", 109),
        ],
        "object_ids": [29, 39, 42, 168],
        "notes": [
            "DOUCHE6.PRC_001 initializes the base infiltration objects. It starts mission instruction object 42 hidden with obj[42].status = -15 and obj[42].frame = -15, while clothes object 168 is visible with status 15.",
            "DOUCHE6.PRC_036 manages the room-15 object display. It sets up clothes object 168 and mission instruction object 42, and only loads object 42's mask when obj[42].status == 15.",
            "DOUCHE.REL_057 is the closet/pocket search on clothes object 168. If the mission instruction has not already been taken or stamped, it displays message 57, sets obj[42].frame = 203, sets obj[42].status = 15, and loads the mission instruction mask.",
            "DOUCHE.REL_051 is the achievement's read branch: examine/look at mission instruction object 42 (REL params p1=0 p2=42 p3=65535). It displays message 51, the literal 'MISSION ORDER' text instructing Agent 743 to buy all the fantastic games of Delphine Software, then message 52, 'You are in total agreement.' It does not change object state.",
            "DOUCHE.REL_109 takes the mission instruction after it has been found, displaying only the generic take message 9 and setting obj[42].status = -3; this is acquisition, not the advertising read.",
            "DOUCHE.REL_053 is an operate/joke branch for the same object, displaying messages 53 and 54; it does not read the order text.",
            "DOUCHE.REL_056 lets John swallow the mission instruction, setting obj[42].status = -1; this is a destructive/alternate interaction and should not count as reading the advertisement.",
            "DOUCHE.REL_071 stamps the mission instruction into authorized mission object 39 and sets obj[42].status = 99. That belongs to the Officially Unofficial forging chain, not this optional read achievement.",
            "A production watcher candidate is DOUCHE.REL_051 reaching message 51 on object 42. Deduplicate repeated reads after unlock. Avoid firing on finding/revealing the order in clothes, taking it into inventory, operating it as a joke, swallowing it, stamping it, or later submitting the authorized mission to the guard.",
        ],
    },
]


def parse_script(path: Path, root: Path) -> dict:
    match = SCRIPT_RE.match(path.name)
    if not match:
        raise ValueError(f"Unexpected script filename: {path}")

    parts = path.relative_to(root).parts
    kind = parts[0]
    group = parts[1]
    resource = parts[2]
    script_index = int(match.group("index"))

    script = {
        "kind": kind,
        "group": group,
        "resource": resource,
        "script": script_index,
        "path": str(path),
        "rel_params": None,
        "loads": [],
        "messages": [],
        "global_writes": [],
        "global_reads": [],
        "object_status_writes": [],
        "object_status_reads": [],
        "object_refs": [],
        "starts": [],
        "ends": [],
        "collisions": [],
        "backgrounds": [],
        "transition_candidates": [],
    }

    pending_lines = []

    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        rel_match = REL_PARAMS_RE.search(raw)
        if rel_match:
            script["rel_params"] = {
                "p1": int(rel_match.group("p1")),
                "p2": int(rel_match.group("p2")),
                "p3": int(rel_match.group("p3")),
            }
            continue

        op_match = OP_RE.match(raw)
        if not op_match:
            continue

        offset = op_match.group("offset")
        op_text = op_match.group("op")
        entry_base = {"offset": offset, "text": op_text}

        for load_match in LOAD_RE.finditer(op_text):
            script["loads"].append({
                **entry_base,
                "op": load_match.group("op"),
                "args": load_match.group("args"),
            })

        bg_match = BG_RE.search(op_text)
        if bg_match:
            script["backgrounds"].append({"offset": offset, "name": bg_match.group("bg")})

        message_match = MESSAGE_RE.search(op_text)
        if message_match:
            script["messages"].append({
                **entry_base,
                "id": int(message_match.group("id")),
                "args": message_match.group("args"),
            })

        global_match = GLOBAL_WRITE_RE.search(op_text)
        if global_match:
            var_id = int(global_match.group("var"))
            value = global_match.group("value")
            script["global_writes"].append({
                **entry_base,
                "var": var_id,
                "value": value,
            })
            if var_id in (240, 241):
                pending_lines.append({"offset": offset, "var": var_id, "value": value, "text": op_text})

        object_match = OBJECT_STATUS_RE.search(op_text)
        if object_match:
            script["object_status_writes"].append({
                **entry_base,
                "object": int(object_match.group("object")),
                "value": object_match.group("value"),
            })

        for read_match in GLOBAL_ACCESS_RE.finditer(op_text):
            if global_match and read_match.start() == global_match.start():
                continue
            script["global_reads"].append({
                **entry_base,
                "var": int(read_match.group("var")),
            })

        for read_match in OBJECT_STATUS_ACCESS_RE.finditer(op_text):
            if object_match and read_match.start() == object_match.start():
                continue
            script["object_status_reads"].append({
                **entry_base,
                "object": int(read_match.group("object")),
            })

        seen_object_refs = set()
        for ref_match in OBJECT_ACCESS_RE.finditer(op_text):
            object_id = int(ref_match.group("object"))
            field = ref_match.group("field")
            key = (object_id, field)
            if key in seen_object_refs:
                continue
            seen_object_refs.add(key)
            script["object_refs"].append({
                **entry_base,
                "object": object_id,
                "field": field,
            })

        start_match = START_SCRIPT_RE.search(op_text)
        if start_match:
            started = int(start_match.group("script"))
            script["starts"].append({**entry_base, "script": started})
            if started == 2 and pending_lines:
                destinations = infer_destinations(pending_lines)
                script["transition_candidates"].append({
                    "at": offset,
                    "destinations": destinations,
                    "lines": list(pending_lines),
                })
                pending_lines = []

        end_match = END_SCRIPT_RE.search(op_text)
        if end_match:
            script["ends"].append({**entry_base, "script": int(end_match.group("script"))})

        collision_match = COLLISION_RE.search(op_text)
        if collision_match:
            script["collisions"].append({**entry_base, "args": collision_match.group("args")})

    return script


def infer_destinations(lines: list[dict]) -> list[dict]:
    destinations = []
    seen = set()
    last_entry = None

    for index, line in enumerate(lines):
        if line["var"] != 240:
            continue

        entry = {"globalVar240": line["value"], "globalVar241": None}
        for later in lines[index + 1:]:
            if later["var"] == 240:
                break
            if later["var"] == 241:
                entry["globalVar241"] = later["value"]
                break

        if entry["globalVar241"] is None:
            for earlier in reversed(lines[:index]):
                if earlier["var"] == 241:
                    entry["globalVar241"] = earlier["value"]
                    break

        key = (entry["globalVar240"], entry["globalVar241"])
        if key not in seen:
            seen.add(key)
            destinations.append(entry)
            last_entry = entry

    if not destinations:
        for line in lines:
            if line["var"] != 241:
                continue
            key = (None, line["value"])
            if key not in seen:
                seen.add(key)
                destinations.append({"globalVar240": None, "globalVar241": line["value"]})
                last_entry = destinations[-1]

    if last_entry is None:
        return []
    return destinations


def build_atlas(root: Path) -> dict:
    scripts = []
    for kind in ("prc", "rel"):
        for path in sorted((root / kind).glob("*/*/*.txt")):
            scripts.append(parse_script(path, root))

    resource_root = guess_resource_root(root)
    messages = load_message_tables(resource_root) if resource_root else {}
    objects = load_object_tables(resource_root) if resource_root else {}
    collision_tables = load_collision_tables(resource_root) if resource_root else {}
    set_tables = load_set_tables(resource_root) if resource_root else {}
    global_object_lookup = build_global_object_lookup(objects)

    message_candidates = build_message_candidates(messages)

    by_group = defaultdict(list)
    for script in scripts:
        attach_message_text(script, messages.get(script["group"], {}), message_candidates)
        attach_object_labels(script, objects.get(script["group"], {}), global_object_lookup)
        by_group[script["group"]].append(script)

    destinations = build_destination_index(scripts)
    object_interactions = build_object_interaction_index(scripts, objects, global_object_lookup)
    scenes = build_scene_index(scripts, collision_tables, set_tables, objects, global_object_lookup)
    flag_index = build_flag_index(scripts)
    message_usage = build_message_usage_index(messages, scripts)
    object_dossiers = build_object_dossiers(scripts, objects, global_object_lookup)
    beat_dossiers = build_beat_dossiers(scripts, objects, global_object_lookup)

    return {
        "source": str(root),
        "resource_source": str(resource_root) if resource_root else None,
        "script_count": len(scripts),
        "destinations": destinations,
        "messages": {
            group: {
                name: len(entries)
                for name, entries in sorted(group_messages.items())
            }
            for group, group_messages in sorted(messages.items())
        },
        "objects": objects,
        "collision_tables": collision_tables,
        "set_tables": set_tables,
        "object_interactions": object_interactions,
        "object_dossiers": object_dossiers,
        "beat_dossiers": beat_dossiers,
        "scenes": scenes,
        "flag_index": flag_index,
        "message_usage": message_usage,
        "action_labels": ACTION_LABELS,
        "message_candidate_count": len(message_candidates),
        "groups": {
            group: sorted(entries, key=lambda s: (s["kind"], s["resource"], s["script"]))
            for group, entries in sorted(by_group.items())
        },
    }


def guess_resource_root(script_root: Path) -> Path | None:
    parts = list(script_root.parts)
    if "script-dumps" not in parts:
        return None

    index = parts.index("script-dumps")
    candidate = Path(*parts[:index], "resource-dumps", *parts[index + 1:])
    if (candidate / "bundles").is_dir():
        return candidate
    return None


def load_message_tables(resource_root: Path) -> dict:
    messages = defaultdict(dict)
    bundles = resource_root / "bundles"
    for path in sorted(bundles.glob("*/*.MSG.txt")):
        group = path.parent.name
        msg_name = path.name[:-4]
        entries = {}
        for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
            match = MSG_LINE_RE.match(raw)
            if not match:
                continue
            entries[int(match.group("id"))] = match.group("text").strip()
        messages[group][msg_name] = entries
    return dict(messages)


def load_object_tables(resource_root: Path) -> dict:
    objects = defaultdict(dict)
    bundles = resource_root / "bundles"
    for path in sorted(bundles.glob("*/*.OBJ")):
        group = path.parent.name
        object_name = path.name
        objects[group][object_name] = parse_object_table(path)
    return {
        group: dict(group_objects)
        for group, group_objects in sorted(objects.items())
    }


def load_collision_tables(resource_root: Path) -> dict:
    tables = {}
    bundles = resource_root / "bundles"
    for path in sorted(bundles.glob("*/*_CT.PI1")):
        try:
            table = parse_collision_table(path)
        except ValueError:
            continue
        tables[path.name.upper()] = table
    return tables


def load_set_tables(resource_root: Path) -> dict:
    tables = {}
    bundles = resource_root / "bundles"
    for path in sorted(bundles.glob("*/*.SET")):
        try:
            table = parse_set_table(path)
        except ValueError:
            continue
        tables.setdefault(path.name.upper(), []).append(table)
    return tables


def parse_set_table(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 6 or data[:3] != b"SET":
        raise ValueError(f"not a SET resource: {path}")

    frame_count = int.from_bytes(data[4:6], "big")
    frames = []
    offset = 6
    for frame in range(frame_count):
        header = data[offset + frame * 0x10:offset + (frame + 1) * 0x10]
        if len(header) < 0x10:
            break
        frames.append({
            "frame": frame,
            "data_offset": int.from_bytes(header[0:4], "big"),
            "width": int.from_bytes(header[4:6], "big"),
            "height": int.from_bytes(header[6:8], "big"),
            "type": int.from_bytes(header[8:10], "big"),
        })

    return {
        "path": str(path),
        "bundle": path.parent.name,
        "name": path.name,
        "frame_count": frame_count,
        "frames": frames,
    }


def parse_collision_table(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 4:
        raise ValueError(f"collision table too small: {path}")

    bpp = int.from_bytes(data[:2], "big")
    if bpp == 8:
        palette_size = 256 * 3
        raw = data[2 + palette_size:2 + palette_size + 320 * 200]
        if len(raw) < 320 * 200:
            raise ValueError(f"short 8-bit collision table: {path}")
        pixels = list(raw)
    else:
        palette_size = 16 * 2
        planar = data[2 + palette_size:2 + palette_size + 160 * 200]
        if len(planar) < 160 * 200:
            raise ValueError(f"short 16-color collision table: {path}")
        pixels = decode_planar_16_color(planar, 160, 200)

    zones = {}
    for y in range(200):
        for x in range(320):
            zone = pixels[y * 320 + x] & 0x0F
            if zone == 0:
                continue
            entry = zones.setdefault(zone, {
                "zone": zone,
                "count": 0,
                "min_x": x,
                "min_y": y,
                "max_x": x,
                "max_y": y,
            })
            entry["count"] += 1
            entry["min_x"] = min(entry["min_x"], x)
            entry["min_y"] = min(entry["min_y"], y)
            entry["max_x"] = builtins.max(entry["max_x"], x)
            entry["max_y"] = builtins.max(entry["max_y"], y)

    return {
        "path": str(path),
        "bpp": bpp,
        "zones": [zones[key] for key in sorted(zones)],
    }


def decode_planar_16_color(data: bytes, width_words: int, height: int) -> list[int]:
    pixels = []
    pos = 0
    for _y in range(height):
        for _x in range(width_words // 8):
            chunk = data[pos:pos + 8]
            if len(chunk) < 8:
                break
            planes = [
                int.from_bytes(chunk[p * 2:p * 2 + 2], "big")
                for p in range(4)
            ]
            for bit in range(16):
                color = 0
                mask = 1 << (15 - bit)
                for plane_idx, plane in enumerate(planes):
                    if plane & mask:
                        color |= 1 << plane_idx
                pixels.append(color)
            pos += 8
    return pixels


def parse_object_table(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 4:
        return {"path": str(path), "entry_count": 0, "entry_size": 0, "entries": []}

    entry_count, entry_size = struct.unpack_from(">HH", data, 0)
    entries = []
    offset = 4
    for index in range(entry_count):
        entry = data[offset + index * entry_size:offset + (index + 1) * entry_size]
        if len(entry) < min(entry_size, OBJ_ENTRY_SIZE):
            break

        if entry_size < OBJ_ENTRY_SIZE:
            entries.append({"id": index, "raw_hex": entry.hex()})
            continue

        x, y, mask, frame, status = struct.unpack_from(">hhHhh", entry, 0)
        raw_name = entry[10:30].split(b"\0", 1)[0]
        name = raw_name.decode("latin-1", errors="replace").strip()
        part = struct.unpack_from(">H", entry, 30)[0]
        entries.append({
            "id": index,
            "x": x,
            "y": y,
            "mask": mask,
            "frame": frame,
            "status": status,
            "name": name,
            "part": part,
        })

    return {
        "path": str(path),
        "entry_count": entry_count,
        "entry_size": entry_size,
        "entries": entries,
    }


def build_object_lookup(group_objects: dict) -> dict:
    lookup = {}
    for object_file, table in group_objects.items():
        for entry in table.get("entries", []):
            lookup.setdefault(entry["id"], []).append({
                "object_file": object_file,
                **entry,
            })
    return lookup


def build_global_object_lookup(objects: dict) -> dict:
    lookup = defaultdict(list)
    for group, group_objects in objects.items():
        for object_file, table in group_objects.items():
            for entry in table.get("entries", []):
                lookup[entry["id"]].append({
                    "group": group,
                    "object_file": object_file,
                    **entry,
                })
    return dict(lookup)


def candidate_object_entries(
    group: str,
    object_id: int,
    group_objects: dict,
    global_object_lookup: dict,
) -> list[dict]:
    lookup = build_object_lookup(group_objects)
    if object_id in lookup:
        return lookup[object_id]

    candidates = []
    seen_names = set()
    for entry in global_object_lookup.get(object_id, []):
        if entry.get("group") not in OBJECT_GROUP_FALLBACKS.get(group, []):
            continue
        name = entry.get("name", "")
        if not name:
            continue
        key = (entry.get("group"), entry.get("object_file"), name)
        if key in seen_names:
            continue
        seen_names.add(key)
        candidates.append({**entry, "candidate": True})
    return candidates


def attach_object_labels(script: dict, group_objects: dict, global_object_lookup: dict) -> None:
    rel = script.get("rel_params")
    if rel:
        for key in ("p2", "p3"):
            value = rel.get(key)
            entries = candidate_object_entries(script["group"], value, group_objects, global_object_lookup)
            if entries:
                rel[f"{key}_objects"] = summarize_object_entries(entries)
        if rel.get("p1") in ACTION_LABELS:
            rel["p1_label"] = ACTION_LABELS[rel["p1"]]

    for write in script["object_status_writes"]:
        entries = candidate_object_entries(script["group"], write["object"], group_objects, global_object_lookup)
        if entries:
            write["objects"] = summarize_object_entries(entries)

    for read in script["object_status_reads"]:
        entries = candidate_object_entries(script["group"], read["object"], group_objects, global_object_lookup)
        if entries:
            read["objects"] = summarize_object_entries(entries)

    for ref in script["object_refs"]:
        entries = candidate_object_entries(script["group"], ref["object"], group_objects, global_object_lookup)
        if entries:
            ref["objects"] = summarize_object_entries(entries)


def summarize_object_entries(entries: list[dict]) -> list[dict]:
    summarized = []
    for entry in entries:
        summarized.append({
            "group": entry.get("group"),
            "object_file": entry["object_file"],
            "id": entry["id"],
            "name": entry.get("name", ""),
            "status": entry.get("status"),
            "part": entry.get("part"),
            "x": entry.get("x"),
            "y": entry.get("y"),
            "candidate": entry.get("candidate", False),
        })
    return summarized[:8]


def build_object_interaction_index(scripts: list[dict], objects: dict, global_object_lookup: dict) -> dict:
    index = defaultdict(lambda: defaultdict(list))
    for script in scripts:
        rel = script.get("rel_params")
        if not rel:
            continue

        group_objects = objects.get(script["group"], {})
        object_ids = []
        for key in ("p2", "p3"):
            value = rel.get(key)
            if isinstance(value, int) and candidate_object_entries(script["group"], value, group_objects, global_object_lookup):
                object_ids.append((key, value))

        for source_param, object_id in object_ids:
            entry = {
                "script": {
                    "kind": script["kind"],
                    "group": script["group"],
                    "resource": script["resource"],
                    "script": script["script"],
                },
                "source_param": source_param,
                "rel_params": rel,
                "messages": script["messages"][:8],
                "object_status_writes": script["object_status_writes"],
                "transition_candidates": script["transition_candidates"],
                "summary": summarize_script(script),
            }
            for object_entry in candidate_object_entries(script["group"], object_id, group_objects, global_object_lookup):
                source_group = object_entry.get("group") or script["group"]
                object_file = object_entry["object_file"]
                object_file_key = object_file if source_group == script["group"] else f"{source_group}/{object_file}"
                index[script["group"]][object_file_key].append({
                    "object": {
                        "id": object_id,
                        "name": object_entry.get("name", ""),
                        "status": object_entry.get("status"),
                        "part": object_entry.get("part"),
                        "x": object_entry.get("x"),
                        "y": object_entry.get("y"),
                        "source_group": source_group,
                        "candidate": object_entry.get("candidate", False),
                    },
                    **entry,
                })

    return {
        group: {
            object_file: sorted(
                entries,
                key=lambda item: (
                    item["object"]["id"],
                    item["script"]["resource"],
                    item["script"]["script"],
                    item["source_param"],
                ),
            )
            for object_file, entries in sorted(files.items())
        }
        for group, files in sorted(index.items())
    }


def build_object_dossiers(scripts: list[dict], objects: dict, global_object_lookup: dict) -> dict:
    dossiers = defaultdict(lambda: defaultdict(dict))

    def ensure_entry(source_group: str, object_file: str, object_id: int, object_entry: dict | None = None) -> dict:
        entry = dossiers[source_group][object_file].get(object_id)
        if not entry:
            object_info = {
                "id": object_id,
                "name": "",
                "x": None,
                "y": None,
                "mask": None,
                "frame": None,
                "status": None,
                "part": None,
                "candidate": False,
            }
            if object_entry:
                object_info.update({
                    "name": object_entry.get("name", ""),
                    "x": object_entry.get("x"),
                    "y": object_entry.get("y"),
                    "mask": object_entry.get("mask"),
                    "frame": object_entry.get("frame"),
                    "status": object_entry.get("status"),
                    "part": object_entry.get("part"),
                    "candidate": object_entry.get("candidate", False),
                })
            entry = {
                "object": object_info,
                "scene_refs": [],
                "interactions": [],
                "status_writes": [],
                "status_reads": [],
                "field_refs": [],
            }
            dossiers[source_group][object_file][object_id] = entry
        elif object_entry and not entry["object"].get("name"):
            entry["object"].update({
                "name": object_entry.get("name", ""),
                "x": object_entry.get("x"),
                "y": object_entry.get("y"),
                "mask": object_entry.get("mask"),
                "frame": object_entry.get("frame"),
                "status": object_entry.get("status"),
                "part": object_entry.get("part"),
                "candidate": object_entry.get("candidate", False),
            })
        return entry

    for group, group_objects in objects.items():
        for object_file, table in group_objects.items():
            for object_entry in table.get("entries", []):
                if object_entry.get("name"):
                    ensure_entry(group, object_file, object_entry["id"], object_entry)

    for script in scripts:
        script_ref = {
            "kind": script["kind"],
            "group": script["group"],
            "resource": script["resource"],
            "script": script["script"],
        }
        group_objects = objects.get(script["group"], {})

        def entries_for_object(object_id: int) -> list[dict]:
            entries = candidate_object_entries(script["group"], object_id, group_objects, global_object_lookup)
            if entries:
                return entries
            return [{"object_file": "<unknown>", "id": object_id, "name": ""}]

        for ref in script["object_refs"]:
            for object_entry in entries_for_object(ref["object"]):
                source_group = object_entry.get("group") or script["group"]
                object_file = object_entry["object_file"]
                dossier = ensure_entry(source_group, object_file, ref["object"], object_entry)
                field_ref = {
                    "script": script_ref,
                    "offset": ref["offset"],
                    "field": ref["field"],
                    "text": ref["text"],
                }
                dossier["field_refs"].append(field_ref)
                if script["kind"] == "prc":
                    scene_ref = {
                        "script": script_ref,
                        "offset": ref["offset"],
                        "field": ref["field"],
                        "backgrounds": [bg["name"] for bg in script["backgrounds"]],
                    }
                    dossier["scene_refs"].append(scene_ref)

        for write in script["object_status_writes"]:
            for object_entry in entries_for_object(write["object"]):
                source_group = object_entry.get("group") or script["group"]
                object_file = object_entry["object_file"]
                dossier = ensure_entry(source_group, object_file, write["object"], object_entry)
                dossier["status_writes"].append({
                    "script": script_ref,
                    "offset": write["offset"],
                    "value": write["value"],
                    "text": write["text"],
                })

        for read in script["object_status_reads"]:
            for object_entry in entries_for_object(read["object"]):
                source_group = object_entry.get("group") or script["group"]
                object_file = object_entry["object_file"]
                dossier = ensure_entry(source_group, object_file, read["object"], object_entry)
                dossier["status_reads"].append({
                    "script": script_ref,
                    "offset": read["offset"],
                    "text": read["text"],
                })

        rel = script.get("rel_params")
        if rel:
            for source_param in ("p2", "p3"):
                object_id = rel.get(source_param)
                if not isinstance(object_id, int):
                    continue
                for object_entry in candidate_object_entries(script["group"], object_id, group_objects, global_object_lookup):
                    source_group = object_entry.get("group") or script["group"]
                    object_file = object_entry["object_file"]
                    dossier = ensure_entry(source_group, object_file, object_id, object_entry)
                    dossier["interactions"].append({
                        "script": script_ref,
                        "source_param": source_param,
                        "rel_params": rel,
                        "messages": script["messages"][:8],
                        "summary": summarize_script(script),
                    })

    return {
        group: {
            object_file: [
                entry
                for _object_id, entry in sorted(entries.items(), key=lambda item: item[0])
                if object_dossier_has_evidence(entry)
            ]
            for object_file, entries in sorted(files.items())
        }
        for group, files in sorted(dossiers.items())
    }


def object_dossier_has_evidence(entry: dict) -> bool:
    return bool(
        entry["object"].get("name") or
        entry["scene_refs"] or
        entry["interactions"] or
        entry["status_writes"] or
        entry["status_reads"] or
        entry["field_refs"]
    )


def build_beat_dossiers(scripts: list[dict], objects: dict, global_object_lookup: dict) -> list[dict]:
    scripts_by_ref = {
        (script["kind"], script["group"], script["resource"], script["script"]): script
        for script in scripts
    }
    beats = []
    for definition in BEAT_DEFINITIONS:
        script_entries = []
        for script_ref in definition["script_refs"]:
            script = scripts_by_ref.get(script_ref)
            if not script:
                script_entries.append({
                    "script": {
                        "kind": script_ref[0],
                        "group": script_ref[1],
                        "resource": script_ref[2],
                        "script": script_ref[3],
                    },
                    "missing": True,
                })
                continue
            script_entries.append({
                "script": {
                    "kind": script["kind"],
                    "group": script["group"],
                    "resource": script["resource"],
                    "script": script["script"],
                },
                "rel_params": script.get("rel_params"),
                "backgrounds": script["backgrounds"],
                "messages": script["messages"][:12],
                "global_writes": script["global_writes"],
                "object_status_writes": script["object_status_writes"],
                "transition_candidates": script["transition_candidates"],
                "summary": summarize_script(script),
            })

        object_entries = []
        seen_objects = set()
        for group in sorted({ref[1] for ref in definition["script_refs"]}):
            group_objects = objects.get(group, {})
            for object_id in definition.get("object_ids", []):
                for entry in candidate_object_entries(group, object_id, group_objects, global_object_lookup):
                    key = (entry.get("group") or group, entry["object_file"], object_id, entry.get("name", ""))
                    if key in seen_objects:
                        continue
                    seen_objects.add(key)
                    object_entries.append({
                        "group": entry.get("group") or group,
                        "object_file": entry["object_file"],
                        "id": object_id,
                        "name": entry.get("name", ""),
                        "x": entry.get("x"),
                        "y": entry.get("y"),
                        "frame": entry.get("frame"),
                        "status": entry.get("status"),
                        "part": entry.get("part"),
                        "candidate": entry.get("candidate", False),
                    })

        beats.append({
            "id": definition["id"],
            "title": definition["title"],
            "status": definition["status"],
            "description": definition["description"],
            "notes": definition.get("notes", []),
            "scripts": script_entries,
            "objects": object_entries,
        })
    return beats


def build_flag_index(scripts: list[dict]) -> dict:
    globals_index = defaultdict(lambda: {"writers": [], "readers": []})
    object_index = defaultdict(lambda: {"writers": [], "readers": []})

    for script in scripts:
        script_ref = {
            "kind": script["kind"],
            "group": script["group"],
            "resource": script["resource"],
            "script": script["script"],
        }

        for write in script["global_writes"]:
            globals_index[str(write["var"])]["writers"].append({
                "script": script_ref,
                "offset": write["offset"],
                "value": write["value"],
                "text": write["text"],
            })

        for read in script["global_reads"]:
            globals_index[str(read["var"])]["readers"].append({
                "script": script_ref,
                "offset": read["offset"],
                "text": read["text"],
            })

        for write in script["object_status_writes"]:
            object_index[str(write["object"])]["writers"].append({
                "script": script_ref,
                "offset": write["offset"],
                "value": write["value"],
                "text": write["text"],
                "objects": write.get("objects", []),
            })

        for read in script["object_status_reads"]:
            object_index[str(read["object"])]["readers"].append({
                "script": script_ref,
                "offset": read["offset"],
                "text": read["text"],
                "objects": read.get("objects", []),
            })

    return {
        "globals": {
            key: value
            for key, value in sorted(globals_index.items(), key=lambda item: int(item[0]))
        },
        "object_status": {
            key: value
            for key, value in sorted(object_index.items(), key=lambda item: int(item[0]))
        },
    }


def build_scene_index(scripts: list[dict], collision_tables: dict, set_tables: dict, objects: dict, global_object_lookup: dict) -> dict:
    scenes = defaultdict(list)
    prc_by_group_number = defaultdict(lambda: defaultdict(list))
    incoming = defaultdict(list)

    for script in scripts:
        if script["kind"] == "prc":
            prc_by_group_number[script["group"]][script["script"]].append(script)

    for script in scripts:
        for transition in script["transition_candidates"]:
            for destination in transition.get("destinations", []):
                raw = destination.get("globalVar240")
                if raw is None or not str(raw).lstrip("-").isdigit():
                    continue
                value = int(raw)
                candidate_numbers = [value]
                encoded = value % 100
                if value >= 100 and encoded != value:
                    candidate_numbers.append(encoded)
                for candidate_number in candidate_numbers:
                    for target in prc_by_group_number[script["group"]].get(candidate_number, []):
                        incoming[(target["group"], target["resource"], target["script"])].append({
                            "from": {
                                "kind": script["kind"],
                                "group": script["group"],
                                "resource": script["resource"],
                                "script": script["script"],
                            },
                            "destination": value,
                            "entry": destination.get("globalVar241"),
                            "offset": transition["at"],
                        })

    for script in scripts:
        if script["kind"] != "prc":
            continue
        if not script["backgrounds"] and not script["transition_candidates"] and not script["collisions"]:
            continue

        control_tables = [
            load for load in script["loads"]
            if load["op"] == "loadCt"
        ]
        for control_table in control_tables:
            table = collision_tables.get(control_table["args"].upper())
            if table:
                control_table["zones"] = table["zones"]

        loaded_sets = build_loaded_set_entries(script["loads"], set_tables)
        referenced_objects = build_scene_referenced_objects(script)
        frame_candidates = build_scene_frame_candidates(script["group"], loaded_sets, objects, global_object_lookup)

        scenes[script["group"]].append({
            "script": {
                "kind": script["kind"],
                "group": script["group"],
                "resource": script["resource"],
                "script": script["script"],
            },
            "backgrounds": script["backgrounds"],
            "incoming": incoming.get((script["group"], script["resource"], script["script"]), []),
            "control_tables": control_tables,
            "loads": [
                load for load in script["loads"]
                if load["op"] != "loadCt"
            ],
            "messages": script["messages"][:8],
            "message_count": len(script["messages"]),
            "global_reads": script["global_reads"],
            "global_writes": script["global_writes"],
            "object_status_reads": script["object_status_reads"],
            "object_status_writes": script["object_status_writes"],
            "object_refs": referenced_objects,
            "loaded_sets": loaded_sets,
            "frame_object_candidates": frame_candidates,
            "transitions": script["transition_candidates"],
            "collisions": script["collisions"],
            "summary": summarize_script(script),
        })

    return {
        group: sorted(
            entries,
            key=lambda entry: (
                entry["script"]["resource"],
                entry["script"]["script"],
            ),
        )
        for group, entries in sorted(scenes.items())
    }


def build_loaded_set_entries(loads: list[dict], set_tables: dict) -> list[dict]:
    entries = []
    for load in loads:
        if load["op"] != "loadABS":
            continue
        args = [arg.strip() for arg in load["args"].split(",")]
        if len(args) != 2 or not args[0].lstrip("-").isdigit():
            continue
        resource = args[1].upper()
        if not resource.endswith(".SET"):
            continue
        index = int(args[0])
        matches = set_tables.get(resource, [])
        entries.append({
            "offset": load["offset"],
            "resource": args[1],
            "index": index,
            "matches": matches[:4],
            "frame_range": [
                index,
                index + matches[0]["frame_count"] - 1 if matches else index,
            ],
        })
    return entries


def build_scene_referenced_objects(script: dict) -> list[dict]:
    refs = {}
    for ref in script["object_refs"]:
        object_id = ref["object"]
        entry = refs.setdefault(object_id, {
            "object": object_id,
            "fields": [],
            "refs": [],
            "objects": ref.get("objects", []),
        })
        if ref["field"] not in entry["fields"]:
            entry["fields"].append(ref["field"])
        entry["refs"].append({
            "offset": ref["offset"],
            "field": ref["field"],
            "text": ref["text"],
        })
        if not entry["objects"] and ref.get("objects"):
            entry["objects"] = ref["objects"]

    return [
        refs[key]
        for key in sorted(refs)
    ]


def build_scene_frame_candidates(group: str, loaded_sets: list[dict], objects: dict, global_object_lookup: dict) -> list[dict]:
    if not loaded_sets:
        return []

    candidates = []
    group_objects = objects.get(group, {})
    seen = set()
    for loaded_set in loaded_sets:
        start, end = loaded_set["frame_range"]
        for object_id, entries in build_object_lookup(group_objects).items():
            for entry in entries:
                frame = entry.get("frame")
                if frame is None or frame < start or frame > end:
                    continue
                key = (loaded_set["resource"], object_id, frame)
                if key in seen:
                    continue
                seen.add(key)
                candidates.append({
                    "loaded_set": loaded_set["resource"],
                    "frame_range": loaded_set["frame_range"],
                    "object": {
                        "id": object_id,
                        "name": entry.get("name", ""),
                        "frame": frame,
                        "status": entry.get("status"),
                        "part": entry.get("part"),
                        "x": entry.get("x"),
                        "y": entry.get("y"),
                    },
                })
    return candidates[:24]


def build_message_candidates(messages: dict) -> dict:
    candidates = defaultdict(list)
    seen = defaultdict(set)
    for group, group_messages in messages.items():
        for msg_name, entries in group_messages.items():
            for message_id, text in entries.items():
                if not text:
                    continue
                key = (group, msg_name, text)
                if key in seen[message_id]:
                    continue
                seen[message_id].add(key)
                candidates[message_id].append({
                    "group": group,
                    "msg_file": msg_name,
                    "text": text,
                })
    return dict(candidates)


def build_message_usage_index(messages: dict, scripts: list[dict]) -> dict:
    index = defaultdict(lambda: defaultdict(dict))
    for group, group_messages in messages.items():
        for msg_file, entries in group_messages.items():
            for message_id, text in entries.items():
                index[group][msg_file][str(message_id)] = {
                    "id": message_id,
                    "text": text,
                    "refs": [],
                    "candidate_refs": [],
                }

    for script in scripts:
        script_ref = {
            "kind": script["kind"],
            "group": script["group"],
            "resource": script["resource"],
            "script": script["script"],
        }
        for message in script["messages"]:
            ref = {
                "script": script_ref,
                "offset": message["offset"],
                "args": message["args"],
            }
            msg_file = message.get("msg_file")
            if msg_file:
                entry = index[script["group"]][msg_file].setdefault(str(message["id"]), {
                    "id": message["id"],
                    "text": message.get("message_text", ""),
                    "refs": [],
                    "candidate_refs": [],
                })
                entry["refs"].append(ref)
                continue

            for candidate in message.get("text_candidates", []):
                entry = index[candidate["group"]][candidate["msg_file"]].setdefault(str(message["id"]), {
                    "id": message["id"],
                    "text": candidate["text"],
                    "refs": [],
                    "candidate_refs": [],
                })
                entry["candidate_refs"].append(ref)

    return {
        group: {
            msg_file: {
                message_id: entry
                for message_id, entry in sorted(entries.items(), key=lambda item: int(item[0]))
            }
            for msg_file, entries in sorted(group_messages.items())
        }
        for group, group_messages in sorted(index.items())
    }


def attach_message_text(script: dict, group_messages: dict, message_candidates: dict) -> None:
    msg_name = None
    if group_messages and len(group_messages) == 1:
        msg_name = next(iter(group_messages))
    elif group_messages:
        resource_stem = Path(script["resource"]).stem
        candidate = f"{resource_stem}.MSG"
        if candidate in group_messages:
            msg_name = candidate

    for message in script["messages"]:
        if msg_name:
            table = group_messages[msg_name]
            text = table.get(message["id"])
            if text:
                message["msg_file"] = msg_name
                message["message_text"] = text
                continue

        candidates = message_candidates.get(message["id"], [])
        if candidates:
            message["text_candidates"] = candidates[:5]


def build_destination_index(scripts: list[dict]) -> dict:
    scripts_by_number = defaultdict(list)
    destinations = defaultdict(list)

    for script in scripts:
        scripts_by_number[script["script"]].append({
            "kind": script["kind"],
            "group": script["group"],
            "resource": script["resource"],
            "script": script["script"],
            "backgrounds": [bg["name"] for bg in script["backgrounds"]],
            "messages": [message["id"] for message in script["messages"][:8]],
        })

    for script in scripts:
        for transition in script["transition_candidates"]:
            for destination in transition.get("destinations", []):
                raw = destination.get("globalVar240")
                if raw is None or not str(raw).lstrip("-").isdigit():
                    continue
                value = int(raw)
                entry = {
                    "from": {
                        "kind": script["kind"],
                        "group": script["group"],
                        "resource": script["resource"],
                        "script": script["script"],
                    },
                    "entry": destination.get("globalVar241"),
                    "direct_script_matches": scripts_by_number.get(value, []),
                    "encoded_script_matches": [],
                }

                encoded_script = value % 100
                if value >= 100 and encoded_script != value:
                    entry["encoded_script_matches"] = scripts_by_number.get(encoded_script, [])

                destinations[str(value)].append(entry)

    return dict(sorted(destinations.items(), key=lambda item: int(item[0])))


def summarize_script(script: dict) -> str:
    bits = []
    if script["backgrounds"]:
        bits.append("bg " + ", ".join(bg["name"] for bg in script["backgrounds"][:3]))
    if script["transition_candidates"]:
        transitions = []
        for transition in script["transition_candidates"][:4]:
            for destination in transition.get("destinations", []):
                transitions.append(f"{destination.get('globalVar240')}:{destination.get('globalVar241')}")
        bits.append("to " + ", ".join(transitions))
    if script["messages"]:
        messages = []
        selected_messages = script["messages"]
        if len(selected_messages) > 8:
            selected_messages = script["messages"][:4] + script["messages"][-4:]
        for message in selected_messages:
            text = message.get("message_text", "")
            if not text and len(message.get("text_candidates", [])) == 1:
                text = message["text_candidates"][0]["text"]
            if text:
                if len(text) > 48:
                    text = text[:45] + "..."
                messages.append(f"{message['id']}={text}")
            elif message.get("text_candidates"):
                unique_candidate_texts = []
                for candidate in message["text_candidates"]:
                    candidate_text = candidate["text"]
                    if candidate_text not in unique_candidate_texts:
                        unique_candidate_texts.append(candidate_text)
                if len(unique_candidate_texts) <= 2:
                    text = " / ".join(unique_candidate_texts)
                    if len(text) > 48:
                        text = text[:45] + "..."
                    messages.append(f"{message['id']}~{text}")
                else:
                    messages.append(f"{message['id']}=?{len(message['text_candidates'])} candidates")
            else:
                messages.append(str(message["id"]))
        if len(script["messages"]) > len(selected_messages):
            messages.insert(4, "...")
        bits.append("msg " + ", ".join(messages))
    if script["object_status_writes"]:
        objects = []
        for write in script["object_status_writes"][:6]:
            label = object_label(write.get("objects", []))
            suffix = f" {label}" if label else ""
            objects.append(f"obj{write['object']}={write['value']}{suffix}")
        bits.append("state " + ", ".join(objects))
    if script["starts"]:
        bits.append("starts " + ", ".join(str(start["script"]) for start in script["starts"][:6]))
    return "; ".join(bits)


def object_label(objects: list[dict]) -> str:
    names = []
    for entry in objects:
        name = entry.get("name")
        if name and name not in names:
            names.append(name)
    if not names:
        return ""
    prefix = "~" if any(entry.get("candidate") for entry in objects) else ""
    return "(" + prefix + " / ".join(names[:3]) + ")"


def format_control_table_zones(control_tables: list[dict]) -> str:
    parts = []
    for control_table in control_tables:
        zones = control_table.get("zones", [])
        if not zones:
            continue
        zone_parts = []
        for zone in zones[:8]:
            zone_parts.append(
                f"{zone['zone']}:{zone['min_x']},{zone['min_y']}-{zone['max_x']},{zone['max_y']} ({zone['count']})"
            )
        if len(zones) > 8:
            zone_parts.append("...")
        parts.append("<br>".join(zone_parts))
    return "<br>".join(parts) or "-"


def format_script_ref(script: dict) -> str:
    return f"`{script['group']}/{script['resource']}_{script['script']:03d}`"


def format_flag_refs(entries: list[dict], include_values: bool) -> str:
    if not entries:
        return "-"

    parts = []
    for entry in entries[:10]:
        value = f"={entry['value']}" if include_values and "value" in entry else ""
        parts.append(f"{format_script_ref(entry['script'])}@`{entry['offset']}`{value}")
    if len(entries) > 10:
        parts.append(f"... +{len(entries) - 10} more")
    return "<br>".join(parts)


def format_message_refs(entries: list[dict]) -> str:
    if not entries:
        return "-"

    parts = []
    for entry in entries[:8]:
        parts.append(f"{format_script_ref(entry['script'])}@`{entry['offset']}`")
    if len(entries) > 8:
        parts.append(f"... +{len(entries) - 8} more")
    return "<br>".join(parts)


def format_message_text(text: str) -> str:
    if not text:
        return "-"
    text = text.replace("|", "\\|").strip()
    if len(text) > 160:
        text = text[:157] + "..."
    return text or "-"


def format_loads(loads: list[dict]) -> str:
    if not loads:
        return "-"
    parts = []
    for load in loads[:12]:
        parts.append(f"`{load['op']}({load['args']})`@`{load['offset']}`")
    if len(loads) > 12:
        parts.append(f"... +{len(loads) - 12} more")
    return "<br>".join(parts)


def format_scene_messages(messages: list[dict], total_count: int) -> str:
    if not messages:
        return "-"
    parts = []
    for message in messages:
        text = message.get("message_text", "")
        if not text and len(message.get("text_candidates", [])) == 1:
            text = message["text_candidates"][0]["text"]
        marker = "=" if text else ""
        if not text and message.get("text_candidates"):
            text = "candidate"
            marker = "~"
        if text and len(text) > 72:
            text = text[:69] + "..."
        parts.append(f"`{message['id']:03d}`{marker}{text}@`{message['offset']}`")
    if total_count > len(messages):
        parts.append(f"... +{total_count - len(messages)} more")
    return "<br>".join(parts)


def format_scene_state(global_reads: list[dict], global_writes: list[dict], object_reads: list[dict], object_writes: list[dict]) -> str:
    parts = []
    if global_reads:
        parts.append("global reads: " + ", ".join(f"`{var}`" for var in unique_limited(read["var"] for read in global_reads)))
    if global_writes:
        parts.append("global writes: " + ", ".join(f"`{item}`" for item in unique_limited(f"{write['var']}={write['value']}" for write in global_writes)))
    if object_reads:
        parts.append("object reads: " + ", ".join(unique_limited(f"`{read['object']}`{object_label(read.get('objects', []))}" for read in object_reads)))
    if object_writes:
        parts.append("object writes: " + ", ".join(unique_limited(f"`{write['object']}={write['value']}`{object_label(write.get('objects', []))}" for write in object_writes)))
    return "<br>".join(parts) or "-"


def format_scene_objects(object_refs: list[dict]) -> str:
    parts = []
    for ref in object_refs[:12]:
        label = object_label(ref.get("objects", []))
        fields = ",".join(ref.get("fields", [])[:5])
        field_text = f" fields {fields}" if fields else ""
        parts.append(f"`{ref['object']}`{label}{field_text}")
    if len(object_refs) > 12:
        parts.append(f"... +{len(object_refs) - 12} referenced")

    return "<br>".join(parts) or "-"


def format_loaded_sets(loaded_sets: list[dict]) -> str:
    if not loaded_sets:
        return "-"
    parts = []
    for loaded_set in loaded_sets[:8]:
        matches = loaded_set.get("matches", [])
        detail = ""
        if matches:
            match = matches[0]
            detail = f" {match['frame_count']} frames from `{match['bundle']}`"
        parts.append(
            f"`{loaded_set['resource']}` -> anim `{loaded_set['frame_range'][0]}-{loaded_set['frame_range'][1]}`{detail}"
        )
    if len(loaded_sets) > 8:
        parts.append(f"... +{len(loaded_sets) - 8} more")
    return "<br>".join(parts)


def format_object_initial(obj: dict) -> str:
    bits = []
    for key in ("x", "y", "frame", "status", "part"):
        value = obj.get(key)
        if value is not None:
            bits.append(f"{key} `{value}`")
    return ", ".join(bits) or "-"


def format_object_scene_refs(refs: list[dict]) -> str:
    if not refs:
        return "-"
    parts = []
    seen = set()
    for ref in refs:
        script = ref["script"]
        backgrounds = ",".join(ref.get("backgrounds", [])[:2])
        key = (script["group"], script["resource"], script["script"], ref.get("field"), backgrounds)
        if key in seen:
            continue
        seen.add(key)
        bg = f" bg `{backgrounds}`" if backgrounds else ""
        parts.append(f"{format_script_ref(script)}@`{ref['offset']}` {ref.get('field')}{bg}")
        if len(parts) >= 8:
            break
    if len(refs) > len(parts):
        parts.append(f"... +{len(refs) - len(parts)} more")
    return "<br>".join(parts)


def format_object_interactions(interactions: list[dict]) -> str:
    if not interactions:
        return "-"
    parts = []
    seen = set()
    for interaction in interactions:
        script = interaction["script"]
        rel = interaction["rel_params"]
        key = (script["group"], script["resource"], script["script"], interaction["source_param"])
        if key in seen:
            continue
        seen.add(key)
        action = rel.get("p1_label") or ACTION_LABELS.get(rel.get("p1"), "")
        action_text = f" {action}" if action else ""
        summary = interaction.get("summary") or ""
        if len(summary) > 100:
            summary = summary[:97] + "..."
        suffix = f" - {summary}" if summary else ""
        parts.append(
            f"{format_script_ref(script)} `{interaction['source_param']}` p1 `{rel['p1']}`{action_text}{suffix}"
        )
        if len(parts) >= 8:
            break
    if len(interactions) > len(parts):
        parts.append(f"... +{len(interactions) - len(parts)} more")
    return "<br>".join(parts)


def format_object_status_refs(refs: list[dict], include_values: bool) -> str:
    if not refs:
        return "-"
    parts = []
    seen = set()
    for ref in refs:
        script = ref["script"]
        value = f"={ref['value']}" if include_values and "value" in ref else ""
        key = (script["group"], script["resource"], script["script"], ref["offset"], value)
        if key in seen:
            continue
        seen.add(key)
        parts.append(f"{format_script_ref(script)}@`{ref['offset']}`{value}")
        if len(parts) >= 8:
            break
    if len(refs) > len(parts):
        parts.append(f"... +{len(refs) - len(parts)} more")
    return "<br>".join(parts)


def format_beat_messages(messages: list[dict]) -> str:
    if not messages:
        return "-"
    parts = []
    for message in messages[:10]:
        text = message.get("message_text", "")
        if not text and len(message.get("text_candidates", [])) == 1:
            text = message["text_candidates"][0]["text"]
        marker = "=" if text else ""
        if not text and message.get("text_candidates"):
            text = "candidate"
            marker = "~"
        if text and len(text) > 90:
            text = text[:87] + "..."
        parts.append(f"`{message['id']:03d}`{marker}{text}@`{message['offset']}`")
    if len(messages) > 10:
        parts.append(f"... +{len(messages) - 10} more")
    return "<br>".join(parts)


def format_beat_state(script_entry: dict) -> str:
    parts = []
    global_writes = script_entry.get("global_writes", [])
    object_writes = script_entry.get("object_status_writes", [])
    transitions = script_entry.get("transition_candidates", [])
    if global_writes:
        parts.append("global: " + ", ".join(
            f"`{write['var']}={write['value']}`@`{write['offset']}`"
            for write in global_writes[:8]
        ))
    if object_writes:
        parts.append("objects: " + ", ".join(
            f"`{write['object']}={write['value']}`{object_label(write.get('objects', []))}@`{write['offset']}`"
            for write in object_writes[:8]
        ))
    if transitions:
        parts.append("transitions: " + format_scene_exits(transitions))
    return "<br>".join(parts) or "-"


def format_beat_objects(objects: list[dict]) -> str:
    if not objects:
        return "-"
    parts = []
    for obj in objects[:16]:
        name = obj.get("name") or f"object {obj['id']}"
        prefix = "~" if obj.get("candidate") else ""
        source = obj.get("group") or ""
        parts.append(
            f"`{obj['id']}` {prefix}{name} ({source}/{obj['object_file']}, status `{obj.get('status')}`, part `{obj.get('part')}`)"
        )
    if len(objects) > 16:
        parts.append(f"... +{len(objects) - 16} more")
    return "<br>".join(parts)


def beat_candidate_note(beat: dict) -> str:
    for note in beat.get("notes", []):
        if "production watcher candidate" in note:
            return note
    for note in beat.get("notes", []):
        if "watcher" in note.lower() or "candidate" in note.lower():
            return note
    return beat.get("description", "")


def format_short_text(text: str, limit: int = 180) -> str:
    text = (text or "").replace("|", "\\|").strip()
    if len(text) > limit:
        return text[: limit - 3] + "..."
    return text or "-"


def unique_limited(values, limit: int = 10) -> list:
    result = []
    for value in values:
        if value in result:
            continue
        result.append(value)
        if len(result) >= limit:
            break
    return result


def format_scene_exits(transitions: list[dict]) -> str:
    exits = []
    for transition in transitions:
        for destination in transition.get("destinations", []):
            exits.append(f"`{destination.get('globalVar240')}:{destination.get('globalVar241')}`@`{transition['at']}`")
    return "<br>".join(exits) or "-"


def format_scene_entries(entries: list[dict]) -> str:
    if not entries:
        return "-"
    parts = []
    seen = set()
    for entry in entries:
        key = (
            entry["from"]["group"],
            entry["from"]["resource"],
            entry["from"]["script"],
            entry.get("destination"),
            entry.get("entry"),
        )
        if key in seen:
            continue
        seen.add(key)
        parts.append(
            f"{format_script_ref(entry['from'])} -> `{entry.get('destination')}:{entry.get('entry')}`@`{entry['offset']}`"
        )
        if len(parts) >= 8:
            break
    if len(entries) > len(parts):
        parts.append(f"... +{len(entries) - len(parts)} more")
    return "<br>".join(parts)


def flag_rows(flag_entries: dict, interesting_ids: set[str] | None = None) -> list[tuple[str, dict]]:
    rows = []
    for flag_id, entry in flag_entries.items():
        if interesting_ids and flag_id not in interesting_ids:
            continue
        if not entry.get("writers") and not entry.get("readers"):
            continue
        rows.append((flag_id, entry))
    return rows


def write_markdown(atlas: dict, output: Path) -> None:
    lines = [
        "# Operation Stealth Static Game Atlas",
        "",
        "Generated from the decompiled Operation Stealth EU VGA script dumps.",
        "This is a structural map for analysis and achievement design, not a list of final hooks.",
        "",
        "## How To Read This",
        "",
        "- PRC scripts are room/control scripts: they load backgrounds, parts, masks, and often handle collision exits.",
        "- REL scripts are object/action scripts: their `p1/p2/p3` parameters identify action metadata from the REL table.",
        "- A likely screen transition is a script that writes `globalVar[240]` and/or `globalVar[241]`, then starts global script 2.",
        "- Object `status` writes are durable state candidates, but their meaning must be interpreted in context.",
        "- Message IDs are anchors into the active MSG file and are useful for finding story beats and optional content.",
        "",
        f"Source: `{atlas['source']}`",
        f"Resource source: `{atlas['resource_source']}`",
        f"Scripts indexed: {atlas['script_count']}",
        "",
        "## Part / Bundle Index",
        "",
        "| Group | PRC resources | REL resources | Scripts | Transition candidates | Messages | Object state writes |",
        "| --- | --- | --- | ---: | ---: | ---: | ---: |",
    ]

    for group, scripts in atlas["groups"].items():
        prcs = sorted({s["resource"] for s in scripts if s["kind"] == "prc"})
        rels = sorted({s["resource"] for s in scripts if s["kind"] == "rel"})
        lines.append(
            f"| `{group}` | {', '.join(f'`{p}`' for p in prcs) or '-'} | "
            f"{', '.join(f'`{r}`' for r in rels) or '-'} | {len(scripts)} | "
            f"{sum(len(s['transition_candidates']) for s in scripts)} | "
            f"{sum(len(s['messages']) for s in scripts)} | "
            f"{sum(len(s['object_status_writes']) for s in scripts)} |"
        )

    if REPEATED_GROUP_NOTES:
        lines.extend(["", "## Repeated Group Notes", ""])
        lines.append("Some script groups intentionally repeat resources across story phases. Use this section before treating repeated VILLE/GROTTES rows as separate evidence.")
        lines.extend(["", "| Groups | Canonical first pass | Note |", "| --- | --- | --- |"])
        for note in REPEATED_GROUP_NOTES:
            groups = ", ".join(f"`{group}`" for group in note["groups"])
            canonical = f"`{note['canonical']}`" if note.get("canonical") else "-"
            lines.append(f"| {groups} | {canonical} | {note['note']} |")

    if atlas.get("beat_dossiers"):
        lines.extend(["", "## Story / Achievement Beat Quick Index", ""])
        lines.append("Use this as the first stop for achievement design. The detailed dossiers later in the file contain the supporting script/message/state rows.")
        lines.extend(["", "| Beat | Status | Candidate / caution |", "| --- | --- | --- |"])
        for beat in atlas["beat_dossiers"]:
            lines.append(
                f"| `{beat['id']}` {beat['title']} | `{beat['status']}` | "
                f"{format_short_text(beat_candidate_note(beat))} |"
            )

    if atlas.get("messages"):
        lines.extend(["", "## Message Tables", ""])
        lines.extend(["| Group | MSG files |", "| --- | --- |"])
        for group, files in atlas["messages"].items():
            msg_files = ", ".join(f"`{name}` ({count})" for name, count in files.items())
            lines.append(f"| `{group}` | {msg_files or '-'} |")

    if atlas.get("message_usage"):
        lines.extend(["", "## Message Text Index", ""])
        lines.append("This lists every decoded MSG row, plus direct and candidate script references. Candidate references mean the message ID matched more than one table or the active table could not be pinned statically.")
        for group, files in atlas["message_usage"].items():
            lines.extend([f"### {group}", ""])
            for msg_file, entries in files.items():
                lines.extend([f"#### {msg_file}", "", "| ID | Text | Direct refs | Candidate refs |", "| ---: | --- | --- | --- |"])
                for _message_id, entry in entries.items():
                    lines.append(
                        f"| `{entry['id']:03d}` | {format_message_text(entry.get('text', ''))} | "
                        f"{format_message_refs(entry.get('refs', []))} | "
                        f"{format_message_refs(entry.get('candidate_refs', []))} |"
                    )
                lines.append("")

    if atlas.get("objects"):
        lines.extend(["", "## Object Tables", ""])
        lines.append("Object tables are decoded from `*.OBJ` resources using the engine loader format. The `status` field is the initial object costume/status value before scripts mutate it.")
        lines.extend(["", "| Group | OBJ file | Entries | Named entries | Notable names |", "| --- | --- | ---: | ---: | --- |"])
        for group, group_objects in atlas["objects"].items():
            for object_file, table in group_objects.items():
                entries = table.get("entries", [])
                named_entries = [entry for entry in entries if entry.get("name")]
                notable = []
                for entry in named_entries:
                    name = entry.get("name", "")
                    if name and name not in notable:
                        notable.append(name)
                    if len(notable) >= 12:
                        break
                lines.append(
                    f"| `{group}` | `{object_file}` | {len(entries)} | {len(named_entries)} | "
                    f"{', '.join(f'`{name}`' for name in notable) or '-'} |"
                )

    if atlas.get("action_labels"):
        lines.extend(["", "## REL Action Parameter Working Labels", ""])
        lines.append("These labels are inferred from repeated object/message evidence, not from a finalized action-table decoder.")
        lines.extend(["", "| p1 | Working label |", "| ---: | --- |"])
        for action_id, label in sorted(atlas["action_labels"].items(), key=lambda item: int(item[0])):
            lines.append(f"| `{action_id}` | {label} |")

    if atlas.get("scenes"):
        lines.extend(["", "## Scene / Room Control Index", ""])
        lines.append("This indexes PRC room/control scripts by loaded background, control table, transitions, collisions, and state writes.")
        for group, scenes in atlas["scenes"].items():
            lines.extend([f"### {group}", "", "| Script | Backgrounds | Control tables | CT zones | Transitions | Collisions | Summary |", "| --- | --- | --- | --- | --- | ---: | --- |"])
            for scene in scenes:
                script = scene["script"]
                backgrounds = ", ".join(f"`{bg['name']}`" for bg in scene["backgrounds"]) or "-"
                control_tables = ", ".join(f"`{ct['args']}`" for ct in scene["control_tables"]) or "-"
                ct_zones = format_control_table_zones(scene["control_tables"])
                transitions = []
                for transition in scene["transitions"]:
                    for destination in transition.get("destinations", []):
                        transitions.append(f"`{destination.get('globalVar240')}:{destination.get('globalVar241')}`")
                lines.append(
                    f"| `{script['resource']}_{script['script']:03d}` | {backgrounds} | {control_tables} | {ct_zones} | "
                    f"{', '.join(transitions) or '-'} | {len(scene['collisions'])} | {scene.get('summary') or '-'} |"
                )
            lines.append("")

    if atlas.get("scenes"):
        lines.extend(["", "## Scene Dossiers", ""])
        lines.append("Expanded PRC room/control view. Each row groups the evidence needed to document a screen: loaded visual/control resources, exits, collision zones, messages, and state reads/writes.")
        for group, scenes in atlas["scenes"].items():
            lines.extend([f"### {group}", "", "| Scene | Visuals / loads | Entry candidates | Exits | Collision zones | Objects | Messages | State touches | Summary |", "| --- | --- | --- | --- | --- | --- | --- | --- | --- |"])
            for scene in scenes:
                script = scene["script"]
                backgrounds = ", ".join(f"`{bg['name']}`" for bg in scene["backgrounds"]) or "-"
                load_text = format_loads(scene["loads"])
                set_text = format_loaded_sets(scene.get("loaded_sets", []))
                visual_text = backgrounds
                if load_text != "-":
                    visual_text += "<br>" + load_text
                if set_text != "-":
                    visual_text += "<br>" + set_text
                lines.append(
                    f"| `{script['resource']}_{script['script']:03d}` | {visual_text} | "
                    f"{format_scene_entries(scene.get('incoming', []))} | "
                    f"{format_scene_exits(scene['transitions'])} | "
                    f"{format_control_table_zones(scene['control_tables'])} | "
                    f"{format_scene_objects(scene.get('object_refs', []))} | "
                    f"{format_scene_messages(scene['messages'], scene.get('message_count', len(scene['messages'])))} | "
                    f"{format_scene_state(scene.get('global_reads', []), scene.get('global_writes', []), scene.get('object_status_reads', []), scene.get('object_status_writes', []))} | "
                    f"{scene.get('summary') or '-'} |"
                )
            lines.append("")

    lines.extend(["", "## Destination Index", ""])
    lines.append("This section indexes observed `globalVar[240]` transition values. Direct matches are scripts with the same number. Encoded matches are a conservative hint for three-digit destinations where the low two digits match a script number; treat those as leads, not proof.")
    lines.extend(["", "| Destination | Observed from | Direct script matches | Encoded script matches |", "| --- | --- | --- | --- |"])
    for destination, entries in atlas.get("destinations", {}).items():
        observed = []
        direct_matches = {}
        encoded_matches = {}
        for entry in entries:
            source = entry["from"]
            observed.append(
                f"`{source['group']}/{source['resource']}_{source['script']:03d}` -> entry `{entry.get('entry')}`"
            )
            for match in entry.get("direct_script_matches", []):
                direct_matches[f"{match['group']}/{match['resource']}_{match['script']:03d}"] = match
            for match in entry.get("encoded_script_matches", []):
                encoded_matches[f"{match['group']}/{match['resource']}_{match['script']:03d}"] = match

        def format_matches(matches: dict) -> str:
            if not matches:
                return "-"
            formatted = []
            for key, match in sorted(matches.items()):
                extras = []
                if match.get("backgrounds"):
                    extras.append("bg " + ",".join(match["backgrounds"][:2]))
                if match.get("messages"):
                    extras.append("msg " + ",".join(str(m) for m in match["messages"][:4]))
                suffix = f" ({'; '.join(extras)})" if extras else ""
                formatted.append(f"`{key}`{suffix}")
            return "<br>".join(formatted[:8])

        lines.append(
            f"| `{destination}` | {'<br>'.join(observed[:8])} | "
            f"{format_matches(direct_matches)} | {format_matches(encoded_matches)} |"
        )

    lines.extend(["", "## Transition Candidates", ""])
    for group, scripts in atlas["groups"].items():
        rows = []
        for script in scripts:
            for transition in script["transition_candidates"]:
                rows.append((script, transition))
        if not rows:
            continue
        lines.extend([f"### {group}", "", "| Script | Destination | Evidence |", "| --- | --- | --- |"])
        for script, transition in rows:
            evidence = "; ".join(line["text"] for line in transition["lines"])
            destinations = ", ".join(
                f"`{destination.get('globalVar240')}:{destination.get('globalVar241')}`"
                for destination in transition.get("destinations", [])
            )
            lines.append(
                f"| `{script['kind']}/{script['resource']}_{script['script']:03d}` | {destinations or '-'} | "
                f"{evidence}; startGlobalScript(2) |"
            )
        lines.append("")

    if atlas.get("object_interactions"):
        lines.extend(["## Object Interaction Index", ""])
        lines.append("This links REL table parameters back to decoded OBJ entries when `p2` or `p3` matches an object ID in the same resource group. It is an index of likely object/action scripts, not a final action-name decoder.")
        lines.append("Rows prefixed with `~` are cross-table candidates from another OBJ bundle because the current script group has no local OBJ table for that ID.")
        for group, files in atlas["object_interactions"].items():
            lines.extend([f"### {group}", ""])
            for object_file, interactions in files.items():
                lines.extend([f"#### {object_file}", "", "| Object | From | Script | REL params | Summary |", "| --- | --- | --- | --- | --- |"])
                for interaction in interactions:
                    obj = interaction["object"]
                    script = interaction["script"]
                    rel = interaction["rel_params"]
                    object_name = obj.get("name") or f"object {obj['id']}"
                    candidate_prefix = "~" if obj.get("candidate") else ""
                    source_suffix = ""
                    if obj.get("candidate") and obj.get("source_group"):
                        source_suffix = f" from `{obj['source_group']}`"
                    action_label = rel.get("p1_label")
                    rel_text = f"`p1={rel['p1']} p2={rel['p2']} p3={rel['p3']}`"
                    if action_label:
                        rel_text += f" ({action_label})"
                    lines.append(
                        f"| `{obj['id']}` {candidate_prefix}{object_name}{source_suffix} | `{interaction['source_param']}` | "
                        f"`{script['kind']}/{script['resource']}_{script['script']:03d}` | "
                        f"{rel_text} | "
                        f"{interaction.get('summary') or '-'} |"
                    )
                lines.append("")

    if atlas.get("object_dossiers"):
        lines.extend(["## Object Dossiers", ""])
        lines.append("This pivots the atlas around objects. It combines decoded OBJ table entries, PRC scene references, REL interactions, and status reads/writes. Blank unnamed object slots with no evidence are omitted from the Markdown.")
        for group, files in atlas["object_dossiers"].items():
            lines.extend([f"### {group}", ""])
            for object_file, entries in files.items():
                if not entries:
                    continue
                lines.extend([f"#### {object_file}", "", "| Object | Initial | Scene refs | Interactions | Status writes | Status reads |", "| --- | --- | --- | --- | --- | --- |"])
                for entry in entries:
                    obj = entry["object"]
                    name = obj.get("name") or f"object {obj['id']}"
                    candidate_prefix = "~" if obj.get("candidate") else ""
                    lines.append(
                        f"| `{obj['id']}` {candidate_prefix}{name} | "
                        f"{format_object_initial(obj)} | "
                        f"{format_object_scene_refs(entry.get('scene_refs', []))} | "
                        f"{format_object_interactions(entry.get('interactions', []))} | "
                        f"{format_object_status_refs(entry.get('status_writes', []), True)} | "
                        f"{format_object_status_refs(entry.get('status_reads', []), False)} |"
                    )
                lines.append("")

    if atlas.get("beat_dossiers"):
        lines.extend(["## Story / Achievement Beat Dossiers", ""])
        lines.append("These are named story-beat groupings over the static atlas evidence. They are documentation aids only; they do not create or imply production achievement hooks.")
        for beat in atlas["beat_dossiers"]:
            lines.extend([f"### {beat['title']}", ""])
            lines.append(f"Status: `{beat['status']}`")
            lines.append("")
            lines.append(beat["description"])
            if beat.get("notes"):
                lines.append("")
                for note in beat["notes"]:
                    lines.append(f"- {note}")
            lines.extend(["", "| Script | Messages | State / transitions | Summary |", "| --- | --- | --- | --- |"])
            for script_entry in beat.get("scripts", []):
                script = script_entry["script"]
                if script_entry.get("missing"):
                    lines.append(f"| {format_script_ref(script)} | missing | missing | missing |")
                    continue
                summary = script_entry.get("summary") or "-"
                lines.append(
                    f"| {format_script_ref(script)} | {format_beat_messages(script_entry.get('messages', []))} | "
                    f"{format_beat_state(script_entry)} | {summary} |"
                )
            lines.extend(["", "| Objects |", "| --- |"])
            lines.append(f"| {format_beat_objects(beat.get('objects', []))} |")
            lines.append("")

    if atlas.get("flag_index"):
        lines.extend(["## Flag / State Index", ""])
        lines.append("This index lists scripts that read or write global variables and object status fields. It is intentionally mechanical; meanings belong in the hand-written atlas once a branch is understood.")

        global_rows = flag_rows(atlas["flag_index"].get("globals", {}))
        if global_rows:
            lines.extend(["", "### Global Variables", "", "| Flag | Writers | Readers |", "| ---: | --- | --- |"])
            for flag_id, entry in global_rows:
                lines.append(
                    f"| `{flag_id}` | {format_flag_refs(entry.get('writers', []), True)} | "
                    f"{format_flag_refs(entry.get('readers', []), False)} |"
                )

        object_rows = flag_rows(atlas["flag_index"].get("object_status", {}))
        if object_rows:
            lines.extend(["", "### Object Status Fields", "", "| Object | Label | Writers | Readers |", "| ---: | --- | --- | --- |"])
            for object_id, entry in object_rows:
                label = ""
                for access in entry.get("writers", []) + entry.get("readers", []):
                    label = object_label(access.get("objects", []))
                    if label:
                        break
                lines.append(
                    f"| `{object_id}` | {label or '-'} | {format_flag_refs(entry.get('writers', []), True)} | "
                    f"{format_flag_refs(entry.get('readers', []), False)} |"
                )

    lines.extend(["## Message And State Heavy Scripts", ""])
    for group, scripts in atlas["groups"].items():
        interesting = [
            script for script in scripts
            if script["messages"] or script["object_status_writes"] or script["backgrounds"]
        ]
        if not interesting:
            continue
        lines.extend([f"### {group}", "", "| Script | REL params | Summary |", "| --- | --- | --- |"])
        for script in interesting:
            rel = script["rel_params"]
            rel_text = "-" if not rel else f"`p1={rel['p1']} p2={rel['p2']} p3={rel['p3']}`"
            if rel and rel.get("p1_label"):
                rel_text += f" ({rel['p1_label']})"
            summary = summarize_script(script)
            if summary:
                lines.append(f"| `{script['kind']}/{script['resource']}_{script['script']:03d}` | {rel_text} | {summary} |")
        lines.append("")

    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("dump_root", type=Path)
    parser.add_argument("--json-out", type=Path)
    parser.add_argument("--md-out", type=Path)
    args = parser.parse_args()

    atlas = build_atlas(args.dump_root)

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(atlas, indent=2), encoding="utf-8")
    if args.md_out:
        args.md_out.parent.mkdir(parents=True, exist_ok=True)
        write_markdown(atlas, args.md_out)

    print(f"Indexed {atlas['script_count']} scripts from {args.dump_root}")


if __name__ == "__main__":
    main()
