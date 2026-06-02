#pragma once

#include <string>
#include <vector>
#include <sstream>

class Predicate {
private:
    std::string name_;
    std::vector<std::string> arguments_;
    std::string signature_;
    bool negation_;

    void generate_signature() {
        std::stringstream ss;
        if (negation_) ss << "(not ";
        ss << name_;
        if (!arguments_.empty()) {
            ss << "(";
            for (size_t i = 0; i < arguments_.size(); ++i) {
                ss << arguments_[i];
                if (i < arguments_.size() - 1) ss << ",";
            }
            ss << ")";
        }
        if (negation_) ss << ")";
        signature_ = ss.str();
    }

public:
    Predicate(std::string name, std::vector<std::string> arguments = {}, bool negation = false)
        : name_(std::move(name)), arguments_(std::move(arguments)), negation_(negation) {
        generate_signature();
    }
    
    const std::string& get_name() const { return name_; }
    const std::vector<std::string>& get_arguments() const { return arguments_; }
    const std::string& get_signature() const { return signature_; }
    bool is_negated() const { return negation_; }

    std::shared_ptr<Predicate> negate() const {
        return std::make_shared<Predicate>(name_, arguments_, !negation_);
    }

    //Contingent Planner Methods 
    std::shared_ptr<Predicate> generate_given(const std::string& tag) const {
        return std::make_shared<Predicate>(name_ + "_" + tag, arguments_, negation_);
    }

    static std::shared_ptr<Predicate> generate_know_predicate(std::shared_ptr<Predicate> p, bool bValue = true) {
        if (!p) return nullptr;
        bool val = bValue;
        if (p->is_negated()) { val = false; }
        std::string prefix = val ? "K" : "KN";
        return std::make_shared<Predicate>(prefix + p->get_name(), p->get_arguments(), false);
    }

    static std::shared_ptr<Predicate> generate_know_whether_predicate(std::shared_ptr<Predicate> p) {
        if (!p) return nullptr;
        return std::make_shared<Predicate>("KW" + p->get_name(), p->get_arguments(), p->is_negated());
    }

    bool operator==(const Predicate& other) const {
        return signature_ == other.signature_;
    }
};

namespace std {
    template <>
    struct hash<Predicate> {
        size_t operator()(const Predicate& p) const {
            return hash<string>{}(p.get_signature());
        }
    };
}

struct PredicatePtrHash {
    size_t operator()(const std::shared_ptr<Predicate>& p) const {
        if (!p) return 0;
        return std::hash<std::string>{}(p->get_signature());
    }
};

struct PredicatePtrEqual {
    bool operator()(const std::shared_ptr<Predicate>& lhs, const std::shared_ptr<Predicate>& rhs) const {
        if (!lhs || !rhs) return lhs == rhs;
        return *lhs == *rhs;
    }
};