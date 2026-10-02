#!/usr/bin/env python3
"""Combine per-platform plugin metadata into one OpenCPN catalog.

Usage: ci/make-catalog.py DIST_DIR VERSION

Writes DIST_DIR/ocpn-plugins.xml from every other *.xml file in DIST_DIR.
OpenCPN reads it as a custom catalog (Options > Plugins > catalog URL), so
a release's latest/download/ocpn-plugins.xml link keeps users up to date.
"""

import datetime
import pathlib
import re
import sys
import xml.etree.ElementTree as ET


def main() -> int:
    dist = pathlib.Path(sys.argv[1])
    version = sys.argv[2]
    out = dist / "ocpn-plugins.xml"
    entries = []
    for path in sorted(dist.glob("*.xml")):
        if path == out:
            continue
        text = path.read_text(encoding="utf-8")
        root = ET.fromstring(text)  # fail loudly on broken metadata
        if root.tag != "plugin" or not root.findtext("tarball-checksum", "").strip():
            raise SystemExit(f"{path.name}: not plugin metadata with a checksum")
        entries.append(re.sub(r"^<\?xml[^>]*\?>\s*", "", text).strip())
    if not entries:
        raise SystemExit(f"no plugin metadata in {dist}")

    date = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M")
    body = "\n".join(entries)
    out.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<plugins>\n"
        f"  <version>{version}</version>\n"
        f"  <date>{date}</date>\n"
        f"{body}\n"
        "</plugins>\n",
        encoding="utf-8",
    )
    ET.parse(out)
    print(f"{out}: {len(entries)} plugin builds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
