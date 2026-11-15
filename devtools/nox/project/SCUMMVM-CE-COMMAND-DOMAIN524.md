# Original command domain and native owners524

Status: VERIFIED for static dispatch classification only. Source14e71152380.
No new code, build or executable run. This closes the finite ordinary command
table question; it does not claim all modal, raster, cold or campaign acceptance.

Original GAME commandLoop71E7..7239 scans30 three-byte entries at723F..7298.
The normal domain has28 entries, all connected to existing native owners:

| Keys | Production owner |
|---|---|
| Four arrows | WorldInput movement/WorldMove and shared movement completion |
| Space | StartupHost WorldWait and finishTurn |
| I, = | WorldInput freeControl/PlayerIcon/torch; retained transport base hero |
| Q | StartupHost shared normal Engine save chooser |
| B, X, Y, J | WorldInput transportCommand, WorldTransportControl and movement |
| C | WorldCastInput; existing spell and navigation owners |
| F | WorldInput fireWorldCannon and shared wait/contact handoff |
| H | WorldCamp and existing world/light refresh |
| D | WorldDig selected-direction owner |
| V | StartupHost existing sound preference owner |
| S, O | SearchInput/WorldOpen shared native interaction and retry owner |
| L, G, ?, / | InformationInput Look/location/help/quests |
| W | HourWait and final shared schedule/light refresh |
| N | PartyOrder and ownership exchange |
| A | WorldAttack and existing encounter preparation |
| Tab | Roster followed by native inventory on Tab/Space |
| T | WorldConversation and existing service/dialogue owners |

The two remaining entries are not additional normal gameplay:

* ! selects950A. It reads a hidden sequence, accepts2 then#, obtains a numeric
  value and clears the original restriction byte70A7 only when the stored
  numeric bytes are0x65/0x02. This is the
  legacy cheat unlock, not a missing player options screen or quest script.
* 3 aliases932A Attack only after that restriction is disabled. Normal71FF/
  7223 routes restricted3 through refusal. The native engine does not activate
  this hidden bypass or expose its alternate command table7299.

Explicit CE exclusion: retain the native ignored legacy !/3 entries. No hidden
cheat unlock, original restriction-byte context, arbitrary debug dispatcher or
new assistance bypass is added. This choice is not source refusal/timing parity;
it does not remove normal A Attack or any source-supported base-game command.
Any future CE assistance must use the established permanent assistance semantics.

Original unknown printable keys call8968 before922E. Native free unknown-key
refusal is likewise a timing replacement already present in WorldInput, not
original world-turn parity. Source613B/613C Search timing is separately qualified
by ENGINE-SEARCH-LIFECYCLE523. Do not infer universal source scheduling parity
from the complete normal dispatch domain.

Sources inspected: immutable GAME table; local original command_loop.inc,
command_actions.inc and combat-contract-research/game.asm; production
native_world_input.h/native_startup_host.h and their named owners. Earlier
474..523 focused receipts retain their own exact executable/scenario scopes.
No proprietary resources or original reference programs are copied into CE Git.
