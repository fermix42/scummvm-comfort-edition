# Dialogue source speech callback506

Status: IN PROGRESS. Contract before production changes.

Original Talk B2F9..B30B checks field12 bit32, calls resident FCC3, then
clears that bit. FCC3 emits the current six-step base command, without
advancing the cursor or toggling the Keep phase. Modes1/3 suppress speech;
clearing the callback flag still occurs. This is a second caller of505's
CWS waveform owner, not a new synthesizer or an audio473 suite regression.
The500 immutable mutation census identifies L018 actor24 header0 as the
source field12 bit5 set producer (encoded bit change69). L01D actor0's71
sets bit7 and does not request this callback.

Native ConversationCallbacks.apply currently clears bit32 without emitting
speech. Preserve its other canonical field/object mutations and redraw
semantics. Route the one transient cue from the typed pending callback through
WorldConversation's existing SoundEvents and current sound-mode policy.
Keep cursor unchanged, use the player's existing turn state as the source,
and ensure successful callback consumption queues at most once. All three
WorldConversation pending callback call sites must share this owner. No
new persistent preference, CPU ABI, copied program or default cursor invented
for resumed nondefault turns. Retain existing save/modal ownership.

Before code: inspect the WorldInput/WorldConversation call boundary for actual
turn-state access and transient rebind. Read the immutable source producer
packet and map metadata for a supported checkpoint. Focus one batch on actual
reply callback, all effect modes, nondefault cursor, exactly-once consumption,
existing field/object mutations and codec continuation. No campaign traversal,
physical listening or normal Engine cold/session claim. One coherent build;
only direct compile/failure reasons allow extra runs; two failed corrective
native batches end the loop. Preserve all505 evidence and accepted473 cores.
