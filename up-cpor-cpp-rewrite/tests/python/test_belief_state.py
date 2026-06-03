import cpor_engine

def test_belief_state():
    print("--- Testing C++ BeliefState Integration ---")
    
    # Initialize the new C++ BeliefState
    belief = cpor_engine.BeliefState()
    
    # Create some facts
    p1 = cpor_engine.Predicate("door_open")
    p2 = cpor_engine.Predicate("has_key")
    
    # Test 1: Add observations
    belief.add_observed(p1)
    belief.add_observed(p2)
    
    # Test 2: Retrieve observations
    observed = belief.get_observed()
    
    print(f"Total Observed Facts: {len(observed)}")
    for p in observed:
        print(f" - {p.get_name()}")
        
    assert len(observed) == 2, "Failed to store or retrieve observed predicates!"
    print("✅ BeliefState C++ mapping is working correctly.")

if __name__ == "__main__":
    test_belief_state()