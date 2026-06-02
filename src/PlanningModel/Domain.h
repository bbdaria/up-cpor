#pragma once
#include <string>
#include <vector>
#include <iostream>

struct PlanningAction {
    std::string name;
    bool is_sensing;
    std::vector<std::string> preconditions;
    std::vector<std::string> effects;
    std::vector<std::string> observed_fluents;

    void debug_print() const {
        std::cout << "Action: " << name 
                  << " | Sensing: " << (is_sensing ? "True" : "False") 
                  << " | Preconds: " << preconditions.size() 
                  << " | Effects: " << effects.size() << "\n";
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