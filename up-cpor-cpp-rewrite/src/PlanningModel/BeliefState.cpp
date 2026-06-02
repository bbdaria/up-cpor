#include "PlanningModel/BeliefState.h"
#include "LogicalUtilities/PredicateNode.h"
#include <iostream>

BeliefState::BeliefState() : predecessor_(nullptr), generating_action_(nullptr) {}

BeliefState::BeliefState(const BeliefState& other) 
    : observed_(other.observed_), unknown_(other.unknown_), 
      predecessor_(other.predecessor_), generating_action_(other.generating_action_) {
    for (const auto& f : other.hidden_formulas_) { // deep copy of formulas
        if (f) hidden_formulas_.push_back(f->clone());
    }
}

void BeliefState::add_hidden_formula(std::shared_ptr<Formula> f) {
    if (!f) return;
    hidden_formulas_.push_back(f);
    
    PredicateSet formula_preds;
    f->get_all_predicates(formula_preds);
    for (const auto& p : formula_preds) {
        if (observed_.find(p) == observed_.end() && observed_.find(p->negate()) == observed_.end()) {
            unknown_.insert(p);
        }
    }
}

bool BeliefState::add_observed(std::shared_ptr<Predicate> p) {
    if (!p) return false;
    if (observed_.find(p) != observed_.end()) return false; // fact already known

    auto negated = p->negate(); 
    observed_.erase(negated); // delete negation
    observed_.insert(p);

    unknown_.erase(p);
    unknown_.erase(negated);
    apply_reasoning();
    return true;
}

void BeliefState::apply_reasoning() {
    bool changed = true;
    
    while (changed) {
        changed = false;
        std::vector<std::shared_ptr<Formula>> new_hidden;
        for (auto& f : hidden_formulas_) {
            if (!f) continue;
            
            auto reduced_f = f->reduce(observed_);
            auto simplified_f = reduced_f->simplify();
            
            if (simplified_f->get_type() == Formula::Type::Predicate) {
                auto pred_node = std::static_pointer_cast<PredicateNode>(simplified_f);
                auto pred = pred_node->get_predicate();
                
                if (pred->get_name() != "TRUE" && pred->get_name() != "FALSE") {
                    if (observed_.find(pred) == observed_.end()) {
                        auto negated = pred->negate();
                        observed_.erase(negated);
                        observed_.insert(pred);
                        unknown_.erase(pred);
                        unknown_.erase(negated);
                        changed = true;
                    }
                }
            } 

            else if (simplified_f->to_string() != "(TRUE)") {
                new_hidden.push_back(simplified_f);
            }
        }
        hidden_formulas_.swap(new_hidden);
    }
}