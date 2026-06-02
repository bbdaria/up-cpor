#pragma once

#include <unordered_set>
#include <vector>
#include <memory>
// #include <utility>
#include "LogicalUtilities/Predicate.h"
#include "PlanningModel/Domain.h"
#include "LogicalUtilities/Predicate.h"

class State : public std::enable_shared_from_this<State> {
private:
    PredicateSet predicates_; // facts currently known to be true in this state
    bool maintain_negations_; // open or closed world assumption
    std::shared_ptr<State> predecessor_;
    std::shared_ptr<PlanningAction> generating_action_;

public:
    State(bool maintain_negations = true) // initial state constructor
        : maintain_negations_(maintain_negations), predecessor_(nullptr), generating_action_(nullptr) {}

    State(std::shared_ptr<State> predecessor, std::shared_ptr<PlanningAction> action) // progression constructor
        : predecessor_(predecessor), generating_action_(action), maintain_negations_(predecessor->maintain_negations_) {
        predicates_ = predecessor->get_predicates();
    }

    void add_predicate(std::shared_ptr<Predicate> p) {
        if (!maintain_negations_ && p->is_negated()) {
            return; 
        }
        auto negated = p->negate();
        predicates_.erase(negated);
        predicates_.insert(p);
    }

    bool contains(std::shared_ptr<Predicate> p) const {
        if (p->is_negated()) {
            if (!maintain_negations_) {
                auto positive = p->negate(); 
                return predicates_.find(positive) == predicates_.end(); 
            }
        }
        return predicates_.find(p) != predicates_.end();
    }

    void remove_negative_predicates() {
        PredicateSet filtered;
        for (const auto& p : predicates_) {
            if (!p->is_negated()) {
                filtered.insert(p);
            }
        }
        predicates_.swap(filtered);
        maintain_negations_ = false;
    }

    const PredicateSet& get_predicates() const {
        return predicates_;
    }
    
    std::shared_ptr<State> apply(std::shared_ptr<PlanningAction> action); 
};