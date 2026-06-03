#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include "../LogicalUtilities/Formula.h"
#include "../LogicalUtilities/Predicate.h"

class Action {
public:
    std::string name;
    std::shared_ptr<Formula> precondition;
    std::vector<std::shared_ptr<Predicate>> add_effects;
    std::vector<std::shared_ptr<Predicate>> del_effects;
    std::shared_ptr<Predicate> observe; // For sensing actions

    Action(std::string n) : name(n), precondition(nullptr), observe(nullptr) {}

    bool is_applicable(const PredicateSet& state) const {
        if (!precondition) return true;
        return precondition->is_true(state);
    }
    void apply(PredicateSet& state) const {
        for (const auto& p : del_effects) {
            state.erase(p);
        }
        for (const auto& p : add_effects) {
            state.insert(p);
        }
    }

    bool is_sensing() const {
        return observe != nullptr;
    }
};