# Native engine blocker register

Status: IN PROGRESS. Source audit November14; no gameplay discovery.

| ID | Classification / owner | Evidence and impact | Disposition / next evidence |
|---|---|---|---|
| B-ORDER | Current-subsystem / party order | WorldInput ignores N; Party has no slot swap. Original item selector7 swaps128-byte character/16-byte equipment records and inventory3/4 ownership bits. | Implement complete owner and modal command; test all slot pairs, ownership masks, cancel, persistence and no side effects. |
| B-COMMAND | Unrelated subsystem / world interface | Recognized keys pass WorldInput's known-command filter then return Ignored. GAME723F dispatch and manual41–42 enumerate roster/location/Look/help/quest/icon/W. | Complete original semantic contracts and production dispatch after party identity. No exploratory launches. |
| B-TORCH | Evidence / interface-items | Manual calls I Ignite Torch; current native I opens inventory; historical handoff called FE37 invisibility. | Inspect original FE37 and light writers; retain enhanced inventory access and establish source-supported torch mapping. Do not perpetuate contradictory interpretations. |
| B-QUICK | Evidence / combat | Existing M4-COMBAT-CONTRACT: shared067D can hold pixel rows8..168; GAME673F, G89FC, UF030 writers. | Complete lifetime/producer proof. Never substitute guessed difficulty. Ordinary/controlled combat proofs retain their scopes. |
| B-SCRIPT | Evidence / quests | Existing gates/story/ending owners and controlled checkpoints do not enumerate all authored producers. | Inventory data-driven conditions and callback owners statically; no campaign progression. |
| B-LEGACY | Integration / persistence | Legacy417 qualifies two historical world snapshots, not entire legacy campaign state domain. | Establish supported import domain and migration contract; preserve normal CE save/session ownership. |
| B-COMMIT | Build/integrity / recovery | Accepted native473 source is substantially ahead of Git HEAD, many preexisting dirty files. Empty initial index. | Explicit-path docs commits first; source commits must include their required baseline, without unrelated CE changes. Preserve frozen audio473 archive. |

UNKNOWN classifications in the matrix include additional unenumerated original
producer scope. They are evidence gaps, not permission to invent behavior or
silently mark it unnecessary. No external blocker currently prevents party-order
implementation. Build/integrity failures take priority over planned work.
