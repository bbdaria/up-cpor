import itertools

import cpor_engine
from unified_planning.shortcuts import get_environment
from up_cpor.converter import ASTConverter
from up_cpor.grounding import ground_actions

def extract_and_map_fluents(node, target_set, fnode_map=None):
    if node.is_fluent_exp():
        args = [a.object().name if a.is_object_exp() else str(a) for a in node.args]
        pred_name = node.fluent().name + "_" + "_".join(args) if args else node.fluent().name
        target_set.add(pred_name)
        if fnode_map is not None:
            fnode_map[pred_name] = node
    elif node.is_and() or node.is_or() or node.is_not():
        for child in node.args:
            extract_and_map_fluents(child, target_set, fnode_map)

def _fluent_to_name(fnode):
    """Convert a grounded UP fluent expression to the engine's flat name,
    e.g. on(b2, b3) -> 'on_b2_b3'. Matches extract_and_map_fluents()."""
    args = [a.object().name if a.is_object_exp() else str(a) for a in fnode.args]
    return fnode.fluent().name + ("_" + "_".join(args) if args else "")


def _literal_to_str(fnode):
    """Convert a (possibly negated) UP literal to an engine literal string,
    e.g. (not on(b2, b3)) -> 'NOT_on_b2_b3'."""
    if fnode.is_not():
        return "NOT_" + _fluent_to_name(fnode.arg(0))
    return _fluent_to_name(fnode)


def _collect_pred_names(node, out):
    """Collect predicate (fluent) names referenced in a condition."""
    if node.is_and() or node.is_or():
        for c in node.args:
            _collect_pred_names(c, out)
    elif node.is_not():
        _collect_pred_names(node.arg(0), out)
    elif node.is_fluent_exp():
        out.add(node.fluent().name)


def _extract_literals(node, out):
    """Collect (fluent_name, polarity) pairs from a precondition/goal conjunction."""
    if node.is_and():
        for c in node.args:
            _extract_literals(c, out)
    elif node.is_not() and node.arg(0).is_fluent_exp():
        out.append((_fluent_to_name(node.arg(0)), False))
    elif node.is_fluent_exp():
        out.append((_fluent_to_name(node), True))


class MiniSATSolver:
    """Initial-state uncertainty reasoner backed by the C++ Z3 BeliefSolver.

    The (oneof ...), (or ...) and (unknown ...) constraints are read from the UP
    ContingentProblem's structured fields and handed to cpor_engine.BeliefSolver,
    which performs complete SAT entailment. This replaces the previous regex-based
    unit-propagation, so arbitrary disjunctive (or) clauses are handled too.
    """
    def __init__(self, problem):
        self._solver = cpor_engine.BeliefSolver()
        self.unknown_facts = set()

        self._closure_cache = {}
        oneof = list(getattr(problem, "oneof_constraints", []))
        self.oneof_members = {_literal_to_str(x) for group in oneof for x in group}
        or_constraints = list(getattr(problem, "or_constraints", []))
        hidden = getattr(problem, "hidden_fluents", [])

        # Factored view of the constraints, used by the fast Python closure and
        # canonical-world sampling below. Valid only while the constraint
        # system is INDEPENDENT positive oneof groups (no or-clauses, no
        # derived hidden clauses, no fluent in two groups) -- checked here and
        # invalidated by add_hidden_clause().
        self.oneof_groups = [[_literal_to_str(x) for x in group] for group in oneof]
        seen_bases = set()
        self.factored = not or_constraints
        for group in self.oneof_groups:
            for lit in group:
                if lit.startswith("NOT_") or lit in seen_bases:
                    self.factored = False
                seen_bases.add(lit)

        for group in oneof:
            self._solver.add_oneof([_literal_to_str(x) for x in group])
        for clause in or_constraints:
            self._solver.add_clause([_literal_to_str(x) for x in clause])
        for hf in hidden:
            if not hf.is_not():
                self.unknown_facts.add(_fluent_to_name(hf))
        self._solver.set_unknown(sorted(self.unknown_facts))

        # A parallel Python-Z3 copy of the constraints, used to ENUMERATE the
        # possible worlds (tags) for the KT translation.
        import z3
        self._z3 = z3
        self._zvars = {}
        self._zsolver = z3.Solver()

        def zlit(lit):
            neg = lit.startswith("NOT_")
            base = lit[4:] if neg else lit
            if base not in self._zvars:
                self._zvars[base] = z3.Bool(base)
            v = self._zvars[base]
            return z3.Not(v) if neg else v

        for group in oneof:
            self._zsolver.add(z3.PbEq([(zlit(_literal_to_str(x)), 1) for x in group], 1))
        for clause in or_constraints:
            self._zsolver.add(z3.Or([zlit(_literal_to_str(x)) for x in clause]))
        self._zlit = zlit

    def enumerate_models(self, facts, cap):
        """Up to `cap` possible worlds consistent with `facts`: each is the set of
        uncertain fluent names true in that world."""
        z3 = self._z3
        self._zsolver.push()
        for f in facts:
            base = f[4:] if f.startswith("NOT_") else f
            if base in self._zvars:
                self._zsolver.add(self._zlit(f))
        models = []
        allvars = list(self._zvars.values())
        while len(models) < cap and self._zsolver.check() == z3.sat:
            m = self._zsolver.model()
            true_names = {name for name, v in self._zvars.items()
                          if z3.is_true(m.eval(v, True))}
            models.append(true_names)
            self._zsolver.add(z3.Or([v != m.eval(v, True) for v in allvars]))
        self._zsolver.pop()
        return models

    def propagate(self, current_facts):
        """Return the logical closure of current_facts under the constraints, or
        None if they are inconsistent. Cached: closed-node matching probes the
        same (observed + obs_set) combinations over and over."""
        key = frozenset(current_facts)
        cached = self._closure_cache.get(key, False)
        if cached is not False:
            return cached
        if self.factored:
            result = self._factored_closure(key)
        else:
            result = self._solver.propagate(list(key))
            result = None if result is None else set(result)
        if len(self._closure_cache) > 200000:
            self._closure_cache.clear()
        self._closure_cache[key] = result
        return result

    def _factored_closure(self, current_facts):
        """Exact closure for independent positive oneof groups, without any
        solver call: an observed positive collapses its group; eliminating all
        but one member entails the survivor."""
        closure = set(current_facts)
        for group in self.oneof_groups:
            pos = [b for b in group if b in closure]
            if len(pos) > 1:
                return None  # two members of a oneof both true
            if pos:
                closure.update("NOT_" + b for b in group if b != pos[0])
                continue
            alive = [b for b in group if ("NOT_" + b) not in closure]
            if not alive:
                return None  # every member eliminated
            if len(alive) == 1:
                closure.add(alive[0])
        return closure

    def canonical_world(self, current_facts):
        """The lexicographically-least hidden assignment consistent with
        current_facts (first surviving member of each oneof group), or None
        when the constraint system is not factored. Canonical sampling keeps
        plans identical across beliefs with equal knowledge, so meta-planner
        subtrees reconverge instead of chasing arbitrary Z3 models."""
        if not self.factored:
            return None
        closure = self.propagate(current_facts)
        if closure is None:
            return None
        world = set()
        for group in self.oneof_groups:
            alive = [b for b in group if ("NOT_" + b) not in closure]
            if not alive:
                return None
            world.add(alive[0])
        return world

    def complete(self, current_facts):
        """One consistent full assignment (set of true constraint-fluent names)
        agreeing with current_facts, or None if inconsistent."""
        result = self._solver.complete(list(current_facts))
        return None if result is None else set(result)
    
    def add_hidden_clause(self, new_hidden_fact, clause):
        self.factored = False  # correlations break group independence
        self._closure_cache.clear()  # constraints changed
        if new_hidden_fact not in self.unknown_facts:
            self.unknown_facts.add(new_hidden_fact)
            self._solver.set_unknown(sorted(self.unknown_facts))
        self._solver.add_clause(clause)
        self._zsolver.add(self._z3.Or([self._zlit(c) for c in clause]))

# class DynamicAction:
#     def __init__(self, info):
#         # info is a contingent-grounding action dict (see up_cpor.grounding).
#         self.name = info["name"]
#         self.observe = info["observe"]
#         self.is_sensing = info["is_sensing"]
#         # Positive dynamic preconditions the planner checks for applicability.
#         self.preconditions = {f for f, pol in info["pre"] if pol}
#         self.add_effects = set(info["add"])
#         self.del_effects = set(info["del"])

#     def is_applicable(self, state_fact_names):
#         return self.preconditions.issubset(state_fact_names)

#     def apply(self, current_facts_set):
#         new_facts = set(current_facts_set)
#         for f in self.add_effects:
#             new_facts.add(f)
#             new_facts.discard(f"NOT_{f}")
#         for f in self.del_effects:
#             new_facts.discard(f)
#             new_facts.add(f"NOT_{f}")
#         return new_facts

class DynamicAction:
    def __init__(self, info):
        self.name = info["name"]
        self.observe = info["observe"]
        self.is_sensing = info["is_sensing"]
        self.preconditions = {f for f, pol in info["pre"] if pol}
        self.add_effects = set(info["add"])
        self.del_effects = set(info["del"])
        self.cond_effects = info.get("cond", [])   # [(cond_lits, fluent, is_add)]
        # Sec 5.7: non-deterministic outcomes -- each a list of (fluent, is_add)
        # literals; None/[] for every deterministic/sensing action.
        self.nondet_outcomes = info.get("nondet") or []

    @property
    def is_nondet(self):
        return bool(self.nondet_outcomes)

    def is_applicable(self, state_fact_names):
        return self.preconditions.issubset(state_fact_names)

    def apply_outcome(self, pre_state, outcome_idx):
        """Progress pre_state through non-deterministic outcome `outcome_idx`
        (mirrors `apply()`, but for one specific psi-outcome rather than the
        single deterministic effect set)."""
        new_facts = set(pre_state)
        for fluent, is_add in self.nondet_outcomes[outcome_idx]:
            if is_add:
                new_facts.add(fluent)
            else:
                new_facts.discard(fluent)
        return new_facts

    def effect_on(self, fact, pre_state):
        if fact in self.add_effects: return True
        if fact in self.del_effects: return False
        for cond_lits, fluent, is_add in self.cond_effects:
            if fluent == fact and all((g in pre_state) == pol for g, pol in cond_lits):
                return is_add
        return None

    def apply(self, pre_state):
        new_facts = set(pre_state)
        new_facts |= self.add_effects
        new_facts -= self.del_effects
        for cond_lits, fluent, is_add in self.cond_effects:
            if all((g in pre_state) == pol for g, pol in cond_lits):
                if is_add: new_facts.add(fluent)
                else: new_facts.discard(fluent)
        return new_facts

class RegressionBelief:
    """Exact explicit-world belief tracker.
    Tracks the exact set of possible worlds to fully support conditional effects
    over uncertain variables without losing information."""

    def __init__(self, sat_solver, worlds, history=(), plan=(), expect=None):
        self.sat = sat_solver
        self.worlds = frozenset(frozenset(w) for w in worlds)
        self.history = tuple(history)
        self.plan = tuple(plan)
        # Parallel to `plan`: the observation outcome (True/False) the plan
        # ASSUMED for each embedded sensing step (None for non-sensing steps
        # or when no assumption is known). Drives branch continuity at
        # observation nodes; deliberately NOT part of signature() -- a node's
        # subtree is valid for any belief with the same (worlds, plan).
        self.expect = tuple(expect) if expect is not None else (None,) * len(self.plan)

    def current_true_facts(self):
        """Returns the intersection of all possible worlds (facts known to be true in all)."""
        if not self.worlds:
            return frozenset()
        return frozenset.intersection(*self.worlds)

    def regress(self, literal):
        pass # Kept for API compatibility, logic explicitly handled now

    def known_value(self, base):
        """Current truth value: True if in all worlds, False if in none, else None."""
        is_true = all(base in w for w in self.worlds)
        is_false = all(base not in w for w in self.worlds)
        if is_true: return True
        if is_false: return False
        return None

    def progress(self, action):
        advance = bool(self.plan) and self.plan[0] == action.name
        new_plan = self.plan[1:] if advance else self.plan
        new_expect = self.expect[1:] if advance else self.expect
        new_worlds = [action.apply(w) for w in self.worlds]
        return RegressionBelief(self.sat, new_worlds, self.history + (action,), new_plan, new_expect)

    def progress_nondet(self, action, outcome_idx):
        """Belief after executing a non-deterministic action (Sec 5.7) whose
        outcome turned out to be `outcome_idx`. The meta-planner branches once
        per outcome (like a sensing observation), so within this branch the
        chosen outcome is applied uniformly to every currently-possible world."""
        advance = bool(self.plan) and self.plan[0] == action.name
        new_plan = self.plan[1:] if advance else self.plan
        new_expect = self.expect[1:] if advance else self.expect
        new_worlds = [action.apply_outcome(w, outcome_idx) for w in self.worlds]
        return RegressionBelief(self.sat, new_worlds, self.history + (action,), new_plan, new_expect)

    def observe(self, literal):
        neg = literal.startswith("NOT_")
        base = literal[4:] if neg else literal
        new_worlds = []
        for w in self.worlds:
            if (base in w) if not neg else (base not in w):
                new_worlds.append(w)
        if not new_worlds:
            return None
        return RegressionBelief(self.sat, new_worlds, self.history, self.plan, self.expect)

    def with_plan(self, plan, expect=None):
        return RegressionBelief(self.sat, self.worlds, self.history, plan, expect)

    def signature(self):
        return (self.worlds, self.plan)

    def sample_worlds(self):
        """Yield full possible worlds (fact sets), most-preferred first."""
        return iter(self.worlds)

    def literal_holds_after(self, extra_literals, literal):
        """Algorithm 3 lines 8-11: would `literal` hold in every possible world
        consistent with also observing every literal in `extra_literals`?
        Vacuously True if no world survives the filter (that observation
        combination cannot occur along this branch, so it cannot violate the
        match either)."""
        def matches(world, lit):
            neg = lit.startswith("NOT_")
            base = lit[4:] if neg else lit
            return (base not in world) if neg else (base in world)

        filtered = [w for w in self.worlds if all(matches(w, o) for o in extra_literals)]
        if not filtered:
            return True
        return all(matches(w, literal) for w in filtered)


class LazyBelief:
    """SAT-backed belief for simple domains whose hidden fluents are STATIC
    and directly sensable (doors, colorballs: no KT need, no effect ever
    modifies an unknown fact).

    In such domains every possible world shares one identical dynamic state,
    so the belief factors into (dynamic facts, observed hidden literals);
    hidden knowledge is the Z3 closure of the initial-state constraints plus
    the observations, computed by the C++ BeliefSolver. This replaces explicit
    world enumeration, which is unsound once the world count passes the
    enumeration cap (doors15: 15^7 worlds vs a cap of 4000) and costs
    O(#worlds) on every belief operation."""

    def __init__(self, sat_solver, facts, observed=(), history=(), plan=(), expect=None):
        self.sat = sat_solver
        self.facts = frozenset(facts)
        self.observed = frozenset(observed)
        self.history = tuple(history)
        self.plan = tuple(plan)
        self.expect = tuple(expect) if expect is not None else (None,) * len(self.plan)
        self._closure = None  # lazily-computed hidden-literal closure

    def closure(self):
        if self._closure is None:
            self._closure = self.sat.propagate(self.observed) or set()
        return self._closure

    def current_true_facts(self):
        return frozenset(self.facts
                         | {l for l in self.closure() if not l.startswith("NOT_")})

    def known_value(self, base):
        if base in self.sat.unknown_facts:
            cl = self.closure()
            if base in cl:
                return True
            if ("NOT_" + base) in cl:
                return False
            return None
        # Dynamic fluents are closed-world over the shared fact set.
        return base in self.facts

    def progress(self, action):
        advance = bool(self.plan) and self.plan[0] == action.name
        new_plan = self.plan[1:] if advance else self.plan
        new_expect = self.expect[1:] if advance else self.expect
        b = LazyBelief(self.sat, action.apply(self.facts), self.observed,
                       self.history + (action,), new_plan, new_expect)
        b._closure = self._closure  # hidden fluents are static
        return b

    def observe(self, literal):
        neg = literal.startswith("NOT_")
        base = literal[4:] if neg else literal
        if base not in self.sat.unknown_facts:
            # Closed-world dynamic fact: the outcome either matches the state
            # (no information) or is impossible.
            return None if (base in self.facts) == neg else self
        cl = self.closure()
        if literal in cl:
            return self  # already entailed: no new information
        if (base if neg else "NOT_" + base) in cl:
            return None  # outcome contradicts current knowledge
        nb = LazyBelief(self.sat, self.facts, self.observed | {literal},
                        self.history, self.plan, self.expect)
        new_closure = self.sat.propagate(nb.observed)
        if new_closure is None:
            return None  # outcome inconsistent with the constraints
        nb._closure = new_closure
        return nb

    def with_plan(self, plan, expect=None):
        b = LazyBelief(self.sat, self.facts, self.observed, self.history, plan, expect)
        b._closure = self._closure
        return b

    def signature(self):
        return (self.facts, self.observed, self.plan)

    def sample_worlds(self, cap=16):
        """Yield full possible worlds: the shared dynamic facts joined with a
        hidden-fluent assignment. The first sample is CANONICAL (first
        surviving member per oneof group) so beliefs with equal knowledge get
        identical plans and the meta-planner's subtrees reconverge; the
        solver-backed samples only run if a caller rejects it and keeps
        iterating (e.g. the goal-violating filter on a hidden goal)."""
        first = self.sat.canonical_world(self.observed)
        if first is None:
            first = self.sat.complete(sorted(self.observed))
            if first is None:
                return
        first = frozenset(self.facts | first)
        yield first
        for model in self.sat.enumerate_models(sorted(self.observed), cap):
            world = frozenset(self.facts | model)
            if world != first:
                yield world

    def literal_holds_after(self, extra_literals, literal):
        """Entailment counterpart of RegressionBelief.literal_holds_after,
        answered by the SAT solver instead of world filtering."""
        neg = literal.startswith("NOT_")
        base = literal[4:] if neg else literal
        hidden_extra = []
        for lit in extra_literals:
            e_neg = lit.startswith("NOT_")
            e_base = lit[4:] if e_neg else lit
            if e_base in self.sat.unknown_facts:
                hidden_extra.append(lit)
            elif (e_base in self.facts) == e_neg:
                return True  # observation impossible here: vacuously true
        if base not in self.sat.unknown_facts:
            return (base not in self.facts) if neg else (base in self.facts)
        closure = self.sat.propagate(self.observed | set(hidden_extra))
        if closure is None:
            return True  # observation combination impossible: vacuously true
        return literal in closure

class NativeSDRImpl:
    def __init__(self, problem, problem_file=None, error_on_failed_checks=False, nondet_effects=None):
        get_environment().error_on_failed_checks = False
        self.problem = problem
        self.problem_file = problem_file  # source PDDL under test (informational)
        self.fnode_map = {}
        self.ff_cache = {}
        # Constraints come from the parsed ContingentProblem (i.e. the file under
        # test), reasoned over by the C++ Z3 BeliefSolver.
        self.sat_solver = MiniSATSolver(problem)
        
        problem.environment.error_on_failed_checks = False

        # Contingent-aware grounding (up_cpor.grounding): grounds every action over
        # its typed parameters, filtering only on static KNOWN predicates, so actions
        # guarded by hidden fluents (move requiring opened/safe, sensed at run time)
        # are NOT pruned away (which is what the UP grounder does). The UP grounder
        # is avoided here (it prunes, and is slow on large domains); it is only used
        # lazily for grounded_action_map (the engine API's UP ActionInstances).
        print("\n[SDR] Contingent grounding...")
        self.kt_action_info = ground_actions(problem, nondet_effects=nondet_effects)
        print(f"[SDR] {len(self.kt_action_info)} grounded actions.")

        self.dynamic_actions = {info["name"]: DynamicAction(info) for info in self.kt_action_info}
        self.observe_map = {info["name"]: info["observe"]
                            for info in self.kt_action_info if info["observe"]}
        # FF sanitizes '-' to '_' in action names; keep a normalized lookup so we can
        # map FF's reported name back to the canonical grounded action key.
        self.normalized_action_map = {name.replace("-", "_"): name for name in self.dynamic_actions}
        # observe-fact -> ALL sensing actions for it (a fact can be sensable
        # from several positions; the injector must pick one applicable NOW).
        self.sensing_map = {}
        for name, act in self.dynamic_actions.items():
            if act.is_sensing and act.observe:
                self.sensing_map.setdefault(act.observe, []).append(name)
        for names in self.sensing_map.values():
            names.sort()

        # Derive hidden-fact correlations from conditional effects whose condition
        # references an already-hidden fact (e.g. localize5's `free-up` is correlated
        # with `at` via checking's when-clauses, but never declared in :init).
        def _neg(lit_pair):
            g, pol = lit_pair
            return ("NOT_" + g) if pol else g

        for info in self.kt_action_info:
            for cond_lits, fluent, is_add in info.get("cond", []):
                if fluent in self.sat_solver.oneof_members:
                    continue  # state-transition effect on a tracked variable, not a new hidden fact
                hidden_lits = [(g, pol) for g, pol in cond_lits
                               if g in self.sat_solver.unknown_facts]
                if not hidden_lits:
                    continue
                target = fluent if is_add else ("NOT_" + fluent)
                clause = [_neg(lit) for lit in hidden_lits] + [target]
                self.sat_solver.add_hidden_clause(fluent, clause)

        # Goals and the fluent->FNode map come from the (ungrounded) problem: goals
        # are already ground and initial_values enumerates every ground fluent.
        self.converter = ASTConverter()
        compiled_goals = [self.converter.compile_formula(g) for g in problem.goals]
        self.compiled_goal = (compiled_goals[0] if len(compiled_goals) == 1
                              else cpor_engine.AndNode(compiled_goals))
        self.goal_strings = set()
        self.goal_literals = []
        for g in problem.goals:
            extract_and_map_fluents(g, self.goal_strings, self.fnode_map)
            _extract_literals(g, self.goal_literals)
        for fnode in problem.initial_values.keys():
            extract_and_map_fluents(fnode, set(), self.fnode_map)
        # Hidden fluents (e.g. wumpus `safe`) may not be in initial_values; map them
        # so the classical relaxation / engine can reference their FNodes.
        for hf in getattr(problem, "hidden_fluents", []):
            extract_and_map_fluents(hf, set(), self.fnode_map)

        # Decide whether to use the (sound but expensive) KT translation: it is
        # needed only when an action has a hidden precondition that NO sensing
        # action observes directly, so its value must be DEDUCED (e.g. wumpus
        # `safe`, deduced from breeze/stench). When every hidden precondition is
        # directly sensable (doors `opened`, colorballs `color`), the cheaper
        # consistent-world fallback suffices.
        hidden_preds = {hf.fluent().name for hf in getattr(problem, "hidden_fluents", [])
                        if not hf.is_not()}
        sensed_preds, precond_preds = set(), set()
        # for a in problem.actions:
        #     for obs in (getattr(a, "observed_fluents", []) or []):
        #         sensed_preds.add(obs.fluent().name)
        #     for pre in a.preconditions:
        #         _collect_pred_names(pre, precond_preds)
        # self.needs_kt = bool((precond_preds & hidden_preds) - sensed_preds)
        for a in problem.actions:
            for obs in (getattr(a, "observed_fluents", []) or []):
                sensed_preds.add(obs.fluent().name)
            for pre in a.preconditions:
                _collect_pred_names(pre, precond_preds)
            for e in getattr(a, "effects", []) or []:
                if not e.condition.is_true():
                    _collect_pred_names(e.condition, precond_preds)
        self.needs_kt = bool((precond_preds & hidden_preds) - sensed_preds)

        # Sec 5.7: does this domain have any non-deterministic actuation
        # action? Gates the ancestor-based cycle detection in CPORMetaPlanner.
        self.has_nondet_actions = any(info.get("nondet") for info in self.kt_action_info)

        # Sec 2.1's "simple contingent problem" test: hidden fluents are static
        # (nothing here changes that -- DynamicAction never mutates a hidden
        # fact), no hidden fluent appears in a conditional effect's CONDITION,
        # and there are no non-deterministic actions. Gates the Sec 5.4
        # belief-equivalence compaction (CPORMetaPlanner), which is only
        # sound/complete for simple domains (mirrors the C#'s
        # `if (!Domain.IsSimple) return IsGoalState();`).
        self.is_simple_domain = (not self.has_nondet_actions) and not any(
            g in self.sat_solver.unknown_facts
            for info in self.kt_action_info
            for cond_lits, _fluent, _is_add in info.get("cond", [])
            for g, _pol in cond_lits
        )

        # LazyBelief eligibility: simple domain, no KT need, and no effect ever
        # MODIFIES an unknown fact (hidden fluents are static, so every possible
        # world shares one dynamic state and hidden knowledge never goes stale).
        # Then the belief needs no world enumeration at all -- crucial when the
        # world count dwarfs the enumeration cap (doors15: 15^7 worlds).
        hidden_modified = any(
            f in self.sat_solver.unknown_facts
            for info in self.kt_action_info
            for f in (list(info["add"]) + list(info["del"])
                      + [fl for _c, fl, _a in info.get("cond", [])])
        )
        self.use_lazy_belief = (self.is_simple_domain and not self.needs_kt
                                and not hidden_modified)

        self._grounded_action_map = None  # built lazily via the UP grounder

    @property
    def grounded_action_map(self):
        """UP grounded action objects keyed by name (for engine-API ActionInstances).
        Built lazily via the UP grounder only when the engine path needs it."""
        if self._grounded_action_map is None:
            with self.problem.environment.factory.Compiler(name="up_grounder") as grounder:
                gp = grounder.compile(self.problem).problem
            self._grounded_action_map = {a.name: a for a in gp.actions}
        return self._grounded_action_map

    def is_goal_facts(self, current_facts):
        # Evaluate the compiled goal Formula over the currently-known facts.
        preds = [cpor_engine.Predicate(f) for f in current_facts]
        return self.compiled_goal.is_true(preds)

    def get_next_action(self, belief, retry=0):
        """Return (action, belief) where belief may carry a freshly-committed plan.
        Returns (None, belief) at the goal or a genuine dead end.

        ``retry`` > 0 is the loop-escape mode (the C# StuckInLoop analogue):
        the meta-planner re-entered a belief already open on the current
        recursion path, so the normal deterministic choice below would repeat
        the ancestor's step forever. Mutate the strategy instead of re-deriving
        the same plan: gather information first (sensing shrinks the world set,
        so the signature can never recur), then replan from a DIFFERENT
        consistent world sample."""
        current_facts = belief.current_true_facts()

        if self.is_goal_facts(current_facts):
            return None, belief

        if retry:
            belief = belief.with_plan(())
            # (a) directly applicable sensing on an unknown fluent
            for name in sorted(self.dynamic_actions):
                act = self.dynamic_actions[name]
                if (act.is_sensing and act.observe in self.sat_solver.unknown_facts
                        and belief.known_value(act.observe) is None
                        and act.preconditions <= current_facts):
                    return act, belief
            # (b) plan toward an unknown sensing action's preconditions
            chosen = self._plan_to_observe(belief, current_facts)
            if chosen is not None:
                return chosen
            # (c) replan from an alternative world sample (a different Z3
            #     model than the one the looping plan was derived from);
            #     goal-satisfying worlds yield the ambiguous empty plan, skip.
            candidates = (w for w in belief.sample_worlds() if not self.is_goal_facts(w))
            worlds = sorted(itertools.islice(candidates, 32), key=sorted)
            if worlds:
                sample = worlds[retry % len(worlds)]
                plan = self._ff_plan_cached(frozenset(sample))
                if plan:
                    chosen = self._follow_committed_plan(belief.with_plan(tuple(plan)), current_facts)
                    if chosen is not None:
                        return chosen
            return None, belief

        # 1. Follow the committed plan if there is one and it is still valid.
        chosen = self._follow_committed_plan(belief, current_facts)
        if chosen is not None:
            return chosen

        # 2. Determinize via the KT (knowledge) translation -- only for domains
        #    that need deduced (not directly sensed) hidden preconditions.
        plan, expect = self._kt_determinize(belief) if self.needs_kt else ([], None)
        if plan:
            return self._follow_committed_plan(belief.with_plan(tuple(plan), expect), current_facts) \
                or (None, belief)

        # 2b. Fallback: consistent world (Z3 model) progressed through history.
        #     Sample a world where the goal is NOT yet satisfied: a goal-
        #     satisfying world yields an EMPTY classical plan, which is
        #     indistinguishable from "no plan found" and used to dead-end
        #     solvable beliefs (localize5: sampling the at-goal world). At
        #     least one goal-violating world exists here, else is_goal_facts
        #     above would have ended the node.
        full_state = next((w for w in belief.sample_worlds() if not self.is_goal_facts(w)),
                          frozenset())
        plan = self._ff_plan_cached(frozenset(full_state))
        if not plan:
            optimistic = set(belief.current_true_facts())
            for f in self.sat_solver.unknown_facts:
                if belief.known_value(f) is None:
                    optimistic.add(f)
            # The optimistic state can satisfy the goal trivially (it sets every
            # unknown true at once); planning from it would again yield the
            # ambiguous empty plan, so skip it in that case.
            if not self.is_goal_facts(optimistic):
                plan = self._ff_plan_cached(frozenset(optimistic))

        if plan:
            chosen = self._follow_committed_plan(belief.with_plan(tuple(plan)), current_facts)
            if chosen is not None:
                return chosen

        # 2c. Sec 5.7 support: a non-deterministic action carries no add/del
        #     effects FF can exploit (its outcomes are chosen by the
        #     meta-planner's branching in CPORMetaPlanner.build_plan_graph, not
        #     by this online step chooser), so classical/KT planning above can
        #     never discover it. If one is directly applicable, take it.
        for name in sorted(self.dynamic_actions):
            act = self.dynamic_actions[name]
            if act.is_nondet and act.preconditions <= current_facts:
                return act, belief

        # 3. Nothing usable: explore by sensing a currently-unknown fluent that is
        #    directly applicable now.
        for name in sorted(self.dynamic_actions):
            act = self.dynamic_actions[name]
            if (act.is_sensing and act.observe in self.sat_solver.unknown_facts
                    and belief.known_value(act.observe) is None
                    and act.preconditions <= current_facts):
                return act, belief

        # 4. Plan-to-observe: no sensing action is applicable right now and the
        #    classical relaxation found no plan to the goal, so we would otherwise
        #    declare a dead end. Before giving up, try to PLAN toward an unknown
        #    sensing action's preconditions (e.g. move to a cell where we can sense)
        #    so the observation can resolve the uncertainty. Mirrors the C# CPOR
        #    PlanToObserveDeadEnd step.
        chosen = self._plan_to_observe(belief, current_facts)
        if chosen is not None:
            return chosen

        return None, belief

    def _plan_to_observe(self, belief, current_facts):
        """When stuck (no goal plan, no directly-applicable sensing action), plan a
        classical path toward an unknown sensing action's preconditions, commit it,
        and follow it (the sensing action itself is appended so it fires on arrival).
        Returns (action, belief) or None if nothing can be set up to observe."""
        
        full_state = next(iter(belief.sample_worlds()), None)
        if full_state is None:
            return None

        for name in sorted(self.dynamic_actions):
            act = self.dynamic_actions[name]
            if not (act.is_sensing and act.observe in self.sat_solver.unknown_facts
                    and belief.known_value(act.observe) is None):
                continue
            if act.preconditions <= current_facts:
                continue  # would already have been picked by step 3
            if not act.preconditions:
                continue
            goal = [(p, True) for p in sorted(act.preconditions)]
            plan = self._run_cpp_ff_plan(full_state, goal=goal)
            if plan:
                committed = belief.with_plan(tuple(plan) + (name,))
                chosen = self._follow_committed_plan(committed, current_facts)
                if chosen is not None:
                    return chosen
        return None

    # The KT translation emits one classical predicate per (uncertain fluent x tag),
    # so the FF domain size is driven by the number of DISTINCT tags, not the number
    # of possible worlds (worlds usually collapse to far fewer distinct projections
    # onto the uncertain fluents). So we enumerate up to MODEL_ENUM_CAP worlds (we
    # must see them ALL for the KT merge actions to stay sound) and then cap the KT
    # path on the number of DISTINCT tags. This lets problems with many worlds but
    # few distinct tags still take the sound KT path instead of the fallback.
    MODEL_ENUM_CAP = 4000   # max worlds we will enumerate (soundness needs all of them)
    KT_MAX_TAGS = 300       # max distinct tags handed to FF (tractability of the KT domain)

    def _kt_determinize(self, belief):
        """Returns (plan, expect): the KT plan's action names plus, parallel to
        it, the observation outcome each embedded sensing step ASSUMES (the
        value in the sample world -- tag 0's world -- progressed to that step).
        The suffix after a sensing step is valid exactly on the branch matching
        that assumption (branch continuity)."""
        worlds = list(belief.worlds)
        if not worlds or len(worlds) > self.MODEL_ENUM_CAP:
            return [], None

        union = set().union(*worlds)
        uncertain = {f for f in union if any(f in w for w in worlds) and any(f not in w for w in worlds)}

        if not uncertain:
            return self._run_cpp_ff_plan(worlds[0]), None

        known_true = set.intersection(*[set(w) for w in worlds])
        seen, tags = set(), []
        for w in worlds:
            t = frozenset(w & uncertain)
            if t not in seen:
                seen.add(t)
                tags.append(t)

        if len(tags) > self.KT_MAX_TAGS:
            return [], None

        actions_arg = [(a["name"], a["is_sensing"], a["observe"], a["pre"], a["add"], a["del"], a.get("cond", []))
                       for a in self.kt_action_info]
        domain, problem = cpor_engine.kt_translate(
            actions_arg, sorted(uncertain), [list(t) for t in tags],
            sorted(known_true), self.goal_literals)
        
        raw = self._ff_solve_pddl(domain, problem)

        plan = []
        for step in raw:
            tok = step.lower().strip().split()
            if not tok: continue
            name = tok[0]
            if name.startswith("merge_") or name.startswith("ref_"): continue
            if name in self.dynamic_actions:
                plan.append(name)
            elif name in self.normalized_action_map:
                plan.append(self.normalized_action_map[name])

        # Simulate the sample world (worlds[0], whose uncertain-projection is
        # tag 0 -- the tag the KT sensing actions sample their outcome from)
        # through the plan to record each sensing step's assumed observation.
        sim = set(worlds[0])
        expect = []
        for name in plan:
            act = self.dynamic_actions[name]
            expect.append((act.observe in sim) if act.is_sensing and act.observe else None)
            sim = act.apply(sim)
        return plan, expect

    def _ff_solve_pddl(self, domain_str, problem_str):
        # In-memory: FF parses the PDDL straight from these strings (fmemopen),
        # so nothing is written to disk.
        return cpor_engine.ff_solve_strings(domain_str, problem_str)

    def _ff_plan_cached(self, facts_frozenset):
        if facts_frozenset in self.ff_cache:
            return self.ff_cache[facts_frozenset]
        plan = self._run_cpp_ff_plan(set(facts_frozenset))
        self.ff_cache[facts_frozenset] = plan
        return plan

    def _follow_committed_plan(self, belief, current_facts):
        """Pick the next action dictated by belief.plan, sensing an unknown
        precondition first if needed. Returns (action, belief) or None if the
        committed plan is empty/invalid (caller should replan)."""
        plan = belief.plan
        if not plan:
            return None

        head = plan[0]
        act = self.dynamic_actions.get(head)
        if act is None:
            return None  # stale name -> replan

        if act.is_sensing and act.observe:
            kv = belief.known_value(act.observe)
            if kv is not None:
                # The committed sensing step's outcome became known en route
                # (knowledge can arrive between commit and execution now that
                # plans survive observations). Mirrors the C#'s "observation
                # action for something that is already known -- continue with
                # the same state": skip the no-op step, unless the plan's
                # assumed outcome is contradicted, in which case the suffix
                # is invalid and the caller must replan.
                if belief.expect[0] is not None and kv != belief.expect[0]:
                    return None
                return self._follow_committed_plan(
                    belief.with_plan(plan[1:], belief.expect[1:]), current_facts)

        missing = act.preconditions - current_facts
        if not missing:
            return act, belief  # applicable now; progress() will advance the plan

        # Sense an unknown missing precondition (keep the committed plan).
        indirect_unknown = False
        for missing_fact in sorted(missing):
            value = belief.known_value(missing_fact)
            if value is False and missing_fact in self.sat_solver.unknown_facts:
                return None  # real world contradicts the committed plan -> replan
            if value is None:
                # Inject a sensor for the unknown precondition -- but only one
                # whose OWN preconditions hold here (a fact can be sensable
                # from several positions; injecting an inapplicable sensor put
                # physically impossible observations into the plan graph and
                # poisoned the closed-node K sets with foreign positions).
                for sense_name in self.sensing_map.get(missing_fact, ()):
                    sense_act = self.dynamic_actions[sense_name]
                    if sense_act.preconditions <= current_facts:
                        return sense_act, belief
                if missing_fact in self.sat_solver.unknown_facts:
                    # Not directly sensable -- either nowhere (wumpus `safe`,
                    # must be DEDUCED) or just not from the current position
                    # (replanning / plan-to-observe will route to a sensor).
                    indirect_unknown = True

        if indirect_unknown:
            # Gather information: sense an applicable sensing action whose value is
            # still unknown (e.g. breeze/stench at the current cell). The Z3
            # closure in observe() then deduces the indirect precondition.
            for name in sorted(self.dynamic_actions):
                sense_act = self.dynamic_actions[name]
                if (sense_act.is_sensing
                        and belief.known_value(sense_act.observe) is None
                        and sense_act.observe in self.sat_solver.unknown_facts
                        and sense_act.preconditions <= current_facts):
                    return sense_act, belief
            return None  # nothing left to sense -> cannot establish it -> replan

        # Remaining "missing" facts are static/non-sensable (e.g. same-block
        # checks recorded without polarity); the action is effectively applicable.
        return act, belief

    def _run_cpp_ff_plan(self, facts, goal=None):
        """Classical relaxation for an assumed world `facts`: the KT translation
        with no uncertainty (uncertain={}) yields a plain classical problem over
        the full contingent-grounded actions (sensing actions are dropped). Returns
        the plan as canonical grounded action names. ``goal`` overrides the problem
        goal (used by plan-to-observe to plan toward a sensing precondition)."""
        # actions_arg = [(a["name"], a["is_sensing"], a["observe"], a["pre"], a["add"], a["del"])
        #                for a in self.kt_action_info]
        actions_arg = [(a["name"], a["is_sensing"], a["observe"], a["pre"], a["add"], a["del"], a.get("cond", []))
                       for a in self.kt_action_info]
        domain, problem = cpor_engine.kt_translate(
            actions_arg, [], [], sorted(facts), self.goal_literals if goal is None else goal)
        raw = self._ff_solve_pddl(domain, problem)
        plan = []
        for step in raw:
            tok = step.lower().strip().split()
            if not tok:
                continue
            name = tok[0]
            if name.startswith("merge_") or name.startswith("ref_"):
                continue
            if name in self.dynamic_actions:
                plan.append(name)
            elif name in self.normalized_action_map:
                plan.append(self.normalized_action_map[name])
        return plan

class CPORMetaPlanner:
    # Loop-escape budget: how many times one signature may be re-entered on
    # the current path (each retry mutates the planning strategy) before the
    # branch is declared a DEAD END.
    MAX_LOOP_RETRIES = 4

    # Closed-node observation sets past this size make the node unmergeable
    # (see _strip_non_hidden); keeps the O(n,l) bookkeeping bounded. Must
    # comfortably exceed the longest sensing CHAIN in a domain (a chain of d
    # sensings legitimately accumulates ~d sets for its deepest deduced
    # literal -- doors15: 14 per column; capping below that poisons exactly
    # the reusable chain-entrance nodes).
    MAX_OBS_SETS_PER_LITERAL = 40

    def __init__(self, simulator, online_planner):
        self.online_planner = online_planner
        self.visited_beliefs = {}

        # Sec 5.4 belief-equivalence plan-graph compaction: reuse a
        # structurally- (and, after a regression-consistency check)
        # semantically-equivalent CLOSED node instead of only exact belief
        # signatures. Backed by the C++ ClosedNodeIndex (K(n)/H(n)/O(n,l)
        # bookkeeping, mirroring CPORLib's IsClosedState/UpdateClosedStates).
        # Restricted to simple domains, as the paper (and the C#) requires.
        self.compaction_enabled = getattr(online_planner, "is_simple_domain", False)
        self.closed_index = cpor_engine.ClosedNodeIndex()
        self._closed_node_graph = {}  # closed-node id -> the graph node it was built for
        self._node_info = {}          # id(graph node dict) -> its ClosedNodeInfo dict

        # base fluent -> its oneof group, for the factored observation-set
        # projection in _strip_non_hidden.
        self._obs_group_of = {}
        sat = getattr(online_planner, "sat_solver", None)
        if sat is not None:
            for group in getattr(sat, "oneof_groups", []):
                members = frozenset(group)
                for base in group:
                    self._obs_group_of[base] = members

        # Sec 5.7 ancestor-based cycle detection for non-deterministic domains,
        # backed by the C++ AncestorCycleGuard (mirrors the C#'s AlreadyVisited/
        # DetectInfiniteLoop(Complete)). Only meaningful (and only enabled) when
        # the domain actually has non-deterministic actions -- in deterministic
        # domains there can be no cycles (paper Sec 2.3).
        self.cycle_detection_enabled = getattr(online_planner, "has_nondet_actions", False)
        self.cycle_guard = cpor_engine.AncestorCycleGuard()
        self._ancestor_nodes = []  # parallel stack of node dicts, index = cycle_guard depth

        # Loop escaping (the C# StuckInLoopPlanBased analogue, with a working
        # escape): signatures of nodes OPEN on the current recursion path, with
        # a repeat count. Re-entering an open signature means the planner's
        # deterministic choice is cycling; instead of silently emitting a
        # cycle edge (an invalid plan in deterministic domains), re-choose via
        # get_next_action(retry=count), which mutates the strategy. Bounded by
        # MAX_LOOP_RETRIES, then the branch is an honest DEAD END.
        self._open_sigs = {}

    def make_initial_belief(self, initial_true_facts):
        """Build the root Belief: SAT-backed (no enumeration) for eligible
        simple domains, else by enumerating all valid initial models ONCE."""
        known_pos = {f for f in initial_true_facts if not f.startswith("NOT_")}
        if getattr(self.online_planner, "use_lazy_belief", False):
            return LazyBelief(self.online_planner.sat_solver, known_pos)
        models = self.online_planner.sat_solver.enumerate_models(list(initial_true_facts), 4000)

        worlds = []
        for m in models:
            worlds.append(known_pos | set(m))

        if not worlds:
            worlds = [known_pos]

        return RegressionBelief(self.online_planner.sat_solver, worlds)

    def _known_and_hidden(self, belief):
        """F(n) split into the full known-literal set (K-candidate universe)
        and the base names of fluents genuinely unknown at n (H-candidate
        universe), per Sec 5.1's belief-state definitions."""
        known = set(belief.current_true_facts())
        hidden = set()
        for f in self.online_planner.sat_solver.unknown_facts:
            value = belief.known_value(f)
            if value is True:
                known.add(f)
            elif value is False:
                known.add("NOT_" + f)
            else:
                hidden.add(f)
        return known, hidden

    def _find_equivalent_closed_node(self, belief, known, hidden):
        """Algorithm 3 (GetClosedNode): a structurally-compatible closed node
        (K(n') subseteq known, H(n') subseteq hidden) whose recorded
        observation-set reasoning still holds against this belief's possible
        worlds. Returns the closed-node id, or None."""
        if not self.compaction_enabled:
            return None
        for match in self.closed_index.find_candidates(list(known), list(hidden)):
            candidate = self.closed_index.info(match["id"])
            observation_sets = candidate["observation_sets"]
            if all(
                belief.literal_holds_after(obs_set, lit)
                for lit in match["pending_literals"]
                for obs_set in observation_sets.get(lit, [])
            ):
                return match["id"]
        return None

    def _register_closed_node(self, node, info):
        info = self._strip_non_hidden(info)
        node_id = self.closed_index.register(info)
        self._closed_node_graph[node_id] = node
        self._node_info[id(node)] = info

    def _strip_non_hidden(self, info):
        """update_sensing's ReasonedT/F fold treats ANY literal known in one
        branch but not the other as 'resolved by this observation' and puts it
        in H(n). But branch ROUTES also differ in static/dynamic facts (e.g. a
        route using door opened_p3-1 needs that static fact; the other route
        doesn't) -- those are never members of hidden(b), so one such entry
        makes the node permanently unmatchable in find_candidates. They already
        stay in K(n) (update_sensing unions both branches' K), where the
        K(n') <= known(b) check handles them exactly; H(n)/O(n,l) are only
        about genuinely-hidden reasoning, so restrict them to unknown facts."""
        sat = self.online_planner.sat_solver
        unknown = sat.unknown_facts
        hidden = set(h for h in info["hidden"] if h in unknown)
        # Every sensing fold prefix-copies the children's observation sets
        # (prefixed with the ancestor's own observation), so they grow
        # combinatorially with depth. In a FACTORED constraint system
        # (independent positive oneof groups) those foreign-group prefixes are
        # semantically inert: S entails l exactly through the literals of l's
        # own group. Projecting each set onto the group before deduping keeps
        # O(n,l) at ~group-size sets and is sound -- a projected set is a
        # subset of the original, so the merge-time entailment check only gets
        # HARDER to satisfy, never easier.
        factored = getattr(sat, "factored", False)
        observation_sets, overflowed = {}, False
        for lit, sets in info["observation_sets"].items():
            base = lit[4:] if lit.startswith("NOT_") else lit
            if base not in unknown:
                continue
            if factored:
                group = self._obs_group_of.get(base, (base,))
                unique = {tuple(x for x in s
                                if (x[4:] if x.startswith("NOT_") else x) in group)
                          for s in sets}
            else:
                unique = {tuple(s) for s in sets}
            if len(unique) > self.MAX_OBS_SETS_PER_LITERAL:
                # Cap overflow: this node's recorded reasoning for `lit` is too
                # rich to keep, so the node cannot prove it is reusable -- mark
                # it unmatchable. (Degrading `lit` to a K requirement instead
                # is NOT safe: the degrade hits one sibling branch and not the
                # other, and update_sensing reads any K asymmetry between
                # siblings as "resolved by this observation", recording a bogus
                # singleton observation set that then fails the semantic check
                # for every future belief and blocks ALL merges into every
                # ancestor -- the doors15 duplicate-subtree explosion.)
                overflowed = True
                hidden.discard(base)
            else:
                observation_sets[lit] = [list(s) for s in unique]
        # A literal with an observation set is resolved by reasoning INSIDE the
        # subtree; update_sensing nevertheless leaves it in K(n), where the
        # syntactic K(n') <= known(b) prefilter would demand every future
        # belief already know it -- defeating the semantic pending-literal
        # check and snowballing K through every fold until nothing matches.
        known = [k for k in info["known"] if k not in observation_sets]
        if overflowed and "__unmergeable__" not in known:
            # No belief ever knows this pseudo-literal, so find_candidates can
            # never return this node (nor any ancestor: K folds upward).
            known.append("__unmergeable__")
        return {"known": known, "hidden": sorted(hidden),
                "observation_sets": observation_sets}

    def _goal_closed_info(self, known):
        goal_literals = [name if pol else "NOT_" + name
                         for name, pol in self.online_planner.goal_literals]
        return cpor_engine.closed_node_goal([l for l in goal_literals if l in known])

    def build_plan_graph(self, belief, via_observation=False):
        # Worlds-based belief: RegressionBelief.worlds is the exact set of
        # possible worlds consistent with everything observed so far. A
        # sensing action expands both observation outcomes; an impossible
        # outcome (observe() -> None) is a DEAD END leaf, but since
        # get_next_action only senses currently-unknown fluents, both outcomes
        # are normally possible -> every reachable branch reaches the goal.
        #
        # `via_observation`: was the edge INTO this node (from build_plan_graph's
        # caller) a branching/information-revealing one -- a sensing observation
        # OR a non-deterministic outcome (Sec 5.7 treats learning which outcome
        # occurred as informative, same as an observation)? Used by the
        # ancestor-cycle check below.
        current_facts = belief.current_true_facts()

        # Sec 5.7 (Algorithm 5): for non-deterministic domains, check this
        # BEFORE the exact-signature memo -- belief-state repetition is the
        # exception, not the rule, in non-deterministic domains, so this is
        # the mechanism actually doing the cycle-avoidance work here, not a
        # backstop for the exact memo.
        if self.cycle_detection_enabled:
            ancestor_depth = self.cycle_guard.find_ancestor_match(sorted(current_facts))
            if ancestor_depth is not None and (via_observation or self.cycle_guard.safe_cycle(ancestor_depth)):
                node = self._ancestor_nodes[ancestor_depth]
                self.visited_beliefs[belief.signature()] = node
                return node

        bs_hash = belief.signature()
        open_count = self._open_sigs.get(bs_hash, 0)
        if bs_hash in self.visited_beliefs and open_count == 0:
            return self.visited_beliefs[bs_hash]

        if self.online_planner.is_goal_facts(current_facts):
            node = {"action": "GOAL REACHED", "children": []}
            self.visited_beliefs[bs_hash] = node
            if self.compaction_enabled:
                known, _hidden = self._known_and_hidden(belief)
                self._register_closed_node(node, self._goal_closed_info(known))
            return node

        known, hidden = self._known_and_hidden(belief)
        equivalent_id = self._find_equivalent_closed_node(belief, known, hidden)
        if equivalent_id is not None:
            node = self._closed_node_graph[equivalent_id]
            self.visited_beliefs[bs_hash] = node
            return node

        if open_count >= self.MAX_LOOP_RETRIES:
            return {"action": "DEAD END", "children": []}

        action, belief = self.online_planner.get_next_action(belief, retry=open_count)
        if action is None:
            return {"action": "DEAD END", "children": []}

        node = {"action": action.name, "children": []}
        if open_count == 0:
            # A retry rebuild must not displace the open ancestor that owns
            # this signature in the memo.
            self.visited_beliefs[bs_hash] = node
        is_nondet = getattr(action, "is_nondet", False)

        if self.cycle_detection_enabled:
            self.cycle_guard.push(sorted(current_facts), via_observation)
            self._ancestor_nodes.append(node)

        self._open_sigs[bs_hash] = open_count + 1
        try:
            if action.is_sensing:
                obs = action.observe
                # Branch continuity (mirrors the C#'s plan-suffix reuse at
                # observation splits). Two cases:
                #  - EMBEDDED sensing (the committed plan's own head, from a KT
                #    plan): the suffix stays valid exactly on the branch
                #    matching the outcome the plan assumed (belief.expect[0],
                #    simulated from the sample world at determinize time);
                #    the other branch clears and replans.
                #  - INJECTED sensing (_follow_committed_plan sensing an
                #    unknown precondition; the plan head is a later action):
                #    keep the plan on BOTH branches -- the follow logic itself
                #    replans when the observation contradicts a needed
                #    precondition, and re-uses the plan when it holds.
                in_plan = bool(belief.plan) and belief.plan[0] == action.name
                expected = belief.expect[0] if in_plan else None
                child_info = {}
                for value, label, key in ((obs, f"Observed: {obs} == True", True),
                                           (f"NOT_{obs}", f"Observed: {obs} == False", False)):
                    next_belief = belief.observe(value)
                    if next_belief is None:
                        child = {"action": "DEAD END", "children": []}
                    else:
                        if in_plan:
                            if expected is not None and key == expected:
                                next_belief = next_belief.with_plan(
                                    next_belief.plan[1:], next_belief.expect[1:])
                            else:
                                next_belief = next_belief.with_plan(())
                        child = self.build_plan_graph(next_belief, via_observation=True)
                    node["children"].append((label, child))
                    child_info[key] = self._node_info.get(id(child))

                if self.compaction_enabled and not is_nondet and child_info[True] and child_info[False]:
                    info = cpor_engine.closed_node_update_sensing(
                        child_info[True], child_info[False], list(action.preconditions), obs)
                    self._register_closed_node(node, info)

            elif is_nondet:
                # Sec 5.7: branch once per possible outcome, exactly like a
                # sensing observation branches once per possible value. No
                # Sec 5.4 compaction here -- that mechanism is restricted to
                # simple (deterministic) domains.
                for idx in range(len(action.nondet_outcomes)):
                    next_belief = belief.progress_nondet(action, idx).with_plan(())
                    child = self.build_plan_graph(next_belief, via_observation=True)
                    node["children"].append((f"Outcome {idx}", child))

            else:
                child = self.build_plan_graph(belief.progress(action), via_observation=False)
                node["children"].append(("Deterministically", child))

                if self.compaction_enabled and not is_nondet:
                    child_info = self._node_info.get(id(child))
                    if child_info is not None:
                        effects = list(action.add_effects) + ["NOT_" + d for d in action.del_effects]
                        info = cpor_engine.closed_node_update_action(
                            child_info, list(action.preconditions), effects)
                        self._register_closed_node(node, info)
        finally:
            if open_count == 0:
                self._open_sigs.pop(bs_hash, None)
            else:
                self._open_sigs[bs_hash] = open_count
            if self.cycle_detection_enabled:
                self.cycle_guard.pop()
                self._ancestor_nodes.pop()

        return node