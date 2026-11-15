# Encounter admission clock ownership513

Status: VERIFIED within focused admission-clock scope. Contract before production changes. Parent0884b098d53.

The host currently calls WorldMove::completeClock for EVERY newly presented
encounter. Original M:F134 calls FBE6 before F137/F515's scripted Shadow
encounter F560..F582;native WorldMove already commits that clock on Arrival.
The presentation call therefore adds an unwanted second advance on this path.
Manual GAME93D1..93D6 invokes LWR159F directly;it does not invoke the actor/world
turn body. Its native admission currently adds a world tick too. Preserve the
separate queued-contact continuation tick for existing walking/wait callers.
Do not infer repairs to the entire shared hazard/notice/portal ordering here.

1. Add typed pending-clock ownership to encounter admission. Existing actor
   events and historical native pending formats retain their existing pending
   tick. Manual attack and scripted arrival explicitly carry no pending tick.
   Dialogue admission remains historically pending until separately sourced;
   do not speculate from its UI callback alone.
2. Host presentation consults the admission policy exactly where it currently
   advances. Presented-state idempotence remains the existing owner. No tick on
   battle return or on loading a presented checkpoint.
3. Persist nondefault policy in a bounded new NWIN wrapper without changing
   historical pending encodings. Validate nested shape,complete consumption and
   actual encounter presence before adopting;retained manual alarm wrappers
   remain compatible. Ordinary no-encounter saves keep existing bytes.
4. One batch: original FBE6 scalar outcomes at ordinary/BCD/light rollover
   boundaries;actual manual A/direction and queued move/stationary contacts
   from declared source-compatible pre-input records;policy/core input codec
   roundtrip and historical pending-format default. Test clock-only dispatch
   through the same helper used by host,not a fabricated battle/outcome.
   Scripted producer's already-completed tick connection is static source
   evidence unless an existing bounded arrival fixture can be reused directly.
5. One integration build and one focused batch;extra runs need direct cause.
   No battle execution,campaign,ordinary cold/session or old audio/animation
   suite. Shared world tail before queued combat remains IN PROGRESS.

Original scalar proof observes/skips FC34 clock text formatting and FB6A
presentation/lighting service after scalar writes;it does not synthesize the
clock outcomes or claim visible clock pixel parity.

First513 build and focused batch passed. ENCOUNTER-CLOCK513-VERIFICATION binds current source,executable,original scalars and scoped input/codec evidence. Shared world tail remains separate.
