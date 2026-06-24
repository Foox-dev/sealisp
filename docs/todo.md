# Todo list for sealisp.
Also has some ideas and stuff, yada yada.

## Todo

- Tail-call optimization. eval/apply currently recurse through the real C stack, so something like `(sum-to 100000)` will blow it. Gets harder to retrofit the more special forms pile on top, so do this before the language surface grows much more

- More special forms: `let`, `let*`, `cond`, `and`, `or`

- Variadic / rest args in lambda, e.g. `(lambda (x . rest) ...)`

- More builtins: `length`, `append`, `reverse`, `map`, `filter`, `apply`, string ops (`string-append`, `string-length`, `substring`)

- Load source from a file (`sealisp foo.lisp`), not just the REPL

- Memory management. Everything currently leaks on purpose (malloc, never freed). Fine for a REPL session, not fine for anything long-running. Decided in `docs/design.md`: uniform reference counting over the persistent trie substrate, not a tracing GC — still needs implementing

- Macros

- Better error messages. Reader doesn't track line/column, so errors just say what went wrong, not where

- REPL history / line editing. Raw fgets right now, no arrow-key history, no readline

- Symbol interning. Every symbol read strdup's a fresh string and compares by strcmp; fine for now, wasteful once programs get bigger

- Test suite, similar to caffeine's test_runner setup

## Ideas

- Once `let`/macros exist, start writing parts of the standard library (`map`, `filter`, etc.) in sealisp itself instead of C
- Swap the tree-walking evaluator for a small bytecode VM once the language semantics settle down
- Pattern matching special form
- Module/namespace system, if this ever grows past a toy
