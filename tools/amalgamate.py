#!/usr/bin/env python3
"""
amalgamate.py — Generate dist/ftxui.hpp, a STB-style single-header for FTXUI BadWolf.

Usage:
  python3 tools/amalgamate.py           # regenerate dist/ftxui.hpp
  python3 tools/amalgamate.py --verify  # check all sources are included

Output structure:
  [Banner + license]
  [Usage instructions]
  #ifndef FTXUI_BADWOLF_HPP
  #define FTXUI_BADWOLF_HPP

  // DECLARATIONS SECTION
  // All headers from include/ftxui/ (screen, dom, component, ui)
  // All template implementations (already in headers)

  #ifdef BADWOLF_IMPLEMENTATION
  // IMPLEMENTATION SECTION
  // All .cpp files from src/ftxui/ (screen, dom, component, ui)
  // Each wrapped with a comment // ── source: src/ftxui/xxx.cpp ──
  #endif // BADWOLF_IMPLEMENTATION

  #endif // FTXUI_BADWOLF_HPP

Mode A — Declarations only (default, include anywhere):
    #include "ftxui.hpp"   // Just class/function declarations + templates

Mode B — Full implementation (in exactly ONE .cpp file):
    #define BADWOLF_IMPLEMENTATION
    #include "ftxui.hpp"   // Declarations PLUS all .cpp implementations
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path
from typing import List, Set

REPO_ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIR = REPO_ROOT / "include"
SRC_DIR = REPO_ROOT / "src"

# Headers in dependency order: screen → dom → component → ui (and util)
HEADER_ORDER = [
    "ftxui/util/autoreset.hpp",
    "ftxui/util/ref.hpp",
    "ftxui/util/warn_windows_macro.hpp",
    "ftxui/screen/box.hpp",
    "ftxui/screen/cell.hpp",
    "ftxui/screen/pixel.hpp",
    "ftxui/screen/color_info.hpp",
    "ftxui/screen/color.hpp",
    "ftxui/screen/image.hpp",
    "ftxui/screen/screen.hpp",
    "ftxui/screen/string.hpp",
    "ftxui/screen/surface.hpp",
    "ftxui/screen/terminal.hpp",
    "ftxui/screen/deprecated.hpp",
    "ftxui/dom/direction.hpp",
    "ftxui/dom/requirement.hpp",
    "ftxui/dom/selection.hpp",
    "ftxui/dom/node.hpp",
    "ftxui/dom/flexbox_config.hpp",
    "ftxui/dom/linear_gradient.hpp",
    "ftxui/dom/take_any_args.hpp",
    "ftxui/dom/canvas.hpp",
    "ftxui/dom/table.hpp",
    "ftxui/dom/elements.hpp",
    "ftxui/dom/deprecated.hpp",
    "ftxui/component/animation.hpp",
    "ftxui/component/captured_mouse.hpp",
    "ftxui/component/event.hpp",
    "ftxui/component/mouse.hpp",
    "ftxui/component/task.hpp",
    "ftxui/component/receiver.hpp",
    "ftxui/component/component_base.hpp",
    "ftxui/component/component_options.hpp",
    "ftxui/component/component.hpp",
    "ftxui/component/loop.hpp",
    "ftxui/component/screen_interactive.hpp",
    "ftxui/component/app.hpp",
]

# Collect remaining ui headers automatically (alphabetical, after the above)
def collect_ui_headers() -> List[str]:
    ui_dir = INCLUDE_DIR / "ftxui" / "ui"
    headers = sorted(f"ftxui/ui/{p.name}" for p in ui_dir.glob("*.hpp"))
    return headers

# Source files in link-order (screen → dom → component → ui)
def collect_sources() -> List[Path]:
    src_subdirs = ["screen", "dom", "component", "ui"]
    sources: List[Path] = []
    for subdir in src_subdirs:
        d = SRC_DIR / "ftxui" / subdir
        if not d.exists():
            continue
        # Sort for determinism; skip tests and fuzzers
        for p in sorted(d.glob("*.cpp")):
            name = p.name
            if "_test" in name or "_fuzzer" in name:
                continue
            sources.append(p)
    return sources


def strip_pragma_once(content: str) -> str:
    """Remove #pragma once directives."""
    return re.sub(r"^\s*#pragma\s+once\s*\n", "", content, flags=re.MULTILINE)


def strip_include_guards(content: str) -> str:
    """
    Remove traditional include guards of the form:
        #ifndef FTXUI_...
        #define FTXUI_...
        ...
        #endif  // FTXUI_...
    We only strip the outer guard, not nested #ifdef usage.
    """
    # Match opening guard: #ifndef FOO / #define FOO at the top of the file
    guard_open = re.compile(
        r"^\s*#ifndef\s+(FTXUI_\w+)\s*\n\s*#define\s+\1\s*\n", re.MULTILINE
    )
    m = guard_open.search(content)
    if not m:
        return content
    guard_name = m.group(1)
    content = content[: m.start()] + content[m.end() :]
    # Remove the matching closing #endif (last one with a comment referencing the guard)
    close_pattern = re.compile(
        r"\n\s*#endif\s*(?://[^\n]*)?\s*$", re.MULTILINE
    )
    # Remove only the very last #endif
    matches = list(close_pattern.finditer(content))
    if matches:
        last = matches[-1]
        content = content[: last.start()] + "\n"
    return content


def rewrite_local_includes(content: str, included_set: set[str]) -> str:
    """
    Rewrite #include "ftxui/..." lines:
    - If already included, comment out the line.
    - Otherwise keep it (will be resolved by the amalgamation or remain for
      system headers).
    We never remove system includes (angle-bracket).
    """
    def replacer(m: re.Match) -> str:
        path = m.group(1)
        if path in included_set:
            return f"// [amalgamated] #include \"{path}\""
        return m.group(0)

    return re.sub(r'#include\s+"(ftxui/[^"]+)"', replacer, content)


BANNER = """\
// =============================================================================
// FTXUI BadWolf — Single-Header Distribution
// Generated by tools/amalgamate.py
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Mode A — Declarations only (include in as many files as you like):    │
// │      #include "ftxui.hpp"                                               │
// │                                                                         │
// │  Mode B — Full implementation (in EXACTLY ONE .cpp file):              │
// │      #define BADWOLF_IMPLEMENTATION                                     │
// │      #include "ftxui.hpp"                                               │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Compile (zero cmake, zero linking):
//   g++ -std=c++20 -pthread -DBADWOLF_IMPLEMENTATION myapp.cpp -o myapp
//
// FTXUI BadWolf — Functional Terminal (X) User Interface
// https://github.com/louiscrocker/FTXUI_BADWOLF
// License: MIT
// =============================================================================
"""

IMPL_BANNER = """\

// =============================================================================
// ██╗███╗   ███╗██████╗ ██╗     ███████╗███╗   ███╗███████╗███╗   ██╗████████╗
// ██║████╗ ████║██╔══██╗██║     ██╔════╝████╗ ████║██╔════╝████╗  ██║╚══██╔══╝
// ██║██╔████╔██║██████╔╝██║     █████╗  ██╔████╔██║█████╗  ██╔██╗ ██║   ██║
// ██║██║╚██╔╝██║██╔═══╝ ██║     ██╔══╝  ██║╚██╔╝██║██╔══╝  ██║╚██╗██║   ██║
// ██║██║ ╚═╝ ██║██║     ███████╗███████╗██║ ╚═╝ ██║███████╗██║ ╚████║   ██║
// ╚═╝╚═╝     ╚═╝╚═╝     ╚══════╝╚══════╝╚═╝     ╚═╝╚══════╝╚═╝  ╚═══╝   ╚═╝
//
// Implementation Section — only compiled when BADWOLF_IMPLEMENTATION is defined.
// Place `#define BADWOLF_IMPLEMENTATION` before this include in exactly one
// translation unit of your project.
// =============================================================================
#ifdef BADWOLF_IMPLEMENTATION
"""

IMPL_FOOTER = """\
#endif  // BADWOLF_IMPLEMENTATION
"""


def amalgamate(output_path: Path) -> None:
    included: Set[str] = set()
    sections: List[str] = []

    all_headers: List[str] = HEADER_ORDER + collect_ui_headers()
    # De-duplicate while preserving order
    seen_headers: Set[str] = set()
    deduped: List[str] = []
    for h in all_headers:
        if h not in seen_headers:
            seen_headers.add(h)
            deduped.append(h)
    all_headers = deduped

    # ── Public headers ────────────────────────────────────────────────────────
    for rel_path in all_headers:
        abs_path = INCLUDE_DIR / rel_path
        if not abs_path.exists():
            print(f"  [warn] header not found: {rel_path}", file=sys.stderr)
            continue

        content = abs_path.read_text(encoding="utf-8")
        content = strip_pragma_once(content)
        content = strip_include_guards(content)
        content = rewrite_local_includes(content, included)

        sections.append(f"\n// ---- {rel_path} ----\n")
        sections.append(content)
        included.add(rel_path)

    # Also include the top-level ui.hpp if it exists
    top_ui = INCLUDE_DIR / "ftxui" / "ui.hpp"
    if top_ui.exists():
        content = top_ui.read_text(encoding="utf-8")
        content = strip_pragma_once(content)
        content = rewrite_local_includes(content, included)
        sections.append("\n// ---- ftxui/ui.hpp ----\n")
        sections.append(content)

    # ── Implementation section ─────────────────────────────────────────────
    sections.append(IMPL_BANNER)

    sources = collect_sources()
    for src in sources:
        rel = src.relative_to(REPO_ROOT)
        content = src.read_text(encoding="utf-8")
        # Replace #include "ftxui/..." with already-seen ones commented out
        content = rewrite_local_includes(content, included)
        sections.append(f"\n// ── source: {rel} ──\n")
        sections.append(content)

    sections.append(IMPL_FOOTER)

    # ── Write output ──────────────────────────────────────────────────────────
    SIZE_WARN_KB = 120

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as f:
        f.write(BANNER)
        f.write("#ifndef FTXUI_BADWOLF_HPP\n")
        f.write("#define FTXUI_BADWOLF_HPP\n\n")
        for s in sections:
            f.write(s)
        f.write("\n#endif  // FTXUI_BADWOLF_HPP\n")

    size_kb = output_path.stat().st_size // 1024
    lines = sum(1 for _ in output_path.open(encoding="utf-8"))
    print(f"Wrote {output_path}  ({lines} lines, {size_kb} KB)")
    if size_kb > SIZE_WARN_KB:
        print(f"  [warn] output exceeds {SIZE_WARN_KB} KB (got {size_kb} KB)", file=sys.stderr)

    # Update companion smoke-test file (always overwrite to stay in sync)
    test_path = output_path.parent / "test_amalgamate.cpp"
    test_path.write_text(
        '#define BADWOLF_IMPLEMENTATION\n'
        '#include "ftxui.hpp"\n'
        'int main() { return 0; }\n'
    )
    print(f"Wrote {test_path}")


def verify(output_path: Path) -> None:
    """Check that every source file is referenced in the amalgamated header."""
    if not output_path.exists():
        print(f"[verify] Output not found: {output_path} — run without --verify first.")
        sys.exit(1)

    content = output_path.read_text(encoding="utf-8")
    sources = collect_sources()
    missing = []
    for src in sources:
        rel = str(src.relative_to(REPO_ROOT))
        if rel not in content:
            missing.append(rel)

    if missing:
        print(f"[verify] {len(missing)} source(s) missing from {output_path}:")
        for m in missing:
            print(f"  - {m}")
        sys.exit(1)
    else:
        print(f"[verify] All {len(sources)} sources present in {output_path}.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate FTXUI BadWolf single-header amalgamation.")
    parser.add_argument(
        "--output", default="dist/ftxui.hpp",
        help="Output path for the amalgamated header (default: dist/ftxui.hpp)"
    )
    parser.add_argument(
        "--repo-root", default=None,
        help="Repository root (default: parent of this script's directory)"
    )
    parser.add_argument(
        "--verify", action="store_true",
        help="Verify that all sources are included in the existing output file"
    )
    args = parser.parse_args()

    global REPO_ROOT, INCLUDE_DIR, SRC_DIR
    if args.repo_root:
        REPO_ROOT = Path(args.repo_root).resolve()
        INCLUDE_DIR = REPO_ROOT / "include"
        SRC_DIR = REPO_ROOT / "src"

    output = Path(args.output)
    if not output.is_absolute():
        output = REPO_ROOT / output

    if args.verify:
        verify(output)
    else:
        amalgamate(output)


if __name__ == "__main__":
    main()
