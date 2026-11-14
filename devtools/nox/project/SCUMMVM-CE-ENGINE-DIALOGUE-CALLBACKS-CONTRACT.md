# Dialogue callbacks479

Status: IN PROGRESS. Defined before production changes; parent97fa864972d.
Source: Talk B2F9-B4D3, immutable map definitions and439 hash-verified packets.
Correct census uses rawAE header terminators, not low-bit46 (a valid field ID).
Local ce-native-dialogue-callbacks479-local.json records nine authored request
producers: fields12/42/58 once each and field46 six times. Field46 callbacks
also accept legitimate externally produced requests; no guessed campaign setup.

## Required complete response effects

1. Admit supported world callbacks during response resolution, while retaining
   an explicit pending owner until they commit. Finish/next response must not
   silently discard pending effects. Unsupported commerce stays explicit.
2. Field12 bit32 requests native interface repaint and is cleared. SourceFCC3
   selects display descriptors7530/7550/7570 by original display mode; native
   uses its established interface instead of adding an obsolete page/layout ABI.
3. Map68 + field58 bit32 copies authored TalkBC4B's twelve object bytes into
   object records0..11. Retain the quest flag; repeated responses are idempotent.
4. Field42 bit128 OR field46 bit2 sets object0 stateFE, clears42 bit128 and
   clears46 bit2, preserving other bytes and source ordering.
5. Field46 bit128 activates the Kayla object-state callback and is cleared.
6. Field46 bit16 sets object byte71 to36 and clears the bit.
7. Field46 bit32 sets object byte147 to36 and clears the bit.
8. Apply callbacks before presenting/continuing the response. Stage world and
   conversation together. Capture records and refresh the world underlay once
   on exit; no extra AI/RNG/clock turn or repeated item/gold handoff.
9. Persist pending/resolved callback ownership and the necessary exit repaint
   through normal native codecs; cold load must not repeat resolved effects.
10. Retain current battle-request and existing training/payment owners.
11. Body census:45 input-substitution markers already have a native owner. The
    remaining literal dollar signs are original unconfigured/default NPC text,
    not an additional executable body language. Permit printable dollar text;
    preserve the existing explicit selected $K substitution. No portrait rewrite.

## Explicit CE record reconciliation

B405 writes main object byte71 and auxiliary object byte3, then performs the
source page/presentation services. The native engine has one canonical mutable
map record owner. Preserve both explicit writes in that owner (bytes71 and3
become36); do not recreate divergent main/aux map images or retain RAM pointers.
BC4B likewise becomes immediately owned canonical object data, rather than only
an auxiliary future-load copy. These are documented native replacements for
dual-image coupling, not a universal machine-memory parity claim.

Map metadata maps TLK018 to Keep67/68/69, TLK01A to Town74, TLK068 to UM104;
never infer map IDs from resource suffixes. The accepted478 isolated packet
checks used controlled context26/type6; they qualify the response/RNG owner,
not normal authored Town74 admission. Do not rerun accepted478 to widen it.

One focused batch uses bounded original callback operations with presentation
services trapped as return-only, compares field/main/persistent record effects
with the declared canonical reconciliation, and exercises native pending/resolved
codecs. One integration build after complete implementation. No campaign or
audio suite. General body layout/portrait/source reachability retains separate
qualification; this batch does not declare NPC/service/story/engine complete.
