import cpor_engine
from unified_planning.plans.contingent_plan import ContingentPlanNode

class CPORMetaPlanner:
    """
    Native Python implementation of the CPOR (Contingent Planning via Online Replanning) 
    algorithm, utilizing the C++ AST Engine for rapid state evaluation.
    """
    def __init__(self, simulator, online_planner):
        self.simulator = simulator
        self.online_planner = online_planner
        self.visited_beliefs = {} # Memoization cache for cycle detection

    def build_plan_graph(self, belief_state) -> ContingentPlanNode:
        # 1. Cycle Detection (Memoize graph nodes to handle non-determinism/loops)
        bs_hash = tuple(sorted([p.get_name() for p in belief_state.get_observed()]))
        if bs_hash in self.visited_beliefs:
            return self.visited_beliefs[bs_hash]

        # 2. Check Goal State
        if self.compiled_goal_ast.is_true(belief_state.get_observed()):
            return None # Reached goal, terminate branch

        # 3. Online Planner Query
        # The underlying SDR/Online planner determines the next best step heuristically
        action = self.online_planner.get_next_action(belief_state)
        
        if action is None:
            raise RuntimeError("Dead end reached during contingent plan graph generation.")

        # Create UP ContingentPlanNode for the final output
        node = ContingentPlanNode(action)
        self.visited_beliefs[bs_hash] = node

        # 4. Contingent Branching (Sensing) vs Linear Execution
        if action.is_sensing():
            # Branch True
            bs_true = self.simulator.update_belief(belief_state, action.observe, True)
            child_true = self.build_plan_graph(bs_true)
            if child_true is not None:
                node.add_child({action.observe: True}, child_true)
                
            # Branch False
            bs_false = self.simulator.update_belief(belief_state, action.observe, False)
            child_false = self.build_plan_graph(bs_false)
            if child_false is not None:
                node.add_child({action.observe: False}, child_false)
        else:
            # Deterministic/Linear apply
            # Pass the belief state to C++ Engine to execute the effects
            new_bs = action.apply(belief_state)
            child = self.build_plan_graph(new_bs)
            if child is not None:
                node.add_child({}, child)

        return node