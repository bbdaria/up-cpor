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

        oneof = list(getattr(problem, "oneof_constraints", []))
        self.oneof_members = {_literal_to_str(x) for group in oneof for x in group}
        or_constraints = list(getattr(problem, "or_constraints", []))
        hidden = getattr(problem, "hidden_fluents", [])

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
        None if they are inconsistent."""
        result = self._solver.propagate(list(current_facts))
        return None if result is None else set(result)

    def complete(self, current_facts):
        """One consistent full assignment (set of true constraint-fluent names)
        agreeing with current_facts, or None if inconsistent."""
        result = self._solver.complete(list(current_facts))
        return None if result is None else set(result)
    
    def add_hidden_clause(self, new_hidden_fact, clause):
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

    def __init__(self, sat_solver, worlds, history=(), plan=()):
        self.sat = sat_solver
        self.worlds = frozenset(frozenset(w) for w in worlds)
        self.history = tuple(history)
        self.plan = tuple(plan)

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
        new_plan = self.plan[1:] if (self.plan and self.plan[0] == action.name) else self.plan
        new_worlds = [action.apply(w) for w in self.worlds]
        return RegressionBelief(self.sat, new_worlds, self.history + (action,), new_plan)

    def progress_nondet(self, action, outcome_idx):
        """Belief after executing a non-deterministic action (Sec 5.7) whose
        outcome turned out to be `outcome_idx`. The meta-planner branches once
        per outcome (like a sensing observation), so within this branch the
        chosen outcome is applied uniformly to every currently-possible world."""
        new_plan = self.plan[1:] if (self.plan and self.plan[0] == action.name) else self.plan
        new_worlds = [action.apply_outcome(w, outcome_idx) for w in self.worlds]
        return RegressionBelief(self.sat, new_worlds, self.history + (action,), new_plan)

    def observe(self, literal):
        neg = literal.startswith("NOT_")
        base = literal[4:] if neg else literal
        new_worlds = []
        for w in self.worlds:
            if (base in w) if not neg else (base not in w):
                new_worlds.append(w)
        if not new_worlds:
            return None
        return RegressionBelief(self.sat, new_worlds, self.history, self.plan)

    def with_plan(self, plan):
        return RegressionBelief(self.sat, self.worlds, self.history, plan)

    def signature(self):
        return (self.worlds, self.plan)

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
        self.sensing_map = {}
        for name, act in self.dynamic_actions.items():
            if act.is_sensing and act.observe:
                self.sensing_map[act.observe] = name

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

    def get_next_action(self, belief):
        """Return (action, belief) where belief may carry a freshly-committed plan.
        Returns (None, belief) at the goal or a genuine dead end."""
        current_facts = belief.current_true_facts()

        if self.is_goal_facts(current_facts):
            return None, belief

        # 1. Follow the committed plan if there is one and it is still valid.
        chosen = self._follow_committed_plan(belief, current_facts)
        if chosen is not None:
            return chosen

        # 2. Determinize via the KT (knowledge) translation -- only for domains
        #    that need deduced (not directly sensed) hidden preconditions.
        plan = self._kt_determinize(belief) if self.needs_kt else []
        if plan:
            return self._follow_committed_plan(belief.with_plan(tuple(plan)), current_facts) \
                or (None, belief)

        # 2b. Fallback: consistent world (Z3 model) progressed through history.
        full_state = list(belief.worlds)[0] if belief.worlds else frozenset()
        plan = self._ff_plan_cached(frozenset(full_state))
        if not plan:
            optimistic = set(belief.current_true_facts())
            for f in self.sat_solver.unknown_facts:
                if belief.known_value(f) is None:
                    optimistic.add(f)
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
        
        # New Explicit World Tracking logic: just pick a valid progressed world
        if not belief.worlds:
            return None
        full_state = list(belief.worlds)[0]

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
        worlds = list(belief.worlds)
        if not worlds or len(worlds) > self.MODEL_ENUM_CAP:
            return []

        union = set().union(*worlds)
        uncertain = {f for f in union if any(f in w for w in worlds) and any(f not in w for w in worlds)}

        if not uncertain:
            return self._run_cpp_ff_plan(worlds[0])

        known_true = set.intersection(*[set(w) for w in worlds])
        seen, tags = set(), []
        for w in worlds:
            t = frozenset(w & uncertain)
            if t not in seen:
                seen.add(t)
                tags.append(t)

        if len(tags) > self.KT_MAX_TAGS:
            return []

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
        return plan

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
                if missing_fact in self.sensing_map:
                    return self.dynamic_actions[self.sensing_map[missing_fact]], belief
                if missing_fact in self.sat_solver.unknown_facts:
                    # Unknown but not directly sensable (e.g. wumpus `safe`): it has
                    # to be DEDUCED from other observations via the constraints.
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

        # Sec 5.7 ancestor-based cycle detection for non-deterministic domains,
        # backed by the C++ AncestorCycleGuard (mirrors the C#'s AlreadyVisited/
        # DetectInfiniteLoop(Complete)). Only meaningful (and only enabled) when
        # the domain actually has non-deterministic actions -- in deterministic
        # domains there can be no cycles (paper Sec 2.3).
        self.cycle_detection_enabled = getattr(online_planner, "has_nondet_actions", False)
        self.cycle_guard = cpor_engine.AncestorCycleGuard()
        self._ancestor_nodes = []  # parallel stack of node dicts, index = cycle_guard depth

    def make_initial_belief(self, initial_true_facts):
        """Build the root Belief by enumerating all valid initial models ONCE."""
        known_pos = {f for f in initial_true_facts if not f.startswith("NOT_")}
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

    @staticmethod
    def _literal_holds_after(worlds, extra_literals, literal):
        """Algorithm 3 lines 8-11: would `literal` hold in every one of `worlds`
        consistent with also observing every literal in `extra_literals`?
        Vacuously True if no world survives the filter (that observation
        combination cannot occur along this branch, so it cannot violate the
        match either)."""
        def matches(world, lit):
            neg = lit.startswith("NOT_")
            base = lit[4:] if neg else lit
            return (base not in world) if neg else (base in world)

        filtered = [w for w in worlds if all(matches(w, o) for o in extra_literals)]
        if not filtered:
            return True
        return all(matches(w, literal) for w in filtered)

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
                self._literal_holds_after(belief.worlds, obs_set, lit)
                for lit in match["pending_literals"]
                for obs_set in observation_sets.get(lit, [])
            ):
                return match["id"]
        return None

    def _register_closed_node(self, node, info):
        node_id = self.closed_index.register(info)
        self._closed_node_graph[node_id] = node
        self._node_info[id(node)] = info

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
        if bs_hash in self.visited_beliefs:
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

        action, belief = self.online_planner.get_next_action(belief)
        if action is None:
            return {"action": "DEAD END", "children": []}

        node = {"action": action.name, "children": []}
        self.visited_beliefs[bs_hash] = node
        is_nondet = getattr(action, "is_nondet", False)

        if self.cycle_detection_enabled:
            self.cycle_guard.push(sorted(current_facts), via_observation)
            self._ancestor_nodes.append(node)

        try:
            if action.is_sensing:
                obs = action.observe
                child_info = {}
                for value, label, key in ((obs, f"Observed: {obs} == True", True),
                                           (f"NOT_{obs}", f"Observed: {obs} == False", False)):
                    next_belief = belief.observe(value)
                    if next_belief is None:
                        child = {"action": "DEAD END", "children": []}
                    else:
                        # SDR: replan at every observation. Clear the committed plan so
                        # each branch re-determinizes from its own belief, instead of
                        # following the sample world's plan into a diverging branch
                        # (which caused re-sensing and impossible-branch dead ends).
                        child = self.build_plan_graph(next_belief.with_plan(()), via_observation=True)
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
            if self.cycle_detection_enabled:
                self.cycle_guard.pop()
                self._ancestor_nodes.pop()

        return node