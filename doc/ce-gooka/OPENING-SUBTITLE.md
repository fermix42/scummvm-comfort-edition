# Opening off-screen speech

The servant's wake-up call (resource 3585) used the audio-only
`kPrimSoundWait` path, while Gooka's reply (3586) used actor dialogue. Both
original Czech text and the English translation already existed.

The opening now explicitly supplies 3585 as the audio wait's optional subtitle
ID. A separate boxed TextSprite is centred at the top of the play area, using
the current language and existing subtitles setting. Its lifetime follows the
same shared audio handle: natural completion or Escape clears it before the
reply. Reset and room change also clear it. No extra delay, actor animation,
audio resource, dialogue order or save-format change was introduced.

Other audio-wait calls remain audio-only. A source/resource cross-check found
that other literal calls sharing text IDs refer to action/item labels such as
Unlock (804), Fill bowl (806), Bellows (24) and Scoop up coal (925); shared door
helpers likewise play effects before separate dialogue. ID coincidence is not
evidence of speech and is never used to automatically create subtitles. This
focused check does not certify every sound in the game.

Engine fix: `0a03598874fa1b261d9404251876f2dde99b6faa` in the maintained fork.
`devtools/ce/test_gooka_opening_subtitle.py` exercises six native runs: Czech
and English, each enabled, disabled and skipped. It captures the caption during
the original 7.523-second voice, checks its disappearance after natural
completion or skip, and verifies that the scene still reaches room 800. The
PNG checks use the caption band rather than unrelated animated scene pixels.
The captured English and Czech text is also visually inspected.

Use this checkout's updated `build-scummvm/Releasex64/scummvm.exe`; enable
subtitles and start a new game to see the call. Original game data, the eXo
data ZIP and published CE v0.1.4 are unchanged.
