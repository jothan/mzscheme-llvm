#!r6rs

(library (llvm)
  (export llvm-assemble)
  (import (rnrs) (llvm-basic))

  (define license
"MzScheme to LLVM 2.6 wrapper
Copyright (C) 2010  Jonathan Bastien-Filiatrault

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or\
\n(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.\n")

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
