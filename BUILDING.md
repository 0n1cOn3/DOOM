# Building linuxdoom

This project can target the newer SDL2 pathway (Wayland-friendly) or the
legacy X11 stack. Choose backends at configure time:

- `VIDEO_BACKEND`: `SDL2` (default; Wayland-ready) or `X11` (deprecated)
- `NETWORK_BACKEND`: `BSD` (default) or `SDL_NET`

## Prerequisites

Ensure the following packages (names from Debian/Ubuntu) are installed for the
selected backends:

- Common: `build-essential`, `cmake`
- SDL2/Wayland video (default): `libsdl2-dev`
- X11 video (deprecated): `libx11-dev`, `libxext-dev`
- BSD networking (default): typically provided by libc
- SDL_net networking: `libsdl2-net-dev`

## Example configurations

Wayland-friendly (default) SDL2 build:

```bash
cmake -S . -B build
cmake --build build
```

SDL2 video with SDL_net networking:

```bash
cmake -S . -B build-sdl -DVIDEO_BACKEND=SDL2 -DNETWORK_BACKEND=SDL_NET
cmake --build build-sdl
```

Deprecated X11 build (for legacy parity only):

```bash
cmake -S . -B build-x11 -DVIDEO_BACKEND=X11 -DNETWORK_BACKEND=BSD
cmake --build build-x11
```

Build artifacts are written to `build/bin` (or the chosen build directory).
