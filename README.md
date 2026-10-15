# Portal-psps

Portal-psps is a small PSP homebrew prototype inspired by portal-style movement and puzzle design. The project is built around a single C source file and uses the official PSPSDK libraries (`pspgum`, `pspgu`, `pspctrl`, etc.).

The repository currently contains a compact playable "test chamber" demo: first-person movement, wall/ground collision, portal placement, story text, and a simple objective loop.

## Features

- First-person movement on the PSP
- Basic portal gun mechanic with blue/orange portals
- Simple room geometry with collision and portal teleport logic
- Story text and objective flow during gameplay
- Minimalistic 3D renderer using PSP graphics APIs

## Project structure

- `main.c` — main game logic and rendering loop
- `Makefile` — build configuration for PSPSDK
- `psp_portal.zip` — packaged project archive (possibly for distribution or reference)

## Requirements

To build this project you need:

- PSP SDK / PSPSDK
- `psp-config` in your PATH
- A POSIX-like environment (Linux/macOS, or a compatible cross-build environment)

## Build

From the repository root:

```bash
make
```

This produces a PSP executable package, typically `EBOOT.PBP`.

## Run on PSP

1. Copy the generated `EBOOT.PBP` to your PSP's game directory.
2. Launch it from the PSP menu.
3. Use the D-pad / analog controls to move and interact with the chamber.

## Controls

- D-pad / analog stick: movement
- Up / Down: look up/down
- Left / Right: turn left/right
- Cross: jump
- L trigger: blue portal
- R trigger: orange portal
- Start/Exit: quit the application via PSP exit callback

## Notes

This repository is intentionally compact and geared toward a single-file PSP demo. It is suitable as a learning project or as a starting point for a larger portal-style homebrew game.

## Future improvements

Possible directions for extending the project:

- split logic into modules (`game.c`, `render.c`, `portal.c`)
- add more levels and puzzle chambers
- improve collision and portal edge handling
- add sound and music
- add a proper menu / title screen / save system

## License

No explicit license file is present in the repository yet. If you plan to distribute or publish this project publicly, it is recommended to add an appropriate license such as MIT or GPL.
