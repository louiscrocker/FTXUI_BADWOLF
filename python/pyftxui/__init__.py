"""
pyftxui — Python bindings for the FTXUI terminal UI library.

Quick start::

    from pyftxui import App, theme
    import pyftxui.reactive as rx
    import pyftxui.components as ui

    counter = rx.Reactive(0)
    app = App()
    app.theme(theme.NORD)

    def view():
        return ui.vbox([
            ui.text(f"Count: {counter.get()}", bold=True),
            ui.hbox([
                ui.button("+ Inc", on_click=lambda: counter.set(counter.get() + 1)),
                ui.button("Quit", on_click=app.quit),
            ]),
        ])

    app.run(view)
"""

from ._ftxui import (  # noqa: F401
    Element,
    Component,
    hbox,
    vbox,
    dbox,
    border,
    border_rounded,
    border_light,
    border_heavy,
    window,
    separator,
    separator_light,
    separator_heavy,
    empty_element,
    flex,
    flex_grow,
    xflex,
    yflex,
    notflex,
    spinner,
    gauge,
    gauge_left,
    gauge_right,
    paragraph,
    vtext,
    markdown,
    markdown_with_width,
    markdown_component,
    run_fullscreen,
    run,
    run_fit_component,
    app_exit,
    app_post_event,
    set_theme,
    get_theme,
    LogPanel,
    LogLevel,
    Color,
)

from .app import App  # noqa: F401
from . import theme  # noqa: F401
from . import reactive  # noqa: F401
from . import components  # noqa: F401

__version__ = "0.1.0"
__all__ = [
    "App",
    "theme",
    "reactive",
    "components",
    "Element",
    "Component",
    "hbox",
    "vbox",
    "dbox",
    "border",
    "border_rounded",
    "border_light",
    "border_heavy",
    "window",
    "separator",
    "empty_element",
    "flex",
    "spinner",
    "gauge",
    "paragraph",
    "markdown",
    "markdown_component",
    "run_fullscreen",
    "run",
    "app_exit",
    "set_theme",
    "get_theme",
    "LogPanel",
    "LogLevel",
    "Color",
]
