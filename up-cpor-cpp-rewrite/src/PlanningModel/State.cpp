#include "LogicalUtilities/Predicate.h"
#include "PlanningModel/State.h"
#include "PlanningModel/Domain.h" 
#include "LogicalUtilities/Formula.h"
#include <iostream>

std::shared_ptr<State> State::apply(std::shared_ptr<PlanningAction> action) {
    if (!action) return nullptr;

    // check if preconditions are satisfied in the current state
    if (action->precondition != nullptr) {
        if (!action->precondition->is_true(predicates_, maintain_negations_)) {
            return nullptr; 
        }
    }

    // create new state with this as predecessor and the action that generated it
    auto new_state = std::make_shared<State>(shared_from_this(), action);

    // apply the effects of the action to the new state
    if (action->effect != nullptr) {
        PredicateSet conditional_preds;
        PredicateSet non_conditional_preds;
        action->effect->get_all_effect_predicates(conditional_preds, non_conditional_preds);

        for (const auto& p : non_conditional_preds) {
            new_state->add_predicate(p);
        }
    }

    // close world assumption optimization
    if (!new_state->maintain_negations_) {
        new_state->remove_negative_predicates();
    }
    return new_state;
}