# MultiSqueeze

MultiSqueeze is a clean, feed-forward multiband compressor built with JUCE 8.
Its interface follows the Moth Production / SplitFire visual language and lets
you edit crossover points and per-band gain directly on the spectrum.

MultiSqueeze is free, with no activation, license key or trial period.

## Features

- 1–16 fully automatable bands with 24 dB/oct Linkwitz–Riley crossovers
- Per-band threshold, ratio, attack, release, output gain, bypass, solo and mute
- Per-band detector input: the band itself, another active band before compression, or one of four external sidechain buses. Muting or soloing a band leaves its detector feed available. If the chosen band number becomes inactive, detection uses the target band until the source is active again.
- In the AU, select a band and enable **Sidechain**. Route a DAW send to **Band N** on that MultiSqueeze instance. Disabling the switch restores the previous detector. Band sends keep their input numbers across preset recall and other band toggles. An unused input below another routed input appears as **Unassigned** because AU input numbers are contiguous. Studio One 7 displays one sidechain per AU instance. Send to **Sidechains → MultiSqueeze** there and enable **Sidechain** on each band that should use that signal. Other hosts control when their send menu refreshes and whether it displays AU input names.
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

## Automated releases

See [release instructions](docs/RELEASE_PIPELINE.md) for macOS/Windows builds,
versioning and separate manual publication. Windows installs VST3 and standalone;
AU is macOS-only. External sidechain bus availability depends on the host.
