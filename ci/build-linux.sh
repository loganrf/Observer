#!/usr/bin/env bash
# Build, test and package the plugin inside a Debian container.
#
# Run from the repository root, for example:
#   docker run --rm -v "$PWD:/src" -w /src \
#     -e TUPLE='debian-x86_64;12;x86_64' -e VERSION=2026.10.0 \
#     debian:bookworm bash ci/build-linux.sh
#
# Environment:
#   TUPLE      OpenCPN target as "target;version;arch" (required)
#   VERSION    plugin version, YYYY.M.N (required)
#   BASE_URL   where the release will host the tarball (optional)
#
# Leaves the tarball and its metadata XML in dist/.

set -euo pipefail
: "${TUPLE:?}" "${VERSION:?}"

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
  build-essential ca-certificates cmake file git libwxgtk3.2-dev \
  lsb-release xauth xvfb >/dev/null

git config --global --add safe.directory "$PWD"

build=build-ci
rm -rf "$build" dist
cmake -S . -B "$build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOCPN_TARGET_TUPLE="$TUPLE" \
  -DOBSERVER_VERSION="$VERSION" \
  -DOBSERVER_TARBALL_BASE_URL="${BASE_URL:-}"
cmake --build "$build" --parallel "$(nproc)"

ctest --test-dir "$build" --output-on-failure

cmake --build "$build" --target tarball
file "$build"/libobserver_pi.so

mkdir -p dist
cp "$build"/*.tar.gz "$build"/*.xml dist/
chmod -R a+rwX dist "$build"
ls -l dist
