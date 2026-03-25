"""Theme presets for pyftxui.

Usage::

    from pyftxui import App, theme
    app = App()
    app.theme(theme.NORD)
"""

from ._ftxui import Theme  # noqa: F401

# Pre-built presets — constructed once at import time.
DEFAULT = Theme.default_theme()
DARK = Theme.dark()
LIGHT = Theme.light()
NORD = Theme.nord()
DRACULA = Theme.dracula()
MONOKAI = Theme.monokai()

__all__ = ["Theme", "DEFAULT", "DARK", "LIGHT", "NORD", "DRACULA", "MONOKAI"]
