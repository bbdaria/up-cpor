cmake_minimum_required(VERSION 3.15)
project(up-cpor-cpp-rewrite CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Required for Python shared libraries

add_library(cpor_core OBJECT
    src/LogicalUtilities/Formula.cpp
    src/LogicalUtilities/PredicateNode.cpp
    src/LogicalUtilities/AndNode.cpp
    src/LogicalUtilities/OrNode.cpp
)

target_include_directories(cpor_core PUBLIC src)



find_package(Python3 COMPONENTS Interpreter Development REQUIRED)


Python3_add_library(cpor_engine MODULE src/main.cpp)

target_link_libraries(cpor_engine PRIVATE cpor_core)


set_target_properties(cpor_engine PROPERTIES PREFIX "")

set_target_properties(cpor_engine PROPERTIES SUFFIX "${PYTHON_EXTENSION_MODULE_SUFFIX}")

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)


add_executable(run_tests 
    rewritten_tests/test_logical_utils.cpp
)


target_link_libraries(run_tests PRIVATE cpor_core Catch2::Catch2WithMain)