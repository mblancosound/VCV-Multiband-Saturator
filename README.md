# VCV-Multiband-Saturator

Multiband Saturator plugin for [VCV Rack 2](https://vcvrack.com/) by **Coal Dynamics** (Manuel Blanco).

Stereo 3-band saturator (Low / Mid / High) with per-band drive, pan and volume, CV inputs, and stereo in/out.

- Plugin slug: `Coal_Dynamics`
- Module: `Multiband Saturator` (`Multiband_Saturator`)
- Version: `2.0.0`
- License: MIT (see `LICENSE.txt`)

## Features

- 3 bands with Biquad crossover filters (stereo L/R)
- Per band: Drive, Pan, Volume + CV for Pan/Volume
- Master Output volume + Mix (dry/wet)
- Custom SVG knobs, ports and panel in `res/`

## Requirements

- VCV Rack SDK 2.6.6 (see `Makefile`: `RACK_DIR ?= /c/Rack-SDK2.6.6`)
- Toolchain compatible with Rack plugin development (GCC/Make on Linux, MSYS2/MinGW on Windows, Xcode/Clang on macOS)

## Build

```bash
# Point RACK_DIR to your local Rack SDK if different
export RACK_DIR=/path/to/Rack-SDK2.6.6

make
make dist
```

The distributable `.vcvplugin` will be created under `dist/`.

## Project structure

```
plugin.json        # plugin + module metadata
Makefile           # Rack build (RACK_DIR, src/*.cpp, res/)
src/
  plugin.cpp / plugin.hpp      # plugin instance + custom widgets
  Saturator.cpp / Saturator.hpp # DSP + UI
res/               # panel + component SVGs
LICENSE.txt
```

## Install (from source)

1. `make dist`
2. Copy `dist/Coal_Dynamics-2.0.0-win-x64.vcvplugin` (name varies by platform) into your Rack `plugins/` folder, or install it from Rack via `Help > Install plugin file...`.
3. Restart Rack.

## License

MIT — see `LICENSE.txt`.
