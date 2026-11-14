# Battle return483

Status: IN PROGRESS. Parent inventory482 commitfdc3d8fe12a. Reuse existing
BattleWorldReturn, BattleAftermath, BattleLoot and actual parent-retirement
connection. No battle/campaign traversal or audio/old suite.

Original R9844-9900 reads parent definitions; L1612/18B6 retires contacts after
results return. Source R98D0 loads A2 before C311; AUXMOVE preserves A/X/Y, so
R98D6 BIT changes flags, not A. With flag64 set and128 clear, R98E9 stores2 as
the queued treasure marker. With128 set, it stores the resulting quest field.
Current native refusal of64-without128 is a genuine missing case.

Primary documentation: Apple Computer's Apple IIe reference manual, auxiliary
memory/AUXMOVE section: https://www.applelogic.org/files/AIIEREF.pdf . Search-
indexed text establishes register preservation; direct PDF fetch timed out.
Existing offline bank-copy service also preserves A; it is consistent evidence,
not the authority for firmware behavior. No ROM/runtime code enters the port.

1. Preserve ordinary compact quest flag update, source slot<244 gate and actual
   post-results parent retirement. Flee changes neither flags nor contacts.
2. Extended state126 selects parallel compact definition: bit128 ORs its field/
   mask; bit64 queues the actual kind/id. Both bits may be independent.
3. Queued marker is resulting field when128, otherwise literal2 from the proven
   preserved source accumulator. Zero resulting field with128 remains no award
   under existing loot semantics; do not manufacture an item or quest outcome.
4. Require actual award sink and supported item kind0..2 before a bit64 mutation.
   Keep unsupported/malformed data atomic, separate from a player refusal.
5. Admit the complete32-slot extended record domain, including248. Its eight
   actual bytes fit the owned tables; parent status3/action7 retire normally.
   The original9-byte transport end wraps within its page at248, while callers
   consume the actual8-byte record. Native deliberately reads canonical records
   instead of reproducing truncated copy/stale scratch. This is an explicit CE
   replacement, not original slot248 byte/image equivalence. No generic bank or
   arbitrary address transport is added.
6. Existing BattleAftermath must transfer the real marker/kind/id into existing
   scripted loot, then retain the actual results/return lifecycle. No new reward
   path, duplicate award or early parent removal.
7. One focused batch: bounded original effects/retire/marker transactions,
   canonical slot248 equivalent-record cases with replacement scope explicit,
   actual scripted Loot owner and codec continuation. No manufactured earned win
   or normal Engine cold/save/campaign acceptance claim. One integration build.

Quick067D lifetime and full script producer closure remain separate requirements.
