# MultiSqueeze

MultiSqueeze is a clean, feed-forward multiband compressor built with JUCE 8.
Its interface follows the Moth Production / SplitFire visual language and lets
you edit crossover points and per-band gain directly on the spectrum.

## Features

- 1–16 fully automatable bands with 24 dB/oct Linkwitz–Riley crossovers
- Per-band threshold, ratio, attack, release, output gain, bypass, solo and mute
- Internal detection or one of four independent external sidechain buses per band
- Typeable numeric fields for every continuous control and crossover edge
- Double-click the spectrum to add/remove splits; drag splits and gain lines
- Input/output FFT traces and per-band gain-reduction readout
- VST3, Audio Unit and standalone targets

The 16-band ceiling exists because VST3/AU hosts require a fixed parameter list.
All sixteen slots are declared when the plugin is loaded, so adding/removing a
band remains safe for automation, preset recall and session interchange.

## Build

```sh
./build.sh
```

This creates and tests universal macOS VST3, AU and Standalone builds under a
single `build/` directory, laid out as:

```
build/
  VST3/MultiSqueeze.vst3
  AU/MultiSqueeze.component
  Standalone/MultiSqueeze.app
```

Use `CONFIG=Debug BUILD_DIR=build-debug ./build.sh` for a debug build in its
own directory (a plain `CONFIG=Debug ./build.sh` reuses `build/` and
overwrites the Release artefacts, since each format's output directory is
pinned directly under `$BUILD_DIR`, not `$BUILD_DIR/<CONFIG>`). JUCE 8.0.4 is
fetched by CMake; for an offline build, use `JUCE_SOURCE_DIR=/path/to/JUCE
./build.sh`.

## Packaging

`packager.sh` builds and tests the universal macOS VST3, AU and Standalone
targets, then creates a ZIP archive and a selectable-component `.pkg`
installer under `build/Release/`:

```sh
./packager.sh
```

For an offline build, set `JUCE_SOURCE_DIR=/path/to/JUCE`. Public distribution
builds can be signed and notarized without storing credentials in the repository:

```sh
CODESIGN_IDENTITY="Developer ID Application: ..." \
INSTALLER_IDENTITY="Developer ID Installer: ..." \
NOTARY_PROFILE="notarytool-profile" \
./packager.sh --notarize
```

Use `./packager.sh --help` for all supported options. Package outputs
(the ZIP archive, the `.pkg` installer and `SHA256SUMS.txt`) are written to
`build/Release/`.
