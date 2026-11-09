# English editorial integration — November 8, 2026

Imported the owner's authoritative editorial JSON after matching all 835 IDs,
original source hashes, current English, statuses and notes against the catalog.
The 127 nonblank revisions agreed exactly with the bilingual editorial workbook.
Only those English values changed; the other 708 values remain identical.
The complete private handoff, Czech export and workbook are not distributed here.

Editorial JSON SHA256:
`9164b4fafbc99d6ca6f7441745e51fc6ab84bf1c0843af1200d1bb20d9bd0f40`.

The wording was imported as supplied, without alternative translations or layout
rewrites. It restores the explicit alibi requirement (836), retains mind-emptying
terminology (918), adopts the proposed provisional nonsense/proverb adaptations
(1338–1339), normalizes Janatris in English (1373), and preserves the mock-scriptural
register and cannot-fly clue (3414–3416). Raschi and Bodelain are standardized in
English only. Original Czech spellings and resource bytes are unchanged.

All seven existing `needs-context-review` flags and notes are deliberately
retained, as the editor explicitly requested: 856, 918, 1338, 1339, 1373, 3216,
3221. The 1373 source-spelling reservation remains even though its revised English
now says Janatris. The sincha's mollusc/crustacean inconsistency is not corrected,
and no new meanings for fictional terms are invented. Native/contextual review
remains particularly relevant to Digor's wordplay.

All 127 revisions encode as CP1250 and retain the prior control/format-command
sequence, LF counts and trailing controls. No revised line exceeds the existing
590-pixel subtitle limit with font 403. Encyclopaedia entries 3202–3221 and the
credits entry 4000 are unmodified. The Glux passage 3414–3416 is spoken by Gooka
through `kPrimSay` in `game.cpp`, and retains its supplied line structure.

`compile.py` regenerated `english.h` by the normal procedure; `--initialize`
was not used. The generated table matches its source, and the focused localization
check passes source hashes, original controls, CP1250 and original book-column
widths. No Czech lookup, language selector, gameplay, font, save serialization or
video/subtitle option logic was changed. CE must import this revision through
its existing pinned, local-edit-protected Gooka importer.
