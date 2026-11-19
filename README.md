# VCV-Multiband-Saturator

[![Build](https://github.com/mblancosound/VCV-Multiband-Saturator/actions/workflows/build.yml/badge.svg)](https://github.com/mblancosound/VCV-Multiband-Saturator/actions/workflows/build.yml)
[![Version](https://img.shields.io/github/v/tag/mblancosound/VCV-Multiband-Saturator)](https://github.com/mblancosound/VCV-Multiband-Saturator/tags)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.txt)

**Anthracite** — three-band saturator module for [VCV Rack 2](https://vcvrack.com/) by **Coal Dynamics** (Manuel Blanco).

Stereo 3-band saturator (Low / Mid / High) with per-band drive, pan, volume, makeup, CV inputs, and stereo in/out.

- Plugin: `Coal Dynamics` (slug `Coal_Dynamics`)
- Module: `Anthracite`
- Version: `2.9.1`
- License: MIT (see `LICENSE.txt`)
- Source: https://github.com/mblancosound/VCV-Multiband-Saturator

## Features

- 3 bands with crossover filters (stereo L/R)
- Per band: Drive, Pan, Volume, Makeup + CV for Drive/Pan/Volume
- Oversampling 1x / 2x / 4x / 8x (default **1x / off**; opt-in)
- Per-band saturation curve selector (Clean/Tanh/Cubic/Arctan/Hard/Foldback/Sine Fold/Tube/Diode), independent for Low/Mid/High
- Per-band solo (respects MIX) and curve copy (Low/Mid/High → all) in the context menu
- Crossover split selectable: `Legacy` (v2.5.0, default) or `LR4` perfect-reconstruction 4th-order Linkwitz-Riley (v2.6.0)
- ADAA (first-order anti-derivative anti-aliasing) on Tanh/Cubic/Arctan/Hard/Foldback/Sine Fold (v2.6.0, default off)
- Per-band Drive CV with its own attenuverter: +/-10 V spans the whole knob (v2.7.0)
- Per-band Makeup (0 dB..+12 dB, boost-only, v2.8.2) plus automatic per-band gain compensation (v2.7.0, default off)
- No module-level bypass: use Rack's own **Ctrl+E**, which routes input to output (v2.7.1)
- Read-only status info in the menu (sample rate, oversampling factor, latency, version)
- Master Output volume + Mix (dry/wet)

## Install

From the **VCV Library** (recommended): install **Coal Dynamics** from Rack's **Library** menu and let Rack handle updates.

From a **GitHub Release**: download the `.vcvplugin` for your platform from the [latest release](https://github.com/mblancosound/VCV-Multiband-Saturator/releases/latest), then install it from Rack via **Help > Install plugin file...**. Restart Rack afterwards.

The full user documentation lives in [`manual/Anthracite.md`](manual/Anthracite.md).

## Build

```bash
export RACK_DIR=/path/to/Rack-SDK-2.6.6   # Makefile default: /c/Rack-SDK2.6.6
make        # ./plugin.dll (or .so / .dylib)
make dist   # dist/Coal_Dynamics-<version>-<platform>.vcvplugin
make test   # offline DSP harness (17 cases; run it from the repo root)
```

## Docs

- [Anthracite module manual](manual/Anthracite.md)
- [Technical notes](docs/technical-notes.md)
- [Changelog](manual/changelog.md)

## License

MIT — see `LICENSE.txt`.
