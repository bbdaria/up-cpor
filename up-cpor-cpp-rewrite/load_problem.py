import sys
import os

# Ensure Python can find the C-API extension
sys.path.append(os.path.abspath("build"))
import cpor_engine

from unified_planning.io import PDDLReader
from up_cpor.converter import ASTConverter

def run_poc_simulator(domain_path, problem_path):
    print(f"Loading {problem_path} via Unified-Planning...")
    reader = PDDLReader()
    problem = reader.parse_problem(domain_path, problem_path)

    converter = ASTConverter()

    # =========================================================
    # Phase 1: Build the 'Belief State' in Python using C++ Memory
    # =========================================================
    print("\n--- 1. Building Initial State ---")
    cpp_state = [] # This list of C++ Predicates is our temporary State object
    
    for fluent, value in problem.initial_values.items():
        if value.is_true():
            signature = converter._get_signature(fluent)
            cpp_pred = cpor_engine.Predicate(signature)
            cpp_state.append(cpp_pred)
            print(f"Added to State: {signature}")

    # =========================================================
    # Phase 2: The Meta-Planner Evaluation Loop
    # =========================================================
    print("\n--- 2. Evaluating Actions via C++ AST ---")
    for action in problem.actions:
        print(f"\nChecking Action: {action.name}")
        
        if not action.preconditions:
            print(" -> Allowed! (No preconditions)")
            continue
            
        try:
            # 1. Compile the unified_planning formula into our C++ AST
            # PDDL actions usually wrap preconditions in an AND block
            cpp_precondition_ast = converter.compile_formula(action.preconditions[0])

            # 2. Fire the C++ Engine!
            is_valid = cpp_precondition_ast.is_true(cpp_state)
            
            if is_valid:
                print(" -> Action is ALLOWED in the current state.")
            else:
                print(" -> Action is BLOCKED.")
                
        except Exception as e:
            print(f" -> Could not evaluate: {e}")

if __name__ == "__main__":
    # Ensure this points to the correct test directory!
    run_poc_simulator("../tests/blocks2/d.pddl", "../tests/blocks2/p.pddl")