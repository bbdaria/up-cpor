#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// Belief-equivalence plan-graph compaction (Maliah, Komarnitski & Shani 2022,
// "Computing Contingent Plan Graphs using Online Planning", Sec 5.4, Algorithms 3-4)
// and ancestor-based cycle detection for non-deterministic domains (Sec 5.7,
// Algorithm 5). Mirrors the C# reference (CPORLib/PlanningModel/PartiallySpecifiedState.cs:
// IsClosedState/UpdateClosedStates for the former, AlreadyVisited/DetectInfiniteLoop(Complete)
// for the latter) as an engineering guide, not a byte-exact port.
//
// Literal convention throughout matches the rest of cpor_engine: a bare name
// ("opened_d") is a positive literal; "NOT_" + name is its negation.
namespace PlanGraph {

// K(n) / H(n) / O(n,l) for one plan-(sub)graph node, per the paper's Sec 5.1/5.4:
//   known           = K(n): literals whose value is required, and known, at n.
//   hidden          = H(n): base fluent names required by the plan but unknown at n.
//   observation_sets = O(n,l): for each literal l this node's hidden fluents may
//                      resolve to, the set of observation sequences (each itself
//                      a conjunction of literals) that let us reason l holds.
struct ClosedNodeInfo {
    std::set<std::string> known;
    std::set<std::string> hidden;
    std::map<std::string, std::vector<std::vector<std::string>>> observation_sets;
};

// A structurally-compatible closed node found by find_candidates: the caller
// still owes a regression-consistency check (Algorithm 3 lines 8-11) for each
// literal in `pending_literals` before treating the match as confirmed.
struct ClosedNodeMatch {
    std::size_t id;
    std::vector<std::string> pending_literals;
};

// Registry of closed (already-planned) nodes plus the structural half of
// Algorithm 3's equivalence test, and the Algorithm 4 K/H/O update rules.
class ClosedNodeIndex {
public:
    std::size_t register_node(ClosedNodeInfo info);
    const ClosedNodeInfo& info(std::size_t id) const;
    std::size_t size() const { return nodes_.size(); }

    // Algorithm 3 lines 5-7 (the structural pre-filter): `known`/`hidden`
    // describe the CURRENT node (its full known-literal set, and the base names
    // of fluents currently unknown). Returns every registered node whose
    // K(n') is contained in `known` and whose H(n') fluents are all still
    // unknown at the current node.
    std::vector<ClosedNodeMatch> find_candidates(const std::set<std::string>& known,
                                                  const std::set<std::string>& hidden) const;

    // Algorithm 4, actuation-action case (eq. 7): fold a single child's K/H/O
    // back through a deterministic action into its parent's.
    static ClosedNodeInfo update_action(const ClosedNodeInfo& child,
                                         const std::vector<std::string>& preconditions,
                                         const std::vector<std::string>& effects);

    // Algorithm 4, sensing-action case (eqs. 10-11 + Sec 5.5's reasoned-literal
    // bookkeeping): fold the true/false observation children back through a
    // sensing action observing `observed_base` into the parent's K/H/O.
    static ClosedNodeInfo update_sensing(const ClosedNodeInfo& true_child,
                                         const ClosedNodeInfo& false_child,
                                         const std::vector<std::string>& preconditions,
                                         const std::string& observed_base);

    // Eq. 6 (goal leaf): K(n) is just the goal literals already known at n.
    static ClosedNodeInfo goal_node(const std::vector<std::string>& goal_literals_known);

private:
    std::vector<ClosedNodeInfo> nodes_;
};

// Ancestor-chain cycle detection for non-deterministic domains (Sec 5.7). The
// caller pushes one frame per node on the CURRENT recursion path (mirroring
// walking `Predecessor` in the C#'s DetectInfiniteLoop) and pops it on return.
class AncestorCycleGuard {
public:
    // `known_facts` = the node's full known-literal set (same convention as
    // ClosedNodeInfo::known); `crossed_observation` = true iff the edge from
    // the PARENT frame to this one was a sensing/observation edge.
    void push(std::vector<std::string> known_facts, bool crossed_observation);
    void pop();

    // Algorithm 5's simplified F(n) check: is `known_facts` identical to an
    // ancestor currently on the stack? Returns its stack depth (0 = root) if so.
    std::optional<std::size_t> find_ancestor_match(const std::vector<std::string>& known_facts) const;

    // True iff at least one observation was crossed strictly between the
    // ancestor at `ancestor_depth` and the top of the stack -- i.e. this is a
    // genuine (informative) cycle, not the C#'s explicit deadend self-loop
    // (an action re-applied with no intervening sensing action).
    bool safe_cycle(std::size_t ancestor_depth) const;

    std::size_t depth() const { return frames_.size(); }

private:
    struct Frame {
        std::set<std::string> known_facts;
        bool crossed_observation;
    };
    std::vector<Frame> frames_;
};

}  // namespace PlanGraph
