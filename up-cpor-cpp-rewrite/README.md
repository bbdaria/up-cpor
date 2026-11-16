# up-cpor-cpp-rewrite

A C++ and Python rewrite of the CPOR contingent planner. It solves contingent
planning problems (planning under partial observability, where some facts are
hidden and discovered through sensing) by reducing them to repeated classical
planning. The planning logic lives in Python (`up_cpor/`); the heavy reasoning
(Metric-FF, a Z3 belief solver, the knowledge translation, and the plan-graph
algorithms) is compiled into a single C++ extension module, `cpor_engine.so`.

## Prerequisites

* CMake 3.15 or newer, a C++17 compiler and a C compiler
  (on macOS: `xcode-select --install`).
* Python 3.12 with development headers. On the development machine the known
  good interpreter is
  `/Library/Frameworks/Python.framework/Versions/3.12/bin/python3`.
* Network access on the first build (CMake fetches Catch2 for the C++ tests).
* Graphviz (`dot`) if you want to render plan graphs to an image
  (on macOS: `brew install graphviz`).

## 1. Install the Python dependencies

Run this before CMake: the `z3-solver` package supplies the Z3 headers and
library that the build links against.

```bash
cd up-cpor-cpp-rewrite
PY=/Library/Frameworks/Python.framework/Versions/3.12/bin/python3
$PY -m pip install -r requirements.txt
```

This installs `z3-solver`, the `unified_planning` fork required for contingent
problems (stock `unified_planning` lacks the contingent model), and `pytest`.

## 2. Build the C++ engine

```bash
cmake -S . -B build -DPython3_EXECUTABLE=$PY
cmake --build build -j
```

This produces `build/cpor_engine.so` (the native module) and `build/run_tests`
(the C++ test suite).

## 3. Run an example

`load_problem.py` builds a contingent plan graph from a domain and problem PDDL
pair. The compiled module lives in `build/`, so put that on the path:

```bash
PYTHONPATH=build $PY load_problem.py ../tests/blocks3/d.pddl ../tests/blocks3/p.pddl
```

It prints a summary line and writes a Graphviz DOT graph to `output.txt`:

```
6 nodes | 1 goal leaves | 0 dead ends | 0 unreachable branches
Plan Graph Generation Complete: every reachable branch reaches the goal.
```

Useful flags:

* `--tree` also prints the plan tree to stdout.
* `-log trace.log` writes the planner decision trace (useful when a branch dead-ends).
* `-o graph.dot` changes the output DOT file name.

Other domains under `../tests/`: `blocks2`, `blocks7`, `doors5`, `localize5`,
`wumpus05`, `wumpus10`, `medpks010`, `colorballs2-2`, `unix1`.

## 4. Render the plan graph (optional)

```bash
dot -Tpng output.txt -o graph.png && open graph.png
```

## 5. Run the tests (optional)

```bash
./build/run_tests                 # C++ unit tests (Catch2)
PYTHONPATH=build $PY -m pytest     # Python integration tests (about 8 minutes)
```

## Troubleshooting

If `cmake` fails with "Could NOT find Python3 (Development)" or a warning about a
missing SDK, the cause is usually a stale CMake cache or a platform SDK that
moved after a toolchain update. Delete the build directory and reconfigure,
pointing CMake at the Python framework and the current SDK:

```bash
rm -rf build
cmake -S . -B build \
  -DPython3_EXECUTABLE=$PY \
  -DPython3_ROOT_DIR=/Library/Frameworks/Python.framework/Versions/3.12 \
  -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path)
cmake --build build -j
```

On macOS the build rewrites the module's reference to `libz3.dylib` so it
resolves at load time, so no `DYLD_*` environment variables are needed as long
as you build against the same interpreter whose `z3-solver` you installed.
