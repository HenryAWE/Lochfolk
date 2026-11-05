#!/usr/bin/env python3
"""Create ZIP archives with unsafe entry names for mount_archive() tests.

Every archive contains an entry whose name must be rejected by the virtual
file system: an absolute name, a ".." traversal, or a "." component. Mounting
any of them must throw instead of mounting the entry outside (or with a
literal dot component inside) the mount point.

`evil_mixed.zip` places a safe entry before an unsafe one: validation must
happen before anything is mounted, so a rejected archive leaves no partially
populated mount point behind.
"""

import os
import sys
import zipfile


def make_archive(output: str, entries: list[tuple[str, bytes]]) -> None:
    os.makedirs(os.path.dirname(output), exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as zf:
        for name, data in entries:
            zf.writestr(name, data)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <output-dir>", file=sys.stderr)
        return 2

    output_dir = os.path.abspath(sys.argv[1])

    try:
        make_archive(
            os.path.join(output_dir, "evil_absolute.zip"),
            [("/evil_absolute.txt", b"pwned")],
        )
        make_archive(
            os.path.join(output_dir, "evil_parent.zip"),
            [("../evil_parent.txt", b"pwned")],
        )
        make_archive(
            os.path.join(output_dir, "evil_dot.zip"),
            [("./evil_dot.txt", b"pwned")],
        )
        make_archive(
            os.path.join(output_dir, "evil_mixed.zip"),
            [("good.txt", b"good"), ("../evil.txt", b"pwned")],
        )
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
