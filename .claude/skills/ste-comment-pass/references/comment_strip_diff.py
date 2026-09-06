#!/usr/bin/env python3
"""Verify that two C/C++ files or revisions have identical code logic after stripping comments.

Usage:
  python3 comment_strip_diff.py <file1> <file2>
  python3 comment_strip_diff.py --git <ref> <file>

Exit status:
  0 if stripped code is identical.
  1 if any code differences exist.
"""

import difflib
import re
import subprocess
import sys


def strip_c_comments(text: str) -> str:
    """Strip C (/* ... */) and C++ (// ...) comments while preserving strings."""
    def replacer(match):
        s = match.group(0)
        if s.startswith('/'):
            # Replace multi-line comment with same number of newlines to keep line numbers aligned
            return '\n' * s.count('\n')
        return s

    pattern = re.compile(
        r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        re.DOTALL | re.MULTILINE
    )
    stripped = re.sub(pattern, replacer, text)
    # Normalize lines: strip trailing whitespace
    lines = [line.rstrip() for line in stripped.splitlines()]
    # Remove blank lines for pure code comparison
    non_empty = [line for line in lines if line.strip()]
    return '\n'.join(non_empty) + '\n'


def main():
    if len(sys.argv) < 3:
        print("Usage: comment_strip_diff.py <file1> <file2>", file=sys.stderr)
        print("       comment_strip_diff.py --git <git_ref> <file>", file=sys.stderr)
        sys.exit(2)

    if sys.argv[1] == "--git":
        ref = sys.argv[2]
        path = sys.argv[3]
        try:
            old_content = subprocess.check_output(
                ["git", "show", f"{ref}:{path}"], text=True
            )
        except subprocess.CalledProcessError:
            print(f"Error reading {path} at {ref}", file=sys.stderr)
            sys.exit(2)

        with open(path, "r", encoding="utf-8") as f:
            new_content = f.read()
        name1 = f"{ref}:{path}"
        name2 = path
    else:
        file1 = sys.argv[1]
        file2 = sys.argv[2]
        with open(file1, "r", encoding="utf-8") as f:
            old_content = f.read()
        with open(file2, "r", encoding="utf-8") as f:
            new_content = f.read()
        name1 = file1
        name2 = file2

    stripped_old = strip_c_comments(old_content)
    stripped_new = strip_c_comments(new_content)

    if stripped_old == stripped_new:
        print(f"PASS: {name2} is byte-identical in code logic to {name1} after comment strip.")
        sys.exit(0)

    print(f"FAIL: Code differences detected between {name1} and {name2}:", file=sys.stderr)
    diff = difflib.unified_diff(
        stripped_old.splitlines(keepends=True),
        stripped_new.splitlines(keepends=True),
        fromfile=name1,
        tofile=name2,
        n=3
    )
    sys.stderr.writelines(diff)
    sys.exit(1)


if __name__ == "__main__":
    main()
