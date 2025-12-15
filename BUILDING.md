# Building linuxdoom

This project now targets the newer SDL2 pathway (Wayland-friendly) for video
and input. Choose the networking backend at configure time:

- `NETWORK_BACKEND`: `SDL_NET` (default) or `BSD`

## Prerequisites

Ensure the following packages (names from Debian/Ubuntu) are installed for the
selected backends:

- Common: `build-essential`, `cmake`
- SDL2 video/input (default): `libsdl2-dev`
- SDL_net networking (default): `libsdl2-net-dev`
- BSD networking: typically provided by libc

## Example configurations

Wayland-friendly (default) SDL2 build with SDL_net networking:

```bash
cmake -S . -B build
cmake --build build
```

SDL2 video with BSD networking:

```bash
cmake -S . -B build-bsd -DNETWORK_BACKEND=BSD
cmake --build build-bsd
```

Build artifacts are written to `build/bin` (or the chosen build directory).
