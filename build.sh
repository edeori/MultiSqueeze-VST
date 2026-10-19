#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

CONFIG="${CONFIG:-Release}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
JUCE_SOURCE_DIR="${JUCE_SOURCE_DIR:-}"

case "$CONFIG" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *)
        echo "Unsupported CONFIG: $CONFIG" >&2
        echo "Use Debug, Release, RelWithDebInfo or MinSizeRel." >&2
        exit 2
        ;;
esac

cmake_args=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$CONFIG"
    -DMULTISQUEEZE_BUILD_TESTS=ON
)
if [[ -n "$JUCE_SOURCE_DIR" ]]; then
    cmake_args+=("-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_SOURCE_DIR")
fi

echo "==> Configuring MultiSqueeze ($CONFIG)"
cmake "${cmake_args[@]}"

if command -v sysctl >/dev/null 2>&1; then
    jobs="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
    jobs="${NUMBER_OF_PROCESSORS:-4}"
fi

echo "==> Building VST3, AU, Standalone and tests"
cmake --build "$BUILD_DIR" --config "$CONFIG" \
    --target MultiSqueeze_VST3 MultiSqueeze_AU MultiSqueeze_Standalone MultiSqueezeTests MultiSqueezeProcessorTests \
    --parallel "$jobs"

echo "==> Running DSP tests"
ctest --test-dir "$BUILD_DIR" -C "$CONFIG" --output-on-failure

# Copy JUCE's format outputs to the documented packaging paths.
ARTEFACTS="$BUILD_DIR/MultiSqueeze_artefacts/$CONFIG"
for format in VST3 AU Standalone; do
    mkdir -p "$BUILD_DIR/$format"
    ditto "$ARTEFACTS/$format" "$BUILD_DIR/$format"
done
VST3="$BUILD_DIR/VST3/MultiSqueeze.vst3"
AU="$BUILD_DIR/AU/MultiSqueeze.component"
APP="$BUILD_DIR/Standalone/MultiSqueeze.app"

for artefact in "$VST3" "$AU" "$APP"; do
    if [[ ! -e "$artefact" ]]; then
        echo "Missing build artefact: $artefact" >&2
        exit 1
    fi
done

for binary in \
    "$VST3/Contents/MacOS/MultiSqueeze" \
    "$AU/Contents/MacOS/MultiSqueeze" \
    "$APP/Contents/MacOS/MultiSqueeze"; do
    lipo "$binary" -verify_arch x86_64 arm64
done

echo
echo "==> Build complete"
echo "    Configuration: $CONFIG"
echo "    VST3:          $VST3"
echo "    AU:            $AU"
echo "    Standalone:    $APP"
