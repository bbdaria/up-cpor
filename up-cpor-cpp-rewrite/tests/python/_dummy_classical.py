"""A minimal classical OneshotPlanner used only to satisfy the MetaEngine
contract in tests. CPORMetaEngineImpl wraps it for the API but does the actual
classical planning internally via the bundled Metric-FF, so this engine's
_solve is never relied upon for a real plan."""
from unified_planning.engines import Engine
from unified_planning.engines.mixins import OneshotPlannerMixin
from unified_planning.engines.results import (
    PlanGenerationResult,
    PlanGenerationResultStatus,
)
from unified_planning.model import ProblemKind


class DummyClassical(Engine, OneshotPlannerMixin):
    def __init__(self, **options):
        Engine.__init__(self)
        OneshotPlannerMixin.__init__(self)

    @property
    def name(self) -> str:
        return "dummy"

    @staticmethod
    def supported_kind() -> ProblemKind:
        supported_kind = ProblemKind()
        supported_kind.set_problem_class("ACTION_BASED")
        supported_kind.set_conditions_kind("NEGATIVE_CONDITIONS")
        return supported_kind

    @staticmethod
    def supports(problem_kind) -> bool:
        return problem_kind <= DummyClassical.supported_kind()

    def _solve(self, problem, heuristic=None, timeout=None, output_stream=None):
        return PlanGenerationResult(
            PlanGenerationResultStatus.UNSOLVABLE_PROVEN, None, self.name
        )

    def destroy(self):
        pass
