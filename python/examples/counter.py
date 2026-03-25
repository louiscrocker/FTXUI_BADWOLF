"""Counter — classic increment/decrement demo for pyftxui."""

import sys
import os

# Allow running from python/ directory without installing the package.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from pyftxui import App, theme
import pyftxui.reactive as rx
import pyftxui.components as ui

counter = rx.Reactive(0)
label = rx.Computed(counter, lambda v: f"Count: {v}")

app = App()
app.theme(theme.NORD)


def view():
    return ui.border(
        ui.vbox([
            ui.text(label.get(), bold=True),
            ui.separator(),
            ui.hbox([
                ui.button("+ Increment", on_click=lambda: counter.set(counter.get() + 1)),
                ui.button("- Decrement", on_click=lambda: counter.set(counter.get() - 1)),
                ui.button("  Quit  ", on_click=app.quit),
            ]),
        ])
    )


if __name__ == "__main__":
    app.run(view)
