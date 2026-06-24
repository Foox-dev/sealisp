# sealisp

A small Lisp interpreter.

## Build

```
make build
```

## Run

```
make run
```

Starts a REPL. Ctrl-D to exit, or call `(exit)`.

## Language

Self-evaluating: numbers, strings, `#t`, `#f`, `()`.

Special forms:

```
(quote x)              ; or 'x
(if cond then else)    ; else is optional
(define name expr)
(define (name args) body...)
(lambda (args) body...)
(begin expr...)
```

Builtins:

```
+ - * /
= < > <= >=
cons car cdr list
null? pair? equal? not
display newline
exit
```

## Example

```
sealisp> (define (square x) (* x x))
square
sealisp> (square 7)
49
```
