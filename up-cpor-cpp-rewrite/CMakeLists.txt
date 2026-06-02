cmake_minimum_required(VERSION 3.15)
project(cpor_cpp_engine CXX)

# ==========================================
# 1. Global Compiler Settings
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Required for Python shared libraries

# ==========================================
# 2. CORE C++ LIBRARY (Standalone)
# ==========================================
add_library(cpor_core STATIC
    src/LogicalUtilities/Formula.cpp
    src/LogicalUtilities/PredicateNode.cpp
    src/LogicalUtilities/AndNode.cpp
    src/LogicalUtilities/OrNode.cpp
)
target_include_directories(cpor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==========================================
# 3. PYTHON NATIVE C-API BINDINGS
# ==========================================
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
Python3_add_library(cpor_engine MODULE bindings/python_module.cpp)
target_link_libraries(cpor_engine PRIVATE cpor_core)
set_target_properties(cpor_engine PROPERTIES PREFIX "" SUFFIX "${PYTHON_EXTENSION_MODULE_SUFFIX}")

# ==========================================
# 4. CATCH2 TEST SUITE
# ==========================================
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Point Catch2 to your C++ test file
add_executable(run_tests tests/cpp/test_logical_utils.cpp)
target_link_libraries(run_tests PRIVATE cpor_core Catch2::Catch2WithMain)