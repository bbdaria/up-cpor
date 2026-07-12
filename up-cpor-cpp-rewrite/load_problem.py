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

_LEAF_LABELS = {"GOAL REACHED": " Goal", "DEAD END": " Dead End"}


def write_dot_graph(root, path="output.txt"):
    """Write the contingent plan graph as a Graphviz DOT file (see
    tests/localize5/out.txt for the format).

    Action/leaf nodes are plain boxes labelled "<idx>)<action>"; a sensing
    action branches through two box-shaped observation nodes ("True"/"False").
    The graph is a DAG -- build_plan_graph returns the same dict object for a
    revisited belief -- so nodes are de-duplicated by object identity.
    """
    node_ids = {}          # id(plan_node) -> dot node id
    action_decls = []      # (dot_id, "<idx>)<name>")
    box_decls = []         # (dot_id, "True" | "False")
    edges = []             # (src_dot_id, dst_dot_id)
    counter = [0]          # shared dot-id counter (actions + boxes)
    step = [0]             # CPOR-style step index used in action labels

    def next_id():
        i = counter[0]
        counter[0] += 1
        return i

    def visit(node):
        if node is None:
            return None
        key = id(node)
        if key in node_ids:
            return node_ids[key]

        my_id = next_id()
        node_ids[key] = my_id
        idx = step[0]
        step[0] += 1

        action = node["action"]
        label = _LEAF_LABELS.get(action, action)
        action_decls.append((my_id, f"{idx}){label}"))

        for obs_label, child in node["children"]:
            if obs_label == "Deterministically":
                child_id = visit(child)
                if child_id is not None:
                    edges.append((my_id, child_id))
            else:
                value = "True" if obs_label.endswith("True") else "False"
                box_id = next_id()
                box_decls.append((box_id, value))
                edges.append((my_id, box_id))
                child_id = visit(child)
                if child_id is not None:
                    edges.append((box_id, child_id))

        return my_id

    root_id = visit(root)

    lines = ["digraph contingent_plan {", '\t_nil [style="invis"];']
    for dot_id, label in action_decls:
        lines.append(f'\t{dot_id} [label="{label}"];')
    for dot_id, value in box_decls:
        lines.append(f'\t{dot_id} [label="{value}" ,shape="box"];')
    for src, dst in edges:
        lines.append(f"\t{src} -> {dst};")
    if root_id is not None:
        lines.append(f'\t_nil -> {root_id} [label=""];')
    lines.append("}")

    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return path


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
    domain_file = "../tests/blocks7/d.pddl"
    problem_file = "../tests/blocks7/p.pddl"

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

    dot_path = write_dot_graph(plan_graph, "output.txt")
    print(f"\n📄 Wrote Graphviz DOT graph to {os.path.abspath(dot_path)}")
    print("\n✅ Plan Graph Generation Complete!")

if __name__ == "__main__":
    main()