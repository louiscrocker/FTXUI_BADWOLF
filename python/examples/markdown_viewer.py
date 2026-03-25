"""Markdown viewer — renders a Markdown file in a scrollable terminal viewer.

Usage::

    python markdown_viewer.py [FILE]   # defaults to ../../README.md
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from pyftxui import markdown_component, run_fullscreen


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(__file__), "..", "..", "README.md"
    )

    try:
        with open(path, encoding="utf-8") as fh:
            md = fh.read()
    except FileNotFoundError:
        print(f"File not found: {path}", file=sys.stderr)
        sys.exit(1)

    component = markdown_component(md)
    run_fullscreen(component)


if __name__ == "__main__":
    main()
