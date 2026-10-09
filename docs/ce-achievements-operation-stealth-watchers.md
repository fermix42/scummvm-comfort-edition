# Operation Stealth CE Achievement Watcher Map

This document maps the documented Comfort Edition achievement keys to the
smallest engine-side watcher facts we currently need. The CE API still uses the
stable achievement keys from `docs/scummvm-ce-api.md`; the event names here are
local app-side rule inputs only.

Do not add broad or speculative event families. Add an event only after the
corresponding script branch, room transition, or state change has been anchored
in the static data or a trace has explained what the static dump missed.

## Implementation Order

Follow the website milestone order, not easiest-static-anchor order:

1. `passport_to_trouble` - implemented and live validated.
2. `shave_and_a_briefing` - implemented and live validated.
3. `say_it_with_flowers` - implemented and live validated.
4. `safe_deposit_unsafe_withdrawal` - implemented and live validated.
5. `escape_clause` - implemented and live validated.
6. `two_for_the_surface` - implemented and live validated.
7. `palace_intrigue` - implemented, pending live validation.
8. `a_safe_bet` - implemented and live validated.
9. `making_waves`
10. `under_new_management`
11. `pen_mightier_than_piranhas`
12. `rats_all_folks`
13. `dressed_to_infiltrate`
14. `fingerprint_fiction`
15. `officially_unofficial`
16. `virus_successfully_installed`
17. `order_of_the_banana`
18. `well_read_intruder` - implemented, pending live validation.
19. `corporate_espionage`

After the static atlas pass, implementation still follows website/play order.
Hook confidence decides how much validation each slice needs, not which
achievement jumps the queue.

## Implemented

Live play validation has been done on the European DOS VGA data. Cross-version
support is static-validated against fresh full dumps for US DOS VGA, European
Amiga, and European Atari ST in:

- `script-dumps/operation-stealth-us-vga-full-20261008`
- `script-dumps/operation-stealth-amiga-full-20261008`
- `script-dumps/operation-stealth-atari-full-20261008`

Do not treat the older concise non-EU dumps as complete evidence; several stop
at the first `break()` and hide the late branches used by achievement hooks.
Where Amiga/Atari or US VGA byte offsets differ, the runtime watcher accepts
only the exact alternate offset found in those fresh full dumps while preserving
the same resource, script, message/object, and state checks.

| Achievement | Needed game fact | Current event | Evidence / hook |
| --- | --- | --- | --- |
| `passport_to_trouble` | Customs accepts the correct forged passport. The correct passport may be English, French, or German depending on the playthrough. | `customs_passport_accepted` | Implemented in `engines/cine/script_fw.cpp` for both observed acceptance branches. DOS VGA uses message line `303` / state line `364`; Amiga/Atari use message line `309` / state line `355`. Routed through the generic CE rule table to `passport_to_trouble`. |
| `shave_and_a_briefing` | The airport razor recording is heard. | `razor_recording_heard` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `AIRPORT.PRC` + `AEROPORT.REL` / `AIRPORT.REL` + `AEROPORT.MSG`, script 95, line 95, message 143. This keeps the early recording separate from the later `DOUCHE` / `SALLE59` razor diversion. |
| `say_it_with_flowers` | The player meets the park contact, takes the card/key information, then leaves the park screen left to the base as part of the drive-by encounter aftermath. | `park_contact_info_obtained` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `AIRPORT.PRC`, script 13, `globalVar[240] = 43`, the left/base exit from park background `22.PI1` to `43J.PI1`. DOS VGA uses line `271`; Amiga/Atari use line `279`. The hook requires the traced post-drive-by/contact state: `globalVar[11] = 1`, `obj[88].status = 3`, and `obj[65].status = -3`, so ordinary park exits before the drive-by/card pickup do not count. This replaces the failed `VILLE.REL_176` / `33:4` watcher and the later failed `AIRPORT.PRC_024` / `371:3` guess. |
| `safe_deposit_unsafe_withdrawal` | The bank device/envelope branch reaches the ambush transition. | `bank_ambush_reached` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `VILLE.REL` + `VILLE.MSG`, either script 163 line 16 after taking the envelope or script 164 line 27 after opening it, when `globalVar[240] = 26` and `globalVar[241] = 101`. The hook requires `obj[79].status = -3` so retrieving the little box/device is part of the achievement, and envelope-only paths do not count. |
| `escape_clause` | The player escapes both the mine/cave and the flooded-water tunnel sequence. | `mine_tunnels_escaped` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `AIRPORT.PRC`, script 25, when the flooded tunnel sequence exits from `30.PI1` to `globalVar[240] = 21` while `globalVar[241]` is still `1`; DOS VGA uses line `319`, Amiga/Atari use line `335`. This replaces the too-early `AIRPORT.PRC_018` hook, which only entered the flooded sequence at `28:1` and did not satisfy the full achievement description. |
| `two_for_the_surface` | The player rescues Julia underwater and surfaces together. | `julia_rescued_underwater` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `BATEAU7.PRC`, script 27, when `globalVar[240] = 42` while `globalVar[241] = 1`; DOS VGA uses line `623`, Amiga/Atari use line `652`. This is the static atlas boundary after rescue message `20`, after John and Julia are boarded onto Zodiac overlay objects `201` and `202`, and immediately before dispatcher script 2 enters the safe post-rescue briefing in `BATEAU7.PRC_045` / room `42`. Avoids underwater entry, intermediate survival in room `41`, and drowning/failure script `013`. |
| `palace_intrigue` | The player exits the final palace maze and reaches the palace interior route toward the office. | `palace_office_reached` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `LABY.PRC` + `LABY.REL` / `LABY.MSG`, script 9, when the maze-completion branch writes `globalVar[240] = 46` after `globalVar[229] = 9`; EU VGA/Amiga/Atari use line `482`, US VGA uses line `425`. The same branch then loads `PALAIS1.PRC` / `PALAIS.REL` / `PALAIS.OBJ` / `PALAIS.MSG`, and `PALAIS1.PRC_001` routes to `46:2` with `globalVar[243] = 44`; `PALAIS1.PRC_013` loads `46.PI1`. This replaces the failed `PALAIS1.PRC_021` room-50 hook and the later `PALAIS1.PRC_022` / `50.PI1` hook. Avoid `PALAIS.REL_000` message `12` (`IT'S OPENING!!!`), which is an earlier hotel/hall-door action, and avoid `PALAIS1.PRC_022` / `globalVar[243] = -50`, which belongs to the envelope-submarine branch. |
| `a_safe_bet` | The player opens the palace safe and takes the envelope. | `palace_safe_envelope_recovered` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `PALAIS1.PRC` + `PALAIS.REL` / `PALAIS.MSG`, script 11, line 2, when envelope object `164` status changes from `-1` to `-3`. Trace showed object script entry 11 running with params `1,164,65535`, then line 8 setting `globalVar[243] = -1`. Avoid safe-open message `17`, solved-combination state alone, countdown/explosion messages, and the later `PALAIS1.PRC_022` documents/submarine handoff. |
| `rats_all_folks` | The player clears the rat mazes inside the hidden underwater base. | `rat_mazes_cleared` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `EGOU.PRC` + `LABY.REL` / `LABY.MSG`, script 9, when `globalVar[240] = 10` from old value `8` while `globalVar[229] = 9`; EU VGA/Amiga/Atari use line `523`, US VGA uses line `451`. Trace validation of exiting rat maze 4 showed `EGOU.PRC_045`, line 176, writing `globalVar[229]` from `0` to `9`, then `EGOU.PRC_009`, line 518, writing `globalVar[199]` from `0` to `1`, followed by `EGOU.PRC_009`, line 523, writing `globalVar[240]` from `8` to `10`. Avoid initial EGOUBASE load, key pickup alone, intermediate maze starts, rat collision/death branches, and the later DOUCHE room load. |
| `dressed_to_infiltrate` | The player takes both soldier-disguise items: clothes and boots. Either order is valid, and the game automatically dresses John after the second item. | `soldier_disguise_acquired` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `DOUCHE6.PRC` + `DOUCHE.REL` / `DOUCHE.MSG`, script 42, line 147, when `globalVar[242]` changes from `1` to `0` while both `obj[58].status` and `obj[55].status` are `-3`. Trace showed clothes-first: `DOUCHE.REL_005`, line 108, set `obj[58].status = -3`; `DOUCHE.REL_077`, line 106, set `obj[55].status = -3`; then `DOUCHE6.PRC_042` performed the automatic costume change and hid both items. Avoid one-item pickup, soldier gag/tie actions, and later officer approval. |
| `officially_unofficial` | The forged/authorized mission order is accepted at the guard opening. | `authorized_mission_submitted` | Implemented for `DOUCHE.REL` + `DOUCHE.MSG`, script 55, message 55, after `obj[39].status = -99`; DOS VGA uses line `66`, Amiga/Atari use line `67`. Avoids finding, reading, taking, stamping, swallowing, guard decoys, and laser failure. |
| `virus_successfully_installed` | The compact disc virus is inserted in the CD player. | `stealth_virus_installed` | Implemented for `DOUCHE.REL` or `SALLE59.REL`, script 92, message 167, with `globalVar[97] = 1`; DOS VGA uses line `164`, Amiga/Atari use line `160`. Avoids CD acquisition, CD-player examine text, timeout/help text, and later finale messages. |
| `order_of_the_banana` | John receives the Santa Paragua Republic Order of the Banana. | `banana_order_awarded` | Implemented for `FIN2.PRC` + `FIN.MSG`, script 15, line 150, immediately after the repeated message-3 award dialogue finishes and before the next national-holiday speech. A runtime latch prevents repeated unlock calls during the same award animation. |
| `well_read_intruder` | The player sees all five randomized humorous palace book titles. | `humorous_book_read` | Implemented for `PALAIS.REL` + `PALAIS.MSG`, script 27, messages 41-45. Each seen title sets one bit in the active-domain `ce_os_well_read_titles_mask`; the event only fires when all five bits are present. Trace confirmed different shelf clicks still route through object 148 / `PALAIS.REL_027`, so title completion is message-based, not hotspot-based. Avoids generic library object 149 / message 46 and unrelated reading text. |
| `corporate_espionage` | John reads the Delphine Software mission order. | `mission_order_read` | Implemented for `DOUCHE.REL` + `DOUCHE.MSG`, script 51, line 0, message 51. Avoids reveal/find, take, operate/joke, swallow, stamp, and authorized-mission submission. |

## Failed / Replaced Watchers

| Achievement | Needed game fact | Current event | Evidence / hook |
| --- | --- | --- | --- |
| `say_it_with_flowers` | The player meets the park contact, takes the card/key information, then leaves the park screen left to the base as part of the drive-by encounter aftermath. | `park_contact_info_obtained` | The first production watcher checked `VILLE.REL` + `VILLE.MSG`, script 176, line 124, `globalVar[240] = 33`, after the durable card/key information state. Two live tests failed to unlock. The generated atlas later showed this was a nearby object/action transition to `33:4`, not the park screen-34 zone-3 collision exit to `371:3`. |
| `say_it_with_flowers` | The player meets the park contact, takes the card/key information, then leaves the park screen left to the base as part of the drive-by encounter aftermath. | `park_contact_info_obtained` | The second production watcher checked `AIRPORT.PRC`, script 24, line 240, `globalVar[240] = 371`, gated by `globalVar[241] = 3`, `obj[88].status = 3`, and split card/key state. A trace playthrough of the drive-by plus safe left/base exit did not execute this candidate. The trace showed the actual safe exit as `AIRPORT.PRC`, script 13, line 271, `globalVar[240] = 43`, with `globalVar[11] = 1`, `obj[88].status = 3`, and `obj[65].status = -3`. |
| `escape_clause` | The player escapes the mine/cave and reaches the flooded-water sequence. | `mine_tunnels_escaped` | The first production watcher checked `GROTTES.REL_188`, line 64, message `36` (`IT'S OPENING, JOHN!`). Live testing after the opening already existed did not trigger it because the player reached the next beat through `AIRPORT.PRC_018`'s collision/transition path. The trace showed the reliable boundary as `AIRPORT.PRC_018`, line 996, `globalVar[240] = 28`, followed by `globalVar[241] = 1` and dispatcher script 2. |
| `escape_clause` | The player escapes both the mine/cave and the flooded-water tunnel sequence. | `mine_tunnels_escaped` | The second production watcher checked `AIRPORT.PRC_018`, line `996` / `1004`, `globalVar[240] = 28`. That fired too early: it only moved from cave background `27.PI1` into flooded sequence `28:1`. The corrected boundary is `AIRPORT.PRC_025`, line `319` / `335`, `globalVar[240] = 21`, reached after swimming through `28.PI1`, `29.PI1`, and `30.PI1`. |

## Next Low-Risk Candidates

| Achievement | Documented condition | Proposed event | Static evidence found | Confidence | Next action |
| --- | --- | --- | --- | --- | --- |

## Needs More Static Tracing

| Achievement | Documented condition | Likely event shape | Current notes |
| --- | --- | --- | --- |
| `making_waves` | Complete the entire jet-ski chase. | `jetski_chase_completed` | Implemented in `engines/cine/script_fw.cpp` for Operation Stealth `PALAIS1.PRC` + `PALAIS.REL` / `PALAIS.MSG`, script 22, when message `29` (`THE DOCUMENTS ARE FINALLY YOURS!`) appears after the submarine pickup animation; DOS VGA uses line `393`, Amiga/Atari use line `451`. Trace showed the player had completed both jet-ski phases and script 22 was moving submarine objects `110`/`111` on `50.PI1` while `globalVar[240] = 50`, `globalVar[243] = -1`, and envelope object `164` was still carried. Avoid the later `SOUSMAR2.PRC_020` decompressor message `45`; that is after the pickup and belongs to the next sequence boundary. |
| `fingerprint_fiction` | Pass the fingerprint-controlled door. | `fingerprint_door_passed` | Implemented for `DOUCHE6.PRC` + `DOUCHE.REL` / `DOUCHE.MSG`, script 7, line 646, when the traced pass-through writes `globalVar[240] = 57` from old value `110` while `globalVar[241] = 3`, John is disguised, and false fingerprint object `102` exists. Trace showed `DOUCHE.REL_041` accepting fingerprint object `102` and opening lock/door object `100`, followed by walking through to room `57:1`. Avoids fingerprint creation, scanner animation, lock examination, and adjacent room movement. |

## Implemented Trace-Informed Watchers

| Achievement | Documented condition | Event | Evidence / hook |
| --- | --- | --- | --- |
| `under_new_management` | Enter the hidden underwater base. | `underwater_base_entered` | Trace of opening the porthole, climbing the ladder, and entering the base reached the visible Dr. Why welcome dialog at `SOUSMAR2.PRC_035`, message `49` (`Hello, Mr. Glames!!! How have you been enjoying our company up until now?`), followed by the `globalVar[200] = 1` completion flag that allows `SOUSMAR2.PRC_031` to continue into the base load. DOS VGA uses line `2251`, Amiga/Atari use line `2261`. The production hook uses that visible dialog, with `SOUSMARI.MSG` active and `globalVar[240] = 55`. Avoid porthole-open message `46`, the later `DOUCHE6.PRC_037` message `97`, later piranha escape state `globalVar[20] = 20`, and later infiltration rooms. |
| `pen_mightier_than_piranhas` | Escape the cage using the pen and watch. | `piranha_cage_escaped` | Trace confirmed the intended sequence: `DOUCHE.REL_110` pen-on-lock messages `189` and `91`, `DOUCHE.REL_111`/`112` watch-cord message `214`, several too-early `DOUCHE.REL_113` grill operations that ended with no state change, then the successful `DOUCHE.REL_113` writing `globalVar[20]` from `1` to `20`. DOS VGA uses line `188`, Amiga/Atari use line `191`. The production hook uses that state write with `DOUCHE6.PRC` / `DOUCHE.REL` / `DOUCHE.MSG` active. Avoid firing on pen use, the first watch cord, message `214`, movement script `39`, failed/too-early grill operations, and later `EGOU.PRC` load. |

## Working Rule

For each new achievement, do this in order:

1. Start from the documented achievement key and condition.
2. Find the exact message/script/state anchor in the static dumps for the
   relevant variants.
3. Add one small watcher event at the success branch.
4. Add a CE rule mapping that event to the documented achievement key.
5. Build once and do one meaningful live smoke test for that slice.
