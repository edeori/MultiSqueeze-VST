#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

PRODUCT_NAME="MultiSqueeze"
BUNDLE_ID="com.mothproduction.multisqueeze"
CONFIG="Release"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
DIST_DIR="${DIST_DIR:-$BUILD_DIR/Release}"
VERSION="${VERSION:-$(sed -nE 's/^[[:space:]]*project\(MultiSqueeze VERSION ([^ )]+).*/\1/p' "$ROOT_DIR/CMakeLists.txt")}"
JUCE_SOURCE_DIR="${JUCE_SOURCE_DIR:-}"
CODESIGN_IDENTITY="${CODESIGN_IDENTITY:-}"
INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
NOTARIZE=false
SKIP_BUILD=false

usage() {
    cat <<'EOF'
Usage: ./packager.sh [--skip-build] [--notarize]

Builds universal macOS VST3, AU and Standalone targets, runs the DSP tests,
then creates both a ZIP archive and a selectable-component macOS installer.

Environment variables:
  BUILD_DIR              CMake build directory (default: build)
  DIST_DIR               Package output directory (default: build/Release)
  JUCE_SOURCE_DIR        Optional existing JUCE checkout for offline builds
  CODESIGN_IDENTITY      Developer ID Application identity
  INSTALLER_IDENTITY     Developer ID Installer identity
  NOTARY_PROFILE         notarytool keychain profile (required by --notarize)
  VERSION                 Package version override

Without Developer ID identities, bundles are ad-hoc signed and the PKG is
unsigned. This is suitable for local testing, but not public distribution.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build) SKIP_BUILD=true ;;
        --notarize) NOTARIZE=true ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ -z "$VERSION" ]]; then
    echo "Could not determine the project version from CMakeLists.txt." >&2
    exit 1
fi

if [[ "$NOTARIZE" == true ]]; then
    if [[ -z "$CODESIGN_IDENTITY" || -z "$INSTALLER_IDENTITY" || -z "$NOTARY_PROFILE" ]]; then
        echo "--notarize requires CODESIGN_IDENTITY, INSTALLER_IDENTITY and NOTARY_PROFILE." >&2
        exit 1
    fi
fi

if [[ "$SKIP_BUILD" == false ]]; then
    echo "==> Building $PRODUCT_NAME $VERSION"
    BUILD_DIR="$BUILD_DIR" CONFIG="$CONFIG" JUCE_SOURCE_DIR="$JUCE_SOURCE_DIR" "$ROOT_DIR/build.sh"
fi

# CMakeLists.txt pins the plugin's output directories to $BUILD_DIR, so each
# format's build result lands directly under build/<FORMAT>/.
VST3_SOURCE="$BUILD_DIR/VST3/MultiSqueeze.vst3"
AU_SOURCE="$BUILD_DIR/AU/MultiSqueeze.component"
APP_SOURCE="$BUILD_DIR/Standalone/MultiSqueeze.app"

for artefact in "$VST3_SOURCE" "$AU_SOURCE" "$APP_SOURCE"; do
    if [[ ! -e "$artefact" ]]; then
        echo "Missing build artefact: $artefact" >&2
        exit 1
    fi
done

for binary in \
    "$VST3_SOURCE/Contents/MacOS/MultiSqueeze" \
    "$AU_SOURCE/Contents/MacOS/MultiSqueeze" \
    "$APP_SOURCE/Contents/MacOS/MultiSqueeze"; do
    lipo "$binary" -verify_arch x86_64 arm64
done

PACKAGE_BASENAME="$PRODUCT_NAME-$VERSION-macOS"
WORK_DIR="$BUILD_DIR/.package-work"
ARCHIVE_ROOT="$WORK_DIR/$PACKAGE_BASENAME"
PKG_ROOT="$WORK_DIR/pkg"
COMPONENTS_DIR="$PKG_ROOT/components"

rm -rf "$WORK_DIR"
mkdir -p "$ARCHIVE_ROOT/VST3" "$ARCHIVE_ROOT/AU" "$ARCHIVE_ROOT/Standalone" "$COMPONENTS_DIR"
ditto "$VST3_SOURCE" "$ARCHIVE_ROOT/VST3/MultiSqueeze.vst3"
ditto "$AU_SOURCE" "$ARCHIVE_ROOT/AU/MultiSqueeze.component"
ditto "$APP_SOURCE" "$ARCHIVE_ROOT/Standalone/MultiSqueeze.app"

cat > "$ARCHIVE_ROOT/INSTALL.txt" <<'EOF'
MultiSqueeze installation

VST3: copy MultiSqueeze.vst3 to /Library/Audio/Plug-Ins/VST3/
AU: copy MultiSqueeze.component to /Library/Audio/Plug-Ins/Components/
Standalone: copy MultiSqueeze.app to /Applications/Moth Production/

After installing the AU, restart the DAW or rescan Audio Units.
EOF

ENTITLEMENTS="$PKG_ROOT/entitlements.plist"
cat > "$ENTITLEMENTS" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.device.audio-input</key>
    <true/>
</dict>
</plist>
EOF

sign_bundle() {
    local bundle="$1"
    local needs_audio_input="${2:-false}"

    if [[ -n "$CODESIGN_IDENTITY" ]]; then
        sign_args=(--force --deep --options runtime --timestamp --sign "$CODESIGN_IDENTITY")
        if [[ "$needs_audio_input" == true ]]; then
            sign_args+=(--entitlements "$ENTITLEMENTS")
        fi
        codesign "${sign_args[@]}" "$bundle"
    else
        codesign --force --deep --sign - "$bundle"
    fi
    codesign --verify --deep --strict "$bundle"
}

echo "==> Signing staged bundles"
sign_bundle "$ARCHIVE_ROOT/VST3/MultiSqueeze.vst3"
sign_bundle "$ARCHIVE_ROOT/AU/MultiSqueeze.component"
sign_bundle "$ARCHIVE_ROOT/Standalone/MultiSqueeze.app" true

mkdir -p "$DIST_DIR"
ZIP_OUTPUT="$DIST_DIR/$PACKAGE_BASENAME.zip"
rm -f "$ZIP_OUTPUT"
ditto -c -k --sequesterRsrc --keepParent "$ARCHIVE_ROOT" "$ZIP_OUTPUT"

STANDALONE_STAGING="$PKG_ROOT/staging-standalone"
STANDALONE_SCRIPTS="$PKG_ROOT/scripts-standalone"
VST3_STAGING="$PKG_ROOT/staging-vst3"
AU_STAGING="$PKG_ROOT/staging-au"
mkdir -p \
    "$STANDALONE_STAGING/Applications/Moth Production" \
    "$STANDALONE_SCRIPTS" \
    "$VST3_STAGING/Library/Audio/Plug-Ins/VST3" \
    "$AU_STAGING/Library/Audio/Plug-Ins/Components"
ditto "$ARCHIVE_ROOT/Standalone/MultiSqueeze.app" "$STANDALONE_STAGING/Applications/Moth Production/MultiSqueeze.app"
ditto "$ARCHIVE_ROOT/VST3/MultiSqueeze.vst3" "$VST3_STAGING/Library/Audio/Plug-Ins/VST3/MultiSqueeze.vst3"
ditto "$ARCHIVE_ROOT/AU/MultiSqueeze.component" "$AU_STAGING/Library/Audio/Plug-Ins/Components/MultiSqueeze.component"

cat > "$STANDALONE_SCRIPTS/postinstall" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
    -f "/Applications/Moth Production/MultiSqueeze.app" >/dev/null 2>&1 || true
exit 0
EOF
chmod +x "$STANDALONE_SCRIPTS/postinstall"

pkgbuild --root "$STANDALONE_STAGING" \
    --identifier "$BUNDLE_ID.standalone" --version "$VERSION" --install-location / \
    --scripts "$STANDALONE_SCRIPTS" "$COMPONENTS_DIR/standalone.pkg"
pkgbuild --root "$VST3_STAGING" \
    --identifier "$BUNDLE_ID.vst3" --version "$VERSION" --install-location / \
    "$COMPONENTS_DIR/vst3.pkg"
pkgbuild --root "$AU_STAGING" \
    --identifier "$BUNDLE_ID.au" --version "$VERSION" --install-location / \
    "$COMPONENTS_DIR/au.pkg"

DIST_XML="$PKG_ROOT/distribution.xml"
cat > "$DIST_XML" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>MultiSqueeze $VERSION</title>
    <organization>com.mothproduction</organization>
    <domains enable_localSystem="true"/>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <choices-outline>
        <line choice="standalone"/>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="standalone" title="Standalone application" start_selected="true">
        <pkg-ref id="$BUNDLE_ID.standalone"/>
    </choice>
    <choice id="vst3" title="VST3 plug-in" start_selected="true">
        <pkg-ref id="$BUNDLE_ID.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit plug-in" start_selected="true">
        <pkg-ref id="$BUNDLE_ID.au"/>
    </choice>
    <pkg-ref id="$BUNDLE_ID.standalone" version="$VERSION" onConclusion="none">standalone.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_ID.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_ID.au" version="$VERSION" onConclusion="none">au.pkg</pkg-ref>
</installer-gui-script>
EOF

PKG_OUTPUT="$DIST_DIR/$PRODUCT_NAME-$VERSION.pkg"
rm -f "$PKG_OUTPUT"
productbuild_args=(--distribution "$DIST_XML" --package-path "$COMPONENTS_DIR")
if [[ -n "$INSTALLER_IDENTITY" ]]; then
    productbuild_args+=(--sign "$INSTALLER_IDENTITY")
fi
productbuild "${productbuild_args[@]}" "$PKG_OUTPUT"

if [[ "$NOTARIZE" == true ]]; then
    echo "==> Notarizing installer"
    xcrun notarytool submit "$PKG_OUTPUT" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG_OUTPUT"
    xcrun stapler validate "$PKG_OUTPUT"
fi

(cd "$DIST_DIR" && shasum -a 256 "$(basename "$ZIP_OUTPUT")" "$(basename "$PKG_OUTPUT")" > SHA256SUMS.txt)

echo
echo "==> Packaging complete"
echo "    VST3:      $VST3_SOURCE"
echo "    AU:        $AU_SOURCE"
echo "    Standalone: $APP_SOURCE"
echo "    ZIP:       $ZIP_OUTPUT"
echo "    Installer: $PKG_OUTPUT"
echo "    Checksums: $DIST_DIR/SHA256SUMS.txt"
if [[ -z "$CODESIGN_IDENTITY" || -z "$INSTALLER_IDENTITY" ]]; then
    echo "    Note: local/ad-hoc signing was used; set Developer ID identities for distribution."
fi
