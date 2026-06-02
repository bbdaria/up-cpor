#pragma once

#include <unordered_set>
#include <vector>
#include <memory>
#include "LogicalUtilities/Predicate.h"
#include "LogicalUtilities/Formula.h"
#include "PlanningModel/Domain.h"

class BeliefState : public std::enable_shared_from_this<BeliefState> {
private:
    PredicateSet observed_; // facts we know for certain about the world 
    std::vector<std::shared_ptr<Formula>> hidden_formulas_; // uncertainty about the world
    PredicateSet unknown_;

    std::shared_ptr<BeliefState> predecessor_;
    std::shared_ptr<PlanningAction> generating_action_;

public:
    BeliefState(); // initial belief state constructor
    BeliefState(const BeliefState& other);

    const PredicateSet& get_observed() const { return observed_; }
    const std::vector<std::shared_ptr<Formula>>& get_hidden_formulas() const { return hidden_formulas_; }
    const PredicateSet& get_unknown() const { return unknown_; }

    bool add_observed(std::shared_ptr<Predicate> p);
    void add_hidden_formula(std::shared_ptr<Formula> f);

    bool consistent_with(std::shared_ptr<Predicate> p) const;
    bool consistent_with(std::shared_ptr<Formula> f) const;

    void apply_reasoning();

    std::shared_ptr<BeliefState> apply(std::shared_ptr<PlanningAction> action);
};