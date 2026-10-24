# Space Quest V Battle Cruiser cheats

SQ5's Cheats tab exposes three independent options, off by default:

| Option | Config key | Effect |
| --- | --- | --- |
| Infinite probes | `sq5_infinite_probes` | Refunds probe consumption and restores availability when a loaded save has zero probes. |
| Probes do not take a turn | `sq5_free_probes` | Returns control to Roger after scanning without advancing to Quirk's turn. |
| Probes reveal the entire board | `sq5_full_board_probes` | Reveals occupied enemy cells in all three sectors when a probe fires. |

The normal CE master cheat switch and playthrough rules apply. With only the
free-turn option enabled, probes are still finite. With only the full-board
option enabled, a probe still costs one probe and one turn. Torpedoes keep their
normal behavior. Revealed cells stay revealed if the option is subsequently
disabled, as with ordinary probes; the cheat does not undo information saved in
the game.

The VM hook checks the original script 850 launchProbe bytecode and its locals
layout before acting. The validated resource is DOS 1.03/1.04 script 850, shared
by the locally prepared Trash Talkin' Edition. Unrecognized layouts are ignored.
No game resources or saves are rewritten. Full-board scanning sets only the
enemy occupied-cell reveal bits; damage, empty cells, player ships, and ship
destruction counters are preserved. Free scanning uses launchProbe's original
handsOn/cleanup branch without modifying its turn-owner local.

## Verification

On October 24, 2026, the Windows Release x64 build completed with zero warnings
and errors. Native checks passed for all eight option combinations and the
original script 850 signatures. Isolated startup checks passed with cheats
enabled and disabled. The user subsequently confirmed that the cheats work
in-game. This confirmation does not establish testing of every localized edition
or every manual acceptance case below.

`devtools/ce/test_sq5_battle_cheats.cpp` is a standalone native test of the actual
hook helpers. Build it with the repository's Visual Studio BuildTools environment,
including the repository root and `engines` as include directories. Assertions
must remain enabled. Passing an optional locally extracted, decompressed script
850 also verifies the real bytecode signatures. Do not commit extracted resources.

The checks cover all eight option combinations, repeated unlimited use, restoring
exhausted probes, all 300 packed enemy cells, both cell bytes, existing damage and
reveal flags, preservation of the player's board, opponent-turn isolation, and
rejection of mismatched signatures, truncated scripts, and incorrect locals.

Native tests and bytecode checks do not establish visible in-game interaction.
For manual acceptance, start Battle Cruiser and try each option separately and
together. Confirm the count, sector reveals and turn transitions, then restore a
save with no probes. With cheats disabled, confirm the normal finite 4x4 scan,
Quirk's response, torpedoes and win/score behavior.
