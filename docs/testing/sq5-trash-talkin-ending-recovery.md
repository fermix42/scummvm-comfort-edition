# Trash Talkin' Edition ending artwork recovery

October 23, 2026: the user explicitly authorized reference-executable playback
and reverse engineering of the two omitted ending headers. Their pixel data
has now been recovered exactly from the supplied executable, installed as CE
external data and reconstructed into local C headers. No recovered pixels or
headers are tracked in the CE repository.

## Method and provenance

The previously verified `payload/runtime/scummvm.exe` has SHA-256
`69fd17cf3ea43ea0166826dc715487eff3043bef1ff462a6010160587766aab3`.
Its COFF symbol table retains names for all five omitted arrays. The supplied
`GfxAnimate::updateEndingFloMouth` source provides their exact dimensions and
usage; no invented frames, screenshot approximations or new animation logic
were needed.

`devtools/ce/sq5_tte_ending_art.py` rejects any other executable hash, resolves
the named symbols through their PE section indices and requires each byte
range to lie entirely in read-only `.rdata`. It emits local `.bin` assets and
a provenance manifest; optional reconstructed headers reproduce the arrays.

| Array | Symbol | File offset | Bytes |
| --- | --- | ---: | ---: |
| Flo mouth, three 15 x 10 frames | `_ZL15kFloEndingMouth` | 7661344 | 450 |
| Flo eyes, open 30 x 10 | `_ZL15kEndingFloEyes0` | 7661824 | 300 |
| Flo eyes, closed 30 x 10 | `_ZL15kEndingFloEyes1` | 7662144 | 300 |
| Droole eyes, open 48 x 25 | `_ZL18kEndingDrooleEyes0` | 7662464 | 1200 |
| Droole eyes, closed 48 x 25 | `_ZL18kEndingDrooleEyes1` | 7663680 | 1200 |

The 3,450 bytes are stored in `build-sq5-analysis/game/voices`. Per-array hashes
are in `build-sq5-analysis/ending-recovery.json` and
`game/voices/ending-art-provenance.json`. Reconstructed headers are local at
`build-sq5-analysis/recovered-source/engines/sci/engine/flo_ending_mouth.h` and
`ending_blinks.h`. Parsing those generated headers back into bytes reproduced
all five original arrays exactly. This reconstructs the compiled data, not
the original headers' formatting, comments or development history.

The game preparer now performs this same hash-bound extraction automatically.
Existing CE rendering already loads these external names and dimensions, so
the tested candidate executable does not need rebuilding. Restart a running
ending test before retesting so any earlier missing-asset cache is cleared.

## Runtime comparison

An isolated reference game directory contains copies of the owned game files
and reconstructed resource patches; voices remain outside its recursive game
search path to avoid the native WAV-resource collision discovered earlier.
The developer executable successfully loaded a copy of the user's test save.

For targeted ending access, another disposable copy of that save was decoded
using the source-documented SCI serializer layout. Its unique script-0 local
segment has 201 globals. Only pending room global 13 was changed to 1041,
leaving the current room and other state unchanged. This requests the native
room-change path. The original save's SHA-256 was checked unchanged afterward.
`build-sq5-analysis/reference/ending-fixture.json` records the offsets/hashes.
This is a synthetic test fixture, not evidence of full-game completion.

Both developer and CE executables played the closing bridge scene from this
fixture. Their separate voice traces reached Flo's `1041-2-0-0-3` line.
Computer Use observations showed the reference closing scene and the CE scene
with the recovered Flo mouth opening during that line and returning to its
resting shape afterward. Screenshot observations do not establish every blink
cycle or all scene transitions. The subsequent human ending-overlay comparison
now passes: the user confirmed Flo's mouth and both characters' blinking look
correct, with no smearing.

No regular CE executable, original distribution or original save was modified.
The candidate SHA-256 remains
`1a0976b7363d2680fc77e8b6b6a76cf17a6cc024596a625b9e09f64836b5daef`.
Five importer/recovery boundary tests pass, including rejection of an unknown
reference binary. All recovered file sizes/hashes and both ending speech
traces were checked directly. No repeat full build or full-game test was needed.

## Reproduction and final checkpoint

From the repository root:

```powershell
python devtools/ce/sq5_tte_ending_art.py --exe 'D:\Codex Projects\SQ5-Trash-Talkin-Edition-PC-0.2.12\payload\runtime\scummvm.exe' --output build-sq5-analysis/game/voices --headers-output build-sq5-analysis/recovered-source/engines/sci/engine
python devtools/ce/test_sq5_trash_talkin.py -v
```

For the final human check, run `build-sq5-analysis/Launch SQ5 CE ending test.cmd`.
It automatically loads disposable slot 99; no debugger commands are needed.
Observe Flo on the right and Droole on the left in the closing bridge scene:
Flo's mouth should animate with her line and settle afterward; their eye
patches should blink without smearing or corrupting the dialogue/background.
The developer comparison uses `Launch SQ5 developer ending test.cmd` if needed.
Final result: the user completed this additional checkpoint and confirmed the
ending test passes: Flo's mouth and both characters' blinking look correct,
with no smearing. All targeted ending-overlay acceptance checks are complete.
Prior intro, save/load, atlas and ordinary portrait visual passes remain valid.
No full-game playthrough or exhaustive scene-transition claim is implied.
