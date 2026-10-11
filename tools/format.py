"""Formats Rhiza's own C++ with the clang-format version pinned beside this file.

    python -m pip install -r tools/requirements.txt
    python tools/format.py           rewrite files in place
    python tools/format.py --check   fail if anything is unformatted (CI)
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATTERNS = ["engine/*.h", "engine/*.cpp", "framework/*.h", "game/*.h", "game/*.cpp", "tests/*.h", "tests/*.cpp"]
EXCLUDED = ("engine/media/",)
INSTALL = "python -m pip install -r tools/requirements.txt"


def find_clang_format():
    # The pip package knows where its own binary is, wherever pip put it - a
    # user install's Scripts folder is often not on PATH.
    try:
        import clang_format
        return clang_format.get_executable("clang-format")
    except ImportError:
        return shutil.which("clang-format")


def pinned_version():
    with open(os.path.join(HERE, "requirements.txt")) as requirements:
        for line in requirements:
            if line.startswith("clang-format=="):
                return line.split("==", 1)[1].strip()
    return None


def main():
    clang_format = find_clang_format()
    if not clang_format:
        sys.exit("clang-format not found: " + INSTALL)

    # Versions format differently, so any other one would fight CI.
    version = subprocess.run([clang_format, "--version"], capture_output=True, text=True).stdout.strip()
    pinned = pinned_version()
    if pinned and f"version {pinned}" not in version:
        sys.exit(f"found '{version}', but {pinned} is pinned: " + INSTALL)

    root = os.path.dirname(HERE)
    listed = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", *PATTERNS],
                            cwd=root, capture_output=True, text=True, check=True).stdout.split()
    files = sorted({f for f in listed if not f.startswith(EXCLUDED)})

    mode = ["--dry-run", "--Werror"] if "--check" in sys.argv else ["-i"]
    return subprocess.run([clang_format, *mode, *files], cwd=root).returncode


if __name__ == "__main__":
    sys.exit(main())
