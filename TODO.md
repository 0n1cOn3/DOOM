# Modernization Completion Checklist

The following tasks must be completed before the SDL2-focused adaptation is considered "done." They build on the initial video, CMake, and networking work already started.

## Build and platform hygiene
- Collapse duplicate build targets in the root `CMakeLists.txt` and rely on one `add_executable` definition. Ensure options like `VIDEO_BACKEND`/`NETWORK_BACKEND` and `DOOM_USE_SDL2` are consistent and documented.
- Remove unconditional X11 and legacy SDL1 dependencies from the build graph; verify that configuring with `-DVIDEO_BACKEND=SDL2 -DNETWORK_BACKEND=SDL_NET` succeeds without X11 headers.
- Add sensible feature toggles for optional components (e.g., MIDI, IPv6) and document them in `BUILDING.md`.

## Video and input
- Finalize the SDL2-only video path: retire `i_video_x11.c`/`i_video_sdl.c`, keep `i_video_sdl2.c`, and purge X11/SDL1 conditionals from `v_video.c`, `i_video.h`, `g_game.c`, and HUD modules (`st_*`, `hu_*`).
- Ensure window resizing, fullscreen toggles, and input focus changes are handled uniformly via SDL2 events; audit mouse/keyboard code for parity with the legacy backends.

## Networking
- Choose a single maintained backend—SDL_net (`i_net_sdl_net.c`) or BSD sockets (`i_net.c`)—and remove the unused code path from `CMakeLists.txt`.
- Align `d_net.c`/`d_net.h` and `net_harness.c` with the chosen API (packet structs, init/teardown), and delete `sdl_net_stub/` if SDL_net becomes mandatory.
- Add basic latency/packet-loss handling hooks so multiplayer remains stable on modern networks.

## Audio
- Unify audio on SDL2 and drop the external `sndserver`: refactor `i_sound.c`/`i_sound.h` for SDL2 audio exclusively and simplify command-line flags that referenced the helper process.
- Clean up mixing assumptions in `s_sound.c` and `sounds.c` to support 16-bit/32-bit output and configurable sample rates.
- Confirm the build only links SDL2 audio libraries (no OSS/ALSA/X11-specific flags) and that startup failures surface clear diagnostics.

## Game data and content gates
- Streamline `gamemode`/`gameversion` conditionals in `g_game.c`, `p_setup.c`, and `info.c` so capabilities are explicit and defaults are sensible regardless of WAD flavor.
- Normalize WAD lump fallbacks in `w_wad.c` and `p_setup.c` to degrade gracefully; refresh UI strings in `dstrings.c`/`dstrings.h` to remove version-locked messaging.

## Rendering, HUD, and resolutions
- Fix aspect handling in `r_main.c`, `r_draw.c`, and HUD rendering (`st_*`, `hu_*`) so 4:3 and widescreen modes display without stretching.
- Re-evaluate BLOCKMAP/REJECT reliance in `p_map.c`, `p_sight.c`, and `p_inter.c`; prototype BSP-friendly alternatives and document any required map-format expectations.
- Audit sprite/patch scaling, automap overlays, and intermission screens for SDL2 resolution independence.

## Save/load, demos, and deterministic behavior
- Verify savegame and demo playback determinism after SDL2, networking, and audio changes; update any timing assumptions in `p_tick.c` and input aggregation code.
- Add minimal regression checks (even manual scripts) to cover save/load and demo playback across common WADs.

## Packaging, docs, and polish
- Update `README.TXT`/`BUILDING.md` with SDL2-only setup steps, dependency lists, and configuration examples.
- Provide a concise changelog entry summarizing removed backends (X11/SDL1, `sndserver`) and new expectations for users.
