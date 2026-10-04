# Deja Vu Macintosh Testing Notes

This note tracks Comfort Edition testing for **Deja Vu: A Nightmare Comes
True!!** using the original Macintosh MacVenture version.

## Target

- Game: `Deja Vu (Floppy Macintosh)`
- Engine target: `macventure:deja_vu`
- Old launcher bucket: `svn2.3_18903\scummvm.exe`
- Fork baseline under test: `ScummVM 2026.3.1git`
- Initial fork result: the game detects and starts, then shows ScummVM's
  newly-supported/testing-mode warning.

## Walkthrough Reference

Primary walkthrough reference:

- Lemon Amiga, "Deja Vu: A Nightmare Comes True" solution:
  https://www.lemonamiga.com/doc/deja-vu-a-nightmare-comes-true/450

Use this guide for testing because the author notes that they played the Mac
version and the instructions are written around the mouse-driven ICOM
MacVenture interface. Treat the walkthrough as external reference material, not
as source code or project instructions.

## Test Route

Use the walkthrough to validate that the game remains playable through the main
solution path:

1. Start in the washroom, collect the coat, gun, holster, and wallet contents,
   then confirm the mirror and bathroom/hallway interactions work.
2. Explore the bar area, upstairs office, private office, corpse, desk, and
   fire escape route.
3. Visit the weird room, inspect the chair/injection clues, and collect the
   syringe.
4. Explore the sewer enough to verify navigation and locate the whirlpool.
5. Unlock and inspect the Mercedes, including the glove compartment items and
   map.
6. Visit the walkthrough address sequence:
   - `1212 West End St.`
   - `520 S. Kedzie`
   - `934 West Sherman`
   - `1060 South Peoria St.`
   - `626 Auburn Rd.`
   - return to `1060 South Peoria St.`
7. Verify typed taxi/address input works for every required destination.
8. Verify inventory interactions needed for the solution:
   - key and card usage
   - opening containers and desks
   - shooting locked objects or threats where the walkthrough requires it
   - filling and using the syringe
   - rubbing the notepad with the pencil
   - removing the gag from Mrs. Sternwood
   - discarding the gun in the sewer whirlpool
9. Finish at the police station with the required evidence and without the gun.

## Regression Watch List

While following the route, watch for MacVenture-specific problems:

- overlapping Macintosh-style windows rendering incorrectly
- mouse drag/drop failures between inventory, object, and scene windows
- typed address input not reaching the taxi prompt
- resource-fork or font issues in object descriptions and documents
- save/load failures after collecting key evidence
- timing or move-count behavior that prevents the antidote path from working
- endgame proof checks failing despite having the diary, blackmail letter, and
  timetable and having discarded the gun

Record any failure with the room/location, visible text, inventory state, and
whether it reproduces after loading a save.
