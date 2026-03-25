"""App wrapper — provides a Pythonic entry point to run a UI."""

from ._ftxui import make_renderer, run_fullscreen, app_exit, set_theme


class App:
    """High-level application runner.

    Example::

        app = App()
        app.theme(theme.NORD)

        def view():
            return ui.text("Hello, world!")

        app.run(view)
    """

    def __init__(self) -> None:
        pass

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------

    def theme(self, t) -> "App":
        """Apply a theme preset before calling run().

        :param t: A ``Theme`` instance (e.g. ``pyftxui.theme.NORD``).
        :returns: self, for chaining.
        """
        set_theme(t)
        return self

    # ------------------------------------------------------------------
    # Control
    # ------------------------------------------------------------------

    def quit(self) -> None:
        """Exit the running event loop.  Safe to call from any callback."""
        app_exit()

    # ------------------------------------------------------------------
    # Run
    # ------------------------------------------------------------------

    def run(self, view_fn) -> None:
        """Start the fullscreen TUI, calling *view_fn* on every render frame.

        :param view_fn: A zero-argument callable that returns an ``Element``.
        """
        component = make_renderer(view_fn)
        run_fullscreen(component)
