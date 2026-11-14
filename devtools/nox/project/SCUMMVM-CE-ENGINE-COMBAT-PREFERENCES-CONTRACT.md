# Combat preferences485

Status: IN PROGRESS. Defined before production changes. Reuse CombatModeRules/
State, QuickBattle, EncounterState, existing restriction/advice and mode-prompt
owners. Preserve completed audio473 and all accepted later subsystem evidence.

Current actual engine exposes only classic saves in game options; Quick/Ask
depend on hidden nox_native_combat_quick/ask INI booleans. Native forced Quick
currently passes constant false to preserveLevel, although original forced
mode can retain067D from unrelated pixel-row writers. Existing source advice
9F1B is independently qualified (rounding,levels/warning and128 preservation).

1. Add one normal game-options selection: Tactical, Quick, Ask. Preserve existing
   classic-save checkbox and loaded-save mode semantics. Use CE widgets/settings;
   no new configuration app, disk/resource writes or original machine options UI.
2. Durable per-game nox_combat_mode0/1/2. Default Tactical preserves current native
   behavior. Recognize prior Quick/Ask flags when new setting absent; explicit
   new setting takes priority. Invalid setting normalizes to Tactical. No fatal
   conflicting-checkbox mode. Keep deterministic replay overrides supported.
3. Source forced-tactical and no-flee restrictions retain priority. Ask prompts
   once, cancels/rejects source keys correctly, and resumes from existing mode
   codec. No preference change rewrites an already selected mode, a battle that
   has acted, or a terminal result. Older private pre-mode0/action0 checkpoints
   remain eligible for initial mode selection; normal CE battle saves remain
   outside existing save eligibility. No reroll.
4. Explicit CE replacement: forced Quick computes the existing source9F1B
   level-based advice and uses its128 preservation flag, matching Ask->Quick.
   Never guess a constant difficulty or retain CPU/pixel scratch. This deliberately
   removes graphics-dependent forced-Quick balance; original067D lifetime is
   still an unresolved historical parity question, not claimed solved.
5. Persist actual selected mode/advice with existing CombatModeState and Quick
   temporary metadata/restore owner; no new CPU ABI or save-schema rewrite.
   Forced tactical is selected once, not repeatedly inferred each UI tick.
   Tactical automatic rounds admit selected mode2 as well as older mode0;
   the current host wrongly excludes2 when all party members are asleep.
6. Preserve existing source group boost and enemy metadata/party-level lifetime,
   the selected music cue transaction and current normal CE save eligibility.
7. One focused preferences/advice/actual formation/Quick preparation/mode codec
   batch and one integration build. Reuse earlier qualified source algorithm
   fixtures/contracts, no old Quick/audio/campaign suite. Normal game-options
   mouse/keyboard layout and cold Engine session remain separate qualification.

Source all normal world command routes were decoded from GAME723F-7298. D Dig
was a genuine omission. !950A is a secret debug unlock sequence, and3 is a hidden
diagnostic Attack caller; neither is a missing normal settings/menu command.
No hidden assist command is silently made into an earned player feature.
