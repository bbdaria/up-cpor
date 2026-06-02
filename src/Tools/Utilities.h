#pragma once

#include <string>
#include <memory>
#include "LogicalUtilities/Predicate.h" 

class Utilities {
public:
    inline static const std::string FALSE_PREDICATE_NAME = "P_FALSE";
    inline static const std::string TRUE_PREDICATE_NAME = "P_TRUE";
    inline static const std::shared_ptr<Predicate> FALSE_PREDICATE = 
        std::make_shared<Predicate>(FALSE_PREDICATE_NAME, std::vector<std::string>{});

    inline static const std::shared_ptr<Predicate> TRUE_PREDICATE = 
        std::make_shared<Predicate>(TRUE_PREDICATE_NAME, std::vector<std::string>{});
};