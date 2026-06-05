import cpor_engine
from collections import deque
import subprocess
import re
import os
from unified_planning.shortcuts import Compiler, CompilationKind, get_environment
from unified_planning.io import PDDLWriter

def extract_and_map_fluents(node, target_set, fnode_map=None):
    if node.is_fluent_exp():
        args = [a.object().name if a.is_object_exp() else str(a) for a in node.args]
        pred_name = node.fluent().name + "_" + "_".join(args) if args else node.fluent().name
        target_set.add(pred_name)
        if fnode_map is not None:
            fnode_map[pred_name] = node
    elif node.is_and() or node.is_or() or node.is_not():
        for child in node.args:
            extract_and_map_fluents(child, target_set, fnode_map)

class DynamicAction:
    def __init__(self, up_action, fnode_map):
        self.name = up_action.name
        self.cpp_action = cpor_engine.Action(self.name)
        self.is_sensing = "sense" in self.name.lower() or "observe" in self.name.lower()
        
        self.preconditions = set()
        for pre in up_action.preconditions:
            extract_and_map_fluents(pre, self.preconditions, fnode_map)
            
        self.observe = None
        if self.is_sensing:
            name_l = self.name.lower()
            parts = self.name.split("_")
            
            if "clear" in name_l:
                self.observe = f"clear_{parts[-1]}"
            elif "on" in name_l and len(parts) >= 3:
                self.observe = f"on_{parts[-2]}_{parts[-1]}"
            elif "color" in name_l:
                self.observe = f"is_blue_{parts[-1]}"
            else:
                self.observe = f"observed_{parts[-1]}"
            
            if not self.observe:
                self.observe = f"observed_{self.name}"
            
        for effect in up_action.effects:
            fluent_names = set()
            extract_and_map_fluents(effect.fluent, fluent_names, fnode_map)
            for pred_name in fluent_names:
                if effect.value.is_true():
                    self.cpp_action.add_effect(cpor_engine.Predicate(pred_name), True)
                elif effect.value.is_false():
                    self.cpp_action.add_effect(cpor_engine.Predicate(pred_name), False)

    def is_applicable(self, state_fact_names):
        return self.preconditions.issubset(state_fact_names)

    def apply(self, state):
        return self.cpp_action.apply(state)

class NativeSDRImpl:
    def __init__(self, problem, error_on_failed_checks=False):
        get_environment().error_on_failed_checks = False
        self.problem = problem
        self.fnode_map = {}
        self.ff_cache = {} # High-Speed Cache to prevent subprocess freezing!
        
        print("\n[SDR] Dynamically grounding PDDL domain...")
        with Compiler(name="up_grounder") as grounder:
            self.grounded_problem = grounder.compile(problem).problem
            
        self.goal_strings = set()
        for goal in self.grounded_problem.goals:
            extract_and_map_fluents(goal, self.goal_strings, self.fnode_map)
            
        for fnode in self.grounded_problem.initial_values.keys():
            extract_and_map_fluents(fnode, set(), self.fnode_map)
            
        print(f"[SDR] Translating {len(self.grounded_problem.actions)} actions to native C++...")
        self.dynamic_actions = {a.name: DynamicAction(a, self.fnode_map) for a in self.grounded_problem.actions}

        self.sensing_map = {}
        for name, act in self.dynamic_actions.items():
            if act.is_sensing and act.observe:
                self.sensing_map[act.observe] = name

    def is_goal(self, belief_state):
        state_facts = set(p.get_name() for p in belief_state.get_observed())
        return self.goal_strings.issubset(state_facts)

    def get_next_action(self, belief_state):
        current_facts = set(p.get_name() for p in belief_state.get_observed())

        if self.goal_strings.issubset(current_facts):
            return None

        # Targeted Optimism: Only assume things are true if they are in the sensing map!
        optimistic_facts = set(current_facts)
        for hidden_fact in self.sensing_map.keys():
            if f"NOT_{hidden_fact}" not in current_facts:
                optimistic_facts.add(hidden_fact)

        # High-Speed Cache Check
        opt_hash = frozenset(optimistic_facts)
        if opt_hash in self.ff_cache:
            first_action_name = self.ff_cache[opt_hash]
        else:
            first_action_name = self._run_cpp_ff(optimistic_facts)
            self.ff_cache[opt_hash] = first_action_name
        
        # Explorer Fallback
        if not first_action_name or first_action_name not in self.dynamic_actions:
            for name, act in self.dynamic_actions.items():
                if act.is_sensing and act.observe not in current_facts and f"NOT_{act.observe}" not in current_facts:
                    if act.is_applicable(current_facts):
                        return act
            return None

        dyn_act = self.dynamic_actions[first_action_name]

        # SDR Validation Check
        missing_preconditions = dyn_act.preconditions - current_facts
        if not missing_preconditions:
            return dyn_act 

        for missing in missing_preconditions:
            if missing in self.sensing_map:
                sense_act_name = self.sensing_map[missing]
                return self.dynamic_actions[sense_act_name]

        return dyn_act 

    def _run_cpp_ff(self, optimistic_facts):
        classical_prob = self.grounded_problem.clone()
        classical_prob._initial_value.clear()
        
        classical_prob.clear_actions()
        for act in self.grounded_problem.actions:
            if "sense" not in act.name.lower() and "observe" not in act.name.lower():
                classical_prob.add_action(act)
        
        for fact_str in optimistic_facts:
            if fact_str in self.fnode_map:
                classical_prob.set_initial_value(self.fnode_map[fact_str], True)

        w = PDDLWriter(classical_prob)
        w.write_domain("temp_d.pddl")
        w.write_problem("temp_p.pddl")

        try:
            result = subprocess.run(["./ff", "-o", "temp_d.pddl", "-f", "temp_p.pddl"], capture_output=True, text=True)
            if result.returncode < 0: return None

            match = re.search(r'step\s*0:\s*(.*)', result.stdout, re.IGNORECASE)
            if match:
                parts = match.group(1).lower().strip().split()
                act_name = parts[0]
                if len(parts) > 1: act_name += "_" + "_".join(parts[1:])
                return act_name
        except FileNotFoundError:
            pass 
        return None

class CPORMetaPlanner:
    def __init__(self, simulator, online_planner):
        self.online_planner = online_planner
        self.visited_beliefs = {}

    def build_plan_graph(self, belief_state):
        bs_hash = tuple(sorted([p.get_name() for p in belief_state.get_observed()]))
        if bs_hash in self.visited_beliefs: return self.visited_beliefs[bs_hash]
        if self.online_planner.is_goal(belief_state): return {"action": "GOAL REACHED", "children": []}

        action = self.online_planner.get_next_action(belief_state)
        if action is None: return {"action": "DEAD END", "children": []}

        node = {"action": action.name, "children": []}
        self.visited_beliefs[bs_hash] = node

        if action.is_sensing:
            bs_true = cpor_engine.BeliefState()
            for p in action.apply(belief_state.get_observed()): bs_true.add_observed(p)
            bs_true.add_observed(cpor_engine.Predicate(action.observe)) 
            child_t = self.build_plan_graph(bs_true)
            if child_t: node["children"].append((f"Observed: {action.observe} == True", child_t))
            
            bs_false = cpor_engine.BeliefState()
            for p in action.apply(belief_state.get_observed()): bs_false.add_observed(p)
            bs_false.add_observed(cpor_engine.Predicate(f"NOT_{action.observe}")) 
            child_f = self.build_plan_graph(bs_false)
            if child_f: node["children"].append((f"Observed: {action.observe} == False", child_f))
            
        else:
            new_bs = cpor_engine.BeliefState()
            for p in action.apply(belief_state.get_observed()): new_bs.add_observed(p)
            child = self.build_plan_graph(new_bs)
            if child: node["children"].append(("Deterministically", child))

        return node