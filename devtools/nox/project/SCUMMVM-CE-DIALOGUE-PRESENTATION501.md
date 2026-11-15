# Authored dialogue body grammar501

Status: VERIFIED for static body grammar review only. Parentd9eeb7874de.
Command: `python conversion/census_dialogue_bodies501.py`, exit0.
439 hash-verified packets/3227 bodies:45 `$K` substitutions,26 literal NPC
labels, zero other body control-byte families beyond newline/CR.
Detailed record offsets stay local: ce-native-dialogue-bodies501-local.json.

Every body `$K` has a matching header `$K` selector and is the body's only
substitution. Native DialogueRules derives the input-prefix length from that
selector; plainDialogueBody inserts the submitted literal query at that exact
point. The inserted text is not reparsed. TextWindow receives the resolved
string and owns wrapping/pagination/scrolling over TextGlyphSurface/HgrFrame.
No new structured body renderer is indicated by this complete packet domain.

All26 other dollar occurrences are immediately preceded by literal `NPC `.
Their suffixes0..B are source record labels, including explicit unconfigured
NPC dialogue. They must print literally. Treating these as formatting or
portrait selectors would corrupt authored text. This corrects the initial
479 raw-dollar candidate interpretation; its census/evidence is preserved.

Original response presentation at TalkB263/B26E/B279 invokes shared7CEC with
the authored prefix/response pointer, then chooses the end chain. The existing
local original-controller description dialogue_outer.inc/presentation_text.inc
provides independent static call/ordering evidence. These retained reference
programs are analysis inputs, never linked semantic implementations.

This finding resolves the specific remaining authored-body formatting question.
It does not prove all shared graphics branches or the absence of separate
raster operations. Historical portrait/raster-parity qualifications remain
explicit pending a concrete source producer; do not invent portrait assets or
mark all graphics VERIFIED from this text census. No production code, schema,
build, runtime test, old dialogue/audio suite or campaign was needed.

Continue shared lifecycle/presentation and remaining source producer closure.
