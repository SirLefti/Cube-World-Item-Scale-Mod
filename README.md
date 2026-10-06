# Cube World Item Scale Mod

Shrinks every item model the UI draws, so bulky/rotating models (especially
every cube shaped objects) stop clipping into their neighbours and out of their
boxes.

This mod is for the **alpha** build of Cube World.

## Installation

* requires [coremaze Cube-World-Mod-Launcher](https://github.com/coremaze/Cube-World-Mod-Launcher/releases/tag/v1.5)
* unpack Mod Launcher in the game directory
* put `ItemScaleMod.dll` into `Mods`
* start the game via `CubeModLauncher.exe`

## Compilation

Requires a compiler that targets 32-bit Windows. Due to the `asm` style, `MSVC`
will not work, use `GCC` or `Clang`. Run `make` in this directory and specify 
the compiler as `CXX` variable if necessary (e.g. 
`make CXX=i686-w64-mingw32-g++`).
