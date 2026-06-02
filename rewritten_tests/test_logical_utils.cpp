#include <catch2/catch_test_macros.hpp>
#include "LogicalUtilities/Predicate.h"
#include "LogicalUtilities/PredicateNode.h"
#include "LogicalUtilities/AndNode.h"
#include "LogicalUtilities/OrNode.h"
#include "LogicalUtilities/NotNode.h"
#include "Tools/Utilities.h"
#include <memory>
#include <unordered_set>

TEST_CASE("Predicate and PredicateNode Base Evaluations", "[LogicalUtilities]") {
    // Setup core grounded predicates
    auto clear_b1 = std::make_shared<Predicate>("clear", std::vector<std::string>{"b1"});
    auto clear_b2 = std::make_shared<Predicate>("clear", std::vector<std::string>{"b2"});
    auto on_b1_b2 = std::make_shared<Predicate>("on", std::vector<std::string>{"b1", "b2"});

    // Create a known state map (Only clear(b1) and on(b1,b2) are true)
    std::unordered_set<std::shared_ptr<Predicate>> state = { clear_b1, on_b1_b2 };

    SECTION("Atomic Predicate Leaf Valuation") {
        PredicateNode node_clear_b1(clear_b1);
        PredicateNode node_clear_b2(clear_b2);

        // Under Closed-World Assumption (contains_negations = false)
        REQUIRE(node_clear_b1.is_true(state) == true);
        REQUIRE(node_clear_b2.is_true(state) == false); // Absent implies false

        REQUIRE(node_clear_b1.is_false(state) == false);
        REQUIRE(node_clear_b2.is_false(state) == true);  // Absent implies true for is_false
    }

    SECTION("Explicit Negation Handling") {
        auto negated_clear_b2 = clear_b2->negate(); // (not clear(b2))
        PredicateNode node_neg_clear_b2(negated_clear_b2);

        // Under CWA: Since clear(b2) is absent, (not clear(b2)) must be true
        REQUIRE(node_neg_clear_b2.is_true(state) == true);
        REQUIRE(node_neg_clear_b2.is_false(state) == false);

        // Switch to Partial Observability / Contingent Mode (contains_negations = true)
        // Since (not clear(b2)) is NOT explicitly present in the state, its value is unknown
        REQUIRE(node_neg_clear_b2.is_true(state, true) == false); 
        REQUIRE(node_neg_clear_b2.is_false(state, true) == false); 

        // If we explicitly inject the negative fact into a belief state...
        state.insert(negated_clear_b2);
        REQUIRE(node_neg_clear_b2.is_true(state, true) == true); // Now it's explicitly proven true
    }
}

TEST_CASE("AndNode Conjunction Logic", "[LogicalUtilities]") {
    auto p1 = std::make_shared<Predicate>("p1");
    auto p2 = std::make_shared<Predicate>("p2");
    
    std::unordered_set<std::shared_ptr<Predicate>> state = { p1 }; // p1 is true, p2 is false

    auto node_p1 = std::make_shared<PredicateNode>(p1);
    auto node_p2 = std::make_shared<PredicateNode>(p2);

    FormulaList operands = { node_p1, node_p2 };
    AndNode and_node(operands);

    SECTION("Evaluation and Short-Circuiting") {
        REQUIRE(and_node.is_true(state) == false); // p1(T) && p2(F) => F
        REQUIRE(and_node.is_false(state) == true);  // At least one operand is false

        // State where both are true
        state.insert(p2);
        REQUIRE(and_node.is_true(state) == true);
        REQUIRE(and_node.is_false(state) == false);
    }

    SECTION("Algebraic Simplification and Flattening") {
        FormulaList nested_operands = { std::make_shared<AndNode>(operands), node_p1 };
        AndNode complex_and(nested_operands);

        // Flattening verification: (and (and p1 p2) p1) => (and p1 p2 p1)
        auto simplified = complex_and.simplify();
        REQUIRE(simplified->get_type() == Formula::Type::And);
        
        auto flat_and = std::static_pointer_cast<AndNode>(simplified);
        REQUIRE(flat_and->get_operands().size() == 3);

        // Short circuit to False if it contains FALSE_PREDICATE constant
        FormulaList false_list = { node_p1, std::make_shared<PredicateNode>(Utilities::FALSE_PREDICATE) };
        AndNode dead_end_and(false_list);
        REQUIRE(dead_end_and.simplify()->to_string() == Utilities::FALSE_PREDICATE_NAME);
    }
}

TEST_CASE("OrNode Disjunction Logic", "[LogicalUtilities]") {
    auto p1 = std::make_shared<Predicate>("p1");
    auto p2 = std::make_shared<Predicate>("p2");

    std::unordered_set<std::shared_ptr<Predicate>> state = { p1 }; // Only p1 is true

    auto node_p1 = std::make_shared<PredicateNode>(p1);
    auto node_p2 = std::make_shared<PredicateNode>(p2);

    FormulaList operands = { node_p1, node_p2 };
    OrNode or_node(operands);

    SECTION("Evaluation and Short-Circuiting") {
        REQUIRE(or_node.is_true(state) == true);   // p1(T) || p2(F) => T
        REQUIRE(or_node.is_false(state) == false);

        // State where both are false
        state.clear();
        REQUIRE(or_node.is_true(state) == false);
        REQUIRE(or_node.is_false(state) == true);
    }

    SECTION("Short-Circuit to True Constant") {
        FormulaList true_list = { node_p2, std::make_shared<PredicateNode>(Utilities::TRUE_PREDICATE) };
        OrNode live_or(true_list);
        REQUIRE(live_or.simplify()->to_string() == Utilities::TRUE_PREDICATE_NAME);
    }
}

TEST_CASE("NotNode Unary Negation Logic", "[LogicalUtilities]") {
    auto p1 = std::make_shared<Predicate>("p1");
    std::unordered_set<std::shared_ptr<Predicate>> state = { p1 }; // p1 is true

    auto node_p1 = std::make_shared<PredicateNode>(p1);
    NotNode not_node(node_p1);

    SECTION("Basic Unary Flip") {
        REQUIRE(not_node.is_true(state) == false); // Not(True) => False
        REQUIRE(not_node.is_false(state) == true);

        state.clear();                             // p1 becomes false
        REQUIRE(not_node.is_true(state) == true);  // Not(False) => True
    }

    SECTION("Double Negation Elimination") {
        auto double_negated = not_node.negate(); // Not(Not(p1))
        auto simplified = double_negated->simplify();
        
        // Verifies tree node recovery to basic predicate layout
        REQUIRE(simplified->get_type() == Formula::Type::Predicate);
        REQUIRE(simplified->to_string() == p1->get_signature());
    }
}

TEST_CASE("De Morgan's Laws Integration", "[LogicalUtilities]") {
    auto p1 = std::make_shared<Predicate>("p1");
    auto p2 = std::make_shared<Predicate>("p2");

    auto node_p1 = std::make_shared<PredicateNode>(p1);
    auto node_p2 = std::make_shared<PredicateNode>(p2);

    SECTION("Not(A and B) == Not(A) or Not(B)") {
        FormulaList ops = { node_p1, node_p2 };
        auto and_node = std::make_shared<AndNode>(ops);
        
        auto negated_and = and_node->negate(); // Returns an OrNode of internally negated operands
        REQUIRE(negated_and->get_type() == Formula::Type::Or);
        
        auto or_flat = std::static_pointer_cast<OrNode>(negated_and);
        
        // Verifies that the child components are flat leaf nodes with flipped internal signatures
        REQUIRE(or_flat->get_operands()[0]->get_type() == Formula::Type::Predicate);
        REQUIRE(or_flat->get_operands()[1]->get_type() == Formula::Type::Predicate);
        
        // Verify semantic evaluation match
        std::unordered_set<std::shared_ptr<Predicate>> state = { p1, p2 }; // Both true
        REQUIRE(and_node->is_true(state) == true);
        REQUIRE(or_flat->is_true(state) == false); // Not(True and True) => False
    }
}