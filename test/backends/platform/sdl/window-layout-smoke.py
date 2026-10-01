#!/usr/bin/env python3
"""Linux/SDL2 launcher integration test with synthetic desktop geometry.

Usage: python3 window-layout-smoke.py /path/to/scummvm /path/to/sdl2-config [--opengl]
Runs the real launcher with SDL's dummy/software or offscreen/OpenGL driver.
CPPFLAGS and LDFLAGS can locate an extracted SDL development package.
The preload shim
supplies monitor/work-area/decorations data; it does NOT verify a native WM.
All configuration and build artifacts live in a temporary directory.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

SHIM = r"""
#include <SDL.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
static SDL_Window *window;
static int rendered;
static int selectedDisplay = -1;
int SDL_GetNumVideoDisplays(void) { return 2; }
int SDL_GetDisplayUsableBounds(int display, SDL_Rect *r) {
    selectedDisplay = display;
    if (getenv("LAYOUT_BOUNDS_FAIL")) return -1;
    *r = display == 1 ? (SDL_Rect){-5120, 48, 5120, 1392} : (SDL_Rect){0, 0, 5120, 1392};
    return 0;
}
int SDL_GetWindowBordersSize(SDL_Window *w, int *t, int *l, int *b, int *r) {
    *t = 31; *l = *b = *r = 8;
    return 0;
}
SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags) {
    SDL_Window *(*real)(const char *, int, int, int, int, Uint32) = dlsym(RTLD_NEXT, "SDL_CreateWindow");
    return window = real(title, x, y, w, h, flags);
}
void SDL_RenderPresent(SDL_Renderer *renderer) {
    void (*real)(SDL_Renderer *) = dlsym(RTLD_NEXT, "SDL_RenderPresent");
    real(renderer);
    rendered++;
}
void SDL_GL_SwapWindow(SDL_Window *w) {
    void (*real)(SDL_Window *) = dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    real(w);
    rendered++;
}
int SDL_PollEvent(SDL_Event *event) {
    int (*real)(SDL_Event *) = dlsym(RTLD_NEXT, "SDL_PollEvent");
    static int finished, openedOptions, resized, togglePhase;
    if (rendered && getenv("LAYOUT_RESIZE") && !resized && SDL_GetTicks() > 300) {
        resized = 1;
        SDL_SetWindowSize(window, 1234, 777);
    }
    if (rendered && getenv("LAYOUT_TOGGLE") && togglePhase < 4 && SDL_GetTicks() > 300 + togglePhase * 200) {
        if (togglePhase == 1)
            fprintf(stderr, "LAYOUT_ENTERED_FULLSCREEN %d\n", !!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
        SDL_zero(*event);
        event->type = togglePhase % 2 ? SDL_KEYUP : SDL_KEYDOWN;
        event->key.keysym.sym = SDLK_RETURN;
        event->key.keysym.scancode = SDL_SCANCODE_RETURN;
        event->key.keysym.mod = KMOD_ALT;
        SDL_SetModState(KMOD_ALT);
        togglePhase++;
        return 1;
    }
    if (!openedOptions && rendered && getenv("LAYOUT_OPTIONS") && SDL_GetTicks() > 300) {
        openedOptions = 1;
        SDL_zero(*event);
        event->type = SDL_KEYDOWN;
        event->key.keysym.sym = SDLK_o;
        event->key.keysym.scancode = SDL_SCANCODE_O;
        return 1;
    }
    if (!finished && rendered && SDL_GetTicks() > 1200) {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);
        fprintf(stderr, "LAYOUT_RESULT %d %d %d %d %d %d %d\n", x, y, w, h,
                !!(SDL_GetWindowFlags(window) & SDL_WINDOW_BORDERLESS), selectedDisplay,
                !!(SDL_GetWindowFlags(window) & SDL_WINDOW_OPENGL));
        finished = 1;
        event->type = SDL_QUIT;
        return 1;
    }
    return real(event);
}
"""


def main():
    binary = str(Path(sys.argv[1]).resolve())
    sdl_config = sys.argv[2] if len(sys.argv) > 2 else "sdl2-config"
    with tempfile.TemporaryDirectory(prefix="scummvm-layout-") as directory:
        root = Path(directory)
        shim = root / "shim.c"
        shim.write_text(SHIM)
        flags = shlex.split(os.environ.get("CPPFLAGS", "") + " " + os.environ.get("LDFLAGS", ""))
        flags += shlex.split(subprocess.check_output([sdl_config, "--cflags", "--libs"], text=True))
        subprocess.run(["cc", "-shared", "-fPIC", str(shim), "-o", str(root / "shim.so"), *flags, "-ldl"], check=True)
        opengl = "--opengl" in sys.argv
        cases = [
            ("preset-bordered", "window_layout=1\n", (8, 31, 3397, 1353, 0, 0)),
            ("saved-maximized", "window_layout=1\nwindow_maximized=true\n", (8, 31, 3397, 1353, 0, 0)),
            ("preset-borderless", "window_layout=1\nwindow_layout_borderless=true\n", (0, 0, 3413, 1392, 1, 0)),
            ("second-monitor", "window_layout=1\nwindow_layout_display=1\n", (-5112, 79, 3397, 1353, 0, 1)),
            ("missing-monitor", "window_layout=1\nwindow_layout_display=99\n", (8, 31, 3397, 1353, 0, 0)),
            ("custom", "window_layout=2\nwindow_layout_width=1200\nwindow_layout_height=900\nwindow_layout_x=200\nwindow_layout_y=100\n", (208, 131, 1184, 861, 0, 0)),
            ("custom-fill-clamped", "window_layout=2\nwindow_layout_width=99999\nwindow_layout_height=0\nwindow_layout_x=99999\n", (8, 31, 5104, 1353, 0, 0)),
            ("manual-resize", "window_layout=1\n", (8, 31, 1234, 777, 0, 0)),
            ("fullscreen-roundtrip", "window_layout=1\n", (8, 31, 3397, 1353, 0, 0)),
            ("borderless-roundtrip", "window_layout=1\nwindow_layout_borderless=true\n", (0, 0, 3413, 1392, 1, 0)),
            ("options-dialog", "window_layout=2\nwindow_layout_width=1280\nwindow_layout_height=720\n", (8, 31, 1264, 681, 0, 0)),
            ("normal", "window_layout=0\nwindow_layout_borderless=true\n", None),
            ("fullscreen", "window_layout=1\nfullscreen=true\n", None),
        ]
        for name, settings, expected in cases:
            config = root / (name + ".ini")
            mode = "opengl" if opengl else "normal"
            config.write_text("[scummvm]\nconfirm_exit=false\ngfx_mode=" + mode + "\nvsync=false\n" + settings)
            env = dict(os.environ, SDL_VIDEODRIVER="offscreen" if opengl else "dummy", SDL_AUDIODRIVER="dummy",
                       SDL_RENDER_DRIVER="software", LD_PRELOAD=str(root / "shim.so"),
                       XDG_CONFIG_HOME=str(root), XDG_CACHE_HOME=str(root), XDG_DATA_HOME=str(root))
            if name == "manual-resize":
                env["LAYOUT_RESIZE"] = "1"
            if name in ("fullscreen-roundtrip", "borderless-roundtrip"):
                env["LAYOUT_TOGGLE"] = "1"
            if name == "options-dialog":
                env["LAYOUT_OPTIONS"] = "1"
            result = subprocess.run([binary, "--config=" + str(config), "--logfile=" + str(root / "scummvm.log")],
                                    env=env, text=True, capture_output=True, timeout=20)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            records = [line for line in result.stderr.splitlines() if line.startswith("LAYOUT_RESULT ")]
            assert records, (name, result.stderr)
            actual = tuple(map(int, records[-1].split()[1:]))
            assert actual[6] == int(opengl), (name, "wrong renderer", actual)
            if name in ("fullscreen-roundtrip", "borderless-roundtrip"):
                assert "LAYOUT_ENTERED_FULLSCREEN 1" in result.stderr, result.stderr
            if expected is not None:
                assert actual[:6] == expected, (name, expected, actual)
            else:
                assert actual[4:6] == (0, -1), (name, actual)
            print("PASS", name, actual)
            if name == "manual-resize":
                del env["LAYOUT_RESIZE"]
                restarted = subprocess.run([binary, "--config=" + str(config), "--logfile=" + str(root / "scummvm.log")],
                                           env=env, text=True, capture_output=True, timeout=20)
                assert restarted.returncode == 0, restarted.stderr
                records = [line for line in restarted.stderr.splitlines() if line.startswith("LAYOUT_RESULT ")]
                assert records, restarted.stderr
                actual = tuple(map(int, records[-1].split()[1:]))
                assert actual[:6] == (8, 31, 3397, 1353, 0, 0), actual
                print("PASS restart-restores-saved-layout", actual)


if __name__ == "__main__":
    main()
