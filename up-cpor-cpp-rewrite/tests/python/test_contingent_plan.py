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


def _build_plan(domain_name, force_compaction=None):
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
    if force_compaction is not None:
        meta_planner.compaction_enabled = force_compaction
    return meta_planner.build_plan_graph(meta_planner.make_initial_belief(initial_true))


def _count_unique_nodes(node, seen=None):
    """DAG-aware node count: nodes shared via belief-equivalence or exact
    signature reuse are counted once (matches how CollectGraph/PlanSize treat
    the C# plan graph)."""
    if seen is None:
        seen = set()
    if node is None or id(node) in seen:
        return seen
    seen.add(id(node))
    for _label, child in node["children"]:
        _count_unique_nodes(child, seen)
    return seen


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


# Sec 5.4 belief-equivalence plan-graph compaction: on each domain below, the
# compacted graph must (a) have no more unique nodes than a baseline built with
# compaction forced off, and (b) still reach the goal on every leaf -- i.e.
# compaction changes the graph's SIZE, never its correctness. blocks2/blocks7/
# doors5/colorballs2-2/wumpus05 are known (empirically) to exhibit real,
# non-exact-signature reuse; blocks3 is too small to exercise it and is
# asserted only non-regressing (<=).
@pytest.mark.parametrize("domain", ["blocks2", "blocks3", "blocks7", "doors5", "colorballs2-2", "wumpus05"])
def test_compaction_shrinks_or_matches_baseline(domain):
    compacted = _build_plan(domain, force_compaction=None)
    baseline = _build_plan(domain, force_compaction=False)

    leaves_compacted, leaves_baseline = [], []
    _collect_leaves(compacted, leaves_compacted)
    _collect_leaves(baseline, leaves_baseline)

    assert all(leaf == "GOAL REACHED" for leaf in leaves_compacted)
    assert all(leaf == "GOAL REACHED" for leaf in leaves_baseline)
    assert len(leaves_compacted) == len(leaves_baseline), (
        f"{domain}: compaction changed the number of goal leaves"
    )

    nodes_compacted = len(_count_unique_nodes(compacted))
    nodes_baseline = len(_count_unique_nodes(baseline))
    assert nodes_compacted <= nodes_baseline, (
        f"{domain}: compaction produced MORE nodes than the baseline "
        f"({nodes_compacted} > {nodes_baseline})"
    )


@pytest.mark.parametrize("domain", ["blocks2", "blocks7", "doors5", "colorballs2-2", "wumpus05"])
def test_compaction_finds_real_non_exact_equivalences(domain):
    """On these domains compaction must do more than exact-signature dedup:
    the compacted graph is STRICTLY smaller than the forced-off baseline."""
    compacted = _build_plan(domain, force_compaction=None)
    baseline = _build_plan(domain, force_compaction=False)
    assert len(_count_unique_nodes(compacted)) < len(_count_unique_nodes(baseline)), (
        f"{domain}: expected belief-equivalence compaction to shrink the graph"
    )
