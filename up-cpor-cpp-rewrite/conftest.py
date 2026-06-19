"""Pytest configuration for the up-cpor-cpp-rewrite test suite.

Ensures the compiled C++ extension (``cpor_engine``) and the ``up_cpor`` package
are importable regardless of the directory pytest is invoked from.
"""
import os
import sys

_REWRITE_ROOT = os.path.dirname(os.path.abspath(__file__))
_BUILD_DIR = os.path.join(_REWRITE_ROOT, "build")
_TEST_PY_DIR = os.path.join(_REWRITE_ROOT, "tests", "python")

# Order matters: insert build/ LAST so it ends up at sys.path[0] and the freshly
# compiled cpor_engine in build/ always wins over any stale copy left in the
# rewrite root (the script dir, which Python also puts on the path).
for path in (_REWRITE_ROOT, _TEST_PY_DIR, _BUILD_DIR):
    if path in sys.path:
        sys.path.remove(path)
    sys.path.insert(0, path)
