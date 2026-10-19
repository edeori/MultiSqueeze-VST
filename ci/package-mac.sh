#!/usr/bin/env bash
set -euo pipefail
./packager.sh --notarize
version=$(sed -nE 's/^project\(MultiSqueeze VERSION ([^ )]+).*/\1/p' CMakeLists.txt)
mkdir -p dist
cp "build/Release/MultiSqueeze-${version}.pkg" dist/installer.pkg
