#!/usr/bin/env python3
"""Archive one committed source tree, with a versioned root and SHA-256 sidecar."""

import argparse
import hashlib
import re
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="HEAD", help="Git commit/tag to archive (default: HEAD)")
    parser.add_argument("--output-dir", type=Path, help="Output directory (default: repository/dist)")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent

    def git(*arguments):
        return subprocess.check_output(["git", "-C", str(root), *arguments], text=True).strip()

    try:
        commit = git("rev-parse", "--verify", "--end-of-options", args.ref + "^{commit}")
        cmake = git("show", commit + ":CMakeLists.txt")
        match = re.search(r"project\(Sofafin\s+VERSION\s+(\d+\.\d+\.\d+)\s", cmake)
        if not match:
            parser.error("the selected commit has no recognized Sofafin project version")
        version = match.group(1)
        if re.fullmatch(r"v\d+\.\d+\.\d+", args.ref) and args.ref != "v" + version:
            parser.error("the release tag does not match the selected commit's CMake version")
        license_text = git("show", commit + ":LICENSE")
        if "GNU GENERAL PUBLIC LICENSE" not in license_text:
            parser.error("the selected commit must include the project GPL license")

        output = (args.output_dir or root / "dist").resolve()
        output.mkdir(parents=True, exist_ok=True)
        name = "sofafin-" + version
        archive = output / (name + ".tar.gz")
        checksum = output / (archive.name + ".sha256")
        if archive.exists() or checksum.exists():
            parser.error("output already exists; choose a new --output-dir")
        with tempfile.TemporaryDirectory(prefix=".sofafin-source-", dir=output) as temporary:
            staged = Path(temporary) / archive.name
            subprocess.run([
                "git", "-C", str(root), "archive", "--format=tar.gz",
                "--prefix=" + name + "/", "--output=" + str(staged), commit,
            ], check=True)
            digest = hashlib.sha256(staged.read_bytes()).hexdigest()
            staged_checksum = Path(temporary) / checksum.name
            staged_checksum.write_text(digest + "  " + archive.name + "\n", encoding="utf-8")
            staged.rename(archive)
            staged_checksum.rename(checksum)
    except (subprocess.CalledProcessError, OSError) as error:
        parser.exit(1, "Source packaging failed: " + str(error) + "\n")

    print("Source commit: " + commit)
    print("Only the selected committed tree is archived; local changes are excluded.")
    print(archive)
    print(checksum)


if __name__ == "__main__":
    main()
