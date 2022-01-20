###############################################################
# Copyright 2020 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
###############################################################

# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys
from manpages import man_pages
import docutils.nodes

# -- Project information -----------------------------------------------------

project = 'flux-core'
copyright = '''Copyright 2014 Lawrence Livermore National Security, LLC and Flux developers.

SPDX-License-Identifier: LGPL-3.0'''

# -- General configuration ---------------------------------------------------

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

master_doc = 'index'
source_suffix = '.rst'

extensions = [
    'sphinx.ext.intersphinx',
    'sphinx.ext.napoleon',
    'domainrefs'
]

domainrefs = {
    'linux:man1': {
        'text': '%s(1)',
        'url': 'http://man7.org/linux/man-pages/man1/%s.1.html',
    },
    'linux:man2': {
        'text': '%s(2)',
        'url': 'http://man7.org/linux/man-pages/man2/%s.2.html',
    },
    'linux:man3': {
        'text': '%s(3)',
        'url': 'http://man7.org/linux/man-pages/man3/%s.3.html',
    },
    'linux:man7': {
        'text': '%s(7)',
        'url': 'http://man7.org/linux/man-pages/man7/%s.7.html',
    },
}

# Disable "smartquotes" to avoid things such as turning long-options
#  "--" into en-dash in html output, which won't make much sense for
#  manpages.
smartquotes = False

# -- Setup for Sphinx API Docs -----------------------------------------------

# Workaround since sphinx does not automatically run apidoc before a build
# Copied from https://github.com/readthedocs/readthedocs.org/issues/1139

script_dir = os.path.normpath(os.path.dirname(__file__))
py_bindings_dir = os.path.normpath(os.path.join(script_dir, "../src/bindings/python/"))

# Make sure that the python bindings are in PYTHONPATH for autodoc
sys.path.insert(0, py_bindings_dir)

# run api doc
def run_apidoc(_):
    # Move import inside so that `gen-cmdhelp.py` can exec this file in LGTM.com
    # without sphinx installed
    # pylint: disable=import-outside-toplevel
    from sphinx.ext.apidoc import main

    try:
        # Check if running under `make`
        build_dir = os.path.normpath(os.environ.get('SPHINX_BUILDDIR'))
    except:
        build_dir = script_dir
    output_path = os.path.join(build_dir, 'python')
    exclusions = [os.path.join(py_bindings_dir, 'setup.py'),]
    main(['-e', '-f', '-M', '-o', output_path, py_bindings_dir] + exclusions)

def man_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    section = int(name[-1])
    page = None
    for man in man_pages:
        if man[1] == text and man[4] == section:
            page = man[0]
            break
    if page == None:
        page = "man7/flux-undocumented"
        section = 7

    node = docutils.nodes.reference(
        rawsource=rawtext,
        text=f"{text}({section})",
        refuri=f"../{page}.html",
        **options,
    )
    return [node], []

# launch setup
def setup(app):
    app.connect('builder-inited', run_apidoc)
    for section in [ 1, 3, 5, 7 ]:
        app.add_role(f"man{section}", man_role)

# ReadTheDocs runs sphinx without first building Flux, so the cffi modules in
# `_flux` will not exist, causing import errors.  Mock the imports to prevent
# these errors.

autodoc_mock_imports = ["_flux", "flux.constants", "yaml"]

napoleon_google_docstring = True

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
# html_static_path = ['_static']

# -- Options for Intersphinx -------------------------------------------------

intersphinx_mapping = {
    "rfc": (
        "https://flux-framework.readthedocs.io/projects/flux-rfc/en/latest/",
        None,
    ),
    "workflow-examples": (
        "https://flux-framework.readthedocs.io/projects/flux-workflow-examples/en/latest/",
        None,
    ),
}
