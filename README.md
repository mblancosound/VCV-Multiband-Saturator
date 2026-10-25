# VCV-Multiband-Saturator

Multiband Saturator plugin for [VCV Rack 2](https://vcvrack.com/) by **Coal Dynamics** (Manuel Blanco).

Stereo 3-band saturator (Low / Mid / High) with per-band drive, pan and volume, CV inputs, and stereo in/out.

- Plugin slug: `Coal_Dynamics`
- Module: `Multiband Saturator` (`Multiband_Saturator`)
- Version: `2.1.0`
- License: MIT (see `LICENSE.txt`)

## Features

- 3 bands with Biquad crossover filters (stereo L/R)
- Per band: Drive, Pan, Volume + CV for Pan/Volume
- Oversampling 1x / 2x / 4x / 8x (default **4x**)
- Master Output volume + Mix (dry/wet)
- Custom SVG knobs, ports and panel in `res/`

## Oversampling

Right-click the module → **Oversampling** → `Off (1x)` / `2x` / `4x` / `8x`. The setting is saved in
the patch; old patches without the key load in **4x**. The change is applied at the start of the next
audio block.

The whole chain (crossovers + `tanh` + pan/vol + band sum) runs at `N × sampleRate`, so the FIR pair
adds a fixed latency of **15 samples** at 2x/4x/8x (0 at 1x). The dry path of MIX is delayed by the
same amount to keep the mix flat. Changing the factor resets the filters, so it can produce a short
**click**; it is a setup control, not an automatable parameter.

Measured at 48 kHz with a 5 kHz sine and full drive: worst in-band aliasing is −14.7 dB at 1x,
−32.8 dB at 4x and −43.3 dB at 8x. See `specs/oversampling_spec.md` for the full table and limits.

## Requirements

- VCV Rack SDK 2.6.6 en `C:\Rack-SDK2.6.6` (ver `Makefile`: `RACK_DIR ?= /c/Rack-SDK2.6.6`)
- MSYS2 + `jq` (Mingw64: `pacman -S mingw-w64-x86_64-jq`; el `Makefile` añade `/mingw64/bin` al `PATH` y falla con mensaje claro si falta `jq`)
- Toolchain compatible con Rack plugin development (GCC/Make on Linux, MSYS2/MinGW on Windows, Xcode/Clang on macOS)

## Build

```bash
make        # compila plugin.dll en Saturator/
make dist   # crea el .vcvplugin en dist/
```

Timings medidos (Windows + MSYS2): limpio 7.2s, 1 fich 2.7s, no-op 0.9s, `make dist` 1.6s.

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
  Saturator.cpp / Saturator.hpp # DSP (filters, drive, pan/vol, mix)
  SaturatorWidget.cpp          # UI + oversampling context menu
  Oversampling.hpp             # 1x/2x/4x/8x stages (header-only)
res/               # panel + component SVGs
LICENSE.txt
```

## Install (from source)

1. `make dist`
2. Copy `dist/Coal_Dynamics-2.1.0-win-x64.vcvplugin` (name varies by platform) into your Rack `plugins/` folder, or install it from Rack via `Help > Install plugin file...`.
3. Restart Rack.

## License

MIT — see `LICENSE.txt`.
