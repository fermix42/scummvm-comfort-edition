# Portal ordinary-turn owner491

Status: IN PROGRESS. Parent9192202cfb6. Contract before edits.

Source F117 tests rendered portal347B after clock/notice. F81A descriptor loads
SRTN.COMBAT.S offset86D1 length0167 at9C01. A1 branch9C84 first tests
field[object9]&mask11. Missing permission shows source9D10. Nonzero destmap
calls9CFB and requires onfoot;destmap0 permits transport. Source9CC9 destination
type4/map5/x7/y8;map0 uses explicit authored coordinates,not guessed surface
anchor. Success source9D20. Existing portal gate helper incorrectly rejects
map0 and every mounted case. All59 authored candidate records are strict56;
two targetmap0. Source bytes disassembled locally only,never production execution.

Use typed WorldPortal preparation over actual rendered portal slot and immutable
source messages. Reuse WorldInput.travel for cargo/navigation/light/schedule/
viewport;do not call small World.enterPortal directly and omit those owners.
Correct gate helper to allow map0 and restrict only nonzero-map mounted travel.
No recursive chaining on destination portal in the same completed turn.

WorldInput.handle wraps existing command transaction:compare only clock phase/
hour/minute before/after,resolve portal once after a completed ordinary turn at
turn boundary (also permits automatic transport substeps). No free-key/torch
or idle animation activation. Pending encounter/modal/defeat cannot activate.
Notice closure resolves portal immediately after its deferred completeClock;
if transition returns immediately as source does,otherwise existing same-key
handling continues. Track checked clock locally,not a new persisted flag.
Existing native battle suspension remains until its actual world continuation
completes a clock turn;no portal while contact/battle result still owns world.

Host Space Wait uses same public finishTurn portal owner after actual advanced
WorldWait;share ordinary transition resource/music refresh without new audio
semantics. Normal transport pending steps stop on portal travel. Existing native
codecs retain notice/clock/world;no save version/eligibility expansion.

Required graphics dependency:authored Castle54->57 portal arrives38,44;return
portal isthere. Current 48x48 projection/lighting fails beyond local map edge.
Explicit CE_REPLACEMENT:for uncompressed maps,presentation-only projection and
lighting samples outside0..47 repeat nearest authored edge terrain. Interior
sampling unchanged;raw WorldStory.terrainAt,World/AI geometry and collision stay
bounded. No machine-memory overflow art,blank tile invention or CPU fallback.
Compressed maps keep source loaded-sector rules. This permits authored local
edge arrivals without changing physical map dimensions or save data.

One focusedbatch:59 actual resource candidates,authorization/onfoot/mounted
precedence/destinations incltwo map0 returns;actual Castle54portal56 missing/
present field60mask4 via ordinary WorldInput arrow,core/world codecs and next
stationary WorldWait/shared finishTurn return fromCastle57 at38,44. Verify
edge projection/lighting succeeds and no same-turn portal chain. Full mounted
map0 cargo/actor/session remains a separate integration qualification;existing
WorldTravel cargo owner reused,not re-tested as an old suite. No campaign,
audio/old portal/gate suite,earned route or normal Engine cold/session claim.

Source priority dependency:WorldWait currently applies F777/F6D4 origin effects
before portal. Source F117 selected travel jumpsF650 anddiscards origin effects.
After plate phase,prepareportal;onselected skip/clear queued origin projectile
andterrain damage (no hit RNG),thencomplete ordinaryclock. Locked/mountedrefused
portal retainsnormalhazards. ActualUM104waterportal160 at103,19 ->Surface0 at
172,64 withHP50/no waterprotection must travel before otherwiselethal71damage.
Five authored localedge destinations exist;fresh projection/lighting checks cover
them. Typed policy batch236 cases (59*permission*onfoot),notallphysicallyearned.

Original objectphaseD025 resets selectedportal347B toFF beforepaint. Reset
scene.objects.portal atactual WorldActorTurn start aswell;retained occupancy
stayssource-owned butold portal selection mustnotleak across visibility/records.

Actual-arrow checkpoints declare source flee-style compact/NPC freezeTimer3
andactive torch120 before first input to isolate gate ordering from incidental
contact/darkness. This is controlled setup,not earned approach or full unassisted
turn evidence. Subsequent actual wait uses retained timer,not reset after input.

Source success9CC6 callsGAME9142:reuse accepted kSpeakerSelection chime once
after committedtravel. Mountedrefusal9D07 uses accepted kSpeakerRefusal event.
SoundEvents supplies both normalinput andSpaceWait paths withsameengine sink;
no synthesis/music waveform change oroldaudiosuite. Countthree success events
innewfocusedportalbatch (two arrowtravels andone stationaryreturn).

491firstbatchsetupdefect:directWorld checkpoint mustapply actualmapclock NPC
schedule;NewGame schedulebelongsKeep7. Added NpcScheduler.apply beforecheckpoint
viewport,source-supported declaredsetup,notpost-operation outcome mutation.

Observed491b stackoverflow:recordedfault__chkstk+55,compiledstartupstack822792
vs1MBPEreserve;deepordinary NPC chainexceedsreserve. Move transactional
EncounterState copies toscopedheap without alteringcopy/commit semantics.
Validatecompiledrootstackreduction andsamefocusedportalbatch;nooldbattlesuite.

Current correction514:source successfulportal9CF8/F650/F6A8 abandonsmovement callers beforeF134,clearsqueuedcontact andskipsclock.491 afterclock wording wasincorrect. Its UM104103,19 supposedwaterfixture isimmutablefloor118,so that oldbatch doesnotprovelethalwaterdamage suppression. Preservehistorical491receipt;current514 focusedportal/contact policies andactualUM98waterdefeat are separate scopes.
