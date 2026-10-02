#!/usr/bin/env bash
# Check ci/calver.sh against a scratch repository with known tags.

set -euo pipefail

calver=$(cd "$(dirname "$0")" && pwd)/calver.sh
repo=$(mktemp -d)
trap 'rm -rf "$repo"' EXIT
cd "$repo"
git init -q
git -c user.name=ci -c user.email=ci@example.invalid commit -q --allow-empty -m init

failures=0
expect() {
  local month=$1 want=$2 got
  got=$("$calver" "$month")
  if [[ $got == "$want" ]]; then
    echo "ok   $month -> $got"
  else
    echo "FAIL $month -> $got, want $want"
    failures=$((failures + 1))
  fi
}

expect 2026-09 2026.9.0          # first release of a month
git tag v2026.9.0
expect 2026-09 2026.9.1
git tag v2026.9.1
git tag v2026.9.10               # numeric, not lexical, order
expect 2026-09 2026.9.11
expect 2026-10 2026.10.0         # a new month starts again at 0
git tag v2026.10.0
git tag v2026.1.4                # January does not match October's tags
expect 2026-10 2026.10.1
expect 2026-01 2026.1.5
git tag v2026.10.2-rc1           # anything else is ignored
git tag not-a-version
expect 2026-10 2026.10.1
expect 2027-10 2027.10.0

if "$calver" 2026-9 >/dev/null 2>&1; then
  echo "FAIL malformed month accepted"
  failures=$((failures + 1))
else
  echo "ok   malformed month rejected"
fi

exit $((failures > 0))
