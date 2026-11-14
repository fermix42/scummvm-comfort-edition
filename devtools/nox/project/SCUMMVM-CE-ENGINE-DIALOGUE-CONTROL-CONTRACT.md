# Native dialogue control478

Status: IN PROGRESS. Parent simulation commit79efa4fa0d5. Existing dialogue
selection/field effects, TextWindow, service and handoff owners are retained.

## Known missing response-control semantics

Conversation currently rejects every T control except0/255, although source
Talk B253-B262 accepts the complete byte domain. Original source loaded from
SRTN.NPC.TALK file0 atAC85 confirms: negative controls continue without RNG;
zero terminates without RNG; positive controls draw through original1089,
terminating on strict draw<control. Fields commit before the control decision;
response callbacks/exit handoffs remain separate. B263 onward displays the
selected reply and selects the end chain. No general executor belongs in CE.

1. Controls128..255 continue, consume no RNG.
2. Control0 terminates, consumes no RNG.
3. Controls1..127 consume one existing GameplayRandom1..255 result; strict<.
4. Stage RNG and authored fields until parsing/field/callback admission succeeds.
5. Production opening, submitted keyword, closing/end chain and payment replay
   use the session RNG explicitly; do not create a private random stream.
6. Persist the resolved terminal state and unchanged selected reply through the
   existing Conversation codec. Cold loading consumes no extra control draw.
7. Preserve existing journal/acknowledgement and item/gold/recruit handoff order.
8. Complete the source-confirmed book shortcut consistency fix in the same
   infrastructure batch (ENGINE-ITEMS-MAGIC-CONTRACT);0..9 consistently.

One focused batch combines bounded original control/RNG proof, actual packet/
Conversation resolution, codec continuation and existing inventory input.
One integration build after implementation is complete; no game traversal.

## Separate unresolved source producers

Conversation::callbacksPending still rejects field12 bit32, selected commerce
requests, field42 bit128, field46 mask178 and map68/field58 bit32. These are
demonstrated missing callback families, not a valid-refusal completion claim.
Their source B2F9-B4D3 effects and authored producer census must be reconciled
before implementation. Structured body/portrait controls are also separate.
Do not invent callback effects or mark the entire NPC/service/story subsystem
complete after response-control integration. Continue source closure afterwards.
