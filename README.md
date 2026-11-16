# CPOR
CPOR is an offline contingent planner that computes a complete plan tree (or graph) where each node is labeled by an action, and edges are labeled by observations. The leaves of the plan tree correspond to goal states. CPOR uses the SDR translation to compute actions. When a sensing action is chosen, CPOR expands both child nodes corresponding to the possible observations. CPOR contains a mechanism for reusing plan segments, resulting in a more compact graph.

More information about CPOR can be found in the paper: "Computing Contingent Plan Graphs using Online Planning" by Maliah and Shani, published in TAAS in 2021.

# SDR
SDR is an online contingent replanner that provides one action at a time and then awaits to receive an observation from the environment. SDR operates by compiling a contingent problem into a classical problem, representing only some of the partial knowledge that the agent has. The classical problem is then solved. If an action is not applicable due to the partial information, SDR modifies the classical problem and replans.

More information about SDR can be found in the paper: "Replanning in Domains with Partial Information and Sensing Actions" by Brafman and Shani, published in JAIR in 2012.

# C++ and Python Rewrite (up-cpor-cpp-rewrite)

The `up-cpor-cpp-rewrite/` directory contains a native C++ and Python reimplementation of the CPOR meta-planner. It replaces the C# backend (CPORLib via pythonnet) with an in-process build of Metric-FF, a Z3 belief solver, the knowledge translation, and the plan-graph algorithms, all compiled into a single Python extension module (`cpor_engine.so`). Full notes are in `up-cpor-cpp-rewrite/README.md`.

## Prerequisites

* CMake 3.15 or newer, a C++17 compiler and a C compiler (on macOS: `xcode-select --install`).
* Python 3.12 with development headers. On the development machine the known good interpreter is `/Library/Frameworks/Python.framework/Versions/3.12/bin/python3`.
* Network access on the first build (CMake fetches Catch2 for the C++ tests).
* Graphviz (`dot`) to render plan graphs (optional; on macOS `brew install graphviz`).

## Install and build

Install the Python dependencies before running CMake: the `z3-solver` package supplies the Z3 headers and library the build links against.

```bash
cd up-cpor-cpp-rewrite
PY=/Library/Frameworks/Python.framework/Versions/3.12/bin/python3
$PY -m pip install -r requirements.txt
cmake -S . -B build -DPython3_EXECUTABLE=$PY
cmake --build build -j
```

This installs `z3-solver`, the `unified_planning` fork required for contingent problems, and `pytest`, then produces `build/cpor_engine.so` and `build/run_tests`.

## Run an example

```bash
PYTHONPATH=build $PY load_problem.py ../tests/blocks3/d.pddl ../tests/blocks3/p.pddl
```

It prints a summary and writes a Graphviz plan graph to `output.txt`. Useful flags: `--tree` prints the plan tree, `-log trace.log` writes the decision trace, `-o graph.dot` changes the output file. Other domains under `../tests/` include `blocks2`, `blocks7`, `doors5`, `localize5`, `wumpus05`, `wumpus10`, `medpks010`, `colorballs2-2`, `unix1`.

Render the graph to an image:

```bash
dot -Tpng output.txt -o graph.png
```

## Tests

```bash
./build/run_tests                 # C++ unit tests (Catch2)
PYTHONPATH=build $PY -m pytest     # Python integration tests
```

## Troubleshooting

If `cmake` fails with "Could NOT find Python3 (Development)" or a missing-SDK warning, the cause is usually a stale CMake cache or a platform SDK that moved after a toolchain update. Delete the build directory and reconfigure, pointing CMake at the Python framework and the current SDK:

```bash
rm -rf build
cmake -S . -B build \
  -DPython3_EXECUTABLE=$PY \
  -DPython3_ROOT_DIR=/Library/Frameworks/Python.framework/Versions/3.12 \
  -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path)
cmake --build build -j
```

# Installation (original Python package)

To install 'up-cpor', clone the project and open a command prompt from the project directory on your PC. Then create a new environment with the following command:

```bash
conda create -n up_cpor_env python=3.8.0
conda activate up_cpor_env
```

Finally, use pip to install the required dependencies:

```bash
pip install -r requirements.txt
```
This will install the unified_planning package and all the essential components to run the algorithm.

## Usage

CPOR and SDR can be used in different ways, depending on the specific requirements of your project. Here are some examples:

### CPOR Engine

```python
import unified_planning.environment as environment
from unified_planning.shortcuts import OneshotPlanner

env = environment.get_environment()
env.factory.add_engine('CPORPlanning', 'up_cpor.engine', 'CPORImpl')

with OneshotPlanner(name='CPORPlanning') as planner:
    result = planner.solve(problem)

```

### CPOR Meta Engine

```python

import unified_planning.environment as environment
from unified_planning.shortcuts import OneshotPlanner

env = environment.get_environment()
env.factory.add_meta_engine('MetaCPORPlanning', 'up_cpor.engine', 'CPORMetaEngineImpl')

with OneshotPlanner(name='MetaCPORPlanning[tamer]') as planner:
    result = planner.solve(problem)

```

### SDR Engine - with UP Simulated Environment

```python
import unified_planning.environment as environment
from unified_planning.model.contingent.environment import SimulatedEnvironment
from unified_planning.shortcuts import ActionSelector

env = environment.get_environment()
env.factory.add_engine('SDRPlanning', 'up_cpor.engine', 'SDRImpl')

with ActionSelector(name='SDRPlanning', problem=problem) as solver:
    simulatedEnv = SimulatedEnvironment(problem)
    while not simulatedEnv.is_goal_reached():
        action = solver.get_action()
        observation = simulatedEnv.apply(action)
        solver.update(observation)

```

### SDR Engine - with SDR Simulated Environment

```python
import unified_planning.environment as environment
from unified_planning.shortcuts import ActionSelector

from up_cpor.simulator import SDRSimulator


env = environment.get_environment()
env.factory.add_engine('SDRPlanning', 'up_cpor.engine', 'SDRImpl')

with ActionSelector(name='SDRPlanning', problem=problem) as solver:
    simulatedEnv = SDRSimulator(problem)
    while not simulatedEnv.is_goal_reached():
        action = solver.get_action()
        observation = simulatedEnv.apply(action)
        solver.update(observation)

```

# Notebooks

We provide two example notebooks to demonstrate the usage of CPOR and SDR engines in different settings:

[CPOR_and_SDR_running_examples.ipynb](https://github.com/aiplan4eu/up-cpor/blob/master/Tests/CPOR_and_SDR_running_examples.ipynb): This notebook contains a full running example on a Windows machine, showcasing the CPOR engine and the SDR engine with a simulated environment. It includes step-by-step instructions and explanations of the code, making it easy to follow along and experiment with the engines.

[CPOR_and_SDR_running_examples_colab.ipynb](https://colab.research.google.com/drive/1e9u9bbjRgBZpF9Trrpu_dcdGgdVFFqwF?usp=sharing): This notebook contains a full running example on Google Colab, showcasing the same engines and the same problem, but running in a cloud environment. This allows you to experiment with the engines even if you don't have a powerful machine or a local Python environment set up.

Both notebooks can be found in the Tests directory of the project repository on GitHub. We recommend starting with the first notebook, which provides a more detailed explanation of the concepts and code, before moving on to the Colab notebook if needed.

We hope these examples will help you get started with using CPOR and SDR engines in your own projects!