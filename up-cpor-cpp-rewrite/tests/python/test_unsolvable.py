"""No-false-positive tests: genuinely unsolvable contingent problems.

The planner must (a) terminate, and (b) report the failure -- the built graph
has DEAD END leaves on reachable observation branches, so
``plan_graph_stats(root)["solved"]`` is False. Claiming an all-goal graph on
any of these would be a false positive.

The two unsolvable variants are derived from doors5:
 - unsolvable-doors5-blocked: the (adj p2-5 p3-5) passage is removed, so in
   the possible world where door 2 is at p2-5 the corridor cannot be crossed.
   4 of the 5 worlds remain solvable; no contingent plan covers all 5.
 - unsolvable-doors5-goal: the goal additionally requires (opened p2-1), a
   STATIC hidden fact that is false in 4 of the 5 possible worlds.
"""
import pytest

pytestmark = pytest.mark.planner

pytest.importorskip("unified_planning")

from up_cpor.native_engine import plan_graph_stats  # noqa: E402
from test_contingent_plan import _build_plan  # noqa: E402


@pytest.mark.parametrize("domain", ["unsolvable-doors5-blocked", "unsolvable-doors5-goal"])
def test_unsolvable_is_reported_not_claimed(domain):
    root = _build_plan(domain)
    stats = plan_graph_stats(root)
    assert stats["dead_end_leaves"] > 0, (
        f"{domain}: expected reachable DEAD END leaves, got {stats}")
    assert not stats["solved"], (
        f"{domain}: planner claimed an unsolvable problem is solved: {stats}")


def test_solvable_control_is_clean():
    """doors5 itself must stay fully solved -- guards against the unsolvable
    tests passing merely because plan_graph_stats miscounts."""
    stats = plan_graph_stats(_build_plan("doors5"))
    assert stats["solved"], stats
    assert stats["dead_end_leaves"] == 0
    assert stats["goal_leaves"] > 0
