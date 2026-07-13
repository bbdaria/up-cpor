#include "PlanningModel/PlanGraphEquivalence.h"

namespace PlanGraph {

namespace {

std::string base_name(const std::string& lit) {
    if (lit.rfind("NOT_", 0) == 0) return lit.substr(4);
    return lit;
}

}  // namespace

std::size_t ClosedNodeIndex::register_node(ClosedNodeInfo info) {
    nodes_.push_back(std::move(info));
    return nodes_.size() - 1;
}

const ClosedNodeInfo& ClosedNodeIndex::info(std::size_t id) const { return nodes_.at(id); }

std::vector<ClosedNodeMatch> ClosedNodeIndex::find_candidates(
    const std::set<std::string>& known, const std::set<std::string>& hidden) const {
    std::vector<ClosedNodeMatch> matches;
    for (std::size_t id = 0; id < nodes_.size(); ++id) {
        const ClosedNodeInfo& candidate = nodes_[id];

        bool known_contained = true;
        for (const auto& k : candidate.known) {
            if (!known.count(k)) { known_contained = false; break; }
        }
        if (!known_contained) continue;

        bool hidden_still_unknown = true;
        for (const auto& h : candidate.hidden) {
            if (!hidden.count(h)) { hidden_still_unknown = false; break; }
        }
        if (!hidden_still_unknown) continue;

        ClosedNodeMatch m;
        m.id = id;
        for (const auto& kv : candidate.observation_sets) m.pending_literals.push_back(kv.first);
        matches.push_back(std::move(m));
    }
    return matches;
}

ClosedNodeInfo ClosedNodeIndex::update_action(const ClosedNodeInfo& child,
                                               const std::vector<std::string>& preconditions,
                                               const std::vector<std::string>& effects) {
    ClosedNodeInfo out;

    std::set<std::string> effect_bases;
    for (const auto& e : effects) effect_bases.insert(base_name(e));

    // K(n) = K(n.a(n)) \ eff(n.a) U pre(n.a)  (Algorithm 4, eq. 7).
    for (const auto& k : child.known) {
        if (!effect_bases.count(base_name(k))) out.known.insert(k);
    }
    for (const auto& p : preconditions) out.known.insert(p);

    // H(n) = H(n.a(n)) -- an actuation action performs no sensing (eq. 9).
    out.hidden = child.hidden;
    out.observation_sets = child.observation_sets;
    return out;
}

ClosedNodeInfo ClosedNodeIndex::update_sensing(const ClosedNodeInfo& true_child,
                                                const ClosedNodeInfo& false_child,
                                                const std::vector<std::string>& preconditions,
                                                const std::string& observed_base) {
    ClosedNodeInfo out;
    const std::string not_q = "NOT_" + observed_base;

    // K(n) = K(true)+K(false)+pre(n.a), minus the literal this action itself
    // determines -- its value only picks the branch, it isn't known before
    // (Algorithm 4, eq. 11).
    for (const auto& k : true_child.known)
        if (k != observed_base && k != not_q) out.known.insert(k);
    for (const auto& k : false_child.known)
        if (k != observed_base && k != not_q) out.known.insert(k);
    for (const auto& p : preconditions) out.known.insert(p);

    // A literal known in one branch but not the other was resolved BY this
    // observation -- i.e. it is hidden at n, and observing q with the value
    // that branch took is exactly the reasoning that resolves it (Sec 5.5's
    // ReasonedT/ReasonedF, folded into O(n,l) as a fresh singleton set).
    auto reasoned = [&](const ClosedNodeInfo& branch, const ClosedNodeInfo& other,
                         const std::string& q_value) {
        for (const auto& k : branch.known) {
            if (k == observed_base || k == not_q) continue;
            if (other.known.count(k)) continue;  // known regardless of q's value
            out.hidden.insert(base_name(k));
            out.observation_sets[k].push_back({q_value});
        }
    };
    reasoned(true_child, false_child, observed_base);
    reasoned(false_child, true_child, not_q);

    // Fluents still hidden in a branch stay hidden at n; carry their existing
    // observation sets forward, prefixed by the value q took on that branch
    // (the reasoning recorded there is only valid once q is known to be that
    // value -- Algorithm 4, eqs. 10 and the O(n,l) update in Sec 5.5/5.6).
    out.hidden.insert(true_child.hidden.begin(), true_child.hidden.end());
    out.hidden.insert(false_child.hidden.begin(), false_child.hidden.end());
    out.hidden.erase(observed_base);

    for (const auto& kv : true_child.observation_sets) {
        for (auto set : kv.second) {
            set.insert(set.begin(), observed_base);
            out.observation_sets[kv.first].push_back(std::move(set));
        }
    }
    for (const auto& kv : false_child.observation_sets) {
        for (auto set : kv.second) {
            set.insert(set.begin(), not_q);
            out.observation_sets[kv.first].push_back(std::move(set));
        }
    }

    return out;
}

ClosedNodeInfo ClosedNodeIndex::goal_node(const std::vector<std::string>& goal_literals_known) {
    ClosedNodeInfo out;
    out.known.insert(goal_literals_known.begin(), goal_literals_known.end());
    return out;
}

// ---------------------------------------------------------------------------

void AncestorCycleGuard::push(std::vector<std::string> known_facts, bool crossed_observation) {
    Frame f;
    f.known_facts = std::set<std::string>(known_facts.begin(), known_facts.end());
    f.crossed_observation = crossed_observation;
    frames_.push_back(std::move(f));
}

void AncestorCycleGuard::pop() {
    if (!frames_.empty()) frames_.pop_back();
}

std::optional<std::size_t> AncestorCycleGuard::find_ancestor_match(
    const std::vector<std::string>& known_facts) const {
    std::set<std::string> target(known_facts.begin(), known_facts.end());
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i].known_facts == target) return i;
    }
    return std::nullopt;
}

bool AncestorCycleGuard::safe_cycle(std::size_t ancestor_depth) const {
    // An observation must occur strictly between the ancestor and the top
    // frame, i.e. on frames [ancestor_depth+1, end). C# DetectInfiniteLoop:
    // a self-loop with no intervening sensing is a dead end, not a cycle.
    for (std::size_t i = ancestor_depth + 1; i < frames_.size(); ++i) {
        if (frames_[i].crossed_observation) return true;
    }
    return false;
}

}  // namespace PlanGraph
