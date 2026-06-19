"""Integration tests: build the full contingent plan graph for the blocks domains.

These exercise the whole online pipeline (UP parsing -> SAT propagation -> FF
classical planner -> meta-planner branching) and lock in the blocks2/blocks3
correctness fixes. They are marked ``planner`` and skip cleanly when the optional
runtime pieces (unified_planning, the bundled ``ff`` binary) are unavailable.
"""
import os
import pytest

pytestmark = pytest.mark.planner

_REWRITE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_REPO_ROOT = os.path.dirname(_REWRITE_ROOT)

up = pytest.importorskip("unified_planning")
from unified_planning.io import PDDLReader  # noqa: E402

from up_cpor.native_engine import NativeSDRImpl, CPORMetaPlanner  # noqa: E402


def _collect_leaves(node, leaves):
    if node is None:
        return
    if not node["children"]:
        leaves.append(node["action"])
        return
    for _label, child in node["children"]:
        _collect_leaves(child, leaves)


def _build_plan(domain_name):
    domain_file = os.path.join(_REPO_ROOT, "tests", domain_name, "d.pddl")
    problem_file = os.path.join(_REPO_ROOT, "tests", domain_name, "p.pddl")

    reader = PDDLReader()
    problem = reader.parse_problem(domain_file, problem_file)

    initial_true = set()
    for fluent, value in problem.initial_values.items():
        if value.is_true():
            args = [a.object().name if a.is_object_exp() else str(a) for a in fluent.args]
            name = fluent.fluent().name + ("_" + "_".join(args) if args else "")
            initial_true.add(name)

    online_planner = NativeSDRImpl(problem=problem, problem_file=problem_file)
    meta_planner = CPORMetaPlanner(simulator=None, online_planner=online_planner)
    return meta_planner.build_plan_graph(meta_planner.make_initial_belief(initial_true))


@pytest.fixture(autouse=True)
def _run_from_rewrite_root(monkeypatch):
    # FF runs in-process (cpor_engine.ff_solve); native_engine still writes the
    # temp_*.pddl handed to FF in the cwd, so run from a stable directory.
    monkeypatch.chdir(_REWRITE_ROOT)


@pytest.mark.parametrize("domain", ["blocks2", "blocks3", "blocks7", "doors5", "colorballs2-2"])
def test_all_branches_reach_goal(domain):
    plan = _build_plan(domain)
    leaves = []
    _collect_leaves(plan, leaves)

    assert leaves, f"{domain}: plan graph produced no leaves"
    assert all(leaf == "GOAL REACHED" for leaf in leaves), (
        f"{domain}: expected every branch to reach the goal, got leaves={leaves}"
    )


def test_blocks3_branches_on_sensing():
    plan = _build_plan("blocks3")
    # The root must be a sensing action that branches into two observation outcomes.
    assert plan["action"].startswith("sense")
    assert len(plan["children"]) == 2
