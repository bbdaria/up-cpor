"""Loop escaping in CPORMetaPlanner (the C# StuckInLoopPlanBased analogue).

A deterministic action that leaves the belief unchanged makes the exact
(worlds, plan) signature recur on the current recursion path. Without the
escape, build_plan_graph would either recurse forever or silently emit a
cycle edge (an invalid plan in a deterministic domain). With it, the repeat
re-enters get_next_action with retry > 0 so the planner can mutate strategy
(here: fall back to sensing), and a planner that never escapes is cut off
after MAX_LOOP_RETRIES with an honest DEAD END leaf.
"""
from up_cpor.native_engine import CPORMetaPlanner, RegressionBelief


class StubAction:
    def __init__(self, name, is_sensing=False, observe=None, add=()):
        self.name = name
        self.is_sensing = is_sensing
        self.observe = observe
        self.add_effects = set(add)
        self.is_nondet = False

    def apply(self, world):
        return set(world) | self.add_effects


WANDER = StubAction("wander")                      # belief fixpoint: signature recurs
SENSE_H = StubAction("sense-h", is_sensing=True, observe="h")
FINISH = StubAction("finish", add=("done",))


class StubSat:
    unknown_facts = {"h"}


class StubPlanner:
    """get_next_action keeps choosing the futile `wander` until the meta
    planner's loop escape re-invokes it with retry > 0."""

    is_simple_domain = False   # compaction off: exercise only the escape
    has_nondet_actions = False
    sat_solver = StubSat()

    def __init__(self, escape_works=True):
        self.escape_works = escape_works
        self.retries_seen = []

    def is_goal_facts(self, current_facts):
        return "done" in current_facts

    def get_next_action(self, belief, retry=0):
        self.retries_seen.append(retry)
        if self.is_goal_facts(belief.current_true_facts()):
            return None, belief
        if retry > 0 and self.escape_works:
            if belief.known_value("h") is None:
                return SENSE_H, belief
            return FINISH, belief
        if retry > 0:
            return WANDER, belief  # a planner that never escapes
        if belief.known_value("h") is not None:
            return FINISH, belief  # after sensing, head straight to the goal
        return WANDER, belief


def _leaves(node, out, seen):
    if id(node) in seen:
        return
    seen.add(id(node))
    if not node["children"]:
        out.append(node["action"])
        return
    for _label, child in node["children"]:
        _leaves(child, out, seen)


def _initial_belief():
    return RegressionBelief(None, [{"h", "base"}, {"base"}])


def test_loop_escape_reroutes_to_sensing():
    planner = StubPlanner(escape_works=True)
    meta = CPORMetaPlanner(simulator=None, online_planner=planner)
    graph = meta.build_plan_graph(_initial_belief())

    assert any(r > 0 for r in planner.retries_seen), \
        "the signature repeat never re-entered get_next_action with retry > 0"
    leaves = []
    _leaves(graph, leaves, set())
    assert leaves and all(l == "GOAL REACHED" for l in leaves), leaves
    # the futile wander stayed in the graph exactly once, then sensing took over
    assert graph["action"] == "wander"
    assert graph["children"][0][1]["action"] == "sense-h"


def test_loop_escape_bounded_by_max_retries():
    planner = StubPlanner(escape_works=False)
    meta = CPORMetaPlanner(simulator=None, online_planner=planner)
    graph = meta.build_plan_graph(_initial_belief())  # must terminate

    assert max(planner.retries_seen) == CPORMetaPlanner.MAX_LOOP_RETRIES - 1
    leaves = []
    _leaves(graph, leaves, set())
    assert leaves == ["DEAD END"]
