# Repository guidance

This codebase contains the legacy Linux Doom sources with a partially modernized build. Use this file for any future edits in the repo.

## Coding and build expectations
- Prefer modern SDL2 backends; the old X11 path is deprecated and should only be touched for removal/migration work.
- The top-level CMake build is the source of truth. Configure with `cmake -S . -B build -DVIDEO_BACKEND=SDL2 -DNETWORK_BACKEND=SDL_NET` and build with `cmake --build build`.
- If you change code (not just docs), run the build after configuration. There are no automated tests.

## Current status of the port to modern APIs
- The SDL2 video backend exists (`linuxdoom-1.10/i_video_sdl2.c`), but the build still wires in the legacy X11/SDL1 code and even requires X11 headers unconditionally.
- The CMake logic is inconsistent: it defines both `DOOM_USE_SDL2` and `VIDEO_BACKEND` options, links X11 even when SDL2 is used, and calls `add_executable(linuxdoom ...)` twice. This needs cleanup to align with today’s SDL2-first toolchains.
- Networking is split between BSD sockets and SDL_net, with a stub fallback; modernizing likely means selecting a single maintained backend.

## Suggested next steps
- Simplify the CMake configuration so SDL2 (and SDL2_net when requested) are the only required multimedia dependencies, with X11 support isolated for removal.
- Prune the duplicate/legacy build branches (extra `add_executable` calls, unconditional X11 dependency) before adjusting any platform APIs.
- After the build is stable on SDL2, audit the SDL2 backend for resize-aware rendering and key/mouse handling parity with the old paths.

## Outstanding work items
- See `TODO.md` for the full modernization completion checklist. The bullets below remain the top engineering themes.
- Finalize SDL2-only video pipeline: retire `i_video_x11.c`/`i_video_sdl.c`, keep `i_video_sdl2.c`, and remove X11/SDL1 conditionals from `v_video.c`, `i_video.h`, and relevant HUD/game code (e.g., `g_game.c`, `st_*`, `hu_*`). Re-run CMake to ensure X11 is no longer required.
- Choose and solidify one networking backend: pick SDL_net (`i_net_sdl_net.c`) or BSD sockets (`i_net.c`), remove the unused path from `CMakeLists.txt`, align `d_net.c`/`d_net.h` and `net_harness.c` with the chosen API, and drop the `sdl_net_stub/` directory if SDL_net is mandatory.
- Unify audio on SDL2 and drop `sndserver`: refactor `i_sound.c`/`i_sound.h` for SDL2 audio only, clean mixing assumptions in `s_sound.c`/`sounds.c`, and ensure CMake links SDL2 audio without legacy OSS/ALSA/X11 flags.
- Streamline game-version conditionals and content gates: replace `gamemode`/`gameversion` gating in `g_game.c`, `p_setup.c`, and `info.c` with explicit capability flags or sane defaults; normalize WAD fallbacks in `w_wad.c`/`p_setup.c`; and update version-locked UI strings in `dstrings.c`/`dstrings.h`.
- Prepare renderer/data structures for modern resolutions: fix aspect handling in `r_main.c`, `r_draw.c`, and HUD code for 4:3 or widescreen bases; evaluate BLOCKMAP/REJECT reliance in `p_map.c`, `p_sight.c`, and `p_inter.c` with BSP-friendly alternatives; and document any resulting data format expectations.
