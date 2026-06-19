"""UP MetaEngine for the C++ CPOR rewrite.

CPORMetaEngineImpl plugs the contingent planner into unified-planning's MetaEngine
API. Following the original up-cpor design, the wrapped classical engine is part
of the public API (the planner is exposed as ``CPORPlanning[<engine>]``), but the
actual classical planning is performed internally by the bundled Metric-FF
(in-process, via cpor_engine.ff_solve) driving the native CPOR meta-planner over
the C++ belief-state / Z3 reasoning core.
"""
import re
from typing import Callable, IO, Optional, Type

import unified_planning as up
import unified_planning.environment as up_environment
from unified_planning.engines import Credits, Engine, MetaEngine
import unified_planning.engines.mixins as mixins
from unified_planning.engines.results import (
    PlanGenerationResult,
    PlanGenerationResultStatus,
)
from unified_planning.model import AbstractProblem, ProblemKind
from unified_planning.model.contingent.contingent_problem import ContingentProblem
from unified_planning.plans import ContingentPlan
from unified_planning.plans.contingent_plan import ContingentPlanNode
from unified_planning.plans.plan import ActionInstance

from up_cpor.native_engine import CPORMetaPlanner, NativeSDRImpl, _fluent_to_name

_OBS_RE = re.compile(r"Observed: (.+) == (True|False)")
_LEAVES = ("GOAL REACHED", "DEAD END")

MetaCredits = Credits(
    "Contingent Planning Algorithms (C++ rewrite)",
    "BGU CPOR Development Team",
    "shanigu@bgu.ac.il",
    "https://github.com/shanigu",
    "",
    "Offline contingent planning via online replanning (CPOR/SDR), C++ backend.",
    "Computes a contingent plan graph; sensing actions branch on observations. "
    "Classical subproblems are solved by the bundled Metric-FF in-process.",
)


class CPORMetaEngineImpl(MetaEngine, mixins.OneshotPlannerMixin):
    def __init__(self, *args, random_seed: Optional[int] = None, **kwargs):
        self.random_seed = None if random_seed is None else int(random_seed)
        kwargs.pop("random_seed", None)
        MetaEngine.__init__(self, *args, **kwargs)
        mixins.OneshotPlannerMixin.__init__(self)

    @property
    def name(self) -> str:
        return f"CPORPlanning[{self.engine.name}]"

    @staticmethod
    def is_compatible_engine(engine: Type[Engine]) -> bool:
        return engine.is_oneshot_planner() and engine.supports(
            ProblemKind({"ACTION_BASED"})
        )

    @staticmethod
    def _supported_kind(engine: Type[Engine]) -> ProblemKind:
        supported_kind = ProblemKind()
        supported_kind.set_problem_class("CONTINGENT")
        supported_kind.set_problem_class("ACTION_BASED")
        supported_kind.set_conditions_kind("NEGATIVE_CONDITIONS")
        supported_kind.set_conditions_kind("DISJUNCTIVE_CONDITIONS")
        supported_kind.set_conditions_kind("EQUALITIES")
        supported_kind.set_conditions_kind("UNIVERSAL_CONDITIONS")
        supported_kind.set_effects_kind("CONDITIONAL_EFFECTS")
        supported_kind.set_typing("FLAT_TYPING")
        supported_kind.set_typing("HIERARCHICAL_TYPING")
        return supported_kind.union(engine.supported_kind())

    @staticmethod
    def _supports(problem_kind: ProblemKind, engine: Type[Engine]) -> bool:
        return problem_kind <= CPORMetaEngineImpl._supported_kind(engine)

    @staticmethod
    def get_credits(**kwargs) -> Optional["Credits"]:
        return MetaCredits

    def _solve(
        self,
        problem: AbstractProblem,
        heuristic: Optional[Callable[["up.model.state.State"], Optional[float]]] = None,
        timeout: Optional[float] = None,
        output_stream: Optional[IO[str]] = None,
    ) -> PlanGenerationResult:
        assert isinstance(problem, ContingentProblem)
        assert isinstance(self.engine, mixins.OneshotPlannerMixin)

        if not self._supports(problem.kind, self.engine):
            return PlanGenerationResult(
                PlanGenerationResultStatus.UNSOLVABLE_PROVEN, None, self.name
            )

        # The native planner leans on UP's global-environment shortcuts (grounder,
        # expression manager). Make the problem's environment current for the
        # duration so everything is built in a single, consistent environment.
        previous_env = up_environment.GLOBAL_ENVIRONMENT
        up_environment.GLOBAL_ENVIRONMENT = problem.environment
        try:
            online = NativeSDRImpl(problem=problem)
            meta = CPORMetaPlanner(simulator=None, online_planner=online)

            initial_true = {
                _fluent_to_name(fluent)
                for fluent, value in problem.initial_values.items()
                if value.is_true()
            }
            graph = meta.build_plan_graph(meta.make_initial_belief(initial_true))

            if graph is None or _contains_dead_end(graph):
                return PlanGenerationResult(
                    PlanGenerationResultStatus.UNSOLVABLE_PROVEN, None, self.name
                )

            # _to_contingent_node accesses grounded_action_map (lazily grounded via
            # the UP grounder), which must run inside the environment swap.
            root = _to_contingent_node(graph, online, problem)
        finally:
            up_environment.GLOBAL_ENVIRONMENT = previous_env

        return PlanGenerationResult(
            PlanGenerationResultStatus.SOLVED_SATISFICING,
            ContingentPlan(root, problem.environment),
            self.name,
        )


def _contains_dead_end(node) -> bool:
    if node is None:
        return False
    if node["action"] == "DEAD END":
        return True
    return any(_contains_dead_end(child) for _label, child in node["children"])


def _to_contingent_node(node, online, problem) -> Optional[ContingentPlanNode]:
    """Translate the native plan-graph dict into a UP ContingentPlanNode tree.

    Leaves ("GOAL REACHED"/"DEAD END") become None: a branch that terminates at
    the goal is simply an action node with no child for that observation.
    """
    if node is None or node["action"] in _LEAVES:
        return None

    action = online.grounded_action_map[node["action"]]
    cpn = ContingentPlanNode(ActionInstance(action))

    em = problem.environment.expression_manager
    for label, child in node["children"]:
        child_node = _to_contingent_node(child, online, problem)
        if child_node is None:
            continue
        if label == "Deterministically":
            observation = {}
        else:
            match = _OBS_RE.match(label)
            fnode = online.fnode_map[match.group(1)]
            value = em.TRUE() if match.group(2) == "True" else em.FALSE()
            observation = {fnode: value}
        cpn.add_child(observation, child_node)

    return cpn
