# Anthracite

**Anthracite** is a stereo three-band saturator module for [VCV Rack 2](https://vcvrack.com/). It splits the incoming left/right signal into **Low**, **Mid** and **High** bands, applies a selectable saturation curve with per-band drive, and sums the bands back into the stereo output.

- **Plugin:** Coal Dynamics (slug `Coal_Dynamics`)
- **Module:** Anthracite
- **Author:** Manuel Blanco
- **License:** MIT

## Overview

Anthracite is a three-band saturation processor. The stereo input is split into three bands; each band is pushed through a saturation curve (selected per band), scaled by that band's **Drive**, and shaped by **Pan**, **Volume** and **Makeup**. The band signals are summed and the result is passed through the global **Mix** and **Output** controls.

The module is intended for mixing, sound design and mastering tasks that require three-band saturation with per-band character and drive.

## Panel and band columns

The module panel is **24 HP** wide and has **three band columns** (Low, Mid, High). Each column groups the controls of one band: Drive, Pan, Volume, Makeup, CV inputs and attenuators, and a pair of saturation lights per band (one per channel, L and R).

The module processes a stereo signal in a single voice; it does not support polyphony.

## Controls per band

### Drive

Each band has a **Drive** knob with range **0…1** (display **0…100 %**), default **0**, unit `%`.

The internal drive value runs from **1** (no extra drive) to **11** (maximum): knob **0** gives drive **1** (no extra drive) and knob **1** gives drive **11** (maximum saturation).

### Pan

Each band has a **Pan** knob, bipolar from **−1** to **+1**, default **0**. It applies a balance law so that positive values pan to the right and the channel that should be silent is attenuated to exactly **0**.

The knob display shows the mark **C** at the centre and the left/right marks at the extremes.

### Volume

Each band has a **Volume** knob from **0…1** (display **0…100 %**), default **1** (100 %). It attenuates the band.

### Makeup

Each band has a **Makeup** control, boost-only, from **0 dB** to **+12 dB** (linear gain **1.0…3.9810717**, default **1.0** = 0 dB), displayed in dB. It is applied after the curve and before the pan and volume.

## CV inputs and attenuators

Each band has three CV inputs, each with its own attenuator: **Drive CV**, **Pan CV** and **Volume CV**:

- **Pan CV:** CV voltage from **−10 V** to **+10 V**, attenuator from **−1** to **+1** (default 0). With the attenuator fully open and +10 V the pan reaches **+1** (full right); with −10 V it reaches **−1** (full left).
- **Volume CV:** with the attenuator fully open and +10 V the volume reaches **1** (100 %); with −10 V it reaches **0** (silence).
- **Drive CV:** with the attenuator fully open and +10 V the drive reaches **11**; with −10 V the drive reaches **1**.

## Output and Mix

- **Output** level: **0…1** (display **0…100 %**), default **1** (100 %). Global.
- **Mix**: **0…1** (display **0…100 %**), default **1**. Dry/wet mix.

The output is computed from the dry signal (delayed by the same amount as the wet signal) plus the wet signal, then scaled by the Output level.

## Context menu

Right-click the module. The custom context menu offers:

### Oversampling

Options: `Off (1x)`, `2x`, `4x`, `8x`. Default: `Off (1x)`.

- The setting is stored in the patch and the change is applied at the start of the next audio block.
- Changing the factor resets the filters, so it can produce a short **click**; it is a setup control, not an automatable parameter.

Latency: **0 samples** at **1x**; **15 samples** at **2x** / **4x** / **8x**.

### Crossover

Options: `Legacy`, `LR4`. Default: `Legacy`.

- `Legacy` is the original split; it uses lowpass **250 Hz**, bandpass **1000 Hz** and highpass **4000 Hz**.
- `LR4` is a 4th-order Linkwitz-Riley split built from two cascaded Butterworth stages; the Mid band is reconstructed as the difference between the input and the low/high bands.

Changing the mode resets the crossover state and can produce a short **click**.

### ADAA

`Off` / `On`, default `Off`. ADAA (anti-derivative anti-aliasing) reduces aliasing on the supported curves. It affects the **Tanh**, **Cubic**, **Arctan**, **Hard**, **Foldback** and **Sine Fold** curves; the **Clean**, **Tube** and **Diode** curves are not covered.

### Auto Makeup

`Off` / `On`, default `Off`. Replaces the manual makeup with a per-band gain compensation. The detector compares the level entering the nonlinearity (the band after the drive) with the level leaving it (the curve output divided by its static calibration value) and applies the ratio, smoothed. The makeup compensates only the compression the curve really does.

The envelopes use a **10 ms** attack and a **100 ms** release; the gain is frozen below **-60 dBV** (1 mV) and capped at **4x** (+12 dB). With Auto Makeup on, the manual makeup knob is ignored.

### Link curves

Toggle. While active, changing the Low, Mid or High curve applies the new curve to all three bands. Enabling the toggle does not copy the existing curves; it only governs the changes made afterwards.

### Low Curve / Mid Curve / High Curve

Per-band curve selector. Nine curves available: Clean, Tanh, Cubic, Arctan, Hard, Foldback, Sine Fold, Tube, Diode. Default: Tanh.

### Solo

`Off` / `Low` / `Mid` / `High`. Default: `Off`.

- The Solo respects the **Mix** control: it silences the dry signal and the other bands (with a ramp of about **5 ms**), letting the selected band pass through with the current Mix value.
- The saturation lights keep measuring the band before Solo.

### Copy Curve

`Low to all` / `Mid to all` / `High to all`. Copies the selected band's curve to all three bands.

### Info

Read-only label: `48.0 kHz | 1x | 0 samples | v2.8.3` — sample rate, oversampling factor, latency in samples (0 at 1x, 15 at 2x/4x/8x) and plugin version.

## Bypass

Rack's own **Ctrl+E** bypass. When bypassed, Rack routes the audio inputs directly to the outputs; the module's processing is bypassed. It is a hard switch and can click.

## Signal flow

~~~
IN L/R
  |
  |-- dry signal  --> delay (same latency as wet) --> mix --> output level --> out
  |
  `-- wet signal: upsample xN -> crossover bands -> curve -> makeup -> pan/volume -> sum bands -> downsample xN -> DC blocker (15 Hz) -> mix -> output level -> out
~~~

## Installation

- **From the VCV Library** (recommended): install from Rack's **Library** menu. Version **2.8.3** is the version submitted to the VCV Library.
- **From a package**: download the `.vcvplugin` from the **GitHub Releases** page of the [Coal Dynamics repository](https://github.com/mblancosound/VCV-Multiband-Saturator), install it via **Help > Install plugin file...** in Rack, and restart Rack.

## Patch compatibility

- Missing or invalid settings fall back to safe defaults: oversampling → `Off (1x)`, crossover → `Legacy`, ADAA → `Off`, Auto Makeup → `Off`, curves → `Tanh`, Solo → `Off`, link curves → `Off`.

## Gain staging

- **Clean** is level-matched and does not add coloration; use it as a transparent reference.
- Non-linear curves add harmonics and can change the perceived level; use **Auto Makeup** or the per-band **Makeup** knob to compensate, and use the band **Volume** knob to attenuate.

## Technical notes

- Clean is normalized to the Tanh RMS at the 5 V reference.
- At 5 V inputs, Clean is within +0.5 / −0.0 / −0.0 dB of Tanh at 0 / 50 / 100 % drive; at quieter ~1 V inputs it sits 9.3–13.8 dB below the saturated curves.
- All non-linear curves stay within ±1 dB at 50 % and 100 % drive.

## Saturation curves

Nine curves: Clean, Tanh, Cubic, Arctan, Hard, Foldback, Sine Fold, Tube, Diode.

## License and credits

MIT — see the repository [LICENSE](https://github.com/mblancosound/VCV-Multiband-Saturator/blob/main/LICENSE.txt). Developed by Manuel Blanco (Coal Dynamics).

[Changelog](changelog.md)