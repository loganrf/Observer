#!/usr/bin/env bash
# Build a universal (arm64 + x86_64) macOS plugin.
#
# Links against the dependency bundle OpenCPN publishes for plugins (the
# one the OpenCPN plugin template uses). Its wxWidgets is the build that
# OpenCPN.app's bundled copy stays compatible with; a wxWidgets built
# here could carry library names OpenCPN.app does not ship. At packaging
# time the plugin's wxWidgets references are pointed at the copies inside
# OpenCPN.app (see cmake/MakeTarball.cmake).
#
# Environment: VERSION (required), BASE_URL (optional).
# Leaves the tarball and its metadata XML in dist/.

set -euo pipefail
: "${VERSION:?}"

deps_url=https://dl.cloudsmith.io/public/nohal/opencpn-plugins/raw/files/macos_deps_universal.tar.xz
# OpenCPN's macOS builds target 10.13; arm64 code needs 11 regardless.
export MACOSX_DEPLOYMENT_TARGET=10.13
jobs=$(sysctl -n hw.ncpu)

wx_config=/usr/local/lib/wx/config/osx_cocoa-unicode-3.2
if [[ ! -x $wx_config ]]; then
  curl -fsSL "$deps_url" -o /tmp/macos_deps_universal.tar.xz
  sudo tar -C /usr/local -xJf /tmp/macos_deps_universal.tar.xz
fi
"$wx_config" --version

build=build-ci
rm -rf "$build" dist
cmake -S . -B "$build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET \
  -DwxWidgets_CONFIG_EXECUTABLE="$wx_config" \
  -DOCPN_TARGET_TUPLE="darwin-wx32;10.13;universal" \
  -DOBSERVER_VERSION="$VERSION" \
  -DOBSERVER_TARBALL_BASE_URL="${BASE_URL:-}"
cmake --build "$build" --parallel "$jobs"
DYLD_LIBRARY_PATH=/usr/local/lib ctest --test-dir "$build" --output-on-failure
cmake --build "$build" --target tarball

lipo -info "$build"/libobserver_pi.dylib
mkdir -p dist
cp "$build"/*.tar.gz "$build"/*.xml dist/
ls -l dist
