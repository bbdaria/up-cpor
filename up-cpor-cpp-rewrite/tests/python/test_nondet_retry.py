"""Integration test for Sec 5.7's non-deterministic actions + ancestor-based
cycle detection (Algorithm 5).

There is no PDDL/unified_planning pathway for non-deterministic effects (the
pinned PDDLReader only parses ``oneof`` for INITIAL-state uncertainty, never
as an action effect -- see up_cpor/grounding.py's ``nondet_effects`` docstring),
so this synthetic domain is built directly via the UP Python API, in the
spirit of the paper's Omelet example: one non-deterministic action with two
outcomes (success/failure), retried on failure, with the goal reachable only
via the success outcome.
"""
import os
import pytest

pytestmark = pytest.mark.planner

_REWRITE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

up = pytest.importorskip("unified_planning")
from unified_planning.shortcuts import Problem, Fluent, InstantaneousAction, Not  # noqa: E402

from up_cpor.native_engine import NativeSDRImpl, CPORMetaPlanner  # noqa: E402


@pytest.fixture(autouse=True)
def _run_from_rewrite_root(monkeypatch):
    monkeypatch.chdir(_REWRITE_ROOT)


def _build_nondet_retry_domain():
    """crack (non-det: outcome 0 sets `good`, outcome 1 is a no-op retry) ->
    use_egg (deterministic, requires `good`, achieves the goal `have_egg`)."""
    good = Fluent("good")
    have_egg = Fluent("have_egg")

    crack = InstantaneousAction("crack")
    crack.add_precondition(Not(have_egg))
    # No .add_effect() calls: the real (non-deterministic) effects are supplied
    # via the nondet_effects side channel below, not UP's effect model.

    use_egg = InstantaneousAction("use_egg")
    use_egg.add_precondition(good)
    use_egg.add_effect(have_egg, True)
    use_egg.add_effect(good, False)

    problem = Problem("nondet_retry1")
    problem.add_fluent(good, default_initial_value=False)
    problem.add_fluent(have_egg, default_initial_value=False)
    problem.add_action(crack)
    problem.add_action(use_egg)
    problem.add_goal(have_egg)

    nondet_effects = {"crack": [[("good", True)], []]}
    return problem, nondet_effects


def _build_plan():
    problem, nondet_effects = _build_nondet_retry_domain()
    online_planner = NativeSDRImpl(problem=problem, nondet_effects=nondet_effects)
    meta_planner = CPORMetaPlanner(simulator=None, online_planner=online_planner)
    return meta_planner.build_plan_graph(meta_planner.make_initial_belief(set()))


def _walk(node, path, leaves, cycles):
    """Traverse the graph, treating a node already on the current path as a
    cycle (not infinite recursion) -- mirrors load_problem.py's
    print_plan_tree cycle-marker logic."""
    if id(node) in path:
        cycles.append(node)
        return
    if not node["children"]:
        leaves.append(node["action"])
        return
    path = path | {id(node)}
    for _label, child in node["children"]:
        _walk(child, path, leaves, cycles)


def test_nondet_domain_is_detected_and_flagged():
    problem, nondet_effects = _build_nondet_retry_domain()
    online_planner = NativeSDRImpl(problem=problem, nondet_effects=nondet_effects)
    assert online_planner.has_nondet_actions is True
    assert online_planner.is_simple_domain is False


def test_plan_graph_terminates_with_a_real_cycle():
    plan = _build_plan()

    # The root must be the non-deterministic action, branching into its two
    # outcomes (mirrors how a sensing action branches into two observations).
    assert plan["action"] == "crack"
    assert len(plan["children"]) == 2
    labels = [label for label, _child in plan["children"]]
    assert labels == ["Outcome 0", "Outcome 1"]

    leaves, cycles = [], []
    _walk(plan, frozenset(), leaves, cycles)

    # Termination: the traversal above only terminates (returns from this
    # test) if there is no infinite recursion -- i.e. the failure outcome was
    # correctly compacted into a back-edge rather than expanded forever.
    assert cycles, "expected the failure outcome to produce a genuine cycle back to an ancestor"

    # Every ACYCLIC leaf reached via the success outcome must be the goal.
    assert leaves, "plan graph produced no leaves"
    assert all(leaf == "GOAL REACHED" for leaf in leaves), (
        f"expected every non-cyclic branch to reach the goal, got leaves={leaves}"
    )


def test_cycle_points_back_to_an_ancestor_on_the_path():
    plan = _build_plan()
    # Outcome 1 (failure) must loop back to the root `crack` node itself.
    outcome_1_label, outcome_1_child = plan["children"][1]
    assert outcome_1_label == "Outcome 1"
    assert outcome_1_child is plan, "the failure outcome should share the ROOT node object (a real cycle)"
