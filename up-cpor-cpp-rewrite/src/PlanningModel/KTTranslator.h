#pragma once

#include <string>
#include <utility>
#include <vector>

// Palacios-Geffner / SDR knowledge (KT) translation, in C++.
//
// Compiles the contingent problem at the current belief into a CLASSICAL problem
// over knowledge literals (see Brafman & Shani, JAIR 2012, Section 3). The
// K-literal predicate names are formed with the engine's Predicate K/KW helpers.
// Produces a PDDL (domain, problem) pair for the classical planner (FF).

struct KTAction {
    std::string name;
    bool is_sensing = false;
    std::string observe;  // observed fluent name; empty when not sensing
    std::vector<std::pair<std::string, bool>> pre;  // (fluent, polarity)
    std::vector<std::string> add;
    std::vector<std::string> del;
};

// tags[i] = set of uncertain-fluent names true in possible world i (tag 0 is the
// distinguished sample s' that fixes observation outcomes).
std::pair<std::string, std::string> kt_translate(
    const std::vector<KTAction>& actions,
    const std::vector<std::string>& uncertain,
    const std::vector<std::vector<std::string>>& tags,
    const std::vector<std::string>& known_true,
    const std::vector<std::pair<std::string, bool>>& goal);
