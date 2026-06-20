#pragma once

#include <string>
#include <vector>

// Run the vendored Metric-FF on the given PDDL domain/problem files (in a forked
// child for isolation) and return the plan as a list of operator strings, each
// formatted "OP arg arg" (FF's internal upper-case names). Returns an empty
// vector if FF finds no plan or fails.
std::vector<std::string> ff_solve(const std::string& domain_path,
                                  const std::string& problem_path);

// Same as ff_solve but takes the domain/problem PDDL as in-memory strings
// (parsed via fmemopen), so no temp files touch the disk.
std::vector<std::string> ff_solve_strings(const std::string& domain_str,
                                          const std::string& problem_str);
