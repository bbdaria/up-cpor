(define (domain grounder_bw-rand-3-domain)
 (:requirements :strips :typing)
 (:types block)
 (:constants
   b2 b1 - block
 )
 (:predicates 
             (clear ?x - block)
             (on-table ?x - block)
             (on ?x - block ?y - block)
             (same ?x - block ?y - block)
 )
 (:action move-b-to-b_b1_b1_b2
  :parameters ()
  :precondition (and (clear b1) (clear b2) (on b1 b1))
  :effect (and (not (clear b2)) (not (on b1 b1)) (on b1 b2) (clear b1)))
 (:action move-b-to-b_b1_b2_b2
  :parameters ()
  :precondition (and (clear b1) (clear b2) (on b1 b2))
  :effect (and (not (clear b2)) (not (on b1 b2)) (on b1 b2) (clear b2)))
 (:action move-b-to-b_b2_b1_b1
  :parameters ()
  :precondition (and (clear b2) (clear b1) (on b2 b1))
  :effect (and (not (clear b1)) (not (on b2 b1)) (on b2 b1) (clear b1)))
 (:action move-b-to-b_b2_b2_b1
  :parameters ()
  :precondition (and (clear b2) (clear b1) (on b2 b2))
  :effect (and (not (clear b1)) (not (on b2 b2)) (on b2 b1) (clear b2)))
 (:action move-to-t_b1_b1
  :parameters ()
  :precondition (and (clear b1) (on b1 b1))
  :effect (and (on-table b1) (not (on b1 b1)) (clear b1)))
 (:action move-to-t_b1_b2
  :parameters ()
  :precondition (and (clear b1) (on b1 b2))
  :effect (and (on-table b1) (not (on b1 b2)) (clear b2)))
 (:action move-to-t_b2_b1
  :parameters ()
  :precondition (and (clear b2) (on b2 b1))
  :effect (and (on-table b2) (not (on b2 b1)) (clear b1)))
 (:action move-to-t_b2_b2
  :parameters ()
  :precondition (and (clear b2) (on b2 b2))
  :effect (and (on-table b2) (not (on b2 b2)) (clear b2)))
 (:action move-t-to-b_b1_b2
  :parameters ()
  :precondition (and (clear b1) (clear b2) (on-table b1))
  :effect (and (not (clear b2)) (not (on-table b1)) (on b1 b2)))
 (:action move-t-to-b_b2_b1
  :parameters ()
  :precondition (and (clear b2) (clear b1) (on-table b2))
  :effect (and (not (clear b1)) (not (on-table b2)) (on b2 b1)))
)
