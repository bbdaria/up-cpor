#include "Predicate.h"
#include "PredicateNode.h"
#include <stdexcept>
#include <algorithm>

PredicateNode::PredicateNode(std::shared_ptr<Predicate> predicate)
    : predicate_(std::move(predicate)) {
    this->size_ = 1;
}

static bool contains_predicate(const PredicateSet& collection, const Predicate& target) {
    for (const auto& item : collection) {
        if (item && *item == target) return true;
    }
    return false;
}

bool PredicateNode::is_true(const PredicateSet& known, bool contains_negations) const {
    if (!predicate_) return false;

    if (!contains_negations) {
        // Closed-World Assumption
        if (predicate_->is_negated()) {
            // If the node is (not clear(b2)), it is TRUE if clear(b2) is absent.
            auto positive = std::make_shared<Predicate>(predicate_->get_name(), predicate_->get_arguments(), false);
            return !contains_predicate(known, *positive);
        } else {
            return contains_predicate(known, *predicate_);
        }
    }
    
    // Partial Observability
    return contains_predicate(known, *predicate_);
}

bool PredicateNode::is_false(const PredicateSet& known, bool contains_negations) const {
    if (!predicate_) return true;

    if (!contains_negations) {
        // Closed-World Assumption: absence implies falsity
        if (predicate_->is_negated()) {
            // If the node is (not clear(b2)), it is FALSE if clear(b2) is present.
            auto positive = std::make_shared<Predicate>(predicate_->get_name(), predicate_->get_arguments(), false);
            return contains_predicate(known, *positive);
        } else {
            // If it's a positive predicate, it is FALSE if it is absent.
            return !contains_predicate(known, *predicate_);
        }
    }

    // Partial Observability: explicit negations required
    auto negated = predicate_->negate();
    if (!negated) return false;
    return contains_predicate(known, *negated);
}

bool PredicateNode::is_true_delete_relaxation(const PredicateSet& known) const {
    if (predicate_->is_negated()) return true;
    return is_true(known, false);
}

std::shared_ptr<Formula> PredicateNode::ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) {
    return clone(); 
}

std::shared_ptr<Formula> PredicateNode::partially_ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::negate() {
    return std::make_shared<PredicateNode>(predicate_->negate());
}

void PredicateNode::get_all_predicates(PredicateSet& predicates) const {
    predicates.insert(predicate_);
}

void PredicateNode::get_all_effect_predicates(PredicateSet& conditional_predicates, 
                                               PredicateSet& non_conditional_predicates) const {
    non_conditional_predicates.insert(predicate_);
}

std::shared_ptr<Formula> PredicateNode::to_cnf() {
    return clone(); 
}

std::shared_ptr<Formula> PredicateNode::clone() const {
    return std::make_shared<PredicateNode>(predicate_);
}

bool PredicateNode::contained_in(const PredicateSet& predicates, bool contains_negations) const {
    if (!contains_negations) {
        if (predicate_->is_negated()) {
            return true;
        } else {
            return contains_predicate(predicates, *predicate_);
        }
    }
    
    if (contains_predicate(predicates, *predicate_)) return true;
    if (contains_predicate(predicates, *(predicate_->negate()))) return false;
    
    return false;
}

std::shared_ptr<Formula> PredicateNode::replace(std::shared_ptr<Formula> org_f, std::shared_ptr<Formula> new_f) {
    if (this == org_f.get()) {
        return new_f;
    }
    return clone();
}

std::shared_ptr<Formula> PredicateNode::simplify() {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::regress(std::shared_ptr<PlanningAction> a, const PredicateSet& observed) {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::regress(std::shared_ptr<PlanningAction> a) {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::reduce(const PredicateSet& known) {
    if (contains_predicate(known, *predicate_)) {
        return std::make_shared<PredicateNode>(Utilities::TRUE_PREDICATE);
    }
    if (contains_predicate(known, *(predicate_->negate()))) {
        return std::make_shared<PredicateNode>(Utilities::FALSE_PREDICATE);
    }
    return clone();
}

void PredicateNode::get_all_optional_predicates(PredicateSet& predicates) const {
}

std::shared_ptr<Formula> PredicateNode::create_regression(std::shared_ptr<Predicate> pred, int choice) {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::generate_given(const std::string& tag, const std::vector<std::string>& always_known) {
    if (std::find(always_known.begin(), always_known.end(), predicate_->get_name()) != always_known.end()) {
        return clone();
    }
    return std::make_shared<PredicateNode>(predicate_->generate_given(tag));
}

std::shared_ptr<Formula> PredicateNode::add_time(int time) {
    throw std::runtime_error("add_time not implemented for PredicateNode");
}

std::shared_ptr<Formula> PredicateNode::replace_negative_effects_in_condition() {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::remove_impossible_options(const PredicateSet& observed) {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::apply_known(const PredicateSet& known) {
    return reduce(known);
}

std::vector<std::shared_ptr<Predicate>> PredicateNode::get_non_deterministic_effects() {
    return std::vector<std::shared_ptr<Predicate>>();
}

std::shared_ptr<Formula> PredicateNode::remove_universal_quantifiers(const std::vector<std::shared_ptr<Constant>>& constants, 
                                                              const std::vector<std::shared_ptr<Predicate>>& constant_predicates, 
                                                              std::shared_ptr<Domain> d) {
    return clone();
}

std::shared_ptr<Formula> PredicateNode::get_knowledge_formula(const std::vector<std::string>& always_known, bool know_whether) {
    if (std::find(always_known.begin(), always_known.end(), predicate_->get_name()) != always_known.end()) {
        return std::make_shared<PredicateNode>(Utilities::TRUE_PREDICATE);
    }
    if (know_whether) {
        return std::make_shared<PredicateNode>(Predicate::generate_know_whether_predicate(predicate_));
    }
    return std::make_shared<PredicateNode>(Predicate::generate_know_predicate(predicate_));
}

std::shared_ptr<Formula> PredicateNode::reduce_conditions(const PredicateSet& known) {
    return reduce(known);
}

std::shared_ptr<Formula> PredicateNode::remove_negations() {
    if (predicate_->is_negated()) {
        return nullptr;
    }
    return clone();
}

std::string PredicateNode::to_string() const {
    return predicate_->get_signature();
}