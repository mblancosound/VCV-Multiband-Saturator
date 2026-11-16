# Technical notes — Anthracite (Coal Dynamics)

Internal technical notes for the **Anthracite** module (plugin **Coal Dynamics**, slug `Coal_Dynamics`), author Manuel
Blanco, MIT license.

Repository-only document: **not distributed** in the `.vcvplugin` package (it is not part of `DISTRIBUTABLES`).

## Oversampling

Right-click the module → **Oversampling** → `Off (1x)` / `2x` / `4x` / `8x`. The setting is saved in the patch; old
patches without the key load in **1x (off)**. The change is applied at the start of the next audio block.

The whole chain (crossovers + selected curve (Tanh by default) + pan/vol + band sum) runs at `N × sampleRate`, so the
FIR pair adds a fixed latency of **15 samples** at 2x/4x/8x (0 at 1x). The dry path of MIX is
delayed by the same amount to keep the mix flat.

Measured at 48 kHz with a 5 kHz sine and full drive: worst in-band aliasing is −14.7 dB at 1x, −32.8 dB at 4x and −43.3 dB
at 8x. These figures were measured with the v2.1.0 single-band method (5 kHz sine, full drive); they are not
directly comparable with the 3-band ADAA table below.

The wet path also has a 15 Hz first-order DC blocker (always on, after the band sum). It removes the offset from
asymmetric curves (Diode/Tube) at the cost of about −1.9 dB at 20 Hz.

## Crossover

Right-click \u2192 **Crossover** -> `Legacy` / `LR4`. `Legacy` is the v2.5.0 split and stays bit-identical; `LR4` is a 4th-order
Linkwitz-Riley split built from two cascaded Butterworth stages (Q = 0.707 at `fc` and at `fc*sqrt(2)`), with the Mid band
reconstructed as `x - low - high`, so the three bands always sum back to the input.

`Legacy` leaves a reconstruction notch of **−9.91 dB at 500 Hz** (three bands at unity gain, Clean curve). With `LR4` the measured deviation from the analytic response is **0.0085 dB** over 20 Hz ... 15 kHz —.

## Anti-aliasing (ADAA)

Right-click \u2192 **ADAA** -> `Off` / `On`. `Off` is the v2.5.0 behaviour and stays bit-identical. `On` applies
first-order anti-derivative anti-aliasing to **Tanh, Cubic, Arctan, Hard, Foldback and Sine Fold**: instead of `f(u)` the
band outputs `(F(u) - F(u_prev)) / (u - u_prev)` with `F = integral of f`, falling back to `f(u)` when the sample-to-sample
step is below 1e-3 V. **Clean, Tube and Diode** are not covered and always run the direct curve.

Measured worst in-band alias (below 20 kHz) at 5 kHz, full drive, 3-band sum, in dB:

| Curve | 1x off | 1x on | 4x off | 4x on |
| --- | --- | --- | --- | --- |
| Tanh | -4.2 | -14.7 | -25.9 | -46.3 |
| Cubic | -8.8 | -23.1 | -22.8 | -48.9 |
| Arctan | -5.2 | -15.7 | -28.4 | -48.8 |
| Hard | -3.9 | -14.2 | -24.2 | -44.7 |
| Foldback | +5.3 | -2.4 | -8.2 | -28.9 |
| Sine Fold | +1.2 | -4.2 | -6.4 | -34.8 |

THD+N at 1 kHz, 50 % drive, Tanh: **+0.38 dB** in one band.

## Curves and TRIM

Right-click the module → **Low Curve** / **Mid Curve** / **High Curve** → curve. The setting is saved in the patch as
`"curves": [low, mid, high]`; old patches without the key load in **Tanh**. Setting it is undoable with **Ctrl+Z**
(v2.4.0); with **Link curves** enabled the change applies to all three bands at once (v2.5.0). Changing the curve can
produce a **click**: it is a setup control, with no crossfade.

Level. Static per-curve TRIM, measured at 1 kHz / 5 V / 50 % drive (TRIM = RMS_Tanh / RMS_curve). All 8 non-Clean curves
stay within ±1 dB at 50 % and 100 % drive; at 0 % drive Foldback is +1.7 dB and Sine Fold +1.1 dB.

## Drive CV and Makeup

Drive CV: `knobEff = clamp(knob + (V/10) * att, 0, 1)`; `drive = 1 + 10 * knobEff`.

Makeup: per-band boost-only 0 dB..+12 dB (linear gain 1.0..3.981), applied after the curve and before pan/volume.

## Bypass, menu and patch keys

Bypass: Rack's own **Ctrl+E** (`configBypass`). Menu: oversampling / crossover / ADAA / auto makeup / link curves /
curves / solo / copy curve / info.

## Test harness

Offline harness (`test/`): 17 cases over the real DSP core (`src/SaturatorDsp.hpp`) with frozen baselines.
