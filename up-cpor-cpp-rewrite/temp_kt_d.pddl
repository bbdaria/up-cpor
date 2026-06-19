(define (domain kt)
  (:requirements :strips :adl)
  (:predicates (KNbreeze_p4_4) (KNbreeze_p5_3) (KNbreeze_p5_5) (KNpit_at_p5_4) (KNstench_p5_5) (KNwumpus_at_p5_4) (Kbreeze_p4_4) (Kbreeze_p5_3) (Kbreeze_p5_5) (Kpit_at_p5_4) (Kstench_p5_5) (Kwumpus_at_p5_4) (adj_p1_1_p1_2) (adj_p1_1_p2_1) (adj_p1_2_p1_1) (adj_p1_2_p1_3) (adj_p1_2_p2_2) (adj_p1_3_p1_2) (adj_p1_3_p1_4) (adj_p1_3_p2_3) (adj_p1_4_p1_3) (adj_p1_4_p1_5) (adj_p1_4_p2_4) (adj_p1_5_p1_4) (adj_p1_5_p2_5) (adj_p2_1_p1_1) (adj_p2_1_p2_2) (adj_p2_1_p3_1) (adj_p2_2_p1_2) (adj_p2_2_p2_1) (adj_p2_2_p2_3) (adj_p2_2_p3_2) (adj_p2_3_p1_3) (adj_p2_3_p2_2) (adj_p2_3_p2_4) (adj_p2_3_p3_3) (adj_p2_4_p1_4) (adj_p2_4_p2_3) (adj_p2_4_p2_5) (adj_p2_4_p3_4) (adj_p2_5_p1_5) (adj_p2_5_p2_4) (adj_p2_5_p3_5) (adj_p3_1_p2_1) (adj_p3_1_p3_2) (adj_p3_1_p4_1) (adj_p3_2_p2_2) (adj_p3_2_p3_1) (adj_p3_2_p3_3) (adj_p3_2_p4_2) (adj_p3_3_p2_3) (adj_p3_3_p3_2) (adj_p3_3_p3_4) (adj_p3_3_p4_3) (adj_p3_4_p2_4) (adj_p3_4_p3_3) (adj_p3_4_p3_5) (adj_p3_4_p4_4) (adj_p3_5_p2_5) (adj_p3_5_p3_4) (adj_p3_5_p4_5) (adj_p4_1_p3_1) (adj_p4_1_p4_2) (adj_p4_1_p5_1) (adj_p4_2_p3_2) (adj_p4_2_p4_1) (adj_p4_2_p4_3) (adj_p4_2_p5_2) (adj_p4_3_p3_3) (adj_p4_3_p4_2) (adj_p4_3_p4_4) (adj_p4_3_p5_3) (adj_p4_4_p3_4) (adj_p4_4_p4_3) (adj_p4_4_p4_5) (adj_p4_4_p5_4) (adj_p4_5_p3_5) (adj_p4_5_p4_4) (adj_p4_5_p5_5) (adj_p5_1_p4_1) (adj_p5_1_p5_2) (adj_p5_2_p4_2) (adj_p5_2_p5_1) (adj_p5_2_p5_3) (adj_p5_3_p4_3) (adj_p5_3_p5_2) (adj_p5_3_p5_4) (adj_p5_4_p4_4) (adj_p5_4_p5_3) (adj_p5_4_p5_5) (adj_p5_5_p4_5) (adj_p5_5_p5_4) (alive) (at_p1_1) (at_p1_2) (at_p1_3) (at_p1_4) (at_p1_5) (at_p2_1) (at_p2_2) (at_p2_3) (at_p2_4) (at_p2_5) (at_p3_1) (at_p3_2) (at_p3_3) (at_p3_4) (at_p3_5) (at_p4_1) (at_p4_2) (at_p4_3) (at_p4_4) (at_p4_5) (at_p5_1) (at_p5_2) (at_p5_3) (at_p5_4) (at_p5_5) (c0_breeze_p4_4) (c0_breeze_p5_3) (c0_breeze_p5_5) (c0_pit_at_p5_4) (c0_stench_p5_5) (c0_wumpus_at_p5_4) (c1_breeze_p4_4) (c1_breeze_p5_3) (c1_breeze_p5_5) (c1_pit_at_p5_4) (c1_stench_p5_5) (c1_wumpus_at_p5_4) (c2_breeze_p4_4) (c2_breeze_p5_3) (c2_breeze_p5_5) (c2_pit_at_p5_4) (c2_stench_p5_5) (c2_wumpus_at_p5_4) (gold_at_p1_1) (gold_at_p1_2) (gold_at_p1_3) (gold_at_p1_4) (gold_at_p1_5) (gold_at_p2_1) (gold_at_p2_2) (gold_at_p2_3) (gold_at_p2_4) (gold_at_p2_5) (gold_at_p3_1) (gold_at_p3_2) (gold_at_p3_3) (gold_at_p3_4) (gold_at_p3_5) (gold_at_p4_1) (gold_at_p4_2) (gold_at_p4_3) (gold_at_p4_4) (gold_at_p4_5) (gold_at_p5_1) (gold_at_p5_2) (gold_at_p5_3) (gold_at_p5_4) (gold_at_p5_5) (got_the_treasure) (ref_0) (ref_1) (ref_2) (safe_p1_1) (safe_p1_2) (safe_p1_3) (safe_p1_4) (safe_p1_5) (safe_p2_1) (safe_p2_2) (safe_p2_3) (safe_p2_4) (safe_p2_5) (safe_p3_1) (safe_p3_2) (safe_p3_3) (safe_p3_4) (safe_p3_5) (safe_p4_1) (safe_p4_2) (safe_p4_3) (safe_p4_4) (safe_p4_5) (safe_p5_1) (safe_p5_2) (safe_p5_3) (safe_p5_4) (safe_p5_5) (stench_p2_2) (stench_p3_1) (stench_p3_3) (stench_p4_2) (stench_p4_4) (stench_p5_3) (wumpus_at_p3_2) (wumpus_at_p4_3))
  (:action move_p1_5_p1_4
   :parameters ()
   :precondition (and (at_p1_5) (safe_p1_4))
   :effect (and (at_p1_4) (not (at_p1_5))))
  (:action move_p4_3_p4_4
   :parameters ()
   :precondition (and (at_p4_3) (safe_p4_4))
   :effect (and (at_p4_4) (not (at_p4_3))))
  (:action move_p3_2_p2_2
   :parameters ()
   :precondition (and (at_p3_2) (safe_p2_2))
   :effect (and (at_p2_2) (not (at_p3_2))))
  (:action move_p2_3_p2_2
   :parameters ()
   :precondition (and (at_p2_3) (safe_p2_2))
   :effect (and (at_p2_2) (not (at_p2_3))))
  (:action move_p4_3_p5_3
   :parameters ()
   :precondition (and (at_p4_3) (safe_p5_3))
   :effect (and (at_p5_3) (not (at_p4_3))))
  (:action move_p5_2_p5_3
   :parameters ()
   :precondition (and (at_p5_2) (safe_p5_3))
   :effect (and (at_p5_3) (not (at_p5_2))))
  (:action move_p1_1_p1_2
   :parameters ()
   :precondition (and (at_p1_1) (safe_p1_2))
   :effect (and (at_p1_2) (not (at_p1_1))))
  (:action move_p3_3_p2_3
   :parameters ()
   :precondition (and (at_p3_3) (safe_p2_3))
   :effect (and (at_p2_3) (not (at_p3_3))))
  (:action move_p3_4_p3_3
   :parameters ()
   :precondition (and (at_p3_4) (safe_p3_3))
   :effect (and (at_p3_3) (not (at_p3_4))))
  (:action move_p4_2_p4_3
   :parameters ()
   :precondition (and (at_p4_2) (safe_p4_3))
   :effect (and (at_p4_3) (not (at_p4_2))))
  (:action move_p1_4_p2_4
   :parameters ()
   :precondition (and (at_p1_4) (safe_p2_4))
   :effect (and (at_p2_4) (not (at_p1_4))))
  (:action move_p1_4_p1_3
   :parameters ()
   :precondition (and (at_p1_4) (safe_p1_3))
   :effect (and (at_p1_3) (not (at_p1_4))))
  (:action move_p4_5_p4_4
   :parameters ()
   :precondition (and (at_p4_5) (safe_p4_4))
   :effect (and (at_p4_4) (not (at_p4_5))))
  (:action move_p2_1_p1_1
   :parameters ()
   :precondition (and (at_p2_1) (safe_p1_1))
   :effect (and (at_p1_1) (not (at_p2_1))))
  (:action move_p2_2_p2_3
   :parameters ()
   :precondition (and (at_p2_2) (safe_p2_3))
   :effect (and (at_p2_3) (not (at_p2_2))))
  (:action move_p4_2_p5_2
   :parameters ()
   :precondition (and (at_p4_2) (safe_p5_2))
   :effect (and (at_p5_2) (not (at_p4_2))))
  (:action move_p2_4_p1_4
   :parameters ()
   :precondition (and (at_p2_4) (safe_p1_4))
   :effect (and (at_p1_4) (not (at_p2_4))))
  (:action move_p1_3_p2_3
   :parameters ()
   :precondition (and (at_p1_3) (safe_p2_3))
   :effect (and (at_p2_3) (not (at_p1_3))))
  (:action move_p3_2_p4_2
   :parameters ()
   :precondition (and (at_p3_2) (safe_p4_2))
   :effect (and (at_p4_2) (not (at_p3_2))))
  (:action move_p5_3_p4_3
   :parameters ()
   :precondition (and (at_p5_3) (safe_p4_3))
   :effect (and (at_p4_3) (not (at_p5_3))))
  (:action move_p4_5_p5_5
   :parameters ()
   :precondition (and (at_p4_5) (safe_p5_5))
   :effect (and (at_p5_5) (not (at_p4_5))))
  (:action move_p4_1_p3_1
   :parameters ()
   :precondition (and (at_p4_1) (safe_p3_1))
   :effect (and (at_p3_1) (not (at_p4_1))))
  (:action move_p3_4_p2_4
   :parameters ()
   :precondition (and (at_p3_4) (safe_p2_4))
   :effect (and (at_p2_4) (not (at_p3_4))))
  (:action move_p2_5_p1_5
   :parameters ()
   :precondition (and (at_p2_5) (safe_p1_5))
   :effect (and (at_p1_5) (not (at_p2_5))))
  (:action move_p2_2_p1_2
   :parameters ()
   :precondition (and (at_p2_2) (safe_p1_2))
   :effect (and (at_p1_2) (not (at_p2_2))))
  (:action move_p5_1_p4_1
   :parameters ()
   :precondition (and (at_p5_1) (safe_p4_1))
   :effect (and (at_p4_1) (not (at_p5_1))))
  (:action move_p3_1_p2_1
   :parameters ()
   :precondition (and (at_p3_1) (safe_p2_1))
   :effect (and (at_p2_1) (not (at_p3_1))))
  (:action move_p5_3_p5_2
   :parameters ()
   :precondition (and (at_p5_3) (safe_p5_2))
   :effect (and (at_p5_2) (not (at_p5_3))))
  (:action move_p5_3_p5_4
   :parameters ()
   :precondition (and (at_p5_3) (safe_p5_4))
   :effect (and (at_p5_4) (not (at_p5_3))))
  (:action move_p1_3_p1_2
   :parameters ()
   :precondition (and (at_p1_3) (safe_p1_2))
   :effect (and (at_p1_2) (not (at_p1_3))))
  (:action move_p3_1_p4_1
   :parameters ()
   :precondition (and (at_p3_1) (safe_p4_1))
   :effect (and (at_p4_1) (not (at_p3_1))))
  (:action move_p3_3_p3_2
   :parameters ()
   :precondition (and (at_p3_3) (safe_p3_2))
   :effect (and (at_p3_2) (not (at_p3_3))))
  (:action move_p4_3_p4_2
   :parameters ()
   :precondition (and (at_p4_3) (safe_p4_2))
   :effect (and (at_p4_2) (not (at_p4_3))))
  (:action move_p5_2_p4_2
   :parameters ()
   :precondition (and (at_p5_2) (safe_p4_2))
   :effect (and (at_p4_2) (not (at_p5_2))))
  (:action move_p2_5_p3_5
   :parameters ()
   :precondition (and (at_p2_5) (safe_p3_5))
   :effect (and (at_p3_5) (not (at_p2_5))))
  (:action move_p2_2_p3_2
   :parameters ()
   :precondition (and (at_p2_2) (safe_p3_2))
   :effect (and (at_p3_2) (not (at_p2_2))))
  (:action move_p5_4_p4_4
   :parameters ()
   :precondition (and (at_p5_4) (safe_p4_4))
   :effect (and (at_p4_4) (not (at_p5_4))))
  (:action move_p3_4_p4_4
   :parameters ()
   :precondition (and (at_p3_4) (safe_p4_4))
   :effect (and (at_p4_4) (not (at_p3_4))))
  (:action move_p4_4_p4_5
   :parameters ()
   :precondition (and (at_p4_4) (safe_p4_5))
   :effect (and (at_p4_5) (not (at_p4_4))))
  (:action move_p2_1_p2_2
   :parameters ()
   :precondition (and (at_p2_1) (safe_p2_2))
   :effect (and (at_p2_2) (not (at_p2_1))))
  (:action move_p5_4_p5_3
   :parameters ()
   :precondition (and (at_p5_4) (safe_p5_3))
   :effect (and (at_p5_3) (not (at_p5_4))))
  (:action move_p3_5_p2_5
   :parameters ()
   :precondition (and (at_p3_5) (safe_p2_5))
   :effect (and (at_p2_5) (not (at_p3_5))))
  (:action move_p4_1_p5_1
   :parameters ()
   :precondition (and (at_p4_1) (safe_p5_1))
   :effect (and (at_p5_1) (not (at_p4_1))))
  (:action move_p4_2_p4_1
   :parameters ()
   :precondition (and (at_p4_2) (safe_p4_1))
   :effect (and (at_p4_1) (not (at_p4_2))))
  (:action move_p4_4_p4_3
   :parameters ()
   :precondition (and (at_p4_4) (safe_p4_3))
   :effect (and (at_p4_3) (not (at_p4_4))))
  (:action move_p5_4_p5_5
   :parameters ()
   :precondition (and (at_p5_4) (safe_p5_5))
   :effect (and (at_p5_5) (not (at_p5_4))))
  (:action move_p3_2_p3_1
   :parameters ()
   :precondition (and (at_p3_2) (safe_p3_1))
   :effect (and (at_p3_1) (not (at_p3_2))))
  (:action move_p5_5_p4_5
   :parameters ()
   :precondition (and (at_p5_5) (safe_p4_5))
   :effect (and (at_p4_5) (not (at_p5_5))))
  (:action move_p4_4_p5_4
   :parameters ()
   :precondition (and (at_p4_4) (safe_p5_4))
   :effect (and (at_p5_4) (not (at_p4_4))))
  (:action move_p3_5_p4_5
   :parameters ()
   :precondition (and (at_p3_5) (safe_p4_5))
   :effect (and (at_p4_5) (not (at_p3_5))))
  (:action move_p2_3_p3_3
   :parameters ()
   :precondition (and (at_p2_3) (safe_p3_3))
   :effect (and (at_p3_3) (not (at_p2_3))))
  (:action move_p3_2_p3_3
   :parameters ()
   :precondition (and (at_p3_2) (safe_p3_3))
   :effect (and (at_p3_3) (not (at_p3_2))))
  (:action move_p1_2_p1_3
   :parameters ()
   :precondition (and (at_p1_2) (safe_p1_3))
   :effect (and (at_p1_3) (not (at_p1_2))))
  (:action move_p2_2_p2_1
   :parameters ()
   :precondition (and (at_p2_2) (safe_p2_1))
   :effect (and (at_p2_1) (not (at_p2_2))))
  (:action move_p1_2_p1_1
   :parameters ()
   :precondition (and (at_p1_2) (safe_p1_1))
   :effect (and (at_p1_1) (not (at_p1_2))))
  (:action move_p2_4_p3_4
   :parameters ()
   :precondition (and (at_p2_4) (safe_p3_4))
   :effect (and (at_p3_4) (not (at_p2_4))))
  (:action move_p4_5_p3_5
   :parameters ()
   :precondition (and (at_p4_5) (safe_p3_5))
   :effect (and (at_p3_5) (not (at_p4_5))))
  (:action move_p1_3_p1_4
   :parameters ()
   :precondition (and (at_p1_3) (safe_p1_4))
   :effect (and (at_p1_4) (not (at_p1_3))))
  (:action move_p1_4_p1_5
   :parameters ()
   :precondition (and (at_p1_4) (safe_p1_5))
   :effect (and (at_p1_5) (not (at_p1_4))))
  (:action move_p1_1_p2_1
   :parameters ()
   :precondition (and (at_p1_1) (safe_p2_1))
   :effect (and (at_p2_1) (not (at_p1_1))))
  (:action move_p4_3_p3_3
   :parameters ()
   :precondition (and (at_p4_3) (safe_p3_3))
   :effect (and (at_p3_3) (not (at_p4_3))))
  (:action move_p2_5_p2_4
   :parameters ()
   :precondition (and (at_p2_5) (safe_p2_4))
   :effect (and (at_p2_4) (not (at_p2_5))))
  (:action move_p5_5_p5_4
   :parameters ()
   :precondition (and (at_p5_5) (safe_p5_4))
   :effect (and (at_p5_4) (not (at_p5_5))))
  (:action move_p4_4_p3_4
   :parameters ()
   :precondition (and (at_p4_4) (safe_p3_4))
   :effect (and (at_p3_4) (not (at_p4_4))))
  (:action move_p2_3_p2_4
   :parameters ()
   :precondition (and (at_p2_3) (safe_p2_4))
   :effect (and (at_p2_4) (not (at_p2_3))))
  (:action move_p2_3_p1_3
   :parameters ()
   :precondition (and (at_p2_3) (safe_p1_3))
   :effect (and (at_p1_3) (not (at_p2_3))))
  (:action move_p1_5_p2_5
   :parameters ()
   :precondition (and (at_p1_5) (safe_p2_5))
   :effect (and (at_p2_5) (not (at_p1_5))))
  (:action move_p3_3_p4_3
   :parameters ()
   :precondition (and (at_p3_3) (safe_p4_3))
   :effect (and (at_p4_3) (not (at_p3_3))))
  (:action move_p2_4_p2_3
   :parameters ()
   :precondition (and (at_p2_4) (safe_p2_3))
   :effect (and (at_p2_3) (not (at_p2_4))))
  (:action move_p3_1_p3_2
   :parameters ()
   :precondition (and (at_p3_1) (safe_p3_2))
   :effect (and (at_p3_2) (not (at_p3_1))))
  (:action move_p3_5_p3_4
   :parameters ()
   :precondition (and (at_p3_5) (safe_p3_4))
   :effect (and (at_p3_4) (not (at_p3_5))))
  (:action move_p5_2_p5_1
   :parameters ()
   :precondition (and (at_p5_2) (safe_p5_1))
   :effect (and (at_p5_1) (not (at_p5_2))))
  (:action move_p1_2_p2_2
   :parameters ()
   :precondition (and (at_p1_2) (safe_p2_2))
   :effect (and (at_p2_2) (not (at_p1_2))))
  (:action move_p2_1_p3_1
   :parameters ()
   :precondition (and (at_p2_1) (safe_p3_1))
   :effect (and (at_p3_1) (not (at_p2_1))))
  (:action move_p3_4_p3_5
   :parameters ()
   :precondition (and (at_p3_4) (safe_p3_5))
   :effect (and (at_p3_5) (not (at_p3_4))))
  (:action move_p2_4_p2_5
   :parameters ()
   :precondition (and (at_p2_4) (safe_p2_5))
   :effect (and (at_p2_5) (not (at_p2_4))))
  (:action move_p4_2_p3_2
   :parameters ()
   :precondition (and (at_p4_2) (safe_p3_2))
   :effect (and (at_p3_2) (not (at_p4_2))))
  (:action move_p3_3_p3_4
   :parameters ()
   :precondition (and (at_p3_3) (safe_p3_4))
   :effect (and (at_p3_4) (not (at_p3_3))))
  (:action move_p4_1_p4_2
   :parameters ()
   :precondition (and (at_p4_1) (safe_p4_2))
   :effect (and (at_p4_2) (not (at_p4_1))))
  (:action move_p5_1_p5_2
   :parameters ()
   :precondition (and (at_p5_1) (safe_p5_2))
   :effect (and (at_p5_2) (not (at_p5_1))))
  (:action smell_wumpus_p5_5
   :parameters ()
   :precondition (and (at_p5_5))
   :effect (and (Kstench_p5_5) (ref_1)))
  (:action feel_breeze_p4_4
   :parameters ()
   :precondition (and (at_p4_4))
   :effect (and (KNbreeze_p4_4) (ref_1) (ref_2)))
  (:action feel_breeze_p5_3
   :parameters ()
   :precondition (and (at_p5_3))
   :effect (and (KNbreeze_p5_3) (ref_1) (ref_2)))
  (:action feel_breeze_p5_5
   :parameters ()
   :precondition (and (at_p5_5))
   :effect (and (KNbreeze_p5_5) (ref_1) (ref_2)))
  (:action grab_p1_1
   :parameters ()
   :precondition (and (at_p1_1) (gold_at_p1_1))
   :effect (and (got_the_treasure) (not (gold_at_p1_1))))
  (:action grab_p1_2
   :parameters ()
   :precondition (and (at_p1_2) (gold_at_p1_2))
   :effect (and (got_the_treasure) (not (gold_at_p1_2))))
  (:action grab_p1_3
   :parameters ()
   :precondition (and (at_p1_3) (gold_at_p1_3))
   :effect (and (got_the_treasure) (not (gold_at_p1_3))))
  (:action grab_p1_4
   :parameters ()
   :precondition (and (at_p1_4) (gold_at_p1_4))
   :effect (and (got_the_treasure) (not (gold_at_p1_4))))
  (:action grab_p1_5
   :parameters ()
   :precondition (and (at_p1_5) (gold_at_p1_5))
   :effect (and (got_the_treasure) (not (gold_at_p1_5))))
  (:action grab_p2_1
   :parameters ()
   :precondition (and (at_p2_1) (gold_at_p2_1))
   :effect (and (got_the_treasure) (not (gold_at_p2_1))))
  (:action grab_p2_2
   :parameters ()
   :precondition (and (at_p2_2) (gold_at_p2_2))
   :effect (and (got_the_treasure) (not (gold_at_p2_2))))
  (:action grab_p2_3
   :parameters ()
   :precondition (and (at_p2_3) (gold_at_p2_3))
   :effect (and (got_the_treasure) (not (gold_at_p2_3))))
  (:action grab_p2_4
   :parameters ()
   :precondition (and (at_p2_4) (gold_at_p2_4))
   :effect (and (got_the_treasure) (not (gold_at_p2_4))))
  (:action grab_p2_5
   :parameters ()
   :precondition (and (at_p2_5) (gold_at_p2_5))
   :effect (and (got_the_treasure) (not (gold_at_p2_5))))
  (:action grab_p3_1
   :parameters ()
   :precondition (and (at_p3_1) (gold_at_p3_1))
   :effect (and (got_the_treasure) (not (gold_at_p3_1))))
  (:action grab_p3_2
   :parameters ()
   :precondition (and (at_p3_2) (gold_at_p3_2))
   :effect (and (got_the_treasure) (not (gold_at_p3_2))))
  (:action grab_p3_3
   :parameters ()
   :precondition (and (at_p3_3) (gold_at_p3_3))
   :effect (and (got_the_treasure) (not (gold_at_p3_3))))
  (:action grab_p3_4
   :parameters ()
   :precondition (and (at_p3_4) (gold_at_p3_4))
   :effect (and (got_the_treasure) (not (gold_at_p3_4))))
  (:action grab_p3_5
   :parameters ()
   :precondition (and (at_p3_5) (gold_at_p3_5))
   :effect (and (got_the_treasure) (not (gold_at_p3_5))))
  (:action grab_p4_1
   :parameters ()
   :precondition (and (at_p4_1) (gold_at_p4_1))
   :effect (and (got_the_treasure) (not (gold_at_p4_1))))
  (:action grab_p4_2
   :parameters ()
   :precondition (and (at_p4_2) (gold_at_p4_2))
   :effect (and (got_the_treasure) (not (gold_at_p4_2))))
  (:action grab_p4_3
   :parameters ()
   :precondition (and (at_p4_3) (gold_at_p4_3))
   :effect (and (got_the_treasure) (not (gold_at_p4_3))))
  (:action grab_p4_4
   :parameters ()
   :precondition (and (at_p4_4) (gold_at_p4_4))
   :effect (and (got_the_treasure) (not (gold_at_p4_4))))
  (:action grab_p4_5
   :parameters ()
   :precondition (and (at_p4_5) (gold_at_p4_5))
   :effect (and (got_the_treasure) (not (gold_at_p4_5))))
  (:action grab_p5_1
   :parameters ()
   :precondition (and (at_p5_1) (gold_at_p5_1))
   :effect (and (got_the_treasure) (not (gold_at_p5_1))))
  (:action grab_p5_2
   :parameters ()
   :precondition (and (at_p5_2) (gold_at_p5_2))
   :effect (and (got_the_treasure) (not (gold_at_p5_2))))
  (:action grab_p5_3
   :parameters ()
   :precondition (and (at_p5_3) (gold_at_p5_3))
   :effect (and (got_the_treasure) (not (gold_at_p5_3))))
  (:action grab_p5_4
   :parameters ()
   :precondition (and (at_p5_4) (gold_at_p5_4))
   :effect (and (got_the_treasure) (not (gold_at_p5_4))))
  (:action grab_p5_5
   :parameters ()
   :precondition (and (at_p5_5) (gold_at_p5_5))
   :effect (and (got_the_treasure) (not (gold_at_p5_5))))
  (:action merge_pos_breeze_p4_4
   :parameters ()
   :precondition (and (or (c0_breeze_p4_4) (ref_0)) (or (c1_breeze_p4_4) (ref_1)) (or (c2_breeze_p4_4) (ref_2)))
   :effect (and (Kbreeze_p4_4)))
  (:action merge_neg_breeze_p4_4
   :parameters ()
   :precondition (and (or (not (c0_breeze_p4_4)) (ref_0)) (or (not (c1_breeze_p4_4)) (ref_1)) (or (not (c2_breeze_p4_4)) (ref_2)))
   :effect (and (KNbreeze_p4_4)))
  (:action merge_pos_breeze_p5_3
   :parameters ()
   :precondition (and (or (c0_breeze_p5_3) (ref_0)) (or (c1_breeze_p5_3) (ref_1)) (or (c2_breeze_p5_3) (ref_2)))
   :effect (and (Kbreeze_p5_3)))
  (:action merge_neg_breeze_p5_3
   :parameters ()
   :precondition (and (or (not (c0_breeze_p5_3)) (ref_0)) (or (not (c1_breeze_p5_3)) (ref_1)) (or (not (c2_breeze_p5_3)) (ref_2)))
   :effect (and (KNbreeze_p5_3)))
  (:action merge_pos_breeze_p5_5
   :parameters ()
   :precondition (and (or (c0_breeze_p5_5) (ref_0)) (or (c1_breeze_p5_5) (ref_1)) (or (c2_breeze_p5_5) (ref_2)))
   :effect (and (Kbreeze_p5_5)))
  (:action merge_neg_breeze_p5_5
   :parameters ()
   :precondition (and (or (not (c0_breeze_p5_5)) (ref_0)) (or (not (c1_breeze_p5_5)) (ref_1)) (or (not (c2_breeze_p5_5)) (ref_2)))
   :effect (and (KNbreeze_p5_5)))
  (:action merge_pos_pit_at_p5_4
   :parameters ()
   :precondition (and (or (c0_pit_at_p5_4) (ref_0)) (or (c1_pit_at_p5_4) (ref_1)) (or (c2_pit_at_p5_4) (ref_2)))
   :effect (and (Kpit_at_p5_4)))
  (:action merge_neg_pit_at_p5_4
   :parameters ()
   :precondition (and (or (not (c0_pit_at_p5_4)) (ref_0)) (or (not (c1_pit_at_p5_4)) (ref_1)) (or (not (c2_pit_at_p5_4)) (ref_2)))
   :effect (and (KNpit_at_p5_4)))
  (:action merge_pos_stench_p5_5
   :parameters ()
   :precondition (and (or (c0_stench_p5_5) (ref_0)) (or (c1_stench_p5_5) (ref_1)) (or (c2_stench_p5_5) (ref_2)))
   :effect (and (Kstench_p5_5)))
  (:action merge_neg_stench_p5_5
   :parameters ()
   :precondition (and (or (not (c0_stench_p5_5)) (ref_0)) (or (not (c1_stench_p5_5)) (ref_1)) (or (not (c2_stench_p5_5)) (ref_2)))
   :effect (and (KNstench_p5_5)))
  (:action merge_pos_wumpus_at_p5_4
   :parameters ()
   :precondition (and (or (c0_wumpus_at_p5_4) (ref_0)) (or (c1_wumpus_at_p5_4) (ref_1)) (or (c2_wumpus_at_p5_4) (ref_2)))
   :effect (and (Kwumpus_at_p5_4)))
  (:action merge_neg_wumpus_at_p5_4
   :parameters ()
   :precondition (and (or (not (c0_wumpus_at_p5_4)) (ref_0)) (or (not (c1_wumpus_at_p5_4)) (ref_1)) (or (not (c2_wumpus_at_p5_4)) (ref_2)))
   :effect (and (KNwumpus_at_p5_4)))
)
