#include "AndNode.h"
#include "OrNode.h"
#include "NotNode.h"
#include "Tools/Utilities.h"
#include "PredicateNode.h"
#include <sstream>
#include <algorithm>

AndNode::AndNode(FormulaList operands) : operands_(std::move(operands)) {
    this->size_ = 1;
    for (const auto& op : operands_) {
        if (op) this->size_ += op->get_size();
    }
}

void AndNode::add_operand(FormulaPtr operand) {
    if (operand) {
        this->size_ += operand->get_size();
        operands_.push_back(std::move(operand));
    }
}

bool AndNode::is_true(const std::unordered_set<std::shared_ptr<Predicate>>& known, bool contains_negations) const {
    for (const auto& op : operands_) {
        if (op && !op->is_true(known, contains_negations)) return false; // Short-circuit
    }
    return true;
}

bool AndNode::is_false(const std::unordered_set<std::shared_ptr<Predicate>>& known, bool contains_negations) const {
    for (const auto& op : operands_) {
        if (op && op->is_false(known, contains_negations)) return true; // Short-circuit if one item is definitely false
    }
    return false;
}

bool AndNode::is_true_delete_relaxation(const std::unordered_set<std::shared_ptr<Predicate>>& known) const {
    for (const auto& op : operands_) {
        if (op && !op->is_true_delete_relaxation(known)) return false;
    }
    return true;
}

std::shared_ptr<Formula> AndNode::negate() {
    // De Morgan's Law: Not(A and B) => Not(A) or Not(B)
    FormulaList negated_ops;
    for (const auto& op : operands_) {
        if (op) negated_ops.push_back(op->negate());
    }
    return std::make_shared<OrNode>(negated_ops);
}

void AndNode::get_all_predicates(std::unordered_set<std::shared_ptr<Predicate>>& predicates) const {
    for (const auto& op : operands_) {
        if (op) op->get_all_predicates(predicates);
    }
}

void AndNode::get_all_effect_predicates(std::unordered_set<std::shared_ptr<Predicate>>& conditional_predicates, 
                                       std::unordered_set<std::shared_ptr<Predicate>>& non_conditional_predicates) const {
    for (const auto& op : operands_) {
        if (op) op->get_all_effect_predicates(conditional_predicates, non_conditional_predicates);
    }
}

bool AndNode::contains_condition() const {
    for (const auto& op : operands_) {
        if (op && op->contains_condition()) return true;
    }
    return false;
}

std::shared_ptr<Formula> AndNode::clone() const {
    FormulaList cloned_ops;
    for (const auto& op : operands_) {
        if (op) cloned_ops.push_back(op->clone());
    }
    return std::make_shared<AndNode>(cloned_ops);
}

bool AndNode::contained_in(const std::unordered_set<std::shared_ptr<Predicate>>& predicates, bool contains_negations) const {
    for (const auto& op : operands_) {
        if (op && !op->contained_in(predicates, contains_negations)) return false;
    }
    return true;
}

std::shared_ptr<Formula> AndNode::replace(std::shared_ptr<Formula> org_f, std::shared_ptr<Formula> new_f) {
    if (this == org_f.get()) return new_f;
    FormulaList replaced_ops;
    for (const auto& op : operands_) {
        if (op) replaced_ops.push_back(op->replace(org_f, new_f));
    }
    return std::make_shared<AndNode>(replaced_ops);
}

std::shared_ptr<Formula> AndNode::simplify() {
    FormulaList simplified_ops;
    for (const auto& op : operands_) {
        if (!op) continue;
        auto simplified_child = op->simplify();
        
        // Flatten nested AND nodes: (A and (B and C)) => (A and B and C)
        if (simplified_child->get_type() == Type::And) {
            auto child_and = std::static_pointer_cast<AndNode>(simplified_child);
            for (const auto& child_op : child_and->get_operands()) {
                simplified_ops.push_back(child_op);
            }
        } else {
            simplified_ops.push_back(simplified_child);
        }
    }
    FormulaList final_ops;
    for (const auto& op : simplified_ops) {
        if (op->to_string() == Utilities::FALSE_PREDICATE_NAME) {
            return std::make_shared<PredicateNode>(Utilities::FALSE_PREDICATE); 
        }
        if (op->to_string() != Utilities::TRUE_PREDICATE_NAME) {
            final_ops.push_back(op);
        }
    }

    if (final_ops.empty()) return std::make_shared<PredicateNode>(Utilities::TRUE_PREDICATE);
    if (final_ops.size() == 1) return final_ops[0];
    
    return std::make_shared<AndNode>(final_ops);
}

std::shared_ptr<Formula> AndNode::reduce(const std::unordered_set<std::shared_ptr<Predicate>>& known) {
    FormulaList reduced_ops;
    for (const auto& op : operands_) {
        if (op) reduced_ops.push_back(op->reduce(known));
    }
    auto result = std::make_shared<AndNode>(reduced_ops);
    return result->simplify();
}

std::string AndNode::to_string() const {
    if (operands_.empty()) return "()";
    std::stringstream ss;
    ss << "(and";
    for (const auto& op : operands_) {
        if (op) ss << " " << op->to_string();
    }
    ss << ")";
    return ss.str();
}

std::shared_ptr<Formula> AndNode::ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) { return clone(); }
std::shared_ptr<Formula> AndNode::partially_ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) { return clone(); }
std::shared_ptr<Formula> AndNode::to_cnf() { return clone(); }
std::shared_ptr<Formula> AndNode::regress(std::shared_ptr<PlanningAction> a, const std::unordered_set<std::shared_ptr<Predicate>>& observed) { return clone(); }
std::shared_ptr<Formula> AndNode::regress(std::shared_ptr<PlanningAction> a) { return clone(); }
bool AndNode::contains_non_deterministic_effect() const { return false; }
int AndNode::get_max_non_deterministic_options() const { return 1; }
void AndNode::get_all_optional_predicates(std::unordered_set<std::shared_ptr<Predicate>>& predicates) const {}
std::shared_ptr<Formula> AndNode::create_regression(std::shared_ptr<Predicate> pred, int choice) { return clone(); }
std::shared_ptr<Formula> AndNode::generate_given(const std::string& tag, const std::vector<std::string>& always_known) { return clone(); }
std::shared_ptr<Formula> AndNode::add_time(int time) { return clone(); }
std::shared_ptr<Formula> AndNode::replace_negative_effects_in_condition() { return clone(); }
std::shared_ptr<Formula> AndNode::remove_impossible_options(const std::unordered_set<std::shared_ptr<Predicate>>& observed) { return clone(); }
std::shared_ptr<Formula> AndNode::apply_known(const std::unordered_set<std::shared_ptr<Predicate>>& known) { return reduce(known); }
std::vector<std::shared_ptr<Predicate>> AndNode::get_non_deterministic_effects() { return {}; }
std::shared_ptr<Formula> AndNode::remove_universal_quantifiers(const std::vector<std::shared_ptr<Constant>>& constants, const std::vector<std::shared_ptr<Predicate>>& constant_predicates, std::shared_ptr<Domain> d) { return clone(); }
std::shared_ptr<Formula> AndNode::get_knowledge_formula(const std::vector<std::string>& always_known, bool know_whether) { return clone(); }
std::shared_ptr<Formula> AndNode::reduce_conditions(const std::unordered_set<std::shared_ptr<Predicate>>& known) { return reduce(known); }
std::shared_ptr<Formula> AndNode::remove_negations() {
    FormulaList clean_ops;
    for (const auto& op : operands_) {
        if (!op) continue;
        auto res = op->remove_negations();
        if (res) clean_ops.push_back(res);
    }
    if (clean_ops.empty()) return nullptr;
    if (clean_ops.size() == 1) return clean_ops[0];
    return std::make_shared<AndNode>(clean_ops);
}