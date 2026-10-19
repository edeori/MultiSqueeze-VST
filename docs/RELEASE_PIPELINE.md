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

## Website and signing configuration

The Shop entry uses `price: 0` and `requires_license: '0'`. The latter explicitly
selects direct download buttons instead of the site's free-license checkout.
The product has Hungarian/English copy and a screenshot of the real application.

The GitHub repository variables `CODESIGN_IDENTITY` and `INSTALLER_IDENTITY`
identify the existing Developer ID Application/Installer certificates. The
shared workflow imports the certificates from Secrets; no keys are stored here.

## Verified release — 2026-10-19

Version **0.1.12**, compiled source `9f26b3b550243b725de50f1cb8a2e6d532d185be`.

- [Build](https://github.com/edeori/MultiSqueeze-VST/actions/runs/39317418031):
  both platforms passed both CTest suites. Mac signature, universal architectures,
  notarization and stapling passed; Windows Defender reported clean.
- [Dry Publish](https://github.com/edeori/MultiSqueeze-VST/actions/runs/39319023388):
  provenance and hash verification passed, upload step skipped.
- [Live Publish](https://github.com/edeori/MultiSqueeze-VST/actions/runs/39319108702):
  both installers uploaded successfully. Public downloads were downloaded again
  and matched the build artifacts byte-for-byte; package/EXE versions are 0.1.12.
- Hungarian and English Shop pages show free direct download buttons, version
  0.1.12, localized copy, and an actual screenshot of the CI-built standalone.
  No license checkout is required. The extracted Mac standalone opened normally.
- Local DSP and processor tests passed. The Shop CTA checks covered existing
  free licensed products, free activation-free downloads, paid products and an
  unavailable activation-free product.

SHA-256:

```text
mac: b377bb34fdc37ff10b3f87b2b19b548377590844e20ae7e48388118fd861fbb7
win: c3bdb1a46b9620ce8de81cc74a93f0bf9f84d79adc49c7f725de48f1231ed1ef
```

The initial 0.1.11 integration attempt was not published. It exposed missing
signing identity variables and a processor-test include-path setup issue; both
were corrected before the successful 0.1.12 build.
