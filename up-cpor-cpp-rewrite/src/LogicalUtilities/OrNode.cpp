#include "Predicate.h"
#include "OrNode.h"
#include "AndNode.h"
#include "NotNode.h"
#include "PredicateNode.h"
#include "Tools/Utilities.h"
#include <sstream>

OrNode::OrNode(FormulaList operands) : operands_(std::move(operands)) {
    this->size_ = 1;
    for (const auto& op : operands_) if (op) this->size_ += op->get_size();
}

bool OrNode::is_true(const PredicateSet& known, bool contains_negations) const {
    for (const auto& op : operands_) {
        if (op && op->is_true(known, contains_negations)) return true; // Short-circuit
    }
    return false;
}

bool OrNode::is_false(const PredicateSet& known, bool contains_negations) const {
    for (const auto& op : operands_) {
        if (op && !op->is_false(known, contains_negations)) return false;
    }
    return true;
}


bool OrNode::is_true_delete_relaxation(const PredicateSet& known) const {
    for (const auto& op : operands_) {
        if (op && op->is_true_delete_relaxation(known)) return true;
    }
    return false;
}

std::shared_ptr<Formula> OrNode::negate() {
    // De Morgan's Law: Not(A or B) => Not(A) and Not(B)
    FormulaList negated_ops;
    for (const auto& op : operands_) if (op) negated_ops.push_back(op->negate());
    return std::make_shared<AndNode>(negated_ops);
}

std::shared_ptr<Formula> OrNode::simplify() {
    FormulaList simplified_ops;
    for (const auto& op : operands_) {
        if (!op) continue;
        auto simplified_child = op->simplify();
        
        if (simplified_child->get_type() == Type::Or) {
            auto child_or = std::static_pointer_cast<OrNode>(simplified_child);
            for (const auto& child_op : child_or->get_operands()) simplified_ops.push_back(child_op);
        } else {
            simplified_ops.push_back(simplified_child);
        }
    }

    FormulaList final_ops;
    for (const auto& op : simplified_ops) {
        if (op->to_string() == Utilities::TRUE_PREDICATE_NAME) {
            return std::make_shared<PredicateNode>(Utilities::TRUE_PREDICATE); // Entire OR is true
        }
        if (op->to_string() != Utilities::FALSE_PREDICATE_NAME) {
            final_ops.push_back(op);
        }
    }

    if (final_ops.empty()) return std::make_shared<PredicateNode>(Utilities::FALSE_PREDICATE);
    if (final_ops.size() == 1) return final_ops[0];
    return std::make_shared<OrNode>(final_ops);
}

void OrNode::get_all_predicates(PredicateSet& predicates) const {
    for (const auto& op : operands_) {
        if (op) op->get_all_predicates(predicates);
    }
}

void OrNode::get_all_effect_predicates(PredicateSet& conditional_predicates, 
                                       PredicateSet& non_conditional_predicates) const {
    for (const auto& op : operands_) {
        if (op) op->get_all_effect_predicates(conditional_predicates, non_conditional_predicates);
    }
}

bool OrNode::contains_condition() const {
    for (const auto& op : operands_) {
        if (op && op->contains_condition()) return true;
    }
    return false;
}

std::shared_ptr<Formula> OrNode::clone() const {
    FormulaList cloned_ops;
    for (const auto& op : operands_) {
        if (op) cloned_ops.push_back(op->clone());
    }
    return std::make_shared<OrNode>(cloned_ops);
}

bool OrNode::contained_in(const PredicateSet& predicates, bool contains_negations) const {
    for (const auto& op : operands_) {
        if (op && op->contained_in(predicates, contains_negations)) return true;
    }
    return false;
}

std::shared_ptr<Formula> OrNode::replace(std::shared_ptr<Formula> org_f, std::shared_ptr<Formula> new_f) {
    if (this == org_f.get()) return new_f;
    FormulaList replaced_ops;
    for (const auto& op : operands_) {
        if (op) replaced_ops.push_back(op->replace(org_f, new_f));
    }
    return std::make_shared<OrNode>(replaced_ops);
}

std::shared_ptr<Formula> OrNode::reduce(const PredicateSet& known) {
    FormulaList reduced_ops;
    for (const auto& op : operands_) {
        if (op) reduced_ops.push_back(op->reduce(known));
    }
    auto result = std::make_shared<OrNode>(reduced_ops);
    return result->simplify();
}

std::string OrNode::to_string() const {
    if (operands_.empty()) return "()";
    std::stringstream ss;
    ss << "(or";
    for (const auto& op : operands_) if (op) ss << " " << op->to_string();
    ss << ")";
    return ss.str();
}


std::shared_ptr<Formula> OrNode::ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) { return clone(); }
std::shared_ptr<Formula> OrNode::partially_ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) { return clone(); }
std::shared_ptr<Formula> OrNode::to_cnf() { return clone(); }
std::shared_ptr<Formula> OrNode::regress(std::shared_ptr<PlanningAction> a, const PredicateSet& observed) { return clone(); }
std::shared_ptr<Formula> OrNode::regress(std::shared_ptr<PlanningAction> a) { return clone(); }
bool OrNode::contains_non_deterministic_effect() const { return false; }
int OrNode::get_max_non_deterministic_options() const { return 1; }
void OrNode::get_all_optional_predicates(PredicateSet& predicates) const {}
std::shared_ptr<Formula> OrNode::create_regression(std::shared_ptr<Predicate> pred, int choice) { return clone(); }
std::shared_ptr<Formula> OrNode::generate_given(const std::string& tag, const std::vector<std::string>& always_known) {
    FormulaList out;
    for (const auto& f : operands_) out.push_back(f->generate_given(tag, always_known));
    return std::make_shared<OrNode>(out);
}
std::shared_ptr<Formula> OrNode::add_time(int time) { return clone(); }
std::shared_ptr<Formula> OrNode::replace_negative_effects_in_condition() { return clone(); }
std::shared_ptr<Formula> OrNode::remove_impossible_options(const PredicateSet& observed) { return clone(); }
std::shared_ptr<Formula> OrNode::apply_known(const PredicateSet& known) { return reduce(known); }
std::vector<std::shared_ptr<Predicate>> OrNode::get_non_deterministic_effects() { return {}; }
std::shared_ptr<Formula> OrNode::remove_universal_quantifiers(const std::vector<std::shared_ptr<Constant>>& constants, const std::vector<std::shared_ptr<Predicate>>& constant_predicates, std::shared_ptr<Domain> d) { return clone(); }
std::shared_ptr<Formula> OrNode::get_knowledge_formula(const std::vector<std::string>& always_known, bool know_whether) {
    FormulaList out;
    for (const auto& f : operands_) out.push_back(f->get_knowledge_formula(always_known, know_whether));
    return std::make_shared<OrNode>(out);
}
std::shared_ptr<Formula> OrNode::reduce_conditions(const PredicateSet& known) { return reduce(known); }
std::shared_ptr<Formula> OrNode::remove_negations() {
    FormulaList clean_ops;
    for (const auto& op : operands_) {
        if (!op) continue;
        auto res = op->remove_negations();
        if (res) clean_ops.push_back(res);
    }
    if (clean_ops.empty()) return nullptr;
    if (clean_ops.size() == 1) return clean_ops[0];
    return std::make_shared<OrNode>(clean_ops);
}