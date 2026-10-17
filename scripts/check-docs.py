#!/usr/bin/env python3
"""Check local inline Markdown links and heading anchors without network access."""

import argparse
import re
import sys
import unicodedata
from pathlib import Path
from urllib.parse import unquote, urlsplit


def prose(text):
    return re.sub(r"^(`{3,}|~{3,}).*?^\1[^\n]*$", "", text, flags=re.MULTILINE | re.DOTALL)


def headings(path):
    anchors, counts = set(), {}
    for heading in re.findall(r"^#{1,6}\s+(.+?)\s*#*\s*$", prose(path.read_text(encoding="utf-8")), re.MULTILINE):
        heading = heading.replace("`", "").lower()
        anchor = "".join(c for c in heading if c in "-_ " or unicodedata.category(c)[0] in "LN").replace(" ", "-")
        count = counts.get(anchor, 0)
        counts[anchor] = count + 1
        anchors.add(anchor + ("-" + str(count) if count else ""))
    return anchors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    root = parser.parse_args().root.resolve()
    files = sorted(root.glob("*.md")) + sorted((root / "docs").rglob("*.md")) + sorted((root / ".github").rglob("*.md"))
    if not files:
        parser.error("no Markdown documents found under " + str(root))
    errors, checked, cache = [], 0, {}
    for source in files:
        for link in re.findall(r"\[[^\]\n]*\]\(([^)\n]+)\)", prose(source.read_text(encoding="utf-8"))):
            target = urlsplit(link.strip().strip("<>"))
            if target.scheme or target.netloc:
                continue
            checked += 1
            destination = (source.parent / unquote(target.path)).resolve() if target.path else source
            if not destination.exists():
                errors.append(str(source.relative_to(root)) + ": missing target " + link)
            elif target.fragment and destination.suffix == ".md":
                if destination not in cache:
                    cache[destination] = headings(destination)
                if unquote(target.fragment) not in cache[destination]:
                    errors.append(str(source.relative_to(root)) + ": missing heading " + link)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Checked {checked} local links in {len(files)} Markdown files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
