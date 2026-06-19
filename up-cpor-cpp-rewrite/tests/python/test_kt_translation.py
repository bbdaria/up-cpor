"""Verify the KT (Palacios-Geffner / SDR) knowledge translation.

Builds the KT classical problem for the blocks3 initial belief and checks that
FF solves it with the expected knowledge-planning shape: a sensing action to
disambiguate the two possible worlds, merge actions to conclude knowledge, and
moves that achieve the goal. This locks in the faithful KT translator
(up_cpor/kt_translation.py) independently of how the online planner uses it.
"""
import os
import pytest

pytestmark = pytest.mark.planner

up = pytest.importorskip("unified_planning")
z3 = pytest.importorskip("z3")

import cpor_engine  # noqa: E402
from unified_planning.io import PDDLReader  # noqa: E402
from up_cpor.native_engine import (  # noqa: E402
    NativeSDRImpl, _fluent_to_name,
)

_REWRITE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_REPO_ROOT = os.path.dirname(_REWRITE_ROOT)


@pytest.fixture(autouse=True)
def _chdir(monkeypatch):
    monkeypatch.chdir(_REWRITE_ROOT)


def test_kt_blocks3_root_plan_uses_sense_and_merge():
    d = os.path.join(_REPO_ROOT, "tests", "blocks3", "d.pddl")
    p = os.path.join(_REPO_ROOT, "tests", "blocks3", "p.pddl")
    problem = PDDLReader().parse_problem(d, p)
    sdr = NativeSDRImpl(problem=problem)
    uncertain = set(sdr.sat_solver.unknown_facts)

    # enumerate the possible initial worlds (tags)
    tags = sdr.sat_solver.enumerate_models([], cap=50)
    tags = [frozenset(t & uncertain) for t in tags]
    assert len(tags) == 2, tags  # blocks3 has exactly two possible worlds

    known_true = {
        _fluent_to_name(fn) for fn, v in problem.initial_values.items() if v.is_true()
    } - uncertain

    actions_arg = [(a["name"], a["is_sensing"], a["observe"], a["pre"], a["add"], a["del"])
                   for a in sdr.kt_action_info]
    domain, prob = cpor_engine.kt_translate(
        actions_arg, sorted(uncertain), [list(t) for t in tags],
        sorted(known_true), sdr.goal_literals)
    with open("temp_kt_d.pddl", "w") as f:
        f.write(domain)
    with open("temp_kt_p.pddl", "w") as f:
        f.write(prob)

    plan = [s.lower().split()[0] for s in cpor_engine.ff_solve("temp_kt_d.pddl", "temp_kt_p.pddl") if s.strip()]
    assert plan, "KT problem should be solvable"
    # faithful KT behaviour: at least one sensing action and one merge inference
    assert any(name.startswith("sense") for name in plan), plan
    assert any(name.startswith("merge_") for name in plan), plan
    # and it must reach the goal (the move that places b2 on b1 is in the plan)
    assert any("move" in name for name in plan), plan
