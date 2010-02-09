#!r6rs

(library (llvm)
  (export llvm-assemble)
  (import (rnrs) (llvm-basic))

  (define (assemble-function mod exp)
    (let* ((name (cadr exp))
	   (ret (caddr exp))
	   (vararg #f)
	   (args (let build-args ((p (cadddr exp)))
		   (cond ((null? p) '())
			 ((eq? (car p) '...) (begin (set! vararg #t) '()))
			 (else (cons (car p) (build-args (cdr p)))))))
	   (func-ty (llvm-type-function ret args vararg)))
      (llvm-function-add! mod name func-ty)))


  (define (llvm-assemble mod exp)
    (cond ((eq? (car exp) 'function)
	   (assemble-function mod exp))))
)
