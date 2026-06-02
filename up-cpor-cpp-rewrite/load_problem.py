import os
import sys
from unified_planning.io import PDDLReader
import cpor_engine

# Import the Meta-Planner we just built
from up_cpor.engine import CPORMetaPlanner 

def convert_up_state_to_cpp(up_state):
    """
    Converts unified_planning initial state into a list of C++ Predicate objects.
    """
    cpp_state = []
    for fluent, value in up_state.items():
        if value.is_true():
            arg_names = []
            for a in fluent.args:
                if a.is_object_exp():
                    arg_names.append(a.object().name)
                else:
                    arg_names.append(str(a)) # Fallback for non-objects
            
            # Construct the flat predicate name (e.g., "on-table_b1")
            if arg_names:
                pred_name = fluent.fluent().name + "_" + "_".join(arg_names)
            else:
                pred_name = fluent.fluent().name
                
            cpp_state.append(cpor_engine.Predicate(pred_name))
            
    return cpp_state

def main():
    print("--- CPOR Hybrid Engine Initialization ---")
    
    # 1. Load the PDDL Problem
    domain_file = "../tests/blocks2/d.pddl"
    problem_file = "../tests/blocks2/p.pddl"
    
    if not os.path.exists(domain_file) or not os.path.exists(problem_file):
        print(f"Error: Could not find PDDL files at {domain_file} and {problem_file}")
        return

    reader = PDDLReader()
    problem = reader.parse_problem(domain_file, problem_file)
    print(f"Loaded UP Problem: {problem.name}")

    # 2. Extract Initial State for C++
    up_initial_state = problem.initial_values
    cpp_initial_state = convert_up_state_to_cpp(up_initial_state)
    
    print("\nInitial State (C++ Memory Bridge):")
    for p in cpp_initial_state:
        print(f"  - {p.get_name()}")

    # ---------------------------------------------------------
    # Currently, cpp_initial_state is a simple Python List representing one world.
    # When your friend finishes the BeliefState class, they will wrap this list:
    # 
    # from up_cpor.belief_state import BeliefState
    # initial_belief = BeliefState([cpp_initial_state]) 
    #
    # The C++ apply() function is already built to handle sets of predicates!
    # ---------------------------------------------------------
    initial_belief = cpp_initial_state 

    # 3. Setup the Meta-Planner Dependencies
    # (Assuming you have these defined in your up_cpor package)
    try:
        from up_cpor.caching_simulator import CachingSequentialSimulator
        from up_cpor.engine import SDRImpl
        
        print("\nInitializing Simulators and Online Planner...")
        simulator = CachingSequentialSimulator(problem)
        
        # The Online Planner (SDR) acts as the heuristic guide
        online_planner = SDRImpl(problem=problem)
        
        meta_planner = CPORMetaPlanner(simulator, online_planner)
        
        print("\n--- Starting Meta-Planner Search ---")
        # Build the Contingent Plan Graph
        plan_graph = meta_planner.build_plan_graph(initial_belief)
        
        print("\n✅ Plan Graph Generation Complete!")
        print(f"Root Node Action: {plan_graph.action.name if plan_graph else 'Goal already met'}")
        
    except ImportError as e:
        print(f"\n[Notice] Could not import Simulator/SDR dependencies yet: {e}")
        print("The C++ Bridge is successfully holding the initial state in memory.")
        print("Ready for your friend's BeliefState and Simulator integration!")

if __name__ == "__main__":
    main()