"""High-level component and element factory functions.

These thin wrappers provide a more Pythonic API over the raw ``_ftxui``
bindings — e.g. ``text("hello", bold=True)`` instead of chaining
``_bold(_text_raw("hello"))``.

Typical usage::

    import pyftxui.components as ui

    el = ui.vbox([
        ui.text("Hello!", bold=True),
        ui.hbox([
            ui.button("OK",     on_click=lambda: None),
            ui.button("Cancel", on_click=lambda: None),
        ]),
    ])
"""

from ._ftxui import (
    _text_raw,
    _bold,
    _dim,
    _italic,
    _inverted,
    _color_element,
    _bgcolor_element,
    hbox as _hbox,
    vbox as _vbox,
    dbox as _dbox,
    border,
    border_rounded,
    border_light,
    border_heavy,
    separator,
    separator_light,
    separator_heavy,
    empty_element,
    flex,
    flex_grow,
    xflex,
    yflex,
    notflex,
    spinner as _spinner,
    gauge,
    gauge_left,
    gauge_right,
    paragraph,
    window,
    make_button,
    make_button_animated,
    make_renderer,
    make_input,
    make_checkbox,
    container_vertical,
    container_horizontal,
    markdown,
    markdown_with_width,
    markdown_component,
)


# ── DOM element factories ──────────────────────────────────────────────────


def text(content, *, bold=False, dim=False, italic=False, inverted=False,
         color=None, bgcolor=None):
    """Render *content* as a terminal text element.

    :param content:  The string to display (coerced with ``str()``).
    :param bold:     Apply bold decoration.
    :param dim:      Apply dim decoration.
    :param italic:   Apply italic decoration.
    :param inverted: Apply inverted-color decoration.
    :param color:    Foreground ``Color`` object.
    :param bgcolor:  Background ``Color`` object.
    """
    el = _text_raw(str(content))
    if bold:
        el = _bold(el)
    if dim:
        el = _dim(el)
    if italic:
        el = _italic(el)
    if inverted:
        el = _inverted(el)
    if color is not None:
        el = _color_element(color, el)
    if bgcolor is not None:
        el = _bgcolor_element(bgcolor, el)
    return el


def hbox(elements):
    """Lay out *elements* horizontally."""
    return _hbox(list(elements))


def vbox(elements):
    """Lay out *elements* vertically."""
    return _vbox(list(elements))


def dbox(elements):
    """Stack *elements* depth-wise (overlay)."""
    return _dbox(list(elements))


def spinner(charset_index: int = 0, frame_index: int = 0):
    """Render a single spinner frame."""
    return _spinner(charset_index, frame_index)


# ── Component factories ────────────────────────────────────────────────────


def button(label: str, on_click=None, *, animated: bool = False):
    """Create a clickable button component.

    :param label:    Button label string.
    :param on_click: Zero-argument callable invoked on click.
    :param animated: Use the animated button style.
    """
    cb = on_click if on_click is not None else lambda: None
    if animated:
        return make_button_animated(label, cb)
    return make_button(label, cb)


def renderer(fn):
    """Create a ``Renderer`` component that calls *fn* on every frame.

    :param fn: Zero-argument callable returning an ``Element``.
    """
    return make_renderer(fn)


class Input:
    """Wraps an FTXUI Input component with Pythonic get/set access.

    Example::

        inp = ui.Input(placeholder="Type here…")
        ui.run_fullscreen(inp.component)
        print(inp.value)
    """

    def __init__(self, placeholder: str = "", value=None):
        """
        :param placeholder: Ghost text shown when empty.
        :param value:       Optional ``ReactiveStr`` to two-way bind.
        """
        result = make_input(placeholder, value)
        self.component, self._get = result

    @property
    def value(self) -> str:
        """Current text content."""
        return self._get()


def input(placeholder: str = "", value=None) -> "Input":
    """Convenience factory — returns an :class:`Input` instance."""
    return Input(placeholder, value)


class Checkbox:
    """Wraps an FTXUI Checkbox component.

    Example::

        cb = ui.Checkbox("Enable feature")
        print(cb.checked)  # False
    """

    def __init__(self, label: str, checked: bool = False, on_change=None):
        comp, get_fn, set_fn = make_checkbox(label, checked, on_change)
        self.component = comp
        self._get = get_fn
        self._set = set_fn

    @property
    def checked(self) -> bool:
        return self._get()

    @checked.setter
    def checked(self, value: bool) -> None:
        self._set(value)


def checkbox(label: str, checked: bool = False, on_change=None) -> "Checkbox":
    """Convenience factory — returns a :class:`Checkbox` instance."""
    return Checkbox(label, checked, on_change)


def container_v(components):
    """Vertical focusable container."""
    return container_vertical(list(components))


def container_h(components):
    """Horizontal focusable container."""
    return container_horizontal(list(components))


__all__ = [
    # DOM elements
    "text",
    "hbox",
    "vbox",
    "dbox",
    "border",
    "border_rounded",
    "border_light",
    "border_heavy",
    "window",
    "separator",
    "separator_light",
    "separator_heavy",
    "empty_element",
    "flex",
    "flex_grow",
    "xflex",
    "yflex",
    "notflex",
    "spinner",
    "gauge",
    "gauge_left",
    "gauge_right",
    "paragraph",
    "markdown",
    "markdown_with_width",
    # Components
    "button",
    "renderer",
    "Input",
    "input",
    "Checkbox",
    "checkbox",
    "container_v",
    "container_h",
]
