#pragma once

#include <string>
#include <vector>

// Run the vendored Metric-FF on the given PDDL domain/problem files (in a forked
// child for isolation) and return the plan as a list of operator strings, each
// formatted "OP arg arg" (FF's internal upper-case names). Returns an empty
// vector if FF finds no plan or fails.
std::vector<std::string> ff_solve(const std::string& domain_path,
                                  const std::string& problem_path);
