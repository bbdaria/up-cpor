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

std::string tag_lit(int i, const std::string& fluent, bool positive) {
    std::string a = "(" + tag_atom(i, fluent) + ")";
    return positive ? a : "(not " + a + ")";
}

// PDDL atom asserting a literal in a precondition/goal (knowledge for
// tag-tracked fluents, plain otherwise).
std::string lit_atom(const std::string& fluent, bool positive,
                     const std::unordered_set<std::string>& tagged) {
    if (tagged.count(fluent))
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

    // Tag-dependency closure (Brafman & Shani 2012, Sec 3: the translation
    // copies per tag every fluent whose value can come to DEPEND on the
    // hidden state, not just the hidden fluents themselves). A certain fluent
    // becomes tag-dependent when a conditional effect gated on a tag-dependent
    // fluent writes it (medpks: `stain` has (when (ill_ik) (stain_sk)), so
    // stain_sk diverges across tags once `stain` executes).
    std::unordered_set<std::string> tagged(uncertain);
    std::vector<std::string> derived_vec;
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto& a : actions) {
            if (a.is_sensing) continue;
            for (const auto& c : a.cond) {
                const std::string& fluent = std::get<1>(c);
                if (tagged.count(fluent)) continue;
                for (const auto& cl : std::get<0>(c)) {
                    if (tagged.count(cl.first)) {
                        tagged.insert(fluent);
                        derived_vec.push_back(fluent);
                        grew = true;
                        break;
                    }
                }
            }
        }
    }
    std::sort(derived_vec.begin(), derived_vec.end());
    std::unordered_set<std::string> derived(derived_vec.begin(), derived_vec.end());

    std::vector<std::string> tagged_vec(uncertain_vec);
    tagged_vec.insert(tagged_vec.end(), derived_vec.begin(), derived_vec.end());

    std::unordered_set<std::string> known_set(known_true.begin(), known_true.end());

    std::set<std::string> preds;
    for (const auto& f : known_true)
        if (!tagged.count(f)) preds.insert("(" + san(f) + ")");
    for (const auto& u : tagged_vec) {
        preds.insert("(" + k_atom(u, true) + ")");
        preds.insert("(" + k_atom(u, false) + ")");
        for (int i = 0; i < n_tags; ++i) preds.insert("(" + tag_atom(i, u) + ")");
    }
    for (int i = 0; i < n_tags; ++i) preds.insert("(ref_" + std::to_string(i) + ")");

    // Full effect of setting/clearing `f`: knowledge (with the opposite
    // K-literal withdrawn) plus, for tag-tracked fluents, every tag copy --
    // an unconditional actuation effect applies in all possible worlds.
    auto full_effect = [&](const std::string& f, bool is_add) {
        std::vector<std::string> out;
        if (tagged.count(f)) {
            out.push_back("(" + k_atom(f, is_add) + ")");
            out.push_back("(not (" + k_atom(f, !is_add) + "))");
            for (int i = 0; i < n_tags; ++i) out.push_back(tag_lit(i, f, is_add));
        } else {
            preds.insert("(" + san(f) + ")");
            out.push_back(is_add ? "(" + san(f) + ")" : "(not (" + san(f) + "))");
        }
        return out;
    };

    std::vector<std::string> blocks;
    for (const auto& a : actions) {
        std::string name = san(a.name);
        std::vector<std::string> pre;
        for (const auto& lit : a.pre) {
            if (!tagged.count(lit.first)) preds.insert("(" + san(lit.first) + ")");
            pre.push_back(lit_atom(lit.first, lit.second, tagged));
        }

        if (!a.is_sensing) {
            std::vector<std::string> eff;
            for (const auto& f : a.add)
                for (const auto& e : full_effect(f, true)) eff.push_back(e);
            for (const auto& f : a.del)
                for (const auto& e : full_effect(f, false)) eff.push_back(e);

            // Tag-wise expansion is emitted ONLY for effects whose target is
            // a DERIVED fluent: those are the tag copies that sensing reads
            // dynamically (medpks' stain_sk), so they must track each world.
            // For uncertain targets the copies are never read back mid-plan
            // (uncertain sensing bakes the sample value in at translation
            // time), so the knowledge-level when alone suffices -- and
            // expanding them anyway multiplies every operator's effect list
            // by n_tags, which blows up FF's grounding and search (localize5:
            // 8 move whens x 19 tags made most seeds time out). A product cap
            // additionally guards FF against very large derived expansions.
            int derived_conds = 0;
            for (const auto& c : a.cond)
                if (derived.count(std::get<1>(c))) ++derived_conds;
            const bool tag_expand = derived_conds > 0 && derived_conds * n_tags <= 600;

            for (const auto& c : a.cond) {
                const auto& cond_lits = std::get<0>(c);
                const std::string& fluent = std::get<1>(c);
                bool is_add = std::get<2>(c);
                bool dep = false;
                for (const auto& cl : cond_lits)
                    if (tagged.count(cl.first)) { dep = true; break; }

                if (!dep) {
                    // Condition entirely over certain fluents: one plain when,
                    // whose consequence still updates knowledge + all tags.
                    std::ostringstream when;
                    when << "(when (and";
                    for (const auto& cl : cond_lits) {
                        preds.insert("(" + san(cl.first) + ")");
                        when << " " << lit_atom(cl.first, cl.second, tagged);
                    }
                    when << ") (and";
                    for (const auto& e : full_effect(fluent, is_add)) when << " " << e;
                    when << "))";
                    eff.push_back(when.str());
                } else {
                    // Tag-wise: in each possible world t the effect fires
                    // exactly when that world satisfies the condition
                    // (fluent is in `tagged` by the closure above).
                    const bool expand_this = tag_expand && derived.count(fluent) > 0;
                    for (int t = 0; expand_this && t < n_tags; ++t) {
                        std::ostringstream when;
                        when << "(when (and";
                        for (const auto& cl : cond_lits) {
                            if (tagged.count(cl.first)) {
                                when << " " << tag_lit(t, cl.first, cl.second);
                            } else {
                                preds.insert("(" + san(cl.first) + ")");
                                when << " " << lit_atom(cl.first, cl.second, tagged);
                            }
                        }
                        when << ") " << tag_lit(t, fluent, is_add) << ")";
                        eff.push_back(when.str());
                    }
                    // Knowledge level: a KNOWN condition yields a known effect.
                    std::ostringstream kwhen;
                    kwhen << "(when (and";
                    for (const auto& cl : cond_lits)
                        kwhen << " " << lit_atom(cl.first, cl.second, tagged);
                    kwhen << ") (and (" << k_atom(fluent, is_add) << ") (not ("
                          << k_atom(fluent, !is_add) << "))))";
                    eff.push_back(kwhen.str());
                }
            }

            if (!eff.empty()) blocks.push_back(mk_action(name, pre, eff));
        } else {
            const std::string& p = a.observe;
            if (p.empty()) continue;
            if (uncertain.count(p)) {
                // Hidden fluent, static across the plan: bake the sample
                // (tag 0) outcome in at translation time.
                bool value = tagset.empty() ? false : tagset[0].count(p) > 0;
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
            } else if (tagged.count(p)) {
                // Tag-dependent (derived) fluent: its value changes as the
                // plan executes, so the observed outcome is whatever holds in
                // the sample world NOW -- condition on tag 0's copy, and
                // refute every tag that currently disagrees with tag 0.
                std::vector<std::string> eff;
                eff.push_back("(when " + tag_lit(0, p, true) + " (and (" + k_atom(p, true) +
                              ") (not (" + k_atom(p, false) + "))))");
                eff.push_back("(when " + tag_lit(0, p, false) + " (and (" + k_atom(p, false) +
                              ") (not (" + k_atom(p, true) + "))))");
                for (int t = 1; t < n_tags; ++t) {
                    eff.push_back("(when (and " + tag_lit(0, p, true) + " " + tag_lit(t, p, false) +
                                  ") (ref_" + std::to_string(t) + "))");
                    eff.push_back("(when (and " + tag_lit(0, p, false) + " " + tag_lit(t, p, true) +
                                  ") (ref_" + std::to_string(t) + "))");
                }
                blocks.push_back(mk_action(name, pre, eff));
            }
            // Observing a plain certain fluent gains nothing: skip.
        }
    }

    // merge actions: conclude unconditional knowledge from per-tag agreement.
    for (const auto& u : tagged_vec) {
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
    for (const auto& f : known_true) {
        if (tagged.count(f)) {
            // Derived tagged fluent, currently true: true in every possible
            // world, and known to be so.
            init.push_back("(" + k_atom(f, true) + ")");
            for (int i = 0; i < n_tags; ++i) init.push_back("(" + tag_atom(i, f) + ")");
        } else {
            init.push_back("(" + san(f) + ")");
        }
    }
    for (const auto& f : derived_vec)
        if (!known_set.count(f)) init.push_back("(" + k_atom(f, false) + ")");
    for (const auto& u : uncertain_vec)
        for (int i = 0; i < n_tags; ++i)
            if (tagset[i].count(u)) init.push_back("(" + tag_atom(i, u) + ")");

    std::ostringstream prob;
    prob << "(define (problem kt-prob)\n  (:domain kt)\n  (:init";
    for (const auto& a : init) prob << " " << a;
    prob << ")\n  (:goal (and";
    for (const auto& g : goal) prob << " " << lit_atom(g.first, g.second, tagged);
    prob << "))\n)\n";

    return {dom.str(), prob.str()};
}
