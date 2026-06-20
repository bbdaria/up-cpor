"""A real unified-planning OneshotPlanner backed by the bundled Metric-FF.

This wraps the in-process Metric-FF (``cpor_engine.ff_solve``) as a legitimate UP
classical planner. It serves two purposes:

1. It makes the bundled FF usable on its own as a classical planner through the
   standard UP API (``OneshotPlanner(name="ff")``).
2. It is the engine the CPOR MetaEngine is parameterised by
   (``CPORPlanning[ff]``), so no throwaway "dummy" engine is needed. (CPOR still
   performs its contingent classical sub-solving internally via the same FF; the
   wrapped engine is the public-API formality, exactly as in the original up-cpor.)
"""
import os
import tempfile

import cpor_engine
from unified_planning.engines import Engine
from unified_planning.engines.mixins import OneshotPlannerMixin
from unified_planning.engines.results import (
    PlanGenerationResult,
    PlanGenerationResultStatus,
)
from unified_planning.io import PDDLWriter
from unified_planning.model import ProblemKind
from unified_planning.plans import ActionInstance, SequentialPlan


class FFEngine(Engine, OneshotPlannerMixin):
    """Classical OneshotPlanner using the vendored, in-process Metric-FF."""

    def __init__(self, **options):
        Engine.__init__(self)
        OneshotPlannerMixin.__init__(self)

    @property
    def name(self) -> str:
        return "ff"

    @staticmethod
    def supported_kind() -> ProblemKind:
        supported_kind = ProblemKind()
        supported_kind.set_problem_class("ACTION_BASED")
        supported_kind.set_typing("FLAT_TYPING")
        supported_kind.set_typing("HIERARCHICAL_TYPING")
        supported_kind.set_conditions_kind("NEGATIVE_CONDITIONS")
        return supported_kind

    @staticmethod
    def supports(problem_kind) -> bool:
        return problem_kind <= FFEngine.supported_kind()

    def _solve(self, problem, heuristic=None, timeout=None, output_stream=None):
        writer = PDDLWriter(problem)
        tmp = tempfile.mkdtemp(prefix="ff_")
        domain_path = os.path.join(tmp, "domain.pddl")
        problem_path = os.path.join(tmp, "problem.pddl")
        writer.write_domain(domain_path)
        writer.write_problem(problem_path)

        raw = cpor_engine.ff_solve(domain_path, problem_path)

        # Map FF's grounded plan steps ("OP arg arg") back to UP ActionInstances.
        actions = {a.name.lower(): a for a in problem.actions}
        objects = {o.name.lower(): o for o in problem.all_objects}
        steps = []
        for line in raw:
            tok = line.strip().lower().split()
            if not tok or tok[0] not in actions:
                continue
            action = actions[tok[0]]
            params = tuple(objects[a] for a in tok[1:] if a in objects)
            steps.append(ActionInstance(action, params))

        if not steps and any(line.strip() for line in raw):
            # FF reported a plan we could not map -> report as not solved rather
            # than returning a bogus empty plan.
            return PlanGenerationResult(
                PlanGenerationResultStatus.INTERNAL_ERROR, None, self.name
            )

        return PlanGenerationResult(
            PlanGenerationResultStatus.SOLVED_SATISFICING,
            SequentialPlan(steps),
            self.name,
        )

    def destroy(self):
        pass
