# Repository guidance

This codebase contains the legacy Linux Doom sources with a partially modernized build. Use this file for any future edits in the repo.

## Coding and build expectations
- Prefer modern SDL2 backends; the old X11 path is deprecated and should only be touched for removal/migration work.
- The top-level CMake build is the source of truth. Configure with `cmake -S . -B build -DNETWORK_BACKEND=SDL_NET` and build with `cmake --build build`.
- If you change code (not just docs), run the build after configuration. There are no automated tests.

## Current status of the port to modern APIs
- The SDL2 video backend exists (`linuxdoom-1.10/i_video_sdl2.c`); legacy X11/SDL1 code remains in-tree but is deprecated and should be retired once parity is confirmed.
- CMake now builds one `linuxdoom` target with SDL2 video and the selected networking backend, auto-injecting the SDL_net stub headers when the system library is absent.
- Networking defaults to SDL_net through `i_net.c`, with the BSD sockets path guarded by `DOOM_USE_LEGACY_NETWORKING` for compatibility.

## Suggested next steps
- Simplify the CMake configuration so SDL2 (and SDL2_net when requested) are the only required multimedia dependencies, with X11 support isolated for removal.
- Prune the duplicate/legacy build branches (extra `add_executable` calls, unconditional X11 dependency) before adjusting any platform APIs.
- After the build is stable on SDL2, audit the SDL2 backend for resize-aware rendering and key/mouse handling parity with the old paths.

## Outstanding work items
- See `TODO.md` for the full modernization completion checklist. The bullets below remain the top engineering themes.
- CMake now builds a single target and isolates SDL2 video plus the requested network backend; keep trimming deprecated X11/SDL1 code and document the backend expectations for packagers.
- Finalize SDL2-only video pipeline: retire `i_video_x11.c`/`i_video_sdl.c`, keep `i_video_sdl2.c`, and remove X11/SDL1 conditionals from `v_video.c`, `i_video.h`, and relevant HUD/game code (e.g., `g_game.c`, `st_*`, `hu_*`). Re-run CMake to ensure X11 is no longer required.
- Choose and solidify one networking backend: SDL_net is the default via `i_net.c`; BSD sockets remain only behind `DOOM_USE_LEGACY_NETWORKING`. Align `d_net.c`/`d_net.h` and `net_harness.c` with the chosen API, and drop the `sdl_net_stub/` directory if SDL_net becomes mandatory.
- Unify audio on SDL2 and drop `sndserver`: refactor `i_sound.c`/`i_sound.h` for SDL2 audio only, clean mixing assumptions in `s_sound.c`/`sounds.c`, and ensure CMake links SDL2 audio without legacy OSS/ALSA/X11 flags.
- Streamline game-version conditionals and content gates: replace `gamemode`/`gameversion` gating in `g_game.c`, `p_setup.c`, and `info.c` with explicit capability flags or sane defaults; normalize WAD fallbacks in `w_wad.c`/`p_setup.c`; and update version-locked UI strings in `dstrings.c`/`dstrings.h`.
- Prepare renderer/data structures for modern resolutions: fix aspect handling in `r_main.c`, `r_draw.c`, and HUD code for 4:3 or widescreen bases; evaluate BLOCKMAP/REJECT reliance in `p_map.c`, `p_sight.c`, and `p_inter.c` with BSP-friendly alternatives; and document any resulting data format expectations.
