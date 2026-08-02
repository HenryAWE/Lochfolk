#!/usr/bin/env python3
"""Create the ZIP archive used by the VFS unit tests.

The archive contains the contents
of `test/archive/` recursively, with paths relative to that directory
(e.g. `info.txt` and `data/value.txt`).

Entries are stored without compression.
"""

import os
import sys
import zipfile


def make_archive(output: str, source_dir: str) -> None:
    output = os.path.abspath(output)
    source_dir = os.path.abspath(source_dir)

    os.makedirs(os.path.dirname(output), exist_ok=True)

    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as zf:
        for root, dirs, files in os.walk(source_dir):
            rel_root = os.path.relpath(root, source_dir)
            for name in sorted(dirs):
                arcname = (
                    name if rel_root == "." else os.path.join(rel_root, name)
                ).replace(os.sep, "/")
                zf.writestr(arcname + "/", b"")
            for name in sorted(files):
                arcname = (
                    name if rel_root == "." else os.path.join(rel_root, name)
                ).replace(os.sep, "/")
                zf.write(os.path.join(root, name), arcname)


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <output.zip> <source-dir>", file=sys.stderr)
        return 2

    try:
        make_archive(sys.argv[1], sys.argv[2])
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
