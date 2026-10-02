#!/usr/bin/env bash
# Print the next calendar version, YYYY.M.N, for a release made now.
#
# N counts the releases already tagged vYYYY.M.* this month: the first
# release of September 2026 is 2026.9.0, the next 2026.9.1, and October
# starts again at 2026.10.0. The month has no leading zero, as semantic
# versioning requires, so versions still sort numerically.
#
# Usage: ci/calver.sh [YYYY-MM]    (defaults to the current UTC month)

set -euo pipefail

month_spec=${1:-$(date -u +%Y-%m)}
if [[ ! $month_spec =~ ^[0-9]{4}-[0-9]{2}$ ]]; then
  echo "calver.sh: expected YYYY-MM, got '$month_spec'" >&2
  exit 2
fi
year=${month_spec%-*}
month=$((10#${month_spec#*-}))

last=-1
while read -r tag; do
  [[ -z $tag ]] && continue
  n=${tag#"v${year}.${month}."}
  [[ $n =~ ^[0-9]+$ ]] || continue
  n=$((10#$n))
  if ((n > last)); then last=$n; fi
done < <(git tag --list "v${year}.${month}.*")

echo "${year}.${month}.$((last + 1))"
