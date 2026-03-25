"""Dashboard — two latency metrics updated from background threads.

Demonstrates thread-safe ``Reactive<float>`` updates while the UI renders
on the main thread.
"""

import sys
import os
import time
import threading
import random
import math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from pyftxui import App, theme
import pyftxui.reactive as rx
import pyftxui.components as ui

# ── Reactive state ────────────────────────────────────────────────────────
api_latency = rx.Reactive(0.0)
db_latency = rx.Reactive(0.0)
api_p99 = rx.Reactive(0.0)
db_p99 = rx.Reactive(0.0)
request_count = rx.Reactive(0)
running = True

# ── Background simulators ─────────────────────────────────────────────────

def _simulate_api():
    while running:
        val = max(0.0, 50.0 + 30.0 * math.sin(time.time() * 0.7)
                  + random.gauss(0, 5))
        p99 = val * 1.8 + random.gauss(0, 3)
        api_latency.set(val)
        api_p99.set(max(0.0, p99))
        request_count.set(request_count.get() + random.randint(1, 5))
        time.sleep(0.25)


def _simulate_db():
    while running:
        val = max(0.0, 20.0 + 15.0 * math.cos(time.time() * 1.1)
                  + random.gauss(0, 3))
        p99 = val * 2.2 + random.gauss(0, 4)
        db_latency.set(val)
        db_p99.set(max(0.0, p99))
        time.sleep(0.20)


# ── App ───────────────────────────────────────────────────────────────────
app = App()
app.theme(theme.DARK)


def _latency_bar(label_str: str, value: float, p99: float, limit: float = 200.0):
    progress = min(1.0, value / limit)
    p99_str = f"p99: {p99:6.1f}ms"
    return ui.vbox([
        ui.hbox([
            ui.text(f"{label_str:<20}", bold=True),
            ui.text(f"{value:7.1f} ms"),
            ui.text("  "),
            ui.text(p99_str),
        ]),
        ui.gauge(progress),
    ])


def view():
    reqs = request_count.get()
    return ui.border(
        ui.vbox([
            ui.text("  Mini Ops Dashboard", bold=True),
            ui.separator(),
            _latency_bar("API latency", api_latency.get(), api_p99.get()),
            ui.separator_light(),
            _latency_bar("DB  latency", db_latency.get(), db_p99.get()),
            ui.separator(),
            ui.hbox([
                ui.text(f"Requests served: {reqs}"),
                ui.flex(ui.empty_element()),
                ui.button("Quit", on_click=app.quit),
            ]),
        ])
    )


if __name__ == "__main__":
    t1 = threading.Thread(target=_simulate_api, daemon=True)
    t2 = threading.Thread(target=_simulate_db, daemon=True)
    t1.start()
    t2.start()

    try:
        app.run(view)
    finally:
        running = False
