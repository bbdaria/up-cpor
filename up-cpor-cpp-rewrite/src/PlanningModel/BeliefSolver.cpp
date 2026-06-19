#include "PlanningModel/BeliefSolver.h"

BeliefSolver::BeliefSolver() : ctx_(), solver_(ctx_) {}

void BeliefSolver::split_literal(const std::string& lit, std::string& base, bool& positive) {
    if (lit.rfind("NOT_", 0) == 0) {
        positive = false;
        base = lit.substr(4);
    } else {
        positive = true;
        base = lit;
    }
}

z3::expr BeliefSolver::get_var(const std::string& name) {
    auto it = var_index_.find(name);
    if (it != var_index_.end()) return vars_[it->second];
    z3::expr e = ctx_.bool_const(name.c_str());
    var_index_[name] = static_cast<unsigned>(vars_.size());
    vars_.push_back(e);
    return e;
}

z3::expr BeliefSolver::literal_expr(const std::string& lit) {
    std::string base;
    bool positive;
    split_literal(lit, base, positive);
    z3::expr v = get_var(base);
    return positive ? v : (!v);
}

void BeliefSolver::add_oneof(const std::vector<std::string>& literals) {
    if (literals.empty()) return;
    z3::expr_vector lits(ctx_);
    for (const auto& l : literals) lits.push_back(literal_expr(l));
    // at least one
    solver_.add(z3::mk_or(lits));
    // at most one (pairwise mutual exclusion)
    for (unsigned i = 0; i < lits.size(); ++i) {
        for (unsigned j = i + 1; j < lits.size(); ++j) {
            solver_.add(!lits[i] || !lits[j]);
        }
    }
}

void BeliefSolver::add_clause(const std::vector<std::string>& literals) {
    if (literals.empty()) return;
    z3::expr_vector lits(ctx_);
    for (const auto& l : literals) lits.push_back(literal_expr(l));
    solver_.add(z3::mk_or(lits));
}

void BeliefSolver::set_unknown(const std::vector<std::string>& names) {
    unknown_ = names;
    for (const auto& n : names) get_var(n);  // ensure a var exists for each
}

z3::expr_vector BeliefSolver::assumptions_for(const std::vector<std::string>& facts) {
    z3::expr_vector assumptions(ctx_);
    for (const auto& f : facts) {
        std::string base;
        bool positive;
        split_literal(f, base, positive);
        // Only literals over constraint variables affect reasoning; ignore
        // unrelated static facts (e.g. same_b1_b1) so we don't create free vars.
        if (var_index_.count(base)) {
            assumptions.push_back(literal_expr(f));
        }
    }
    return assumptions;
}

bool BeliefSolver::is_consistent(const std::vector<std::string>& facts) {
    z3::expr_vector assumptions = assumptions_for(facts);
    return solver_.check(assumptions) != z3::unsat;
}

std::optional<std::vector<std::string>> BeliefSolver::complete(const std::vector<std::string>& facts) {
    z3::expr_vector assumptions = assumptions_for(facts);
    if (solver_.check(assumptions) == z3::unsat) {
        return std::nullopt;
    }
    z3::model m = solver_.get_model();
    std::vector<std::string> true_facts;
    for (const auto& kv : var_index_) {
        z3::expr value = m.eval(vars_[kv.second], /*model_completion=*/true);
        if (value.is_true()) true_facts.push_back(kv.first);
    }
    return true_facts;
}

std::optional<std::set<std::string>> BeliefSolver::propagate(const std::vector<std::string>& facts) {
    std::set<std::string> closure(facts.begin(), facts.end());

    z3::expr_vector assumptions = assumptions_for(facts);
    if (solver_.check(assumptions) == z3::unsat) {
        return std::nullopt;  // contradiction
    }

    // Entailment is complete under Z3, so a single pass over the unknowns yields
    // the full closure (no fixpoint iteration needed).
    for (const auto& u : unknown_) {
        if (closure.count(u) || closure.count("NOT_" + u)) continue;
        z3::expr v = get_var(u);

        z3::expr_vector test_true(ctx_);
        for (unsigned i = 0; i < assumptions.size(); ++i) test_true.push_back(assumptions[i]);
        test_true.push_back(!v);
        if (solver_.check(test_true) == z3::unsat) {  // facts entail v
            closure.insert(u);
            continue;
        }

        z3::expr_vector test_false(ctx_);
        for (unsigned i = 0; i < assumptions.size(); ++i) test_false.push_back(assumptions[i]);
        test_false.push_back(v);
        if (solver_.check(test_false) == z3::unsat) {  // facts entail !v
            closure.insert("NOT_" + u);
        }
    }

    return closure;
}
