"""Reactive primitives for pyftxui.

``Reactive`` is a thread-safe observable value that triggers UI redraws
on mutation. ``Computed`` derives a read-only value from one or more
``Reactive`` sources.

Example::

    import pyftxui.reactive as rx

    counter = rx.Reactive(0)
    label   = rx.Computed(counter, lambda v: f"Count: {v}")

    counter.set(5)
    print(label.get())  # "Count: 5"
"""

from ._ftxui import ReactiveInt, ReactiveFloat, ReactiveStr  # noqa: F401


class Reactive:
    """Factory that returns the appropriate typed reactive based on *initial*.

    Supported Python types and their C++ backing:

    * ``int``   → ``ReactiveInt``
    * ``float`` → ``ReactiveFloat``
    * ``str``   → ``ReactiveStr``
    """

    def __new__(cls, initial=0):
        if isinstance(initial, bool):
            # bool is a subclass of int — treat as int (0/1)
            return ReactiveInt(int(initial))
        if isinstance(initial, int):
            return ReactiveInt(initial)
        if isinstance(initial, float):
            return ReactiveFloat(initial)
        if isinstance(initial, str):
            return ReactiveStr(initial)
        raise TypeError(
            f"Reactive does not support type {type(initial).__name__!r}; "
            "use int, float, or str."
        )


class Computed:
    """A read-only value derived from one ``Reactive`` source.

    :param source: A ``Reactive`` (or any object with ``get()`` and
                   ``on_change(fn)`` methods).
    :param fn:     Transformation applied to the source value.

    Example::

        counter = Reactive(0)
        label   = Computed(counter, lambda v: f"Count: {v}")
    """

    __slots__ = ("_fn", "_source", "_value", "_listener_id")

    def __init__(self, source, fn):
        self._fn = fn
        self._source = source
        self._value = fn(source.get())
        self._listener_id = source.on_change(self._update)

    def _update(self, v) -> None:
        self._value = self._fn(v)

    def get(self):
        """Return the current computed value."""
        return self._value

    def __del__(self):
        try:
            self._source.remove_listener(self._listener_id)
        except Exception:
            pass


__all__ = ["Reactive", "Computed", "ReactiveInt", "ReactiveFloat", "ReactiveStr"]
