(define (problem grounder_bw-rand-3-problem)
 (:domain grounder_bw-rand-3-domain)
 (:objects
 )
 (:init
              (same b1 b1)
              (on b2 b1)
              (same b2 b2)
              (on-table b1)
              (clear b2)
 )
 (:goal (and 
           (on b1 b2)
        )
 )
)
