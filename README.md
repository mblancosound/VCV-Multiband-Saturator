# VCV-Multiband-Saturator

[![Build](https://github.com/mblancosound/VCV-Multiband-Saturator/actions/workflows/build.yml/badge.svg)](https://github.com/mblancosound/VCV-Multiband-Saturator/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/mblancosound/VCV-Multiband-Saturator)](https://github.com/mblancosound/VCV-Multiband-Saturator/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.txt)

**Anthracite** — three-band saturator module for [VCV Rack 2](https://vcvrack.com/) by **Coal Dynamics** (Manuel Blanco).

Stereo 3-band saturator (Low / Mid / High) with per-band drive, pan and volume, CV inputs, and stereo in/out.

- Plugin: `Coal Dynamics` (slug `Coal_Dynamics`)
- Module: `Anthracite` (`Anthracite`)
- Version: `2.7.1`
- License: MIT (see `LICENSE.txt`)
- Source: https://github.com/mblancosound/VCV-Multiband-Saturator

## Features

- 3 bands with Biquad crossover filters (stereo L/R)
- Per band: Drive, Pan, Volume, Makeup + CV for Drive/Pan/Volume
- Oversampling 1x / 2x / 4x / 8x (default **1x / off**; opt-in)
- Per-band saturation curve selector (Clean/Tanh/Cubic/Arctan/Hard/Foldback/Sine Fold/Tube/Diode), independent for Low/Mid/High
- Per-band solo (respects MIX) and curve copy (Low/Mid/High → all) in the context menu
- Crossover split selectable: `Legacy` (v2.5.0, default) or `LR4` perfect-reconstruction 4th-order Linkwitz-Riley (v2.6.0)
- ADAA (first-order anti-derivative anti-aliasing) on Tanh/Cubic/Arctan/Hard/Foldback/Sine Fold (v2.6.0, default off)
- Per-band Drive CV with its own attenuverter: +/-10 V spans the whole knob (v2.7.0)
- Per-band Makeup (0..4x) plus automatic per-band gain compensation (v2.7.0, default off)
- No module-level bypass: use Rack's own **Ctrl+E**, which routes input to output (v2.7.1)
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

## Crossover (v2.6.0)

Right-click -> **Crossover** -> `Legacy` / `LR4`. `Legacy` is the v2.5.0 split and stays bit-identical;
`LR4` is a 4th-order Linkwitz-Riley split built from two cascaded Butterworth stages (Q = 0.707 at `fc`
and at `fc*sqrt(2)`), with the Mid band reconstructed as `x - low - high`, so the three bands always sum
back to the input. Saved in the patch as `"crossover": "legacy"|"lr4"`; old patches, and any value other
than `"lr4"`, load as `Legacy`. Changing the mode resets the crossover state and can produce a short
**click**: it is a setup control.

`Legacy` leaves a reconstruction notch of **-9.91 dB at 500 Hz** (three bands at unity gain, Clean
curve). With `LR4` the measured deviation from the analytic response is **0.0085 dB** over
20 Hz ... 15 kHz.

## Anti-aliasing (ADAA, v2.6.0)

Right-click -> **ADAA** -> `Off` / `On`. `Off` is the v2.5.0 behaviour and stays bit-identical. `On`
applies first-order anti-derivative anti-aliasing to **Tanh, Cubic, Arctan, Hard, Foldback and Sine
Fold**: instead of `f(u)` the band outputs `(F(u) - F(u_prev)) / (u - u_prev)` with `F = integral of f`,
falling back to `f(u)` when the sample-to-sample step is below 1e-3 V. **Clean, Tube and Diode** are not
covered and always run the direct curve. Saved in the patch as `"adaa": true|false`; old patches, and any
value other than boolean `true`, load as `Off`.

Measured worst in-band alias (below 20 kHz) at 5 kHz, full drive, 3-band sum, in dB:

| Curve | 1x off | 1x on | 4x off | 4x on |
| --- | ---: | ---: | ---: | ---: |
| Tanh | -4.2 | -14.7 | -25.9 | -46.3 |
| Cubic | -8.8 | -23.1 | -22.8 | -48.9 |
| Arctan | -5.2 | -15.7 | -28.4 | -48.8 |
| Hard | -3.9 | -14.2 | -24.2 | -44.7 |
| Foldback | +5.3 | -2.4 | -8.2 | -28.9 |
| Sine Fold | +1.2 | -4.2 | -6.4 | -34.8 |

The cost in the audible band is small: at 1 kHz, 50 % drive, Tanh, THD+N changes by **+0.38 dB** on a
single band. It pays off most combined with 2x or more.

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
-8.5 dB. v2.2.1 had no ADAA; from v2.6.0 it can be enabled (see *Anti-aliasing*), or lower the drive or use 8x.

**Methodology note.** The Oversampling table above (−32.8 dB at 4x) was measured with the v2.1.0 method
(isolated high band/curve) and is not directly comparable with this table, which sums the 3 bands and
suffers phase cancellation at 5 kHz. Tanh, bit-identical to v2.1.0, gives −25.9 dB with this method: it is
not a regression.

## Drive CV (v2.7.0)

Each band has a **Drive CV** input and an attenuverter next to the drive knob. The CV is applied in
knob units and clamped, so the effective drive always stays in the range of the knob:

```
knobEff = clamp(knob + (V/10) * att, 0, 1)
drive   = 1 + 10 * knobEff
```

+10 V with the attenuverter fully open sweeps the whole knob; the attenuverter scales that depth and
its centre position disables the CV. With nothing patched the drive is bit-identical to v2.6.0, so old
patches are unaffected. No new key is stored in the patch: the attenuverters are regular parameters.

## Makeup and Auto Makeup (v2.7.0, law fixed in v2.7.1)

Each band has a **Makeup** gain from 0 to 4x (default 1.0 = no change), applied after the curve and
before pan and volume.

**Auto Makeup** (context menu, default `Off`) replaces the manual makeup with a per-band gain
compensation. The detector compares the level that **enters** the non-linearity - the band after the
drive, `envIn` - with the level that **leaves** it, with the curve's static trim divided out
(`envOut`), and applies `envIn / envOut`, smoothed. The makeup therefore compensates only the
compression the curve is really doing: in the linear region the gain is 1.0, so turning the drive up
still makes the band louder, which is the whole point of the drive control.

The envelopes use a 10 ms attack and a 100 ms release and the gain is frozen below -60 dBFS, so
silence does not pump. The gain is capped at 4x (+12 dB). Curves whose slope at the origin is not 1
(Cubic 1.5, Tube 1.46) divide their own slope out, so their linear-region gain is about 0.67; that is
the curve's own gain, not a compensation error. With Auto Makeup on, the manual makeup knob is
ignored. Saved in the patch as `"autoMakeup": true|false`; old patches load with it off.

Measured through the real chain at 1x and 4x: switching it on in the linear region (Clean and Tanh,
drive 3, 20 mV peak) moves the level by at most 0.01 dB, and at drive 11 with 0.2 V peak it recovers
between 5.8 dB (Hard, Tube) and 9.0 dB (Foldback) of the compression. Peak-to-peak ripple after
200 ms is below 0.02 dB.

**v2.7.1 fixed a wrong law.** v2.7.0 compared the band *before* the drive with the output *after*
the curve, two different scales. The gain then came out as roughly `1 / drive` in the linear region,
so it cancelled the drive control (Clean was a constant +11 dB) and clamped at 4x with hot signals.
If you used Auto Makeup in 2.7.0 and it seemed to kill the saturation, that was the bug.

## Bypass (v2.7.1)

There is **no bypass entry in the context menu** any more. Use Rack's own bypass, **Ctrl+E**, which
the module declares through `configBypass()`: while bypassed, Rack routes each input straight to the
corresponding output and stops calling the module, so the module's own DSP, the Output level and the
saturation lights do not take part. It is a hard switch, not a crossfade, so it can click on loud
material. A patch saved by 2.7.0 with `"bypass": true` loads normally and simply ignores that key.

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
- **Crossover** -> `Legacy` / `LR4`. See *Crossover* above. Undoable (*Undo change crossover*).
- **ADAA** -> `Off` / `On`. See *Anti-aliasing* above. Undoable (*Undo toggle adaa*).
- **Auto Makeup** -> `Off` / `On`. See *Makeup and Auto Makeup* above. Undoable (*Undo toggle auto makeup*).
- **Info** (read-only label): `48.0 kHz | 1x | 0 samples | v2.7.1` - sample rate, oversampling
  factor, latency in samples (0 at 1x, 15 at 2x/4x/8x) and plugin version.

The saturation lights keep metering each band **before** the solo, so a muted band still shows its
saturation level.

**Undo (v2.4.0, extended in v2.5.0, v2.6.0 and v2.7.0).** The context-menu actions — curve change, the linked change (one
single entry for the three bands), **Copy Curve** (also one single entry), **Solo**, **Oversampling**, **Crossover**, **ADAA**, **Auto Makeup**
and the **Link curves** toggle — are undoable with **Ctrl+Z** and redoable with **Ctrl+Shift+Z**,
using Rack's standard history (e.g. *Undo change curve*, *Undo change curves*, *Undo toggle link
curves*). Re-selecting an already active option creates no history entry, and loading a patch is never
recorded as an undoable action.

## Install

Download the `.vcvplugin` for your platform from
[Releases](https://github.com/mblancosound/VCV-Multiband-Saturator/releases/latest) — `win-x64`,
`lin-x64`, `mac-x64`, `mac-arm64` — and install it from Rack via **Help > Install plugin file...**.
Restart Rack afterwards.

## Build

Requirements: VCV Rack SDK 2.6.6 (`RACK_DIR`), `jq`, and a native toolchain (GCC on Linux, MSYS2/MinGW on
Windows, Xcode/Clang on macOS). CI builds the four platforms with these same commands.

```bash
export RACK_DIR=/path/to/Rack-SDK-2.6.6   # Makefile default: /c/Rack-SDK2.6.6
make        # ./plugin.dll (or .so / .dylib)
make dist   # dist/Coal_Dynamics-<version>-<platform>.vcvplugin
make test   # offline DSP harness (12 cases; run it from the repo root)
```

Windows + MSYS2 timings: clean 7.2 s, one file 2.7 s, no-op 0.9 s, `make dist` 1.6 s.

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

## License

MIT — see `LICENSE.txt`.
