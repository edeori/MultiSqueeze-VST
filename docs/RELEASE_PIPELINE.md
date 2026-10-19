# MultiSqueeze releases

MultiSqueeze is free and has no activation, licensing client, trial timeout or
license-server dependency. The release build leaves the layout tuner disabled.

- **Build installers** runs on main pushes and manual dispatch. The shared
  workflow reserves a patch version, commits it back with `[skip ci]`, and builds
  that exact SHA on both platforms. Rerunning a build preserves its version.
- **Publish installers** is a separate manual workflow. Select **Latest successful
  build** (default) or **Specific build ID**. `verify_only=true` validates the
  artifacts without uploading; false publishes both installers.
- macOS: universal Intel/Apple Silicon AU, VST3 and standalone, Developer ID
  signing, notarization and stapling.
- Windows: x64 VST3 and standalone, Inno Setup installer, required Defender scan.
  The Windows installer is not Authenticode-signed.
- DSP and processor regression tests run on both platforms before packaging.
- Artifacts are retained for 30 days. Publish checks provenance and the EXE hash
  against the clean Defender report.

Product ID: `multisqueeze`; repository: `edeori/MultiSqueeze-VST`.

Stable downloads:
- https://download.mothproduction.hu/downloads/plugins/multisqueeze/mac/MultiSqueeze.pkg
- https://download.mothproduction.hu/downloads/plugins/multisqueeze/win/MultiSqueeze-Setup.exe

## Release review

The first pipeline release adds phase compensation to lower crossover bands so
unity-gain band summing retains a flat magnitude response. Cutoffs are limited
below Nyquist at low sample rates. Both parameter and host bypass preserve the
announced lookahead latency; delay history stays current while lookahead is off.
The UI rejects edits where coincident automated crossover edges leave no valid
frequency interval. Existing parameter IDs, plugin IDs and state format remain.

Regression coverage includes flat summed response at 22.05/48/96 kHz with
3/8/16 bands, lookahead history, bypass impulse timing, state recall, the 16-band
limit and low-rate processing, plus existing compressor curve/sidechain tests.
This is a targeted release review, not a claim of compatibility with every DAW.
