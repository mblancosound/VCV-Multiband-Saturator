# VCV-Multiband-Saturator

Multiband Saturator plugin for [VCV Rack 2](https://vcvrack.com/) by **Coal Dynamics** (Manuel Blanco).

Stereo 3-band saturator (Low / Mid / High) with per-band drive, pan and volume, CV inputs, and stereo in/out.

- Plugin slug: `Coal_Dynamics`
- Module: `Multiband Saturator` (`Multiband_Saturator`)
- Version: `2.5.0`
- License: MIT (see `LICENSE.txt`)

## Features

- 3 bands with Biquad crossover filters (stereo L/R)
- Per band: Drive, Pan, Volume + CV for Pan/Volume
- Oversampling 1x / 2x / 4x / 8x (default **1x / off**; opt-in)
- Per-band saturation curve selector (Clean/Tanh/Cubic/Arctan/Hard/Foldback/Sine Fold/Tube/Diode), independent for Low/Mid/High
- Per-band solo (respects MIX) and curve copy (Low/Mid/High → all) in the context menu
- Read-only status info in the menu (sample rate, oversampling factor, latency, version)
- Master Output volume + Mix (dry/wet)
- Custom SVG knobs, ports and panel in `res/`

## Oversampling

Right-click the module → **Oversampling** → `Off (1x)` / `2x` / `4x` / `8x`. The setting is saved in
the patch; old patches without the key load in **1x (off)**. The change is applied at the start of the next
audio block.

The whole chain (crossovers + selected curve (Tanh by default) + pan/vol + band sum) runs at `N × sampleRate`, so the FIR pair
adds a fixed latency of **15 samples** at 2x/4x/8x (0 at 1x). The dry path of MIX is delayed by the
same amount to keep the mix flat. Changing the factor resets the filters, so it can produce a short
**click**; it is a setup control, not an automatable parameter.

Measured at 48 kHz with a 5 kHz sine and full drive: worst in-band aliasing is −14.7 dB at 1x,
−32.8 dB at 4x and −43.3 dB at 8x. See `specs/oversampling_spec.md` for the full table and limits.

The wet path also has a 15 Hz first-order DC blocker (always on, after the band sum). It removes the offset
from asymmetric curves (Diode/Tube) at the cost of about −1.9 dB at 20 Hz.

## Curves (v2.2.1)

Right-click the module → **Low Curve** / **Mid Curve** / **High Curve** → curve. The active curve is shown
with a radio check. The setting is saved in the patch as `"curves": [low, mid, high]`; old patches without
the key load in **Tanh**. Setting it is undoable with **Ctrl+Z** (v2.4.0); with **Link curves**
enabled the change applies to all three bands at once (v2.5.0). Changing the curve can produce a
**click**: it is a setup control, with no crossfade.

| Curve | Character | Note |
| --- | --- | --- |
| Clean | Linear (no Drive; level-matched) | Scaled to the same RMS as Tanh at the 5 V reference (TRIM = 0.279824): never louder than the saturated curves. Ignores Drive and passes the band through at −11.1 dB of its dry level. |
| Tanh | Original v2.1.0 behaviour | Smooth odd saturation. |
| Cubic | Odd polynomial | Knee at \|u\| = 1; smoother than Hard, but 1.5x slope at zero (more low-level THD than Tanh). |
| Arctan | Rounded saturation | The softest curve. |
| Hard | Hard clip | Bright; high alias. |
| Foldback | Wavefolder | Very aggressive; high alias even at 8x. |
| Sine Fold | Sine folding | Soft at low level; high alias at 1x/4x. |
| Tube | Asymmetry with clipping at −1 | Generates H2. |
| Diode | Asymmetry +1/−0.5 | H2 and DC (the blocker removes it). |

**Level.** Static per-curve TRIM, measured at 1 kHz / 5 V / 50 % drive (TRIM = RMS_Tanh / RMS_curve). All 8
non-Clean curves stay within ±1 dB at 50 % and 100 % drive; at 0 % drive Foldback is +1.7 dB and Sine Fold
+1.1 dB. **Clean (v2.2.1 safety fix)** ignores Drive and is scaled to Tanh's RMS at the 5 V reference:
at 5 V inputs it is +0.5/−0.0/−0.0 dB vs Tanh at 0/50/100 % drive (never above +1 dB). At quieter ~1 V
inputs it sits 9.3–13.8 dB **below** the saturated curves — expected, since those curves compress loud
inputs down to ~±1 while Clean does not. Gain control (per band or OUTPUT_VOL) compensates if needed.

**Alias warning** (measured at 5 kHz, 100 % drive, full 3-band chain). At 4x, these exceed −25 dB: Foldback
−8.2 dB, Sine Fold −6.4 dB, Cubic −22.8 dB, Hard −24.2 dB and Tube −23.5 dB. At 8x, Foldback is still
−8.5 dB. There is no ADAA in v2.2.1: lower the drive or use 8x.

**Methodology note.** The Oversampling table above (−32.8 dB at 4x) was measured with the v2.1.0 method
(isolated high band/curve) and is not directly comparable with this table, which sums the 3 bands and
suffers phase cancellation at 5 kHz. Tanh, bit-identical to v2.1.0, gives −25.9 dB with this method: it is
not a regression.

## Context menu

Right-click the module:

- **Solo** → `Off` / `Low` / `Mid` / `High`. A real per-band solo that **respects MIX**: it mutes
  the dry path and the other two bands while the selected band keeps going through the current MIX
  setting, so `MIX = 0` gives silence. The transition is ramped over ~5 ms (no click) and saved in
  the patch as `"solo": -1|0|1|2` (`-1` = Off). Old patches without the key load in `Off`; an
  invalid value also falls back to `Off`.
- **Link curves** (check): while enabled, changing the curve on any band applies it to all three, as a
  single undo entry (*Undo change curves*). Enabling it does **not** overwrite the existing curves: it
  only affects later changes (use **Copy Curve** to unify them right away). Saved in the patch as
  `"linkCurves": true|false`; old patches without the key, or with an invalid value, load with it off.
- **Copy Curve** → `Low to all` / `Mid to all` / `High to all`. Copies the current curve of that
  band to the three bands. It uses the existing `"curves"` key, so nothing new is saved. It can
  click, exactly like picking a curve by hand.
- **Info** (read-only label): `48.0 kHz | 1x | 0 samples | v2.5.0` — sample rate, oversampling
  factor, latency in samples (0 at 1x, 15 at 2x/4x/8x) and plugin version.

The saturation lights keep metering each band **before** the solo, so a muted band still shows its
saturation level.

**Undo (v2.4.0, extended in v2.5.0).** The context-menu actions — curve change, the linked change (one
single entry for the three bands), **Copy Curve** (also one single entry), **Solo**, **Oversampling**
and the **Link curves** toggle — are undoable with **Ctrl+Z** and redoable with **Ctrl+Shift+Z**,
using Rack's standard history (e.g. *Undo change curve*, *Undo change curves*, *Undo toggle link
curves*). Re-selecting an already active option creates no history entry, and loading a patch is never
recorded as an undoable action.

## Requirements

- VCV Rack SDK 2.6.6 en `C:\Rack-SDK2.6.6` (ver `Makefile`: `RACK_DIR ?= /c/Rack-SDK2.6.6`)
- MSYS2 + `jq` (Mingw64: `pacman -S mingw-w64-x86_64-jq`; el `Makefile` añade `/mingw64/bin` al `PATH` y falla con mensaje claro si falta `jq`)
- Toolchain compatible con Rack plugin development (GCC/Make on Linux, MSYS2/MinGW on Windows, Xcode/Clang on macOS)

## Build

```bash
make        # builds ./plugin.dll (repo root)
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
  SaturatorWidget.cpp          # UI + context menus (oversampling, curves, solo, copy, info)
  Oversampling.hpp             # 1x/2x/4x/8x stages (header-only)
  Curves.hpp                   # 9 per-band saturation curves (header-only)
res/               # panel + component SVGs
LICENSE.txt
```

## Install (from source)

1. `make dist`
2. Copy `dist/Coal_Dynamics-2.5.0-win-x64.vcvplugin` (name varies by platform) into your Rack `plugins/` folder, or install it from Rack via `Help > Install plugin file...`.
3. Restart Rack.

## License

MIT — see `LICENSE.txt`.
