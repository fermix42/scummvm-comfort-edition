# Napoleon's interrupted sneeze

The owner's November 21, 2026 Act 2 capture reproduced a missing Santa portrait
with only the separate blinking eyes left visible. The transient sneeze actor
had been cleared by caption cleanup before it could release the paused, hidden
normal portrait. No renderer failure was reported.

The native PCM request started playback before installing the linked visual
list. Napoleon's list has an authored initial pose followed by the A9 sound-start
wait. Audio could finish before the visual reached its portrait-release command;
the next caption reset then stopped the transient actor. This depends on timing:
an additional ordinary-input baseline run completed the sneeze successfully.

PCM requests now install their visual list and let a verified linear lead-in
reach A9 before admitting playback. Programs with branching or external-control
prefixes impose no inferred wait. Inactive or paused actors do not block sound.
Graphics transfers must be published before admission. Caption-role cleanup,
artwork, audio rate and authored frame durations retain their existing behavior.

`-SnatcherPresentationOnly` on the workspace clean-environment build script runs
the animation, scene-runtime and PCM-map contracts. The paired-portrait regression
reproduces premature cleanup with early sound and checks the visible portrait
with aligned start. Native CLI qualification also uses ordinary topic selections
to reach a fresh sneeze, checks the rendered Santa portrait, and verifies a cold
reload with matching story state. SDL dummy checks do not establish physical
audio-device or whole-game audiovisual equivalence.

An already-glitched save contains the canceled visual state. Loading that save
alone does not undo it. A subsequent complete sneeze naturally restored the
portrait in the retained baseline and candidate runs. Owner saves are preserved;
no automatic state repair or story modification is performed.
