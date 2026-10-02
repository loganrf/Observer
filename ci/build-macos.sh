#!/usr/bin/env bash
# Build a universal (arm64 + x86_64) macOS plugin.
#
# Builds a universal wxWidgets 3.2 into $WX_PREFIX first if it is not
# there already; CI caches that directory. wxWidgets uses only its
# built-in image and regex libraries, so the result does not depend on
# Homebrew. At packaging time the plugin's wxWidgets references are
# pointed at the copies inside OpenCPN.app (see cmake/MakeTarball.cmake).
#
# Environment: VERSION (required), BASE_URL (optional),
#              WX_PREFIX (default ~/wx-universal).
# Leaves the tarball and its metadata XML in dist/.

set -euo pipefail
: "${VERSION:?}"

wx_version=3.2.6
wx_prefix=${WX_PREFIX:-$HOME/wx-universal}
# OpenCPN 5.8 still ran on macOS 10.13; arm64 code needs 11 regardless.
export MACOSX_DEPLOYMENT_TARGET=10.13
jobs=$(sysctl -n hw.ncpu)

if [[ ! -x $wx_prefix/bin/wx-config ]]; then
  src=$(mktemp -d)
  curl -fsSL \
    "https://github.com/wxWidgets/wxWidgets/releases/download/v$wx_version/wxWidgets-$wx_version.tar.bz2" |
    tar -C "$src" -xjf -
  pushd "$src/wxWidgets-$wx_version" >/dev/null
  ./configure --prefix="$wx_prefix" \
    --with-osx-cocoa \
    --enable-universal_binary=arm64,x86_64 \
    --with-macosx-version-min=$MACOSX_DEPLOYMENT_TARGET \
    --disable-sys-libs \
    --disable-debug \
    --enable-aui \
    --with-opengl \
    --without-subdirs >/dev/null
  make -j"$jobs" >/dev/null
  make install >/dev/null
  popd >/dev/null
  rm -rf "$src"
fi
"$wx_prefix/bin/wx-config" --version

build=build-ci
rm -rf "$build" dist
cmake -S . -B "$build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET \
  -DwxWidgets_CONFIG_EXECUTABLE="$wx_prefix/bin/wx-config" \
  -DOCPN_TARGET_TUPLE="darwin-wx32;10.13;universal" \
  -DOBSERVER_VERSION="$VERSION" \
  -DOBSERVER_TARBALL_BASE_URL="${BASE_URL:-}"
cmake --build "$build" --parallel "$jobs"
DYLD_LIBRARY_PATH="$wx_prefix/lib" ctest --test-dir "$build" --output-on-failure
cmake --build "$build" --target tarball

lipo -info "$build"/libobserver_pi.dylib
mkdir -p dist
cp "$build"/*.tar.gz "$build"/*.xml dist/
ls -l dist
