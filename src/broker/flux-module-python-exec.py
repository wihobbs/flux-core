##############################################################
# Copyright 2026 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

# flux-module-python-exec - load and run a Python broker module
#
# This program is used as a module loader by the Flux broker when loading
# Python broker modules (.py suffix).  It implements the broker module
# loader protocol described in RFC 5:
#
#   1. Open a handle using FLUX_MODULE_URI from the environment.
#   2. Initialize the module via flux_module_initialize().
#   3. Register standard module handlers via flux_module_register_handlers().
#   4. Dynamically import the Python module at PATH and call mod_main(h, *args).
#      The Python module is responsible for running the reactor.
#   5. Finalize the module via flux_module_finalize().
#
# The Python module must define:
#
#   def mod_main(h, *args): ...
#
# where h is a flux.Flux handle and args are the (possibly empty) module args
# split from the space-delimited string returned by flux_module_initialize().
#
# Usage: flux module-python-exec PATH

import errno as errno_mod
import importlib.util
import os
import sys
import traceback

from _flux._core import ffi, lib
from flux.core.handle import Flux


def die(msg):
    print(f"{os.path.basename(sys.argv[0])}: {msg}", file=sys.stderr)
    sys.exit(1)


def die_error(msg, error):
    text = ffi.string(error.text).decode("utf-8", errors="replace")
    die(f"{msg}: {text}")


def main():
    if len(sys.argv) != 2:
        die(f"Usage: {os.path.basename(sys.argv[0])} PATH")
    path = sys.argv[1]
    uri = os.environ.get("FLUX_MODULE_URI")
    if not uri:
        die("FLUX_MODULE_URI is not set")

    try:
        h = Flux(url=uri)
    except OSError as exc:
        die(f"flux_open: {exc}")

    error = ffi.new("flux_error_t[1]")
    args_p = ffi.new("char **")
    if lib.flux_module_initialize(h.handle, args_p, error) < 0:
        die_error("flux_module_initialize", error[0])

    modargs = None
    if args_p[0] != ffi.NULL:
        modargs = ffi.string(args_p[0]).decode("utf-8")
        lib.free(args_p[0])

    if lib.flux_module_register_handlers(h.handle, error) < 0:
        die_error("flux_module_register_handlers", error[0])

    spec = importlib.util.spec_from_file_location("flux_module", path)
    if spec is None:
        die(f"could not load module from {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    args = modargs.split() if modargs else []
    errnum = 0
    try:
        mod.mod_main(h, *args)
    except Exception:  # pylint: disable=broad-except
        traceback.print_exc()
        errnum = errno_mod.ECONNRESET

    error = ffi.new("flux_error_t[1]")
    if lib.flux_module_finalize(h.handle, errnum, error) < 0:
        die_error("flux_module_finalize", error[0])


if __name__ == "__main__":
    main()

# vi: ts=4 sw=4 expandtab
