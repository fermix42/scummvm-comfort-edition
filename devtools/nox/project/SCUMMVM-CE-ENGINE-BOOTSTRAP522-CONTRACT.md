# Complete production New Game RNG bootstrap522

Status: VERIFIED within the production bootstrap contract. Contract before production changes. Parent249037bdfc0.

Concrete source defect:ordinary beginWorld unconditionally initializes gameplay
RNG with119,the same fixed development replay seed. Replay is explicitly known
from startup_keys;loaded/Continue states already own a persisted RNG andmustnot
be reseeded. Initial source data/party/world/clock/default entry77..79 were also
reviewed:the50,116,49fallback matches menuInitialize's real source,not a random
quest placeholder. No unrelated bootstrap rewrite is needed.

Use ScummVM Common::RandomSource only to obtain the fresh16-bit entropy word
for actual New Game. Keep existing original-qualified GameplayRandom seed
expansion/LCG/rejection and all gameplay draw ordering. Explicit replays keep119.
This is an explicit native engine entropy replacement for hardware/keyboard
startup timing;respect engine random_seed/event-recorder policy. Never replace
or reset the RNG during load/Continue/recovery;already-initialized owner rejects
bootstrap unchanged. Optionalobserved entropy is diagnostic output only.

One coherent helper/production connection andone integration build. Onefocused
batch:controlled Common random_seed inputs exercise the real engine entropy
source/factory wiring,replays remain119,loaded-owner refusal preserves allRNG
bytes;one actual input-earned New/story/movement sequence must retain the same
RAND/TURN as520's matching deterministic input. No oldRNG35/audio/world suites,
8process savebatch,campaign or exploratoryplay. No new testframework/adapter.
Do not assert random draws differ by chance;use the existing engine seed override
before operations for reproducible source-policy inputs. Preserve518/520 evidence.

522b passes two controlled engine-entropy cases, two deterministic replay cases,
loaded-owner refusal without mutation, and actual input-earned replay RAND/TURN
equality against the hash-bound initial520 snapshot. One native batch, no rerun.
The first external comparison used a later overwritten520 endpoint; reference
correction and retained actual outputs are recorded explicitly in the receipt.
Build: zero warnings/errors. BOOTSTRAP522-VERIFICATION binds374 source files,
the executable, source archive and qualifications. No campaign/audio acceptance.
