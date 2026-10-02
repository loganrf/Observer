#!/usr/bin/env bash
# Build the plugin as a Flatpak extension of OpenCPN from Flathub.
#
# Usage: ci/build-flatpak.sh stable|beta
#
# The OpenCPN Flatpak on the chosen Flathub branch decides the
# freedesktop runtime, and so the SDK and the plugin's target version.
# When the beta branch runs on the same runtime as stable, the stable
# build already covers it and nothing is produced.
#
# Environment: VERSION (required), BASE_URL (optional).
# Leaves the tarball and its metadata XML in dist/.

set -euo pipefail
: "${VERSION:?}"
branch=${1:?usage: build-flatpak.sh stable|beta}

if [[ $branch == beta ]]; then remote=flathub-beta; else remote=flathub; fi
app=org.opencpn.OpenCPN

if [[ -n ${CI:-} ]]; then
  sudo apt-get update -qq
  sudo apt-get install -y -qq flatpak flatpak-builder >/dev/null
  # Ubuntu 24.04 stops unprivileged user namespaces, which bubblewrap needs.
  sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0 || true
fi

flatpak remote-add --user --if-not-exists flathub \
  https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak remote-add --user --if-not-exists flathub-beta \
  https://flathub.org/beta-repo/flathub-beta.flatpakrepo

# The freedesktop version OpenCPN runs on: "Sdk: org.freedesktop.Sdk/x86_64/24.08"
runtime_of() {
  flatpak remote-info --user "$1" "$app//$2" |
    awk -F': *' '/^ *Sdk:/ {print $2}' | awk -F/ '{print $NF}'
}
sdk=$(runtime_of "$remote" "$branch")
if [[ -z $sdk ]]; then
  echo "Cannot find $app//$branch on $remote" >&2
  exit 1
fi
if [[ $branch == beta && $sdk == "$(runtime_of flathub stable)" ]]; then
  echo "OpenCPN beta uses the stable runtime ($sdk); nothing more to build."
  exit 0
fi
arch=$(flatpak --default-arch)
echo "Building for OpenCPN $branch on freedesktop $sdk ($arch)"

flatpak install --user -y --noninteractive "$remote" "$app//$branch"
flatpak install --user -y --noninteractive flathub "org.freedesktop.Sdk//$sdk"

work=$PWD/build-flatpak
rm -rf "$work" dist
mkdir -p "$work" dist

tuple="flatpak-$arch;$sdk;$arch"
cat > "$work/org.opencpn.OpenCPN.Plugin.observer.json" <<JSON
{
  "id": "org.opencpn.OpenCPN.Plugin.observer",
  "runtime": "$app",
  "runtime-version": "$branch",
  "sdk": "org.freedesktop.Sdk//$sdk",
  "build-extension": true,
  "separate-locales": false,
  "appstream-compose": false,
  "modules": [
    {
      "name": "observer",
      "buildsystem": "simple",
      "build-commands": [
        "cmake -S . -B build-fp -DCMAKE_BUILD_TYPE=Release -DOBSERVER_BUILD_TESTS=OFF '-DOCPN_TARGET_TUPLE=$tuple' -DOBSERVER_VERSION=$VERSION '-DOBSERVER_TARBALL_BASE_URL=${BASE_URL:-}'",
        "cmake --build build-fp --parallel \$FLATPAK_BUILDER_N_JOBS",
        "cmake --build build-fp --target tarball"
      ],
      "sources": [
        {
          "type": "dir",
          "path": "$PWD",
          "skip": [".git", ".flatpak-builder", "build", "build-ci", "build-flatpak", "dist"]
        }
      ]
    }
  ]
}
JSON

flatpak-builder --user --force-clean --keep-build-dirs --disable-rofiles-fuse \
  --state-dir="$work/state" "$work/app" \
  "$work/org.opencpn.OpenCPN.Plugin.observer.json"

out=$(ls -d "$work"/state/build/observer*/build-fp | head -1)
cp "$out"/*.tar.gz "$out"/*.xml dist/
ls -l dist
