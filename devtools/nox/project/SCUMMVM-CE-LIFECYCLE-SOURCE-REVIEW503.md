# Shared lifecycle and presentation source review503

Status: VERIFIED for static known implementation/connection review only.
Parentf34c1bfae9f. No new executable run or build. Source still matches498b.

| Known requirement | Native transaction and production connection | Verification qualification |
|---|---|---|
| lifecycle.02 startup New/Continue/Quit | Native startup registers the owned PlayerSession before Engine save_slot or Continue loading. StartupMenu N enters creation, C invokes the actual shared load chooser and leaves the menu only on success, Q/OS quit exits. The main event loop routes menu/creation/story/world/battle by owned phases. | Existing scoped startup/player receipts retain their executable identities; no fresh whole normal startup acceptance. |
| lifecycle.04 opening/first world | CharacterCreation commits NewGame only at confirmation. OpeningStory/OpeningBackdrop/TextWindow own authored story, paging and acknowledgement. First-world rules, window, text, viewport/status and animation initialize before sceneReady; no exploratory actor turn is substituted for initialization. | Source-supported ordering, not a new earned opening or raster comparison. |
| lifecycle.05 loaded save mode | NoxEngine.loadGameState stages/validates world before committing session ownership and gameplay, then restores metadata.mode and total playtime. Classic1/2 versus Modern0..999 is SavePolicy/MetaEngine's shared policy; the new-session configuration does not override the loaded mode. | Existing mode/session/durable-save evidence remains scoped. Later I1 must exercise ordinary cold Continue with current integrated controls. |
| lifecycle.06 shutdown/transient lifetime | runNativeStartup's ScopedPtr owns PlayerSessionState. NativeSessionRegistration clears the Engine's borrowed player pointer on every scope exit before player destruction. PlayerSessionState deletes staged candidates; commit/discard/reset replace state through the owned transaction. Engine destructor stops mixer handles and frees the surface. | Static lifetime review only; no new fault sweep or audio rerun. |
| interface.16 CE mode presentation | Architecture sections211..218 define Classic/Modern as save-slot policies. MetaEngine presents the corresponding chooser/slot capability and native game frames share one HGR/RGB renderer. They are not two missing Apple display engines. | Clarify the matrix label to actual chooser presentation/shared raster; historical source display flags remain separate raster qualifications. No requirement to implement speculative new display modes. |

The ordinary host repaints party/status after handled world commands unless
the active owner intentionally owns the whole frame. WorldConversation restores
its saved underlay; callback/recruit closure explicitly refreshes world records,
and changed gold/food are painted by the host. The native surface/render path
also supplies the save preview. A second service-specific renderer is unnecessary.

PlayerSession.stageLoad reconstructs rules and encounter ownership on the
candidate; commitLoad resets presentation timing. The ordinary loop rebinds
world/move/wait/conversation/inventory/cast sound-event sinks after CE frame/load
processing. No dangling temporary callback pointer was found. Accepted audio473
is unchanged; these observations do not reopen its implementation or tests.

These five rows become IMPLEMENTED for their known source requirements. Broader
runtime statuses stay unchanged. Original display-bank scratch, exact historical
pixel parity, arbitrary legacy snapshots and additional unclassified script
contexts are not silently promoted. No new native gap is demonstrated in this
coherent shared lifecycle pass. Continue concrete original-code context review
and source-phase reconciliation; the later integration plan remains unexecuted.
