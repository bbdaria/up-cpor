"""Drive CPORMetaEngineImpl through unified-planning's MetaEngine API.

Parameterises the MetaEngine with the real bundled Metric-FF (``up_cpor.ff_engine``)
exposed as a classical OneshotPlanner, so the planner is ``CPORPlanning[ff]``. CPOR
performs its contingent classical sub-solving internally via the same FF; the wrapped
engine is the public-API formality (as in the original up-cpor). Marked ``planner``
(needs unified_planning + in-process FF).
"""
import os
import pytest

pytestmark = pytest.mark.planner

up = pytest.importorskip("unified_planning")

import unified_planning.environment as up_environment  # noqa: E402
from unified_planning.io import PDDLReader  # noqa: E402
from unified_planning.engines.results import PlanGenerationResultStatus  # noqa: E402
from unified_planning.plans import ContingentPlan  # noqa: E402

_REWRITE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_REPO_ROOT = os.path.dirname(_REWRITE_ROOT)


@pytest.fixture(autouse=True)
def _chdir_rewrite_root(monkeypatch):
    # native_engine writes temp_*.pddl for FF in the cwd.
    monkeypatch.chdir(_REWRITE_ROOT)


def _make_env():
    env = up_environment.Environment()
    env.credits_stream = None
    env.factory.add_engine("ff", "up_cpor.ff_engine", "FFEngine")
    env.factory.add_meta_engine("MetaCPORPlanning", "up_cpor.engine", "CPORMetaEngineImpl")
    return env


def _solve(domain):
    env = _make_env()
    reader = PDDLReader(env)
    d = os.path.join(_REPO_ROOT, "tests", domain, "d.pddl")
    p = os.path.join(_REPO_ROOT, "tests", domain, "p.pddl")
    problem = reader.parse_problem(d, p)
    with env.factory.OneshotPlanner(
        name="MetaCPORPlanning[ff]", params={"random_seed": 0}
    ) as planner:
        return planner.solve(problem)


@pytest.mark.parametrize("domain", ["blocks2", "blocks3"])
def test_meta_engine_finds_contingent_plan(domain):
    result = _solve(domain)
    assert result.status == PlanGenerationResultStatus.SOLVED_SATISFICING, result.status
    assert isinstance(result.plan, ContingentPlan)
    assert result.plan.root_node is not None


def test_blocks3_root_is_sensing_with_two_branches():
    result = _solve("blocks3")
    root = result.plan.root_node
    assert root.action_instance.action.name.startswith("sense")
    assert len(root.children) == 2
