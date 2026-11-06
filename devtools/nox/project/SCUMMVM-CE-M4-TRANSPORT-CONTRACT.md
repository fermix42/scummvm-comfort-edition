# Original v119 transport workflow contract

Status: IN PROGRESS. This subsystem deepens W1/W4's architectural inventory.
It is not complete transport integration or native campaign acceptance.

The immutable base HDV remains the primary authority. GAME's command triples
at723F dispatch B to931B and X to94FB. Both clear the pending-jump flag29F9 and
invoke overlay3 through1CB3, retaining the normalized command in X. The source
descriptor selects NA/SRTN.INVENTORY offset92F3,length598,loaded9600..9855.
Private source ranges and hashes are in transport-workflow-research/source1.
Original addresses here identify analysis coordinates, never runtime dispatch.

## Boarding, leaving and owned state

The current transport is an object-table byte offset,not a combat actor index.
Pictures119/123/124 are frigate/caravel/skiff;127/238 are mounts;128 has its own
movement/exit branch. Keep its exact behavior without inventing an identity.
Objects retain all four bytes,including the frigate's skiff-count bits.

Boarding resolves the actual object under the party. Unknown/absent objects
refuse. Maps46..49 invoke the original alarm helper for unowned skiffs,
caravels lacking ownership bit64,and frigates lacking bit128. Ownership lives
in quest field66. Alarm helper9417 changes the matching NPC response flags
and criminal-state scalar;do not silently grant ownership or omit NPC effects.

Boarding a frigate from a skiff refuses when count bit1 is set. Otherwise it
stores lowbit+1 and clears the old skiff record. Boarding aligns the party to
the frigate's original top-left cell. The source performs west then north
alignment where needed,through its real movement/cache/actor/hazard routines.
A viewport refresh alone is not equivalent to that sequence.

Exit clears current-object ownership,pace and speed before testing its branch.
Frigate count0 returns to foot;count1/2 deploys a skiff and stores count0/1.
The allocator searches exact-zero records C0..FC,then uses its original rotating
overwrite cursor,including wrap and lower object slots. It is not a generic
first-free allocator. Leaving a skiff over a frigate returns it to inventory
unless full;full refusal restores the active skiff but retains the earlier
speed/pace clearing. Ordinary exit restores the source base/status icon;
picture128 also clears its separate special-mode scalar.

WorldTransportState/WorldTransportControl are current semantic owners of this
record/control transaction. Build339/341 wires ordinary B/X/J/Y and movement;
20 connected actual341 invocations qualify the named board/sail/exit workflows,
not all transport contexts.
896 original transactions qualify records,control metadata,alarms,capacity,
allocation/refusal and scalar alignment;896 state roundtrips pass in337.
The oracle deliberately stops alignment before the graphics/actor/hazard phase.
Its comparison cannot establish a completed boarding-and-sailing workflow.

## Movement, effects and battle dependencies

Main resident F274..F436 is the player movement gate. It dispatches by the
current object's picture,distinct from world actor collision rules. F161..F1D0
owns water/river eligibility and rough-sea exposure. F1D1..F273 is the walking
probe used by mounts and picture128. Preserve actor/object precedence,authored
map-script requests and the horse's farther-cell check. That check is the J jump gate:MainFE5B sets29F9 andpace2 for pictures127/238.
GAME9289..92BB owns29FA substeps andclears29F9 afterthe pacedarrow completes.
It doesnotbyitselfprove a completedtwo-cell worldmovement.

Frigates test both source-authored leading cells,farther entry first,for rough
seas. North76/77,south127/128,east95/112,west92/109 are relative to origin93.
Skiffs admit river tiles205..210 and water229..232,with the source rough-sea
damage branch. Caravels retain an8-bit rough-exposure counter;normal water clears
it. Frigates admit rough sea229 only. Do not replace unsafe terrain with a
blanket blocked result: source damage and exposure are gameplay effects.

F7A3 reads authored damage magnitudes from GAME6155+mode,tests protection using
quest field57's bit4,and applies actual party damage through1AC2. Protection
changes messages/effects but not the preceding exposure update. Mud/current-
terrain timing checks and conditional RNG in F274 also remain part of the gate.
Native WorldTransportMotion returns these effects for one atomic caller commit;
135072 original gate/exposure/realpartyHP/protectedsea/RNG cases now match
build338.96 authored script contexts remain excluded andexplicitly open.

Required integration joins WorldInput,WorldMove/WorldCommand,WorldActorTurn,
CompactWorldAction,NpcWorldAction,WorldWait,WorldStatus/animation and viewport
ownership. Current on-foot guards and hardcoded partyObject255 cannot simply
be removed. Carry the real object/appearance through spatial/contact policy,
move the owned object with the party,and commit party effects/clock/RNG once.
Persist the transport control and automatic-movement state with normal world
and battle continuations. Preserve already accepted on-foot save versions.

EncounterPreparation/EncounterState now carry mounted ownership in339/341. Their
existing source-qualified transport flags/formation matrices are reusable;
the actual caller must supply the true object picture,appearance and layout.
Keep the parent transport through tactical/Quick mode,station/cannon turns,
victory/results or permitted/refused flee,and the following world command.
Do not assume cannon qualification alone establishes boarding or sea combat.

## Batch acceptance

Use deterministic complete source-state comparisons during implementation.
Then run ordinary B/move/X,frigate/skiff transfers,ownership/capacity/terrain
refusal,real sea damage/protection and cold splits. Finish encounter branches
through battle/results/return or terminal defeat and another world input.
Capture selected visual/audio milestones. Controlled placement/ownership/party
state is not earned progression. All supported forms,normal CE saves/sessions,
native-only release and the earned campaign/actual ending remain required.

## Source caller refinement before ordinary integration

GAME8968 is a whole stationary world phase,not a display-only return:it jumps
toMainECE6,which calls visual preparation,mainD000 actorprocessing andF6D4
hazards. The control-only reference excludes this callback and alignment AI;
its original adapter description was too narrow. Preserve that scope.
Small changedB/X actions andfrigate/skiff exit runone stationary worldphase.
Frigateboarding atitsorigin runsone phase;one/two-cell alignment runsitsone/two
real movement/actor/hazardphases,withno extra stationaryphase afterward.
Retain original intermediate objectcoordinate corrections westthennorth.

Y is the68-byte LOADER.P programat263A,matchingretainedoriginalAuxE5A:E9D
andselectedbyGAME6055 descriptor29. Itclears pendingjump,thentogglesmountpace
0/2;picture128 toggles0/4 andits special-mode scalar,printingmessage39 on4.
J requestsonly127/238 andtemporarilysetspace2;GAME92BB restoresordinarypace
afterthe real ordered substeps. Native sourcecode isexplicit state/arithmetic,
not a copied instruction program. J/Y reference2 adds28originalnormal/refusal
cases to896B/Xcontrolcases;native comparison remains pendingnextbuild.

## Connected lookup lifecycle correction341

MainEDF4/EEB5/EF6A/F027 moves the owned object then writesFF to0450+93
before vertical/horizontal lookup scroll. Other cells retain source ordering.
The first339 native workflow omitted that clear and shifted the old own lookup
behind the party, incorrectly blocking the next return move. WorldMove now
clears exactly that input cell while shifting. INVENTORY9742-974D clears93,94,
110,111 after the frigate origin-alignment world phases (or stationary8968).
WorldInput now commits that cleanup at the same boundary.20 actual341 workflows
pass, including return and ordered corner alignment. This is a source-supported
integration correction, not expected-state injection or blanket occupancy reset.
