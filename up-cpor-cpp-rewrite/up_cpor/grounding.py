"""Contingent-aware grounding.

The UP grounder prunes any action whose precondition references a fluent that is
never true in the initial state -- which wrongly drops actions guarded by HIDDEN
fluents (e.g. move requiring (opened ?j) / (safe ?j), sensed at run time). This
grounder instead grounds every action over its typed parameters and only filters
on STATIC, KNOWN predicates (those never appearing in any effect and not hidden,
e.g. adj / same): a static precondition is evaluated against the initial state
and, if satisfied, dropped; if violated, the grounding is discarded. Dynamic and
hidden preconditions are kept, so hidden-guarded actions survive.

Returns a list of action-info dicts:
    {name, is_sensing, observe, pre:[(fluent,polarity)], add:[fluent], del:[fluent]}
matching native_engine's kt_action_info / DynamicAction format.
"""
import itertools


def _fluent_name(fluent_exp, binding):
    """Grounded flat name of a fluent expression under a parameter binding."""
    args = []
    for a in fluent_exp.args:
        if a.is_parameter_exp():
            args.append(binding[a.parameter().name])
        elif a.is_object_exp():
            args.append(a.object().name)
        else:
            args.append(str(a))
    base = fluent_exp.fluent().name
    return base + ("_" + "_".join(args) if args else "")


def _collect_literals(node, binding, out):
    """Collect (predicate_name, grounded_fluent_name, polarity) from a condition."""
    if node.is_and():
        for c in node.args:
            _collect_literals(c, binding, out)
    elif node.is_not() and node.arg(0).is_fluent_exp():
        fl = node.arg(0)
        out.append((fl.fluent().name, _fluent_name(fl, binding), False))
    elif node.is_fluent_exp():
        out.append((node.fluent().name, _fluent_name(node, binding), True))


def _precond_literals(action):
    """Lifted (fluent_exp, polarity) literals of the action's precondition."""
    out = []
    for pre in action.preconditions:
        _collect_fnode_literals(pre, out)
    return out


def _collect_fnode_literals(node, out):
    if node.is_and():
        for c in node.args:
            _collect_fnode_literals(c, out)
    elif node.is_not() and node.arg(0).is_fluent_exp():
        out.append((node.arg(0), False))
    elif node.is_fluent_exp():
        out.append((node, True))


def ground_actions(problem, nondet_effects=None):
    """`nondet_effects`: optional {lifted_action_name: [outcome_1, outcome_2, ...]}
    where each outcome is a list of (ground_fluent_name, is_add) literals (Sec
    5.7's non-deterministic actuation actions, psi as an explicit outcome list
    rather than a DNF formula). There is no PDDL/unified_planning pathway for
    non-deterministic effects (confirmed: the pinned PDDLReader only parses
    `oneof` for initial-state uncertainty), so this is a side-channel supplied
    directly by the caller for actions built via the UP Python API rather than
    parsed from PDDL text. Outcomes are used AS-IS (no parameter binding), so
    this only supports UNPARAMETERIZED non-deterministic actions.
    """
    nondet_effects = nondet_effects or {}
    effect_preds = set()
    for a in problem.actions:
        for e in a.effects:
            effect_preds.add(e.fluent.fluent().name)

    hidden_preds = {hf.fluent().name for hf in getattr(problem, "hidden_fluents", [])
                    if not hf.is_not()}
    all_preds = {f.name for f in problem.fluents}
    # Filterable = static (never an effect) AND fully known (not hidden).
    static_filter = (all_preds - effect_preds) - hidden_preds

    init_true = set()
    static_facts = {}  # static predicate name -> set of object-name tuples true in init
    for fn, v in problem.initial_values.items():
        if v.is_true():
            init_true.add(_fluent_name(fn, {}))
            pred = fn.fluent().name
            if pred in static_filter:
                static_facts.setdefault(pred, set()).add(
                    tuple(a.object().name for a in fn.args))

    infos = []
    for action in problem.actions:
        observed = list(getattr(action, "observed_fluents", []) or [])
        is_sensing = len(observed) > 0
        params = action.parameters
        param_names = [p.name for p in params]

        # Fast path: if a static-positive precondition's args are exactly the
        # action's parameters (each once), enumerate only its true instances
        # (e.g. adj(?i,?j)) instead of the full type cross-product.
        bindings = None
        for fexp, pol in _precond_literals(action):
            if (pol and fexp.fluent().name in static_filter
                    and all(a.is_parameter_exp() for a in fexp.args)
                    and {a.parameter().name for a in fexp.args} == set(param_names)
                    and len(fexp.args) == len(param_names)):
                arg_params = [a.parameter().name for a in fexp.args]
                bindings = [dict(zip(arg_params, objs))
                            for objs in static_facts.get(fexp.fluent().name, ())]
                break
        if bindings is None:
            param_objs = [[o.name for o in problem.objects(p.type)] for p in params]
            combos = itertools.product(*param_objs) if params else [()]
            bindings = (dict(zip(param_names, c)) for c in combos)

        for binding in bindings:
            lits = []
            for pre in action.preconditions:
                _collect_literals(pre, binding, lits)

            ok = True
            dyn_pre = []
            for pred, gname, pol in lits:
                if pred in static_filter:
                    satisfied = (gname in init_true) if pol else (gname not in init_true)
                    if not satisfied:
                        ok = False
                        break
                else:
                    dyn_pre.append((gname, pol))
            if not ok:
                continue

            observe = _fluent_name(observed[0], binding) if is_sensing else None
            suffix = "_" + "_".join(binding[p.name] for p in params) if params else ""

            if action.name in nondet_effects:
                # Sec 5.7: non-deterministic actuation action -- outcomes are
                # supplied as-is (see the nondet_effects docstring above), not
                # computed from action.effects (UP has no representation for
                # this, so action.effects is expected to be empty/unused here).
                infos.append({
                    "name": action.name + suffix,
                    "is_sensing": is_sensing,
                    "observe": observe,
                    "pre": dyn_pre,
                    "add": [],
                    "del": [],
                    "cond": [],
                    "nondet": [list(outcome) for outcome in nondet_effects[action.name]],
                })
                continue

            add, dele, cond = [], [], []
            for e in action.effects:
                gname = _fluent_name(e.fluent, binding)
                if e.condition.is_true():
                    (add if e.value.is_true() else dele).append(gname)
                else:
                    cond_lits = []
                    _collect_literals(e.condition, binding, cond_lits)
                    ground_cond = [(g, pol) for (_, g, pol) in cond_lits]
                    cond.append((ground_cond, gname, e.value.is_true()))

            infos.append({
                "name": action.name + suffix,
                "is_sensing": is_sensing,
                "observe": observe,
                "pre": dyn_pre,
                "add": add,
                "del": dele,
                "cond": cond,
                "nondet": None,
            })
    return infos
