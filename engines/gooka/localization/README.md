# Gooka Czech / English text

This fork maintains only Gooka, based on Kungergely/scummvm at
66b8f9703e73fa7a2c9bd8e81ea312a824e97fdf. All original game assets remain owned
by their respective rights holders. No BIN files, bitmap fonts, speech or videos
are redistributed. Tools/engine code are GPL-3.0-or-later; newly contributed
English translation wording is offered under the same license to the extent
the contributors can license it. Original story, character and credit rights
remain with their owners. This is an unofficial machine-assisted localization.

## Player setup

Add the original Czech CD version, open ScummVM's Game Options and select English
or Czech in Language. Restart the game to apply. Czech/default returns original
CP1250 bytes; English requires the versioned `gooka.dat` translation file in the game folder
or ScummVM extra-data search path (portable packages place it beside the executable).
English startup fails with a clear installation/Czech-selection message if the file
is missing, corrupt, incomplete or incompatible. No compiled translation or Czech
fallback is used for English. Czech needs no translation file. No network, special
launcher or separate saves are required. Speech and videos remain the original Czech assets;
enable subtitles for English dialogue. Existing targets can be refreshed by
re-adding the game if their stored GUI language list predates English support.

## Catalog and corrections

The owned Czech Windows CD contains 835 text IDs (834 nonempty, one empty;
one nonempty entry is a lone newline). `english.json` maps all 835, preserving
control bytes and `@n` picture/credit commands. Original names and existing English
system messages are retained. Seven entries are marked `needs-context-review`:
856, 918, 1338, 1339, 1373, 3216, 3221. See each entry's note. A mapped ID is not
proof that every game screen is localized or reviewed. No complete playthrough
or native-speaker sign-off is claimed. Text baked into graphics/video is not
covered by this text-resource lookup.

`catalog.py --game <owned BIN directory> --output <private catalog.json>` extracts
CP1250 text and merges English/status by source hash. It records ID, original
Czech, English, status, source hash, container/language and source-reference
context. Numeric source references are candidates for context, not proof of use.
Keep the resulting full Czech catalog private; do not commit proprietary assets.
An extraction with changed source hashes is not silently marked translated.

Edit the UTF-8 `english.json` for corrections; `python compile.py` regenerates
`dists/engine-data/gooka.dat`, and `python compile.py --check` verifies it. Replace
the distributed file and restart the game to apply text corrections; no executable
rebuild is required. Changes to text rendering still require an engine rebuild. Every byte is encoded explicitly as CP1250
for the existing bitmap fonts. `english.tsv` retains the initial natural-language
drafts; `--initialize <owned catalog>` is a one-time bootstrap, not a normal
update (it replaces layout edits). Book paragraph anchors preserve headings and
picture locations. The optional `--fit-book-font <private widths.json>` fits book
paragraphs to their original line counts and 250-pixel columns. Widths must be
measured from font resource 403 with GookaFont's advance-width rules. Do not
distribute the game font. Normal corrections need only regenerate the data file. Source MD5 and SHA-256
fingerprints are metadata for the original text resources, not the English text;
retain them during corrections. Re-extract them if adding a different game edition.

The normal TextSprite applies English-only soft wrapping at 590 pixels. Czech
layout is unchanged. Encyclopaedia pages retain their original control sequence,
paragraphs and image commands; no font or interface redesign is involved.

`python test_localization.py --game <owned data>` checks extraction, coverage,
controls, encoding, generated data and actual book glyph widths. CE's existing
native hooks can render representative texts and load the same save under both
language settings. Save serialization and gameplay scripts are unmodified.

## Upstream synchronization

The single development branch is `master` in fermix42/scummvm; `origin` points to
that fork, `upstream` to https://github.com/Kungergely/scummvm. Preserve history.

1. `git fetch upstream master` (no build required).
2. Inspect `git log HEAD..upstream/master -- engines/gooka` and
   `git diff HEAD...upstream/master -- engines/gooka`.
3. Integrate Gooka-only commits with `git cherry-pick`; for mixed commits use
   `git cherry-pick --no-commit`, review and retain only Gooka-required changes
   without discarding any pre-existing work. Resolve localization conflicts,
   re-extract changed owned resources and keep source hashes/status honest.
   Never merge the whole upstream tree into CE just to refresh this engine.
4. Run focused localization/build checks, commit and push `master`.
5. In CE's existing development branch, run `devtools/ce/update_gooka.py` with
   the exact reviewed fork commit. Its manifest retains original provenance,
   verifies local edits and imports only engines/gooka including localization.
   CE retains the explicit room-list adaptation for Visual Studio and all other
   CE-specific build/runtime integration outside the imported directory.
6. Review the engine-only diff, run the importer verification and focused CE
   Gooka checks, then commit/push CE. Never move published v0.1.4.

## Data format and CE integration

`gooka.dat` v1 has the eight-byte magic GOOKADAT, big-endian uint16 version=1,
uint16 encoding=1250, uint32 entry count, uint32 payload length, and uint32 CRC32
of the payload. Sorted entries are uint16 resource ID, uint32 CP1250 byte length,
16-byte original-resource MD5, then text bytes without a terminator. Payloads are
bounded to 2 MiB, 4096 entries and 65535 bytes per string. The loader checks exact
coverage against the original resource directories, source fingerprints, encoding,
control/format/picture tokens and complete consumption before admitting English.
The file is loaded once per game session and is not part of save serialization.

The portable packager includes the generated data through engine_data.mk.
The CE importer records this external-data implementation as a local override;
upstream refreshes require reconciling the feature in the maintained fork first.
Its original upstream provenance is retained. No fork push or release publication
is performed by this change. The former compiled english.h is removed.
