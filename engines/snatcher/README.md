# Experimental independent native Snatcher engine

Approach C: independently implemented C++ interprets original resources through
ScummVM CE services. There is no CPU interpreter, original executable translation,
Sega CD hardware runtime, or reference-port implementation.

Build explicitly with `--enable-engine=snatcher`. Read-only input supports the
verified USA CHDv5 profile and single/multi-BIN MODE1/2352 CUE layouts, identified by
boot and SUBCODE content. Direct CHD operation requires no conversion. Full
campaign transport acceptance currently applies to CHD; CUE audio gameplay has
not received equivalent acceptance.

The native engine implements scenario progression, scenes, dialogue, original
menus and entry screens, shooting, audio, original four-slot services and CE
saves. A qualified Phase15 uninterrupted native campaign reached the ending,
credits and title, with four disclosed CE mouse examination inputs. This is
separate from complete audiovisual or original digital-input equivalence.

PC mouse interaction is enabled by default. The game option **Original controls
only** (`snatcher_original_controls_only=true`) disables it. Remappable keyboard
and gamepad controls remain available. Escape returns to the launcher. P pauses
and resumes; the PC pause view also offers a mouse Resume button.

- Move over a visible menu item to highlight it; left-click selects it.
- Left-click advances waiting dialogue. Right-click goes back where available;
  in an entry grid it performs the original erase action.
- Click the original character, numeric or phone grid to enter a value. Original
  key animation and completion barriers remain in force. Numeric entry provides
  a visible PC OK button for the original Start action.
- Examination uses game-space pointer coordinates and the original hotspot map.
  Invalid points are ignored; no directional polling or edge clamping is used.
- Shooting uses an unsnapped absolute crosshair and left-click fire. Source hit
  regions, accepted-fire timing, recoil, damage and outcomes remain game-owned.
  Meaningful directional/weapon input switches back to original aiming; mouse
  movement switches to absolute aim. Releasing the mouse does not recenter it.
- Title, settings and original slots use source-derived visible target geometry.
  Cinematics retain their original progression rules; clicks do not skip them.

ScummVM supplies transformed game-space mouse events, including scaling and
aspect correction. Backend and physical hardware breadth require their own
qualification. Native tests are genuinely windowless and use ordinary events.

Hold Cancel (Backspace/gamepad B) with a direction to adjust the examination
pointer. This uses ScummVM's native slow-pointer speed policy and the configured
keyboard pointer speed; it does not reproduce CPU-dependent console polling.
Directions, bounds and hotspot-cache publication on release remain source-owned.
Keyboard repeat messages are unnecessary. Pause, focus loss and load release
physical holds; saved source positions and cached hotspots remain exact.

Saves retain active dialogue, entry, scene work, timers/events, native audio/CD,
shooting and cinematic continuations through the existing owner codecs. Saving
is unavailable before initialization, in diagnostic scenario fixtures, with an
unresolved subsystem error, during the synchronous original-slot disk commit,
or beyond the bounded FM history contract. This is saving at engine dispatch
boundaries, not interruption of a C++ operation. Historical tests and fresh
qualification have different scopes; see the companion completion ledger.
Whole-game audiovisual fidelity and fresh current-source campaign acceptance
remain separate gates. Unknown scenario semantics still fail explicitly.

Standalone C++11 component tests use CMake in `tests/`. Original-data corpus and
runtime drivers consume privately supplied game files and write only to owned
private output directories. No copyrighted game assets are distributed. The
companion research checkout contains exact candidate identities, qualified
scopes, failures and remaining requirements. Vendored decoder and audio-only
synthesizer notices and source identities accompany their source trees.
