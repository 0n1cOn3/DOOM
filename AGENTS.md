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
