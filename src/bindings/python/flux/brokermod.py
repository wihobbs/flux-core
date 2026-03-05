###############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

from _flux._core import ffi, lib
from flux.constants import FLUX_MSGTYPE_EVENT, FLUX_MSGTYPE_REQUEST


def request_handler(topic, prefix=True):
    """Decorator to register a method as a request handler.

    The handler is invoked with (self, msg) when a request matching
    ``<module_name>.<topic>`` is received.  If ``prefix=False``, the topic
    is used as-is without prepending the module name.
    """

    def decorator(func):
        func._flux_handler = {
            "msgtype": FLUX_MSGTYPE_REQUEST,
            "topic": topic,
            "prefix": prefix,
        }
        return func

    return decorator


def event_handler(topic, prefix=True):
    """Decorator to register a method as an event handler.

    The handler is invoked with (self, msg) when an event matching
    ``<module_name>.<topic>`` is received.  If ``prefix=False``, the topic
    is used as-is without prepending the module name.
    """

    def decorator(func):
        func._flux_handler = {
            "msgtype": FLUX_MSGTYPE_EVENT,
            "topic": topic,
            "prefix": prefix,
        }
        return func

    return decorator


class BrokerModule:
    """Base class for Python broker modules.

    Subclass this and use the :func:`request_handler` and
    :func:`event_handler` decorators to register message handlers, then
    call :meth:`run` to start the reactor.  Handler topics are automatically
    prefixed with the module name unless ``prefix=False`` is passed to the
    decorator.

    Example::

        from flux.brokermod import BrokerModule, event_handler, request_handler

        class MyModule(BrokerModule):

            @request_handler("info")
            def info(self, msg):
                self.handle.respond(msg, self.name)

            @event_handler("panic")
            def panic(self, msg):
                self.stop_error()

        def mod_main(h, *args):
            MyModule(h, *args).run()
    """

    def __init__(self, h, *args):
        self._h = h
        self._args = args
        name_p = ffi.cast("char *", lib.flux_aux_get(h.handle, b"flux::name"))
        self._name = ffi.string(name_p).decode() if name_p != ffi.NULL else "unknown"
        self._watchers = []
        self._stopped_with_error = False
        self._register_handlers()

    @property
    def handle(self):
        """The underlying :class:`flux.Flux` handle."""
        return self._h

    @property
    def name(self):
        """The module name as assigned by the broker."""
        return self._name

    @property
    def args(self):
        """Tuple of module arguments passed at load time."""
        return self._args

    def stop(self):
        """Stop the reactor and exit :meth:`run` normally."""
        self._h.reactor_stop()

    def stop_error(self):
        """Stop the reactor and cause :meth:`run` to raise :exc:`OSError`."""
        self._stopped_with_error = True
        self._h.reactor_stop_error()

    def run(self):
        """Run the reactor until stopped.

        Raises :exc:`OSError` if the reactor was stopped with an error
        (e.g. via :meth:`stop_error`) or if a Python exception was set
        on the handle during a callback.
        """
        if self._h.reactor_run() < 0 or self._stopped_with_error:
            raise OSError("reactor exited with error")

    def _register_handlers(self):
        for cls in type(self).__mro__:
            for attr_name, func in cls.__dict__.items():
                spec = getattr(func, "_flux_handler", None)
                if spec is None:
                    continue
                method = getattr(self, attr_name)
                msgtype = spec["msgtype"]
                topic = (
                    f"{self._name}.{spec['topic']}" if spec["prefix"] else spec["topic"]
                )
                if msgtype == FLUX_MSGTYPE_EVENT:
                    self._h.event_subscribe(topic)

                def _make_cb(m):
                    def cb(handle, mtype, msg, arg):
                        m(msg)

                    return cb

                w = self._h.msg_watcher_create(_make_cb(method), msgtype, topic)
                w.start()
                self._watchers.append(w)


# vi: ts=4 sw=4 expandtab
