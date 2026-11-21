# Gens YM2612 component

Game Music Emu isolated `gme/Ym2612_GENS.cpp` and `.h`, based on Gens 2.10.
Pinned repository: https://github.com/libgme/game-music-emu
Commit: `8238afd9503a59ffcec0e2685584d5ce819ad7c6`.
Original-file hashes are recorded in UPSTREAM.json.

Copyright 2002 Stéphane Dallongeville; copyright 2004–2006 Shay Green.
Upstream grants GNU LGPL version 2.1 or (at your option) any later version.
Original notices remain in the source and COPYING contains upstream LGPL2.1.
For this GPL3 ScummVM combined build we exercise the later-version permission
and use LGPL3, supplied as COPYING.LESSER3, with the application GPL3 COPYING.
The component remains available under its upstream LGPL2.1-or-later grant.

Local modifications, 2026-11-19:
- Compile only this component, independently of GME's VGM build selector.
- Allow its standard C runtime calls under ScummVM's symbol policy.
- Rename the detune-table loop variable for MSVC's shadow-warning-as-error policy.
- Add vector-based portable scalar save/load API and Gens_State.inc (LGPL2.1+).
  It archives runtime scalars and symbolic immutable-table indices, reconstructs
  tables at the declared rate, and validates a staged state before commitment.
  Synthesis equations, tables, gains and register ordering are unchanged.

No GME player, Gens CPU/platform/emulator or game implementation is included.
The application adapter fm_gens.cpp/h is GPL3-or-later and uses the public API.
The standard C/C++ runtime and system math library are the only other dependencies.

The local build includes complete modifiable corresponding application and
component source in the adjacent worktree, generated projects, object files and
libraries. No reverse-engineering restriction or installation lock is imposed.
To modify/relink, edit these component sources, then run the Phase30 build script
with the same Visual Studio2022/vcpkg prerequisites recorded in the integration
report; the generated scummvm.sln can also relink the retained application objects.
The source, copyright notices, both LGPL texts, application GPL3 text and rebuild
instructions must accompany any future conveyance. This task authorizes only
an owner-local build, not public distribution.

GNU compatibility guidance:
https://www.gnu.org/licenses/gpl-faq.html#AllCompatibility
https://www.gnu.org/licenses/quick-guide-gplv3.html
