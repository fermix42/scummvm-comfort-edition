# ScummVM Comfort Edition

ScummVM Comfort Edition is a fork of [ScummVM](https://www.scummvm.org/)
focused on making classic adventure games more comfortable on modern displays,
especially ultra-wide desktop setups.

This repository keeps ScummVM's licensing, credits, and upstream history intact.
For the original ScummVM project README, see [UPSTREAM-README.md](UPSTREAM-README.md).

## Downloads

Windows x64 portable binary releases are published from tagged GitHub Releases
for users who cannot or do not want to build from source.

Game data is not included. You still need your own supported game files.

Each binary release identifies the exact matching source tag in the release
notes and includes the ScummVM license, copyright, credits, dependency notices,
this README, and the upstream ScummVM README.

## Why This Exists

I use an ultra-wide monitor, and one of my biggest annoyances is that many
games and applications do not scale to make the most of the display in the way
I actually want to use it.

For classic adventure games, I often want a browser window open with a
walkthrough while the game uses the rest of the screen. I still want the game
to preserve its intended aspect ratio, but I want it to fill as much of that
available space as possible. Mainline ScummVM does not provide that kind of
desktop window layout control, so this build adds it.

Comfort Edition is also meant to make games easier to relax with. As time goes
on, I will add more optional cheats for more games when I feel the need. Cheats
are game-specific, opt-in settings: they live in the **Cheats** tab under
**Game Options**, and supported games can also change them from the in-game menu
opened with **Ctrl+F5**.

## Ultra-Wide Window Layout

The ultra-wide layout options are available in SDL desktop builds.

To configure them:

1. From the launcher, open **Global Options**.
2. Go to the **Backend** tab.
3. Set **Windowed layout** to **Left two-thirds**, **Right two-thirds**,
   **Custom**, or **Normal**.
4. Pick the target display in **Monitor**.
5. Turn **Borderless window** on or off.
6. In the **Graphics** tab, keep **Fullscreen mode** off, set scaling to
   **Fit to window**, and enable **Aspect ratio correction**.

What the layout modes do:

- **Normal** leaves ScummVM's window behavior alone. Use this to turn off
  Comfort Edition's desktop placement.
- **Left two-thirds** places the ScummVM window on the left two-thirds of the
  selected monitor's usable desktop area.
- **Right two-thirds** places it on the right two-thirds of the selected
  monitor's usable desktop area.
- **Custom** lets you enter an outer window **Width**, **Height**,
  **Left offset**, and **Top offset**.

The two-thirds modes are designed for a common ultra-wide workflow: keep the
game in most of the screen and leave the remaining third free for a browser,
notes, or a walkthrough. ScummVM uses the operating system's usable work area,
so a visible taskbar or dock is left accessible.

The layout controls resize and place the ScummVM window; they do not stretch
the game image incorrectly. **Fit to window** makes the game image as large as
it can be inside the window, and **Aspect ratio correction** preserves the
game's intended shape. That means a 4:3 game in an ultra-wide window will still
have side bars instead of being horizontally distorted.

For **Custom**, width and height are the outer window size in desktop
coordinates, including the title bar and borders when **Borderless window** is
off. A value of `0` for width or height fills the usable area on that axis.
Offsets are measured from the selected monitor's usable top-left corner and are
clamped so the window stays on screen.

Settings are applied when you save the options, at startup, after returning
from fullscreen, and when the graphics backend recreates the window. You can
still manually move or resize a normal decorated window, but those temporary
changes do not replace the saved layout.

## Changes

User-facing fork changes are tracked in [FORK-CHANGES.md](FORK-CHANGES.md).

Current highlights include:

- Ultra-wide desktop window layout options.
- SCI Windows AVI scaling behavior tied to `enable_hq_video`.
- A dedicated Cheats tab for cheat-style game options.
- Optional cheats for Indiana Jones and the Last Crusade boxing.
- Optional cheats for the DOS 256-color release of Operation Stealth.

## Using Comfort Edition

1. Download the latest Windows x64 portable release from this repository's
   GitHub Releases page.
2. Extract the full archive to a writable folder.
3. Run `scummvm.exe`.
4. Add your own supported game data through the launcher.

The included `Start window layout test.cmd` uses a separate test configuration
for the window-layout preset and does not change your normal ScummVM
configuration.

## Source And License

ScummVM Comfort Edition is distributed under ScummVM's GPL license terms. See
[COPYING](COPYING), [COPYRIGHT](COPYRIGHT), and [AUTHORS](AUTHORS).

The release notes for each binary link to the matching source tag. The source
tree is the canonical form for review and modification.

## Upstream Project

ScummVM is developed by the ScummVM team. Visit the upstream project for general
documentation, supported-game information, downloads, and community resources:

- [ScummVM website](https://www.scummvm.org/)
- [ScummVM documentation](https://docs.scummvm.org/)
- [ScummVM upstream repository](https://github.com/scummvm/scummvm)
