#pragma once

#include "Formula.h"

class AndNode : public Formula {
private:
    FormulaList operands_;

public:
    AndNode() { this->size_ = 1; }
    explicit AndNode(FormulaList operands);
    using Formula::is_true;
    using Formula::is_false;
    void add_operand(FormulaPtr operand);
    const FormulaList& get_operands() const { return operands_; }

    virtual Type get_type() const override { return Type::And; }
    
    virtual bool is_true(const std::unordered_set<std::shared_ptr<Predicate>>& known, bool contains_negations) const override;
    virtual bool is_false(const std::unordered_set<std::shared_ptr<Predicate>>& known, bool contains_negations) const override;
    virtual bool is_true_delete_relaxation(const std::unordered_set<std::shared_ptr<Predicate>>& known) const override;

    virtual std::shared_ptr<Formula> ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) override;
    virtual std::shared_ptr<Formula> partially_ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) override;
    
    virtual std::shared_ptr<Formula> negate() override;
    virtual void get_all_predicates(std::unordered_set<std::shared_ptr<Predicate>>& predicates) const override;
    virtual void get_all_effect_predicates(std::unordered_set<std::shared_ptr<Predicate>>& conditional_predicates, 
                                           std::unordered_set<std::shared_ptr<Predicate>>& non_conditional_predicates) const override;
    
    virtual std::shared_ptr<Formula> to_cnf() override;
    virtual bool contains_condition() const override;
    virtual std::shared_ptr<Formula> clone() const override;
    
    virtual bool contained_in(const std::unordered_set<std::shared_ptr<Predicate>>& predicates, bool contains_negations) const override;
    virtual std::shared_ptr<Formula> replace(std::shared_ptr<Formula> org_f, std::shared_ptr<Formula> new_f) override;
    virtual std::shared_ptr<Formula> simplify() override;

    virtual std::shared_ptr<Formula> regress(std::shared_ptr<PlanningAction> a, const std::unordered_set<std::shared_ptr<Predicate>>& observed) override;
    virtual std::shared_ptr<Formula> regress(std::shared_ptr<PlanningAction> a) override;
    virtual std::shared_ptr<Formula> reduce(const std::unordered_set<std::shared_ptr<Predicate>>& known) override;

    virtual bool contains_non_deterministic_effect() const override;
    virtual int get_max_non_deterministic_options() const override;
    virtual void get_all_optional_predicates(std::unordered_set<std::shared_ptr<Predicate>>& predicates) const override;
    virtual std::shared_ptr<Formula> create_regression(std::shared_ptr<Predicate> pred, int choice) override;
    virtual std::shared_ptr<Formula> generate_given(const std::string& tag, const std::vector<std::string>& always_known) override;
    virtual std::shared_ptr<Formula> add_time(int time) override;
    virtual std::shared_ptr<Formula> replace_negative_effects_in_condition() override;
    virtual std::shared_ptr<Formula> remove_impossible_options(const std::unordered_set<std::shared_ptr<Predicate>>& observed) override;
    virtual std::shared_ptr<Formula> apply_known(const std::unordered_set<std::shared_ptr<Predicate>>& known) override;
    virtual std::vector<std::shared_ptr<Predicate>> get_non_deterministic_effects() override;
    
    virtual std::shared_ptr<Formula> remove_universal_quantifiers(const std::vector<std::shared_ptr<Constant>>& constants, 
                                                                  const std::vector<std::shared_ptr<Predicate>>& constant_predicates, 
                                                                  std::shared_ptr<Domain> d) override;
    virtual std::shared_ptr<Formula> get_knowledge_formula(const std::vector<std::string>& always_known, bool know_whether) override;
    virtual std::shared_ptr<Formula> reduce_conditions(const std::unordered_set<std::shared_ptr<Predicate>>& known) override;
    virtual std::shared_ptr<Formula> remove_negations() override;

    virtual std::string to_string() const override;
};