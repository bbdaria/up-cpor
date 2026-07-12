#pragma once

#include <z3++.h>

#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Z3-backed reasoner over the *initial-state* uncertainty constraints of a
// contingent problem (the (unknown ...), (oneof ...) and (or ...) declarations).
//
// Literals are strings using the engine's convention: "on_b2_b3" for a positive
// fact, "NOT_on_b2_b3" for its negation. This replaces the previous hand-rolled
// regex unit-propagation with complete SAT entailment (so it handles arbitrary
// (or ...) clauses, not just oneof unit propagation).
class BeliefSolver {
public:
    BeliefSolver();

    // Exactly-one-of the given literals (PDDL oneof).
    void add_oneof(const std::vector<std::string>& literals);
    // Disjunction of the given literals (PDDL or).
    void add_clause(const std::vector<std::string>& literals);
    // Register the fact names whose truth value is initially unknown.
    void set_unknown(const std::vector<std::string>& names);

    // Given a set of literals known to hold, return the logical closure
    // (input literals plus every unknown literal entailed by them under the
    // constraints), or nullopt if the literals are inconsistent with the
    // constraints.
    std::optional<std::set<std::string>> propagate(const std::vector<std::string>& facts);

    // True iff the given literals are jointly satisfiable with the constraints.
    bool is_consistent(const std::vector<std::string>& facts);

    // One satisfying assignment consistent with `facts`: the constraint variables
    // that are TRUE in the model. nullopt if unsatisfiable. Used to plan for a
    // single coherent possible world rather than a contradictory all-true guess.
    std::optional<std::vector<std::string>> complete(const std::vector<std::string>& facts);

    // True iff `base_facts` entails `literal` under the constraints (i.e. every
    // model of base_facts also satisfies literal). Used by the plan-graph
    // belief-equivalence check (paper Sec 5.4, Algorithm 3 lines 8-11): does
    // observing a candidate node's recorded observation sequence, regressed to
    // this initial layer, still force the same relevant hidden literal?
    bool implies(const std::vector<std::string>& base_facts, const std::string& literal);

private:
    z3::context ctx_;
    z3::solver solver_;
    std::unordered_map<std::string, unsigned> var_index_;
    std::vector<z3::expr> vars_;
    std::vector<std::string> unknown_;

    z3::expr get_var(const std::string& name);
    z3::expr literal_expr(const std::string& lit);
    z3::expr_vector assumptions_for(const std::vector<std::string>& facts);
    static void split_literal(const std::string& lit, std::string& base, bool& positive);
};
