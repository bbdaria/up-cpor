cmake_minimum_required(VERSION 3.15)
project(up_cpor_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)


FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)


FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)


set(CORE_SOURCES
    src/LogicalUtilities/Formula.cpp
    src/LogicalUtilities/PredicateNode.cpp
    src/LogicalUtilities/AndNode.cpp
    src/LogicalUtilities/OrNode.cpp
)


set(CPOR_SOURCES src/main.cpp ${CORE_SOURCES})
pybind11_add_module(cpor_engine ${CPOR_SOURCES})
target_include_directories(cpor_engine PRIVATE src)
target_compile_options(cpor_engine PRIVATE -O3)


enable_testing()

add_executable(run_tests 
    rewritten_tests/test_logical_utils.cpp
    ${CORE_SOURCES}
)

target_include_directories(run_tests PRIVATE src)
target_link_libraries(run_tests PRIVATE Catch2::Catch2WithMain)


add_test(NAME logical_utilities_test COMMAND run_tests)