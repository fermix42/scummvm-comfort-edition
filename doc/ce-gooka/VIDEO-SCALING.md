# Gooka AVI size option

Gooka uses ScummVM's AVIDecoder for `12.AVI` (intro) and `13.AVI` (ending).
Both owned CD files are 400×300. Previously they were copied at native size
to the centre of the 640×480 game screen, leaving 120-pixel side borders and
90-pixel top/bottom borders.

Game Options → Game → **Scale videos to full height** enables
`gooka_scale_videos`. Default is off, preserving native-size playback. With
the option enabled, both original videos fill the 640×480 game screen at
their original 4:3 aspect ratio. The normal backend then presents that game
screen in the selected window/fullscreen mode. A wider future movie is fitted
within the screen without cropping or stretching, leaving letterboxing as needed.

The engine converts decoded frames to the screen format when necessary, then
uses ScummVM's nearest-neighbour scaleBlit with one reusable scaled buffer per
movie. Decoder scheduling, audio, keyboard/mouse skipping and return to the title
or credits remain unchanged. No AVI file is modified. This is a per-target
setting; apply it before the next video plays. Existing targets with old cached
GUI options refresh when launched once; reopen Game Options afterward.

`devtools/ce/test_gooka_video_scaling.py` uses the opt-in existing Gooka test
script's `video:12`/`video:13` commands, actual CD videos, native screenshots and
input. It verifies both destination rectangles, intro skipping and natural ending
completion with the option off/on. It does not constitute full-game acceptance
or manual GUI-widget navigation. Captures and video files stay outside Git.

Source is maintained in the Gooka-only fork and pinned by the existing protected
CE importer. The released v0.1.4 assets and eXo game-data package remain unchanged.
