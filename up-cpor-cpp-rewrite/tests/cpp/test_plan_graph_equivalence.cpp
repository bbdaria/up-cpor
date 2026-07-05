#include <catch2/catch_test_macros.hpp>
#include "PlanningModel/PlanGraphEquivalence.h"

using namespace PlanGraph;

// Fixtures below mirror the paper's Doors worked example (Sec 5, Fig. 3 /
// Example 5.2): two levels of three doors each, exactly one door per level is
// unlocked, discovered via a "check_<door>" sensing action.

TEST_CASE("ClosedNodeIndex::find_candidates: structural filter", "[PlanGraph]") {
    ClosedNodeIndex index;

    // n1: reached after checking d locked; only the agent's position is known,
    // and (unlocked_e, unlocked_f) are the relevant-but-hidden fluents, with a
    // single recorded observation set for f: {NOT_unlocked_e}.
    ClosedNodeInfo n1;
    n1.known = {"at_2_2"};
    n1.hidden = {"unlocked_e", "unlocked_f"};
    n1.observation_sets["unlocked_f"] = {{"NOT_unlocked_e"}};
    std::size_t id1 = index.register_node(n1);

    SECTION("a node whose K/H are compatible is returned as a candidate") {
        // n2: agent at the same position, e/f still unknown -- structurally
        // compatible even though (per the paper) it is NOT actually equivalent
        // yet, because door d hasn't been checked here. That distinction needs
        // the regression/SAT check the caller performs on `pending_literals`.
        std::set<std::string> known_n2 = {"at_2_2"};
        std::set<std::string> hidden_n2 = {"unlocked_d", "unlocked_e", "unlocked_f"};

        auto matches = index.find_candidates(known_n2, hidden_n2);
        REQUIRE(matches.size() == 1);
        CHECK(matches[0].id == id1);
        REQUIRE(matches[0].pending_literals.size() == 1);
        CHECK(matches[0].pending_literals[0] == "unlocked_f");
    }

    SECTION("a node with a missing known literal is rejected") {
        std::set<std::string> known_missing_position = {};  // agent position unknown here
        std::set<std::string> hidden = {"unlocked_e", "unlocked_f"};
        auto matches = index.find_candidates(known_missing_position, hidden);
        CHECK(matches.empty());
    }

    SECTION("a node where a required-hidden fluent is already resolved is rejected") {
        // e is already known here (say, known false) -- H(n1) requires it to
        // still be genuinely unknown, so n1 cannot apply.
        std::set<std::string> known = {"at_2_2", "NOT_unlocked_e"};
        std::set<std::string> hidden = {"unlocked_f"};  // e is deliberately absent: it's known
        auto matches = index.find_candidates(known, hidden);
        CHECK(matches.empty());
    }
}

TEST_CASE("ClosedNodeIndex::update_action: actuation folds K/H/O through effects", "[PlanGraph]") {
    ClosedNodeInfo child;
    child.known = {"at_2_2", "NOT_unlocked_e"};
    child.hidden = {"unlocked_f"};
    child.observation_sets["unlocked_f"] = {{"NOT_unlocked_e"}};

    // move(2,1 -> 2,2): precondition at_2_1, effect at_2_2 (and implicitly
    // removes at_2_1, not modeled here since it's not in child.known).
    ClosedNodeInfo parent = ClosedNodeIndex::update_action(child, {"at_2_1"}, {"at_2_2"});

    CHECK(parent.known.count("at_2_1") == 1);
    CHECK(parent.known.count("at_2_2") == 0);  // the effect is no longer a precondition of n
    CHECK(parent.known.count("NOT_unlocked_e") == 1);  // unrelated known literal carried through
    CHECK(parent.hidden == child.hidden);
    CHECK(parent.observation_sets == child.observation_sets);
}

TEST_CASE("ClosedNodeIndex::update_sensing: branch merge + reasoning capture", "[PlanGraph]") {
    // check_e: true branch learns unlocked_e directly; false branch learns
    // NOT_unlocked_e AND (via the oneof d/e/f constraint, already reflected in
    // the child's K since regression/reasoning happened downstream) unlocked_f.
    ClosedNodeInfo true_child;
    true_child.known = {"unlocked_e"};

    ClosedNodeInfo false_child;
    false_child.known = {"NOT_unlocked_e", "unlocked_f"};

    ClosedNodeInfo parent =
        ClosedNodeIndex::update_sensing(true_child, false_child, {"at_2_2"}, "unlocked_e");

    // The sensed literal itself is not relevant BEFORE sensing it.
    CHECK(parent.known.count("unlocked_e") == 0);
    CHECK(parent.known.count("NOT_unlocked_e") == 0);
    CHECK(parent.known.count("at_2_2") == 1);

    // unlocked_f was known only in the false branch -> hidden at n, resolved by
    // observing NOT_unlocked_e.
    CHECK(parent.hidden.count("unlocked_f") == 1);
    REQUIRE(parent.observation_sets.count("unlocked_f") == 1);
    REQUIRE(parent.observation_sets.at("unlocked_f").size() == 1);
    CHECK(parent.observation_sets.at("unlocked_f")[0] == std::vector<std::string>{"NOT_unlocked_e"});
}

TEST_CASE("AncestorCycleGuard: match + deadend-loop rejection", "[PlanGraph]") {
    AncestorCycleGuard guard;

    guard.push({"at_home"}, /*crossed_observation=*/false);              // depth 0: root
    guard.push({"holding_egg"}, /*crossed_observation=*/false);          // depth 1: pick up
    guard.push({"holding_egg", "broken_bad"}, /*crossed_observation=*/true); // depth 2: sensed bad

    SECTION("an ancestor with identical known-facts is found") {
        auto match = guard.find_ancestor_match({"holding_egg"});
        REQUIRE(match.has_value());
        CHECK(*match == 1);
    }

    SECTION("a cycle back through a sensing edge is safe") {
        auto match = guard.find_ancestor_match({"holding_egg"});
        REQUIRE(match.has_value());
        CHECK(guard.safe_cycle(*match) == true);
    }

    SECTION("a cycle with no intervening observation is unsafe (deadend self-loop)") {
        guard.push({"holding_egg", "broken_bad"}, /*crossed_observation=*/false);  // no-op retry
        auto match = guard.find_ancestor_match({"holding_egg", "broken_bad"});
        REQUIRE(match.has_value());
        CHECK(*match == 2);
        CHECK(guard.safe_cycle(*match) == false);
    }

    SECTION("no match when known-facts differ from every ancestor") {
        auto match = guard.find_ancestor_match({"totally_different"});
        CHECK_FALSE(match.has_value());
    }

    SECTION("pop unwinds the stack") {
        guard.pop();
        CHECK(guard.depth() == 2);
        auto match = guard.find_ancestor_match({"holding_egg", "broken_bad"});
        CHECK_FALSE(match.has_value());
    }
}
