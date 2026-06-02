import cpor_engine

def print_result(name, success):
    status = "✅ PASSED" if success else "❌ FAILED"
    print(f"{status} | {name}")

def run_all_tests():
    print("--- Starting CPOR Engine Edge Case Tests ---\n")

    # ---------------------------------------------------------
    # 1. THE VANILLA ACTION (Basic Functionality)
    # ---------------------------------------------------------
    try:
        act = cpor_engine.Action("move")
        p_at_A = cpor_engine.Predicate("at_A")
        p_at_B = cpor_engine.Predicate("at_B")
        
        act.add_effect(p_at_A, False) # Delete at_A
        act.add_effect(p_at_B, True)  # Add at_B
        
        initial_state = [p_at_A]
        new_state = act.apply(initial_state)
        
        # Should now have exactly 1 predicate (at_B)
        assert len(new_state) == 1
        print_result("1. Basic Effect Application", True)
    except Exception as e:
        print_result("1. Basic Effect Application", False)
        print(e)

    # ---------------------------------------------------------
    # 2. THE VOID (Empty State Handling)
    # ---------------------------------------------------------
    try:
        # Applying an action to an empty universe
        act_void = cpor_engine.Action("create_matter")
        p_matter = cpor_engine.Predicate("matter_exists")
        act_void.add_effect(p_matter, True)

        empty_state = []
        # is_applicable should default to True if no preconditions exist
        assert act_void.is_applicable(empty_state) == True
        
        # Apply to empty state
        new_state = act_void.apply(empty_state)
        assert len(new_state) == 1
        print_result("2. The Void (Empty State & Preconditions)", True)
    except Exception as e:
        print_result("2. The Void (Empty State & Preconditions)", False)
        print(e)

    # ---------------------------------------------------------
    # 3. THE INDESTRUCTIBLE FACT (Deleting what doesn't exist)
    # ---------------------------------------------------------
    try:
        # Attempting to delete a predicate that isn't in the state
        # In C++, unordered_set.erase() should safely ignore it. If it segfaults, we fail.
        act_del = cpor_engine.Action("delete_ghost")
        p_ghost = cpor_engine.Predicate("ghost")
        p_real = cpor_engine.Predicate("real_object")
        
        act_del.add_effect(p_ghost, False) # Delete effect
        
        state = [p_real]
        new_state = act_del.apply(state)
        
        assert len(new_state) == 1 # Real object remains unharmed
        print_result("3. Indestructible Fact (Safe Deletion)", True)
    except Exception as e:
        print_result("3. Indestructible Fact (Safe Deletion)", False)
        print(e)

    # ---------------------------------------------------------
    # 4. THE HOARDER (Adding existing facts)
    # ---------------------------------------------------------
    try:
        # What if we add an effect that is already true?
        act_add = cpor_engine.Action("add_again")
        p_water = cpor_engine.Predicate("has_water")
        
        act_add.add_effect(p_water, True)
        
        state = [p_water]
        new_state = act_add.apply(state)
        
        # Because C++ uses std::unordered_set, it should handle duplicates natively.
        # Length should still be exactly 1.
        assert len(new_state) == 1
        print_result("4. The Hoarder (Set Duplication Prevention)", True)
    except Exception as e:
        print_result("4. The Hoarder (Set Duplication Prevention)", False)
        print(e)

    # ---------------------------------------------------------
    # 5. THE PARADOX (Simultaneous Add and Delete)
    # ---------------------------------------------------------
    try:
        # PDDL semantics vary, but usually Add overrides Delete.
        # In our C++ apply() logic, the loop processes del_effects FIRST, then add_effects.
        # So if we Delete and Add the same fact, it should SURVIVE.
        act_paradox = cpor_engine.Action("schrodingers_box")
        p_cat = cpor_engine.Predicate("cat_alive")
        
        act_paradox.add_effect(p_cat, False) # Delete it
        act_paradox.add_effect(p_cat, True)  # Add it right back
        
        state = [p_cat]
        new_state = act_paradox.apply(state)
        
        assert len(new_state) == 1 # The cat survives!
        print_result("5. The Paradox (Execution Order Test)", True)
    except Exception as e:
        print_result("5. The Paradox (Execution Order Test)", False)
        print(e)

    # ---------------------------------------------------------
    # 6. THE OBSERVER (Sensing without touching)
    # ---------------------------------------------------------
    try:
        # A contingent sensing action changes NO state, it just sets the observation pointer
        act_sense = cpor_engine.Action("sense_door")
        p_locked = cpor_engine.Predicate("door_locked")
        
        act_sense.set_observe(p_locked)
        
        # Apply should do absolutely nothing
        state = [p_locked]
        new_state = act_sense.apply(state)
        
        assert len(new_state) == 1
        print_result("6. The Observer (Sensing Actions)", True)
    except Exception as e:
        print_result("6. The Observer (Sensing Actions)", False)
        print(e)
        
    print("\n--- Testing Complete ---")

if __name__ == "__main__":
    run_all_tests()