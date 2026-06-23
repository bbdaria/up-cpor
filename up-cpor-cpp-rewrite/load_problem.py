import os
import cpor_engine
from unified_planning.io import PDDLReader
from up_cpor.native_engine import NativeSDRImpl as SDRImpl, CPORMetaPlanner

def convert_up_state_to_cpp(up_state):
    cpp_state = []
    for fluent, value in up_state.items():
        if value.is_true():
            arg_names = [a.object().name if a.is_object_exp() else str(a) for a in fluent.args]
            pred_name = fluent.fluent().name + "_" + "_".join(arg_names) if arg_names else fluent.fluent().name
            cpp_state.append(cpor_engine.Predicate(pred_name))
    return cpp_state

def print_plan_tree(node, depth=0, path=None):
    """Recursively draws the Contingent Plan Graph with cycle detection."""
    if path is None:
        path = set()
        
    if node is None:
        return
        
    indent = "  " * depth
    
    # Cycle detection: If we've seen this exact node in our path, stop!
    if id(node) in path:
        print(f"{indent}▶ [CYCLE DETECTED: Loops back to '{node['action']}']")
        return
        
    print(f"{indent}▶ Execute: {node['action']}")
    
    for obs_str, child in node['children']:
        print(f"{indent}  └─ [{obs_str}]")
        # Pass a copy of the path down to the children
        print_plan_tree(child, depth + 2, path | {id(node)})

def main():
    print("--- CPOR Hybrid Engine Initialization ---")
    domain_file = "../tests/localize5/d.pddl"
    problem_file = "../tests/localize5/p.pddl"
    
    if not os.path.exists(domain_file):
        return print("Error: Could not find PDDL files")

    reader = PDDLReader()
    problem = reader.parse_problem(domain_file, problem_file)
    
    cpp_initial_state = convert_up_state_to_cpp(problem.initial_values)
    initial_true = {p.get_name() for p in cpp_initial_state}

    print("\nInitializing Online Planner (SDR)...")
    online_planner = SDRImpl(problem=problem, problem_file=problem_file)
    meta_planner = CPORMetaPlanner(simulator=None, online_planner=online_planner)

    print("\n--- Generating Native C++ Contingent Plan Graph ---\n")
    plan_graph = meta_planner.build_plan_graph(meta_planner.make_initial_belief(initial_true))
    
    print_plan_tree(plan_graph)
    print("\n✅ Plan Graph Generation Complete!")

if __name__ == "__main__":
    main()