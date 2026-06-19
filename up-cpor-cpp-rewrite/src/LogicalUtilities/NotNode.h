#pragma once
#include "Predicate.h"

#include "Formula.h"

class NotNode : public Formula {
private:
    std::shared_ptr<Formula> child_;

public:
    NotNode() : Formula() {
        this->child_ = nullptr;
        this->size_ = 0; 
    }
    explicit NotNode(std::shared_ptr<Formula> child) : child_(std::move(child)) {
        this->size_ = 1 + (child_ ? child_->get_size() : 0);
    }
    using Formula::is_true;
    using Formula::is_false;
    std::shared_ptr<Formula> get_child() const { return child_; }
    virtual Type get_type() const override { return Type::Not; }
    
    virtual bool is_true(const PredicateSet& known, bool contains_negations) const override {
        return child_ ? child_->is_false(known, contains_negations) : false;
    }

    virtual bool is_false(const PredicateSet& known, bool contains_negations) const override {
        return child_ ? child_->is_true(known, contains_negations) : true;
    }

    virtual bool is_true_delete_relaxation(const PredicateSet& known) const override {
        return true; 
    }
    
    virtual std::shared_ptr<Formula> negate() override {
        return child_;
    }

    virtual void get_all_predicates(PredicateSet& predicates) const override {
        if (child_) child_->get_all_predicates(predicates);
    }

    virtual void get_all_effect_predicates(PredicateSet& conditional_predicates, 
                                           PredicateSet& non_conditional_predicates) const override {
        if (child_) child_->get_all_effect_predicates(conditional_predicates, non_conditional_predicates);
    }

    virtual std::shared_ptr<Formula> clone() const override {
        return std::make_shared<NotNode>(child_ ? child_->clone() : nullptr);
    }

    virtual bool contained_in(const PredicateSet& predicates, bool contains_negations) const override {
        return child_ ? !child_->contained_in(predicates, contains_negations) : false;
    }

    virtual std::shared_ptr<Formula> replace(std::shared_ptr<Formula> org_f, std::shared_ptr<Formula> new_f) override {
        if (this == org_f.get()) return new_f;
        return std::make_shared<NotNode>(child_ ? child_->replace(org_f, new_f) : nullptr);
    }

    virtual std::shared_ptr<Formula> simplify() override {
        if (!child_) return clone();
        auto simplified_child = child_->simplify();
        
        if (simplified_child->get_type() == Type::Not) {
            return std::static_pointer_cast<NotNode>(simplified_child)->get_child();
        }
        return std::make_shared<NotNode>(simplified_child);
    }

    virtual std::shared_ptr<Formula> reduce(const PredicateSet& known) override {
        if (!child_) return clone();
        auto res = std::make_shared<NotNode>(child_->reduce(known));
        return res->simplify();
    }

    virtual std::string to_string() const override {
        return child_ ? "(not " + child_->to_string() + ")" : "(not)";
    }

    virtual std::shared_ptr<Formula> ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& b) override { return clone(); }
    virtual std::shared_ptr<Formula> partially_ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& b) override { return clone(); }
    virtual std::shared_ptr<Formula> to_cnf() override { return clone(); }
    virtual bool contains_condition() const override { return child_ ? child_->contains_condition() : false; }
    virtual std::shared_ptr<Formula> regress(std::shared_ptr<PlanningAction> a, const PredicateSet& o) override { return clone(); }
    virtual std::shared_ptr<Formula> regress(std::shared_ptr<PlanningAction> a) override { return clone(); }
    virtual bool contains_non_deterministic_effect() const override { return false; }
    virtual int get_max_non_deterministic_options() const override { return 1; }
    virtual void get_all_optional_predicates(PredicateSet& p) const override {}
    virtual std::shared_ptr<Formula> create_regression(std::shared_ptr<Predicate> p, int c) override { return clone(); }
    virtual std::shared_ptr<Formula> generate_given(const std::string& t, const std::vector<std::string>& a) override {
        return child_ ? std::make_shared<NotNode>(child_->generate_given(t, a)) : clone();
    }
    virtual std::shared_ptr<Formula> add_time(int t) override { return clone(); }
    virtual std::shared_ptr<Formula> replace_negative_effects_in_condition() override { return clone(); }
    virtual std::shared_ptr<Formula> remove_impossible_options(const PredicateSet& o) override { return clone(); }
    virtual std::shared_ptr<Formula> apply_known(const PredicateSet& k) override { return reduce(k); }
    virtual std::vector<std::shared_ptr<Predicate>> get_non_deterministic_effects() override { return {}; }
    virtual std::shared_ptr<Formula> remove_universal_quantifiers(const std::vector<std::shared_ptr<Constant>>& c, const std::vector<std::shared_ptr<Predicate>>& cp, std::shared_ptr<Domain> d) override { return clone(); }
    virtual std::shared_ptr<Formula> get_knowledge_formula(const std::vector<std::string>& a, bool kw) override {
        // Push negation inward so the K/KW predicate is formed on the (negated)
        // atom: K(¬p) -> KN p, KW(¬p) -> KW p (knowing-whether is sign-agnostic).
        return child_ ? child_->negate()->get_knowledge_formula(a, kw) : clone();
    }
    virtual std::shared_ptr<Formula> reduce_conditions(const PredicateSet& k) override { return reduce(k); }
    virtual std::shared_ptr<Formula> remove_negations() override { return nullptr; }
};