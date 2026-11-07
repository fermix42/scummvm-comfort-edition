# Focused human acceptance — consolidated CE 2026.4.0

This checklist covers acceptance gaps, not a request to repeat every automated
test or complete every game. Use a **copy of the complete candidate folder** in
a writable location. Keep accepted installations and original saves unchanged.
The release-readiness report identifies the exact tested executable hash and
which native menu checks were automated successfully.

Record the candidate hash, game edition, controls/renderer and observed result
for each completed item. Do not mark an unavailable case as passed.

- [ ] **Actual achievement gameplay and popup presentation — requires human
  acceptance.** In a supported customized game such as Operation Stealth or Deja
  Vu, earn one legitimate event through ordinary play. Observe the correct popup,
  title/icon, optional voice and placement. Save/restart and check repeat-popup
  suppression; start a different Play Session and check independent ownership.
  Do not grant or edit achievement state to simulate success.
- [ ] **Live synchronization — unavailable authorized staging test data until
  supplied.** Use only a separately authorized staging account/token in a fresh
  test folder. Verify the earned event and Play Session arrive with the correct
  ownership, survive an offline retry and do not duplicate. Never use production
  credentials or upload copied personal progress. Native CLI deliberately blocks
  HTTP; its local queue checks are not a live synchronization pass.
- [ ] **MI4 PS2 native Load menu — requires human acceptance.** Windows native Load
  navigation and its confirmation dialog passed a fresh headless test. On PS2, choose a
  save through its actual native UI and confirm the correct scene and session.
  The existing deferred-load API/cold-restore tests alone do not prove menu use.
- [ ] **Older MI4 saves — unavailable test data.** If legitimate pre-repair or
  older CE Windows/PS2 saves become available, copy the entire collection with its
  registries/sidecars into a separate test folder. Check listing, thumbnails,
  native restoration and ownership. Preserve originals and do not merge colliding
  registries or bank filenames blindly.
- [ ] **Representative customized gameplay — requires human acceptance.** Short
  checks are sufficient: SQ5 Trash Talkin speech/lip sync and save/session UI;
  Deja Vu expanded desktop and a supported enhancement; Operation Stealth's
  edition-appropriate patch/cheat or an Indy3 boxing enhancement. Automated
  importer, startup and helper checks do not substitute for these observations.
- [ ] **A representative newly supported upstream game — unavailable test data
  where no legitimate assets are supplied.** When one suitable title is available,
  check detection, a short playable scene and a save/restart. Do not acquire data
  merely to manufacture a pass or require testing every engine.
- [ ] **Human display/input — requires human acceptance.** Check normal launcher
  use, fullscreen toggle/return, titled and borderless layouts, scaling/cursor
  mapping, intended monitor placement and a real controller if used. Alternate
  DPI/secondary monitors are conditional on available hardware. Do not change
  system display settings as part of automated acceptance.

If an observed failure is reproducible, keep the disposable logs/configuration
and report the exact trigger. A confirmed material regression warrants an
engineering investigation. Missing assets, staging authorization or hardware
remain explicit acceptance gaps; they do not become automatic passes.

The candidate is not installed, publicly released or approved by this checklist.
Publication, installation and merging into the original main branch remain the
user's decisions.
