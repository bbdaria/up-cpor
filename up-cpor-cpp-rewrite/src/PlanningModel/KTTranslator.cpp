#include "PlanningModel/KTTranslator.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <unordered_set>

#include "LogicalUtilities/Predicate.h"

namespace {

std::string san(const std::string& name) {
    std::string out = name;
    for (char& c : out) {
        if (c == '-' || c == '/' || c == '(' || c == ')' || c == ',' || c == ' ')
            c = '_';
    }
    return out;
}

// Knowledge-predicate name for a literal, using the Predicate K/KW helper:
// positive -> K<f>, negative -> KN<f>.
std::string k_atom(const std::string& fluent, bool positive) {
    auto p = std::make_shared<Predicate>(fluent, std::vector<std::string>{}, !positive);
    auto kp = Predicate::generate_know_predicate(p);
    return san(kp->get_name());
}

std::string tag_atom(int i, const std::string& fluent) {
    return "c" + std::to_string(i) + "_" + san(fluent);
}

// PDDL atom asserting a literal in a precondition/goal (knowledge for uncertain
// fluents, plain otherwise).
std::string lit_atom(const std::string& fluent, bool positive,
                     const std::unordered_set<std::string>& uncertain) {
    if (uncertain.count(fluent))
        return "(" + k_atom(fluent, positive) + ")";
    return positive ? "(" + san(fluent) + ")" : "(not (" + san(fluent) + "))";
}

std::string mk_action(const std::string& name,
                      const std::vector<std::string>& pre,
                      const std::vector<std::string>& eff) {
    std::ostringstream os;
    os << "  (:action " << name << "\n   :parameters ()";
    if (!pre.empty()) {  // FF rejects an empty () precondition
        os << "\n   :precondition (and";
        for (const auto& a : pre) os << " " << a;
        os << ")";
    }
    os << "\n   :effect (and";
    for (const auto& a : eff) os << " " << a;
    os << "))";
    return os.str();
}

}  // namespace

std::pair<std::string, std::string> kt_translate(
    const std::vector<KTAction>& actions,
    const std::vector<std::string>& uncertain_vec,
    const std::vector<std::vector<std::string>>& tags,
    const std::vector<std::string>& known_true,
    const std::vector<std::pair<std::string, bool>>& goal) {

    std::unordered_set<std::string> uncertain(uncertain_vec.begin(), uncertain_vec.end());
    const int n_tags = static_cast<int>(tags.size());
    std::vector<std::unordered_set<std::string>> tagset;
    for (const auto& t : tags) tagset.emplace_back(t.begin(), t.end());

    std::set<std::string> preds;
    for (const auto& f : known_true) preds.insert("(" + san(f) + ")");
    for (const auto& u : uncertain_vec) {
        preds.insert("(" + k_atom(u, true) + ")");
        preds.insert("(" + k_atom(u, false) + ")");
        for (int i = 0; i < n_tags; ++i) preds.insert("(" + tag_atom(i, u) + ")");
    }
    for (int i = 0; i < n_tags; ++i) preds.insert("(ref_" + std::to_string(i) + ")");

    std::vector<std::string> blocks;
    for (const auto& a : actions) {
        std::string name = san(a.name);
        std::vector<std::string> pre;
        for (const auto& lit : a.pre) {
            if (!uncertain.count(lit.first)) preds.insert("(" + san(lit.first) + ")");
            pre.push_back(lit_atom(lit.first, lit.second, uncertain));
        }

        if (!a.is_sensing) {
            std::vector<std::string> eff;
            for (const auto& f : a.add) {
                if (uncertain.count(f)) {
                    eff.push_back("(" + k_atom(f, true) + ")");
                    eff.push_back("(not (" + k_atom(f, false) + "))");
                    for (int i = 0; i < n_tags; ++i) eff.push_back("(" + tag_atom(i, f) + ")");
                } else {
                    preds.insert("(" + san(f) + ")");
                    eff.push_back("(" + san(f) + ")");
                }
            }
            for (const auto& f : a.del) {
                if (uncertain.count(f)) {
                    eff.push_back("(" + k_atom(f, false) + ")");
                    eff.push_back("(not (" + k_atom(f, true) + "))");
                    for (int i = 0; i < n_tags; ++i) eff.push_back("(not (" + tag_atom(i, f) + "))");
                } else {
                    preds.insert("(" + san(f) + ")");
                    eff.push_back("(not (" + san(f) + "))");
                }
            }
            if (!eff.empty()) blocks.push_back(mk_action(name, pre, eff));
        } else {
            const std::string& p = a.observe;
            if (p.empty() || !uncertain.count(p)) continue;
            bool value = tagset.empty() ? false : tagset[0].count(p) > 0;  // sample = tag 0
            std::vector<std::string> eff;
            if (value) {
                eff.push_back("(" + k_atom(p, true) + ")");
                for (int i = 0; i < n_tags; ++i)
                    if (!tagset[i].count(p)) eff.push_back("(ref_" + std::to_string(i) + ")");
            } else {
                eff.push_back("(" + k_atom(p, false) + ")");
                for (int i = 0; i < n_tags; ++i)
                    if (tagset[i].count(p)) eff.push_back("(ref_" + std::to_string(i) + ")");
            }
            blocks.push_back(mk_action(name, pre, eff));
        }
    }

    // merge actions: conclude unconditional knowledge from per-tag agreement.
    for (const auto& u : uncertain_vec) {
        std::vector<std::string> pos_pre, neg_pre;
        for (int i = 0; i < n_tags; ++i) {
            pos_pre.push_back("(or (" + tag_atom(i, u) + ") (ref_" + std::to_string(i) + "))");
            neg_pre.push_back("(or (not (" + tag_atom(i, u) + ")) (ref_" + std::to_string(i) + "))");
        }
        blocks.push_back(mk_action("merge_pos_" + san(u), pos_pre, {"(" + k_atom(u, true) + ")"}));
        blocks.push_back(mk_action("merge_neg_" + san(u), neg_pre, {"(" + k_atom(u, false) + ")"}));
    }

    std::ostringstream dom;
    dom << "(define (domain kt)\n  (:requirements :strips :adl)\n  (:predicates";
    for (const auto& p : preds) dom << " " << p;
    dom << ")\n";
    for (const auto& b : blocks) dom << b << "\n";
    dom << ")\n";

    std::vector<std::string> init;
    for (const auto& f : known_true) init.push_back("(" + san(f) + ")");
    for (const auto& u : uncertain_vec)
        for (int i = 0; i < n_tags; ++i)
            if (tagset[i].count(u)) init.push_back("(" + tag_atom(i, u) + ")");

    std::ostringstream prob;
    prob << "(define (problem kt-prob)\n  (:domain kt)\n  (:init";
    for (const auto& a : init) prob << " " << a;
    prob << ")\n  (:goal (and";
    for (const auto& g : goal) prob << " " << lit_atom(g.first, g.second, uncertain);
    prob << "))\n)\n";

    return {dom.str(), prob.str()};
}
