#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
from pathlib import Path


def patch_install_script(path: Path) -> bool:
    if not path.exists():
        raise FileNotFoundError(f"Install script not found: {path}")

    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    output: list[str] = []
    skip_until = -1

    for i, line in enumerate(lines):
        if i < skip_until:
            continue

        if re.search(r"\bif\s*\(", line):
            block = ""
            endif_idx = -1
            depth = 0

            for j in range(i, len(lines)):
                block += lines[j]
                depth += len(re.findall(r"(?<!e)\bif\s*\(", lines[j])) - lines[j].count("endif()")
                if "endif()" in lines[j] and depth <= 0:
                    endif_idx = j
                    break

            if endif_idx >= 0 and ("qtrest.dll" in block or "qtrest.lib" in block):
                skip_until = endif_idx + 1
                continue

        output.append(
            line.replace(
                '"/usr/local/include/qtrest"',
                '"${CMAKE_INSTALL_PREFIX}/include/qtrest"',
            )
        )

    new_content = "".join(output)
    old_content = "".join(lines)
    changed = new_content != old_content
    if changed:
        path.write_text(new_content, encoding="utf-8")
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Patch qtrest-generated cmake_install.cmake files for cross-platform staging installs."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="Paths to generated qtrest cmake_install.cmake files.",
    )
    args = parser.parse_args()

    for raw in args.paths:
        file_path = Path(raw)
        changed = patch_install_script(file_path)
        status = "patched" if changed else "no changes"
        print(f"{status}: {file_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())