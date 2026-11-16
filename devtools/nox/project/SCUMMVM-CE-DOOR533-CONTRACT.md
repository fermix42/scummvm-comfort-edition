# Door lock difficulty533

Status: IN PROGRESS. Observed native failure, not speculative acceptance work.
Accepted source e073e27e3e3 / executable345bcc4dbdd6604b790250cb6539ee19ffe430212d2711b325b3a7cf641779ed preserved.

Actual earned Open at CSTL78/21,12 targeting20,12, then Cutter2, reaches
unsupported interaction. Original object slot8 has state17 and raw difficulty17
at offset7 (the next ordinary door record). Native World.pickDoor/LockRules
reject >10. Original Open9623 copies the full byte without this restriction.
Open9862 computes the threshold with byte arithmetic; Open9761 doubles the
difficulty with byte wrap and reads the word at9837+index. Open resource
SRTN.INVENTORY offset9549 length277 covers9600..9876, including table indices
0..31. Index17 reads original loaded bytes10c9, divided by4 by original damage
service; it must not be clamped, granted success, or treated as an engine fatal.

General correction: retain Search/container0..10 rules. Give Open its own
source-derived damage data and allow every raw byte whose wrapped table index
lies within the original Open resource (low7bits<=31). Keep exact RNG, picks,
attempt count, practice and damage. No original execution in production.
Focused comparison exposed a second Open/Search distinction: Open978B calls
the RNG after division leaves its bounds1..255; Search restores1..199. Preserve
that Open-specific trap-spread range as well. First533 comparison failed case2
on this actual difference; retain its executable/evidence. One justified second
build533b and corrective comparison are required; no campaign replay occurred.
Other authored raw values53 and254 reach bytes beyond that loaded resource;
their original inherited-memory behavior remains unresolved, not verified or
silently replaced. No current earned success is claimed for those doors.

One focused original/native attempt batch covers difficulty17 and wrapped145,
ordinary control difficulties, success/refusal/trap/no-tools/cancel paths.
Use existing lock oracle adapter with a bounded subset version, one isolated
native-only build, then load nearest actual normal998 and resume normal inputs.
Preserve failed branch/modal observation/stderr/save hashes. No audio suite,
campaign prefix replay, expected outcome injection or scene-specific bypass.
