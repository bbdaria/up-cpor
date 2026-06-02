#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <atomic>

class Predicate;
class Parameter;
class Constant;
class PlanningAction;
class CompoundFormula;
class Domain;

class Formula {
protected:
    int size_;
    int id_;
    static std::atomic<int> formula_count_;

public:
    enum class Type {
        Predicate,
        And,
        Or,
        Not
    };
    Formula() {
        id_ = formula_count_++;
        size_ = 1;
    }
    
    virtual ~Formula() = default;

    int get_id() const { return id_; }
    int get_size() const { return size_; }
    virtual Type get_type() const = 0;
    virtual bool is_true(const std::unordered_set<std::shared_ptr<Predicate>>& known, bool contains_negations) const = 0;
    virtual bool is_false(const std::unordered_set<std::shared_ptr<Predicate>>& known, bool contains_negations) const = 0;
    virtual bool is_true_delete_relaxation(const std::unordered_set<std::shared_ptr<Predicate>>& known) const = 0;

    bool is_true(const std::unordered_set<std::shared_ptr<Predicate>>& known) const { 
        return this->is_true(known, false); 
    }
    bool is_false(const std::unordered_set<std::shared_ptr<Predicate>>& known) const { 
        return this->is_false(known, false); 
    }

    virtual std::shared_ptr<Formula> ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) = 0;
    virtual std::shared_ptr<Formula> partially_ground(const std::unordered_map<std::shared_ptr<Parameter>, std::shared_ptr<Constant>>& bindings) = 0;
    
    virtual std::shared_ptr<Formula> negate() = 0;
    virtual void get_all_predicates(std::unordered_set<std::shared_ptr<Predicate>>& predicates) const = 0;
    virtual void get_all_effect_predicates(std::unordered_set<std::shared_ptr<Predicate>>& conditional_predicates, 
                                           std::unordered_set<std::shared_ptr<Predicate>>& non_conditional_predicates) const = 0;
    
    virtual std::shared_ptr<Formula> to_cnf() = 0;
    virtual bool contains_condition() const = 0;
    virtual std::shared_ptr<Formula> clone() const = 0;
    
    virtual bool contained_in(const std::unordered_set<std::shared_ptr<Predicate>>& predicates, bool contains_negations) const = 0;
    virtual std::shared_ptr<Formula> replace(std::shared_ptr<Formula> org_f, std::shared_ptr<Formula> new_f) = 0;
    virtual std::shared_ptr<Formula> simplify() = 0;

    virtual std::shared_ptr<Formula> regress(std::shared_ptr<PlanningAction> a, const std::unordered_set<std::shared_ptr<Predicate>>& observed) = 0;
    virtual std::shared_ptr<Formula> regress(std::shared_ptr<PlanningAction> a) = 0;
    virtual std::shared_ptr<Formula> reduce(const std::unordered_set<std::shared_ptr<Predicate>>& known) = 0;

    virtual bool contains_non_deterministic_effect() const = 0;
    virtual int get_max_non_deterministic_options() const = 0;
    virtual void get_all_optional_predicates(std::unordered_set<std::shared_ptr<Predicate>>& predicates) const = 0;
    virtual std::shared_ptr<Formula> create_regression(std::shared_ptr<Predicate> pred, int choice) = 0;
    virtual std::shared_ptr<Formula> generate_given(const std::string& tag, const std::vector<std::string>& always_known) = 0;
    virtual std::shared_ptr<Formula> add_time(int time) = 0;
    virtual std::shared_ptr<Formula> replace_negative_effects_in_condition() = 0;
    virtual std::shared_ptr<Formula> remove_impossible_options(const std::unordered_set<std::shared_ptr<Predicate>>& observed) = 0;
    virtual std::shared_ptr<Formula> apply_known(const std::unordered_set<std::shared_ptr<Predicate>>& known) = 0;
    virtual std::vector<std::shared_ptr<Predicate>> get_non_deterministic_effects() = 0;
    
    virtual std::shared_ptr<Formula> remove_universal_quantifiers(const std::vector<std::shared_ptr<Constant>>& constants, 
                                                                  const std::vector<std::shared_ptr<Predicate>>& constant_predicates, 
                                                                  std::shared_ptr<Domain> d) = 0;
    virtual std::shared_ptr<Formula> get_knowledge_formula(const std::vector<std::string>& always_known, bool know_whether) = 0;
    virtual std::shared_ptr<Formula> reduce_conditions(const std::unordered_set<std::shared_ptr<Predicate>>& known) = 0;
    virtual std::shared_ptr<Formula> remove_negations() = 0;

    virtual std::string to_string() const = 0;

    size_t get_hash_code() const {
        return std::hash<std::string>{}(to_string());
    }
};

using FormulaPtr = std::shared_ptr<Formula>;
using FormulaList = std::vector<FormulaPtr>;