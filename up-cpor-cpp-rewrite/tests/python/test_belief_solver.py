"""Tests for the Z3-backed BeliefSolver exposed via cpor_engine."""
import cpor_engine


def _blocks3_solver():
    s = cpor_engine.BeliefSolver()
    for group in [
        ["on-table_b3", "on_b3_b2"],
        ["on-table_b2", "on_b2_b3"],
        ["clear_b3", "on_b2_b3"],
        ["clear_b2", "on_b3_b2"],
        ["clear_b3", "clear_b2"],
        ["on-table_b3", "on-table_b2"],
    ]:
        s.add_oneof(group)
    s.add_clause(["NOT_on_b3_b2", "NOT_on_b2_b3"])
    s.set_unknown(
        ["on-table_b3", "clear_b3", "on_b3_b2", "on-table_b2", "clear_b2", "on_b2_b3"]
    )
    return s


def test_oneof_mutual_exclusion():
    s = cpor_engine.BeliefSolver()
    s.add_oneof(["a", "b"])
    s.set_unknown(["a", "b"])
    assert s.is_consistent(["a"]) is True
    assert s.is_consistent(["a", "b"]) is False  # at most one


def test_oneof_at_least_one():
    s = cpor_engine.BeliefSolver()
    s.add_oneof(["a", "b"])
    s.set_unknown(["a", "b"])
    # both false violates "at least one"
    assert s.is_consistent(["NOT_a", "NOT_b"]) is False


def test_clause_forbids_both():
    s = _blocks3_solver()
    assert s.is_consistent(["on_b2_b3", "on_b3_b2"]) is False


def test_propagation_closure():
    s = _blocks3_solver()
    closure = s.propagate(["on_b2_b3"])
    assert closure is not None
    assert set(closure) == {
        "on_b2_b3",
        "NOT_on_b3_b2",
        "NOT_on-table_b2",
        "NOT_clear_b3",
        "clear_b2",
        "on-table_b3",
    }


def test_propagation_detects_contradiction():
    s = _blocks3_solver()
    assert s.propagate(["on_b2_b3", "on_b3_b2"]) is None


def test_unrelated_facts_preserved_in_closure():
    s = _blocks3_solver()
    closure = s.propagate(["on_b2_b3", "same_b1_b1"])
    assert closure is not None
    # a static fact not mentioned in any constraint is passed through unchanged
    assert "same_b1_b1" in closure
