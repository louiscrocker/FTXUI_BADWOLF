"""Counter — classic increment/decrement demo for pyftxui."""

import sys
import os

# Allow running from python/ directory without installing the package.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from pyftxui import App, theme, run_fullscreen
import pyftxui.reactive as rx
import pyftxui.components as ui

counter = rx.Reactive(0)

app = App()
app.theme(theme.NORD)

# Build a Component tree (buttons are Components, not Elements).
btn_inc  = ui.button("+ Increment", on_click=lambda: counter.set(counter.get() + 1))
btn_dec  = ui.button("- Decrement", on_click=lambda: counter.set(counter.get() - 1))
btn_quit = ui.button("  Quit  ",    on_click=app.quit)
buttons  = ui.container_h([btn_inc, btn_dec, btn_quit])

# Renderer composes the layout: static text + rendered buttons row.
def render():
    return ui.border(
        ui.vbox([
            ui.text(f"Count: {counter.get()}", bold=True),
            ui.separator(),
            buttons.Render(),
        ])
    )

root = ui.renderer(render)

if __name__ == "__main__":
    run_fullscreen(root)

