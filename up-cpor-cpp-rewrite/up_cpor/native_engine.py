import cpor_engine
from collections import deque
from unified_planning.plans.contingent_plan import ContingentPlanNode
from unified_planning.shortcuts import Compiler, CompilationKind, get_environment

class DynamicAction:
    def __init__(self, up_action):
        self.name = up_action.name
        self.cpp_action = cpor_engine.Action(self.name)
        self.is_sensing = "sense" in self.name.lower() or "observe" in self.name.lower()
        
        # 1. Parse Preconditions
        self.preconditions = set()
        for pre in up_action.preconditions:
            self._extract_fluents(pre, self.preconditions)
            
        # 2. Dynamically determine the sensing target
        self.observe = None
        if self.is_sensing:
            target = self.name.split("_")[-1]
            if "color" in self.name: self.observe = f"is_blue_{target}"
            elif "clear" in self.name: self.observe = f"clear_{target}"
            else: self.observe = f"observed_{target}"
            
        # 3. Parse Effects and map to C++
        for effect in up_action.effects:
            fluent_names = set()
            self._extract_fluents(effect.fluent, fluent_names)
            for pred_name in fluent_names:
                if effect.value.is_true():
                    self.cpp_action.add_effect(cpor_engine.Predicate(pred_name), True)
                elif effect.value.is_false():
                    self.cpp_action.add_effect(cpor_engine.Predicate(pred_name), False)

    def _extract_fluents(self, node, target_set):
        """Recursively unpacks logical blocks to find the actual fluents."""
        if node.is_fluent_exp():
            args = [a.object().name if a.is_object_exp() else str(a) for a in node.args]
            pred_name = node.fluent().name + "_" + "_".join(args) if args else node.fluent().name
            target_set.add(pred_name)
        elif node.is_and() or node.is_or():
            for child in node.args:
                self._extract_fluents(child, target_set)

    def is_applicable(self, state_fact_names):
        # The action can only execute if the C++ state contains all preconditions
        return self.preconditions.issubset(state_fact_names)

    def apply(self, state):
        return self.cpp_action.apply(state)


class NativeSDRImpl:
    def __init__(self, problem, error_on_failed_checks=False):
        # 1. Globally disable UP's strict classical-only safety checks!
        get_environment().error_on_failed_checks = False
        
        self.problem = problem
        from unified_planning.engines.sequential_simulator import UPSequentialSimulator
        self.up_simulator = UPSequentialSimulator(problem, error_on_failed_checks=False)
        
        self.goal_strings = set()
        for goal in problem.goals:
            self._extract_fluents(goal, self.goal_strings)
            
        print("\n[SDR] Dynamically grounding PDDL domain...")
        # 2. Request the grounder explicitly by name to bypass factory compatibility checks
        with Compiler(name="up_grounder") as grounder:
            self.grounded_problem = grounder.compile(problem).problem
            
        print(f"[SDR] Translating {len(self.grounded_problem.actions)} actions to native C++...")
        self.dynamic_actions = [DynamicAction(a) for a in self.grounded_problem.actions]

    def _extract_fluents(self, node, target_set):
        if node.is_fluent_exp():
            args = [a.object().name if a.is_object_exp() else str(a) for a in node.args]
            pred_name = node.fluent().name + "_" + "_".join(args) if args else node.fluent().name
            target_set.add(pred_name)
        elif node.is_and() or node.is_or():
            for child in node.args:
                self._extract_fluents(child, target_set)

    def is_goal(self, belief_state):
        state_facts = set(p.get_name() for p in belief_state.get_observed())
        return self.goal_strings.issubset(state_facts)

    def get_next_action(self, belief_state):
        """
        The Replanner: Uses Python to evaluate preconditions and the 
        C++ Engine to rapidly generate and hash future states.
        """
        initial_state = belief_state.get_observed()
        initial_names = frozenset(p.get_name() for p in initial_state)
        
        # BFS Queue: (current_c++_state_list, set_of_fact_names, path_of_actions)
        queue = deque([(initial_state, initial_names, [])])
        visited = set([initial_names])
        
        fallback_sensing = None

        # Bounded BFS
        while queue:
            current_state, current_names, plan = queue.popleft()
            if len(plan) > 6: continue # Keep search shallow and fast

            for dyn_act in self.dynamic_actions:
                if dyn_act.is_applicable(current_names):
                    
                    if dyn_act.is_sensing and fallback_sensing is None:
                        if dyn_act.observe not in current_names:
                            fallback_sensing = dyn_act

                    new_state = dyn_act.apply(current_state)
                    new_names = frozenset(p.get_name() for p in new_state)
                    
                    if new_names not in visited:
                        visited.add(new_names)
                        new_plan = plan + [dyn_act]
                        
                        # Goal Check
                        if self.goal_strings.issubset(new_names):
                            return new_plan[0] # Return the first step toward the goal!
                            
                        queue.append((new_state, new_names, new_plan))
                        
        # If deterministic search fails, we must be missing a fact- trigger sensing
        if fallback_sensing:
            return fallback_sensing
            
        return None

class CPORMetaPlanner:
    def __init__(self, simulator, online_planner):
        self.online_planner = online_planner
        self.visited_beliefs = {}

    def build_plan_graph(self, belief_state):
        bs_hash = tuple(sorted([p.get_name() for p in belief_state.get_observed()]))
        if bs_hash in self.visited_beliefs:
            return self.visited_beliefs[bs_hash]

        if self.online_planner.is_goal(belief_state):
            return {"action": "GOAL REACHED", "children": []}

        action = self.online_planner.get_next_action(belief_state)
        if action is None:
            return {"action": "DEAD END", "children": []}

        node = {"action": action.name, "children": []}
        self.visited_beliefs[bs_hash] = node

        if action.is_sensing:
            # --- Contingent Branching ---
            bs_true = cpor_engine.BeliefState()
            for p in action.apply(belief_state.get_observed()): bs_true.add_observed(p)
            bs_true.add_observed(cpor_engine.Predicate(action.observe)) 
            child_t = self.build_plan_graph(bs_true)
            if child_t: node["children"].append((f"Observed: {action.observe} == True", child_t))
            
            bs_false = cpor_engine.BeliefState()
            for p in action.apply(belief_state.get_observed()): bs_false.add_observed(p)
            child_f = self.build_plan_graph(bs_false)
            if child_f: node["children"].append((f"Observed: {action.observe} == False", child_f))
            
        else:
            # --- Deterministic Apply ---
            new_bs = cpor_engine.BeliefState()
            for p in action.apply(belief_state.get_observed()): new_bs.add_observed(p)
            child = self.build_plan_graph(new_bs)
            if child: node["children"].append(("Deterministically", child))

        return node