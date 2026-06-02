#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <memory>

class Formula;
class Predicate;
struct PlanningAction {
    std::string name;
    bool is_sensing;

    std::shared_ptr<Formula> precondition;
    std::shared_ptr<Formula> effect;
    std::vector<std::shared_ptr<Predicate>> observed_fluents;

    void debug_print() const {
        std::cout << "Action: " << name 
                  << " | Sensing: " << (is_sensing ? "True" : "False") 
                  << " | Preconds: " << (precondition ? "Defined" : "None") 
                  << " | Effects: " << (effect ? "Defined" : "None") << "\n";
    }
};
struct ProblemContext {
    std::string problem_name;
    std::vector<PlanningAction> actions;

    void add_action(const PlanningAction& action) {
        actions.push_back(action);
    }

    void debug_print() const {
        std::cout << "--- C++ Problem Loaded: " << problem_name << " ---\n";
        std::cout << "Total Actions: " << actions.size() << "\n";
        for (const auto& a : actions) {
            a.debug_print();
        }
    }
};