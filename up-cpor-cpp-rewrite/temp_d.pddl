(define (domain grounder_doors_5-domain)
 (:requirements :strips :typing)
 (:types
    object_ - object
    pos - object_
 )
 (:constants
   p1_4 p2_2 p4_2 p1_5 p2_5 p3_2 p3_3 p2_3 p1_1 p4_3 p5_2 p5_3 p3_5 p4_1 p5_4 p1_2 p2_4 p3_4 p1_3 p4_5 p4_4 p3_1 p5_1 p5_5 p2_1 - pos
 )
 (:predicates (adj ?i - object_ ?j - object_) (at_ ?i - object_) (opened ?i - object_))
 (:action move_p1_1_p1_2
  :parameters ()
  :precondition (and (at_ p1_1))
  :effect (and (not (at_ p1_1)) (at_ p1_2)))
 (:action move_p1_2_p1_1
  :parameters ()
  :precondition (and (at_ p1_2))
  :effect (and (not (at_ p1_2)) (at_ p1_1)))
 (:action move_p1_2_p1_3
  :parameters ()
  :precondition (and (at_ p1_2))
  :effect (and (not (at_ p1_2)) (at_ p1_3)))
 (:action move_p1_3_p1_2
  :parameters ()
  :precondition (and (at_ p1_3))
  :effect (and (not (at_ p1_3)) (at_ p1_2)))
 (:action move_p1_3_p1_4
  :parameters ()
  :precondition (and (at_ p1_3))
  :effect (and (not (at_ p1_3)) (at_ p1_4)))
 (:action move_p1_4_p1_3
  :parameters ()
  :precondition (and (at_ p1_4))
  :effect (and (not (at_ p1_4)) (at_ p1_3)))
 (:action move_p1_4_p1_5
  :parameters ()
  :precondition (and (at_ p1_4))
  :effect (and (not (at_ p1_4)) (at_ p1_5)))
 (:action move_p1_5_p1_4
  :parameters ()
  :precondition (and (at_ p1_5))
  :effect (and (not (at_ p1_5)) (at_ p1_4)))
 (:action move_p2_1_p1_1
  :parameters ()
  :precondition (and (at_ p2_1))
  :effect (and (not (at_ p2_1)) (at_ p1_1)))
 (:action move_p2_1_p3_1
  :parameters ()
  :precondition (and (at_ p2_1))
  :effect (and (not (at_ p2_1)) (at_ p3_1)))
 (:action move_p2_2_p1_2
  :parameters ()
  :precondition (and (at_ p2_2))
  :effect (and (not (at_ p2_2)) (at_ p1_2)))
 (:action move_p2_2_p3_2
  :parameters ()
  :precondition (and (at_ p2_2))
  :effect (and (not (at_ p2_2)) (at_ p3_2)))
 (:action move_p2_3_p1_3
  :parameters ()
  :precondition (and (at_ p2_3))
  :effect (and (not (at_ p2_3)) (at_ p1_3)))
 (:action move_p2_3_p3_3
  :parameters ()
  :precondition (and (at_ p2_3))
  :effect (and (not (at_ p2_3)) (at_ p3_3)))
 (:action move_p2_4_p1_4
  :parameters ()
  :precondition (and (at_ p2_4))
  :effect (and (not (at_ p2_4)) (at_ p1_4)))
 (:action move_p2_4_p3_4
  :parameters ()
  :precondition (and (at_ p2_4))
  :effect (and (not (at_ p2_4)) (at_ p3_4)))
 (:action move_p2_5_p1_5
  :parameters ()
  :precondition (and (at_ p2_5))
  :effect (and (not (at_ p2_5)) (at_ p1_5)))
 (:action move_p2_5_p3_5
  :parameters ()
  :precondition (and (at_ p2_5))
  :effect (and (not (at_ p2_5)) (at_ p3_5)))
 (:action move_p3_1_p3_2
  :parameters ()
  :precondition (and (at_ p3_1))
  :effect (and (not (at_ p3_1)) (at_ p3_2)))
 (:action move_p3_2_p3_1
  :parameters ()
  :precondition (and (at_ p3_2))
  :effect (and (not (at_ p3_2)) (at_ p3_1)))
 (:action move_p3_2_p3_3
  :parameters ()
  :precondition (and (at_ p3_2))
  :effect (and (not (at_ p3_2)) (at_ p3_3)))
 (:action move_p3_3_p3_2
  :parameters ()
  :precondition (and (at_ p3_3))
  :effect (and (not (at_ p3_3)) (at_ p3_2)))
 (:action move_p3_3_p3_4
  :parameters ()
  :precondition (and (at_ p3_3))
  :effect (and (not (at_ p3_3)) (at_ p3_4)))
 (:action move_p3_4_p3_3
  :parameters ()
  :precondition (and (at_ p3_4))
  :effect (and (not (at_ p3_4)) (at_ p3_3)))
 (:action move_p3_4_p3_5
  :parameters ()
  :precondition (and (at_ p3_4))
  :effect (and (not (at_ p3_4)) (at_ p3_5)))
 (:action move_p3_5_p3_4
  :parameters ()
  :precondition (and (at_ p3_5))
  :effect (and (not (at_ p3_5)) (at_ p3_4)))
 (:action move_p4_1_p3_1
  :parameters ()
  :precondition (and (at_ p4_1))
  :effect (and (not (at_ p4_1)) (at_ p3_1)))
 (:action move_p4_1_p5_1
  :parameters ()
  :precondition (and (at_ p4_1))
  :effect (and (not (at_ p4_1)) (at_ p5_1)))
 (:action move_p4_2_p3_2
  :parameters ()
  :precondition (and (at_ p4_2))
  :effect (and (not (at_ p4_2)) (at_ p3_2)))
 (:action move_p4_2_p5_2
  :parameters ()
  :precondition (and (at_ p4_2))
  :effect (and (not (at_ p4_2)) (at_ p5_2)))
 (:action move_p4_3_p3_3
  :parameters ()
  :precondition (and (at_ p4_3))
  :effect (and (not (at_ p4_3)) (at_ p3_3)))
 (:action move_p4_3_p5_3
  :parameters ()
  :precondition (and (at_ p4_3))
  :effect (and (not (at_ p4_3)) (at_ p5_3)))
 (:action move_p4_4_p3_4
  :parameters ()
  :precondition (and (at_ p4_4))
  :effect (and (not (at_ p4_4)) (at_ p3_4)))
 (:action move_p4_4_p5_4
  :parameters ()
  :precondition (and (at_ p4_4))
  :effect (and (not (at_ p4_4)) (at_ p5_4)))
 (:action move_p4_5_p3_5
  :parameters ()
  :precondition (and (at_ p4_5))
  :effect (and (not (at_ p4_5)) (at_ p3_5)))
 (:action move_p4_5_p5_5
  :parameters ()
  :precondition (and (at_ p4_5))
  :effect (and (not (at_ p4_5)) (at_ p5_5)))
 (:action move_p5_1_p5_2
  :parameters ()
  :precondition (and (at_ p5_1))
  :effect (and (not (at_ p5_1)) (at_ p5_2)))
 (:action move_p5_2_p5_1
  :parameters ()
  :precondition (and (at_ p5_2))
  :effect (and (not (at_ p5_2)) (at_ p5_1)))
 (:action move_p5_2_p5_3
  :parameters ()
  :precondition (and (at_ p5_2))
  :effect (and (not (at_ p5_2)) (at_ p5_3)))
 (:action move_p5_3_p5_2
  :parameters ()
  :precondition (and (at_ p5_3))
  :effect (and (not (at_ p5_3)) (at_ p5_2)))
 (:action move_p5_3_p5_4
  :parameters ()
  :precondition (and (at_ p5_3))
  :effect (and (not (at_ p5_3)) (at_ p5_4)))
 (:action move_p5_4_p5_3
  :parameters ()
  :precondition (and (at_ p5_4))
  :effect (and (not (at_ p5_4)) (at_ p5_3)))
 (:action move_p5_4_p5_5
  :parameters ()
  :precondition (and (at_ p5_4))
  :effect (and (not (at_ p5_4)) (at_ p5_5)))
 (:action move_p5_5_p5_4
  :parameters ()
  :precondition (and (at_ p5_5))
  :effect (and (not (at_ p5_5)) (at_ p5_4)))
)
