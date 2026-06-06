import unified_planning as up
import cpor_engine

class ASTConverter:
    def __init__(self):
        # Cache to ensure we reuse the exact same memory for identical predicates
        self.predicate_cache = {}

    def compile_formula(self, up_node: up.model.FNode):
        """
        Recursively translates a unified-planning node into a pure C++ cpor_engine AST.
        """
        # 1. Base Case: It's a grounded predicate (e.g., clear(b1))
        if up_node.is_fluent_exp():
            signature = self._get_signature(up_node)
            
            if signature not in self.predicate_cache:
                self.predicate_cache[signature] = cpor_engine.Predicate(signature)
            
            cpp_predicate = self.predicate_cache[signature]
            return cpor_engine.PredicateNode(cpp_predicate)

        # 2. Compound Case: AND node
        elif up_node.is_and():
            cpp_children = [self.compile_formula(child) for child in up_node.args]
            return cpor_engine.AndNode(cpp_children)

        # 3. Compound Case: OR node
        elif up_node.is_or():
            cpp_children = [self.compile_formula(child) for child in up_node.args]
            return cpor_engine.OrNode(cpp_children)

        # 4. Unary Case: NOT node
        elif up_node.is_not():
            cpp_child = self.compile_formula(up_node.arg(0))
            return cpor_engine.NotNode(cpp_child)
            
        else:
            raise ValueError(f"Unsupported node type in compiler: {up_node}")

    def _get_signature(self, up_node: up.model.FNode) -> str:
        """Helper to convert UP fluent into a single string like 'on_b1_b2'.
        MUST match native_engine.extract_and_map_fluents naming exactly."""
        fluent_name = up_node.fluent().name
        args = [a.object().name if a.is_object_exp() else str(a)
                for a in up_node.args]
        if not args:
            return fluent_name
        return f"{fluent_name}_{'_'.join(args)}"