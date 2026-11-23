# Shared release engine data

Portable release packaging includes these files in the installation root beside
`scummvm.exe` and `kyra.dat`:

- `gooka.dat`: required for Gooka English, with no compiled fallback.
- `snatcher-decensor.dat`: required for Snatcher Uncensored mode.

Both are registered in `dists/engine-data/engine_data.mk`. The Windows base
packager copies that inventory into the package root. Release preparation and
sealing reject missing or stale copies of either file (and kyra.dat), comparing
against the checked-in source assets. The portable profile sets `extrapath=.`.
No user profile, saves, original game disc image or game-data directory is bundled.

The Snatcher integration report of November 21 describes the earlier checkpoint,
when its optional sidecar was deliberately not bundled. The owner subsequently
requested shared release inclusion on November 22. The approved 29,736-byte
sidecar already used in the test installation is now part of the engine-data
inventory. This change prepares future distributions; it does not publish or
replace an existing release installation.
