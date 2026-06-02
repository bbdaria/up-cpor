import sys

sys.path.append("build") 
import cpor_engine

from unified_planning.io import PDDLReader
from unified_planning.model import SensingAction

def load_pddl_to_cpp(domain_path, problem_path):
    print("Parsing PDDL using unified_planning...")
    reader = PDDLReader()
    problem = reader.parse_problem(domain_path, problem_path)
    
    cpp_context = cpor_engine.ProblemContext()
    cpp_context.problem_name = problem.name

    for action in problem.actions:
        cpp_action = cpor_engine.PlanningAction()
        cpp_action.name = action.name
        cpp_action.is_sensing = isinstance(action, SensingAction)
        cpp_action.preconditions = [str(p) for p in action.preconditions]
        cpp_action.effects = [str(e) for e in action.effects]
        
        if cpp_action.is_sensing:
            cpp_action.observed_fluents = [str(obs) for obs in action.observed_fluents]

        cpp_context.add_action(cpp_action)
        
    print("Hand-off complete. Calling C++ debug print...\n")
    cpp_context.debug_print()

if __name__ == "__main__":
    load_pddl_to_cpp("Tests/blocks2/d.pddl", "Tests/blocks2/p.pddl")