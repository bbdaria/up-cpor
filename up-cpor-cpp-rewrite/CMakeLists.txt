cmake_minimum_required(VERSION 3.15)
project(cpor_cpp_engine CXX C)

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
    src/PlanningModel/KTTranslator.cpp # KT (knowledge) translation
    src/PlanningModel/PlanGraphEquivalence.cpp # belief-equivalence compaction + cycle detection
)
target_include_directories(cpor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==========================================
# 2b. VENDORED METRIC-FF (classical planner), built in-process
# ==========================================
set(FF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/FF-v2.3)
# OBJECT (not STATIC): guarantees every FF object is linked into the Python
# module. A static archive would drop main.o under macOS's -undefined
# dynamic_lookup, leaving gplan_capture_fd / ff_main unresolved at dlopen.
add_library(cpor_ff OBJECT
    ${FF_DIR}/main.c
    ${FF_DIR}/memory.c
    ${FF_DIR}/output.c
    ${FF_DIR}/parse.c
    ${FF_DIR}/inst_pre.c
    ${FF_DIR}/inst_easy.c
    ${FF_DIR}/inst_hard.c
    ${FF_DIR}/inst_final.c
    ${FF_DIR}/orderings.c
    ${FF_DIR}/relax.c
    ${FF_DIR}/search.c
    ${FF_DIR}/scan-fct_pddl.tab.c   # #includes lex.fct_pddl.c
    ${FF_DIR}/scan-ops_pddl.tab.c   # #includes lex.ops_pddl.c
    ${FF_DIR}/ff_wrapper.cpp
)
target_include_directories(cpor_ff PUBLIC ${FF_DIR})
# Metric-FF is legacy C: build as gnu99 and silence its many warnings.
set_source_files_properties(
    ${FF_DIR}/main.c ${FF_DIR}/memory.c ${FF_DIR}/output.c ${FF_DIR}/parse.c
    ${FF_DIR}/inst_pre.c ${FF_DIR}/inst_easy.c ${FF_DIR}/inst_hard.c
    ${FF_DIR}/inst_final.c ${FF_DIR}/orderings.c ${FF_DIR}/relax.c
    ${FF_DIR}/search.c ${FF_DIR}/scan-fct_pddl.tab.c ${FF_DIR}/scan-ops_pddl.tab.c
    PROPERTIES COMPILE_OPTIONS "-std=gnu99;-w"
)

# ==========================================
# 3. PYTHON NATIVE C-API BINDINGS
# ==========================================
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Locate the Z3 C++ API shipped with the z3-solver pip package (provides the
# SAT/CNF reasoning backend for BeliefSolver).
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import os,z3;print(os.path.dirname(z3.__file__))"
    OUTPUT_VARIABLE Z3_PKG_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE Z3_LOOKUP_RESULT
)
if(NOT Z3_LOOKUP_RESULT EQUAL 0)
    message(FATAL_ERROR "z3-solver not found in the active Python; run `pip install z3-solver`.")
endif()
# set(Z3_INCLUDE_DIR ${Z3_PKG_DIR}/include)
if(NOT Z3_INCLUDE_DIR)
    set(Z3_INCLUDE_DIR ${Z3_PKG_DIR}/include)
endif()
# find_library(Z3_LIBRARY NAMES z3 libz3 PATHS ${Z3_PKG_DIR}/lib NO_DEFAULT_PATH)
find_library(Z3_LIBRARY NAMES z3 libz3 PATHS "${Z3_PKG_DIR}/lib" "${Z3_PKG_DIR}" NO_DEFAULT_PATH)
message(STATUS "Z3 include: ${Z3_INCLUDE_DIR}")
message(STATUS "Z3 library: ${Z3_LIBRARY}")

Python3_add_library(cpor_engine MODULE
    bindings/python_module.cpp
    src/PlanningModel/BeliefSolver.cpp
)
target_include_directories(cpor_engine PRIVATE ${Z3_INCLUDE_DIR})
target_link_libraries(cpor_engine PRIVATE cpor_core cpor_ff ${Z3_LIBRARY})
set_target_properties(cpor_engine PROPERTIES
    PREFIX "" SUFFIX ".so"
    BUILD_RPATH "${Z3_PKG_DIR}/lib"
    INSTALL_RPATH "${Z3_PKG_DIR}/lib"
)
if(APPLE)
    # The z3 dylib's install name is the bare "libz3.dylib"; dyld ignores LC_RPATH
    # for bare names. Rewrite the dependency to @rpath/libz3.dylib so the rpath
    # above (the z3 package lib dir) resolves it at load time.
    add_custom_command(TARGET cpor_engine POST_BUILD
        COMMAND install_name_tool -change libz3.dylib @rpath/libz3.dylib $<TARGET_FILE:cpor_engine>
        VERBATIM
    )
endif()

# ==========================================
# 4. CATCH2 TEST SUITE (optional; needs network to fetch Catch2)
# ==========================================
option(BUILD_CPP_TESTS "Build the C++ Catch2 test suite" ON)
if(BUILD_CPP_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    # Point Catch2 to your C++ test file
    add_executable(run_tests
        tests/cpp/test_logical_utils.cpp
        tests/cpp/test_plan_graph_equivalence.cpp
    )
    target_link_libraries(run_tests PRIVATE
    cpor_core Catch2::Catch2WithMain)
endif()