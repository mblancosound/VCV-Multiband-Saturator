# Changelog

All notable changes to **Anthracite** (Coal Dynamics) since the last
VCV Library release, **2.6.2**. Newest first.

## [2.8.3] - 2026-11-12

### Added

- User manual for the plugin and the module, submitted to the VCV Library:
  `manual/plugin.md` and `manual/Anthracite.md` (packaged with `DISTRIBUTABLES += manual`).

### Changed

- Version bump to `2.8.3`; module description improved in `plugin.json`.

### Notes

- Documentation-only release: no DSP, parameter ID, range, default, widget or SVG changes.

## [2.8.2] - 2026-11-11

### Fixed
- Coal logo: converted the wordmark from Montserrat `<text>` to vector paths,
  so it is visible in Rack's SVG renderer.

### Changed
- Per-band **Makeup** is now a boost-only control from **0 dB to +12 dB**
  (linear gain 1.0 to 3.9810717), default **0 dB**. The band **Volume** knob
  remains the attenuation control. The internal DSP still reads the same
  linear gain: no parameter ID, DSP or audio code changed.

## [2.8.1] - 2026-11-11

### Changed
- Band **Volume** defaults to **100%** (was 80%) for new instances.
- **Makeup** knobs display **dB** instead of percentage: `0.00 dB` at unity,
  `+12.04 dB` at the old maximum, `-inf` at zero. The parameter range, DSP
  and IDs are unchanged in this release.

## [2.8.0] - 2026-11-11

### Changed
- Panel redesigned from **18 HP to 24 HP** (360 x 380 px), with three band
  sections at X = 60 / 180 / 300 on the 2.54 mm grid: Drive, Pan/Volume/
  Makeup, CV jacks and attenuators, audio I/O, labels and screws.
- Panel-only change: parameter IDs, input/output IDs, DSP and saved-patch
  compatibility are untouched. Rack stores modules by slug and parameters
  by index, so a wider panel does not break patches.
- Added the panel specification and a 167-check geometry verifier.

## [2.7.1] - 2026-11-08

### Fixed
- **Auto Makeup law**: the detector now compares the post-drive band
  (`envIn`) with the curve output divided by its static trim (`envOut`), so
  the gain is about 1 in the linear region and compensates only the real
  compression. The old law compared pre-drive with post-curve and cancelled
  the Drive control.
- **Bypass**: removed the custom 5 ms crossfade and now uses Rack's own
  **Ctrl+E** via `configBypass(IN_L, OUT_L)` / `(IN_R, OUT_R)`. It is a hard
  switch (can click); the Output level and saturation lights are bypassed.
- **Identity**: restored the VCV Library identity to plugin **Coal Dynamics**
  and module **Anthracite** (the 2.7.0 build had renamed them to
  `Multiband Saturator` / `Multiband_Saturator`, which would have broken
  patches).
- Panel label **ANTHRACITE**.

### Changed
- Test suite updated to **17 cases**: the four custom-bypass cases were
  removed, the Auto Makeup tests were redefined, and `T-AUTOG-LIN` was added.

## [2.7.0] - 2026-11-07

### Added
- **Drive CV per band** with its own attenuverter. `+10 V` with the
  attenuverter fully open spans the whole Drive knob; the CV is applied in
  knob units and clamped, so an unpatched CV is bit-identical to 2.6.2.
- **Makeup per band** and **Auto Makeup** (context menu, default off):
  per-band gain compensation with 10 ms attack / 100 ms release, frozen
  below -60 dBFS, capped at 4x (+12 dB).
- **Bypass** with a 5 ms crossfade (replaced by Rack's Ctrl+E in 2.7.1).
