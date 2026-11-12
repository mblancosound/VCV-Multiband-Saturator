# Changelog

All notable changes to **Anthracite** (Coal Dynamics) since the last VCV Library release, **2.6.2**. Newest first.

## [2.8.3] - 2026-11-12

### Added

- User manual for the plugin and the module, submitted to the VCV Library:
  [`plugin.md`](plugin.md) and [`Anthracite.md`](Anthracite.md).

### Changed

- Version bump to `2.8.3`; module description improved in `plugin.json`.

### Notes

- Documentation-only release: no DSP, parameter ID, range, default, widget or SVG changes.

## [2.8.2] - 2026-11-11

### Fixed

- Coal logo: converted the wordmark from Montserrat text to vector paths, so it is visible in Rack's SVG renderer.

### Changed

- Per-band **Makeup** is now a boost-only control from **0 dB** to **+12 dB** (linear gain 1.0 to 3.9810717), default **0 dB**. The band **Volume** knob remains the attenuation control.

## [2.8.1] - 2026-11-11

### Changed

- Band **Volume** defaults to **100%** (was 80%).
- **Makeup** knobs display **dB** instead of percentage: `0.00 dB` at unity, `+12.04 dB` at the old maximum, `-inf` at zero.

## [2.8.0] - 2026-11-11

### Changed

- Panel redesigned from **18 HP** to **24 HP**, with three band sections: Drive, Pan/Volume/Makeup, CV jacks and attenuators, audio I/O, labels and screws.

## [2.7.1] - 2026-11-08

### Fixed

- **Auto Makeup**: the detector now compares the level entering the nonlinearity with the level leaving it, so the gain is about 1 in the linear region and compensates only the real compression.

- **Bypass**: now uses Rack's own **Ctrl+E** bypass, configured between the audio inputs and outputs. It is a hard switch (can click).

### Changed

- Test suite updated to **17 cases**.

## [2.7.0] - 2026-11-07

### Added

- **Drive CV** per band with its own attenuverter. `+10 V` with the attenuverter fully open spans the whole Drive knob.

- **Makeup** per band and **Auto Makeup** (context menu, default off).

### Changed

- **Bypass** with a crossfade (replaced by Rack's Ctrl+E in 2.7.1).