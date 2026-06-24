# sealisp design: persistent data and lambda calculus

This is a decision record, not a status report. sealisp currently works as a
plain cons-list interpreter — `quote`/`if`/`define`/`lambda`/`begin`, ~20
builtins, everything malloc'd and leaked on purpose. This doc captures a
deliberate rearchitecture of the core around two pillars: persistent,
structurally-shared data structures instead of naive cons chains, and
lambda-calculus-grounded evaluation (currying + lexical addressing) instead of
ad hoc named-variable lookup. It's architecture and rationale only — build
order is a separate, later decision.

## Persistence is the only data model

### One substrate, two branching disciplines

The literal reading of "one structure that's simultaneously a list, a vector,
and a map" doesn't survive contact with the algorithms. A HAMT branches on the
*hash* of a key — there's no notion of "the second element" in a hash-bucketed
structure. A plain index-radix trie (Clojure's vector) is tail-biased: cheap to
extend/shrink at one end, not the other. Lisp's whole idiom is cheap
operations at the *head* (`car`/`cdr`), which neither structure gives you on
its own.

So instead of one polymorphic structure, sealisp gets **one node shape, used
two ways**:

```c
typedef struct {
	uint32_t bitmap;     /* which of the 32 slots are populated */
	uint32_t refcount;   /* node-level refcount, see below */
	uint8_t  kind;       /* NODE_BRANCH | NODE_LEAF | NODE_COLLISION */
	void    *slots[];    /* popcount(bitmap) live entries */
} trie_node_t;
```

As a **vector-trie node**, it branches on 5 bits of an *index* at a time (32
children per level, `log32(n)` deep). As a **HAMT node**, it branches on 5
bits of an FNV-1a *hash* at a time, with collision nodes as the fallback when
two keys hash identically all the way down. Same node, same refcounting, same
path-copying discipline — different bits feed the branch decision. That's the
actual "same substrate" claim, made checkable rather than hand-wavy.

We considered and rejected going further: RRB-trees (relaxed radix balanced
trees, the structure behind Clojure's/Scala's newer `Vector`) give true O(log
n) arbitrary slicing and concatenation, at the cost of per-node size tables and
rebalancing invariants. Finger trees give amortized O(1) push/pop at *both*
ends, but are a structurally unrelated node shape (annotated 2-3 trees), which
would mean maintaining two unrelated trie families instead of one — defeating
the point. Neither is needed: nothing here asks for `subseq` or fast
mid-sequence splicing, only cheap head operations.

### Lists: a head-view over a reversed vector-trie

A list is `(root, shift, offset, length)` — a thin view over a vector-trie
that was built by pushing elements onto its growing end. The mental model to
get right: **logical list order is the reverse of trie push order.** `cons`ing
`1` onto `(2 3)` pushes `1` onto the *end* of the backing trie (because that's
the cheap end), and the view's `offset` always points at "most recently
pushed." Get the direction backwards and you've built a structure that's fast
at the wrong end.

- `cons` = push onto the trie. O(log32 n), often O(1) via the Clojure-style
  editable tail buffer for small/growing sequences.
- `car` = read at `offset`. Same complexity as `cons`.
- `cdr` = **bump `offset` by one, same root.** O(1), full stop — zero trie
  nodes touched, because it doesn't need to be: it's a new view over the exact
  same shared backing structure.

### Vectors and maps

Vectors are the index-radix discipline directly: a trie plus an editable tail,
giving O(log32 n) random access and amortized O(1) push-at-end — this is
Clojure's `PersistentVector` design, unchanged. Maps (and sets, as maps with a
sentinel value) are the HAMT discipline: sparse, bitmap-compressed branching
on hash bits, collision nodes when needed.

### Tradeoffs we're accepting

Stated plainly, not buried as footnotes:

- **`cdr` doesn't free anything eagerly.** A classic cons-cell walk lets the
  allocator reclaim each head cell as soon as you pass it. Here, `cdr`-ing
  through a long list keeps the *entire* backing trie alive as long as any
  view into it — even one only looking at the last element — is alive. This is
  the single biggest behavioral deviation from a normal cons list, and it's
  worth remembering before reaching for "just `cdr` through it" as a way to
  process something memory-conscious.
- **No efficient arbitrary slicing or concatenation** — only "view from offset
  to the end." Fine, since nothing here needs `append` to be fast; if that
  changes later, the upgrade path is relaxing the vector-trie's "every
  internal node is exactly full" invariant toward RRB without changing the
  node shape itself.
- **Small lists are just an array, not a trie at all.** Below ~32 elements —
  i.e. virtually every argument list, every small literal — a list is one
  flat tail-buffer allocation, not a multi-level trie. That's *better* than N
  individual cons-cell allocations, not worse, for the overwhelmingly common
  case.

## Reference counting, not a tracing GC

### Two refcount domains

There are two distinct counters in play, and conflating them is the easiest
way to get this wrong:

- **Node-level refcount** (on `trie_node_t`): how many *parent nodes or value
  views* point at this specific interior node. Because of structural sharing,
  one node can be pointed at by many different logical values at once — `lst`
  and `(cdr lst)` share every node except the view wrapper itself.
- **Value-level refcount** (on `value_t`): how many holders point at *this
  logical value* — a C stack frame, another value's slot, an env binding.

Per the decision made for this design, **every `value_t` gets a refcount,
including bare numbers, symbols, and booleans** — not just the new container
types. That's the consistent reading of "one substrate," but it's worth
naming explicitly: it means every existing value constructor, not only the
new trie-backed ones, needs a refcount field and a retain/release discipline.

Construction only ever **path-copies**: building a new version of a structure
touches the O(log n) nodes from root to the changed leaf, bumping refcounts
on every node it points at; everything off that path is shared by pointer,
untouched. `cdr` doesn't even do that much — it touches zero interior
refcounts, only the new view's hold on the (already-referenced) root.
Teardown, symmetrically, decrements down from a dropped root and only
recurses into children whose count *also* hits zero — a shared subtree with
other live parents simply stops the recursion there. Cost is proportional to
the unshared portion of whatever's being dropped, not to the whole structure.

### Why no cycle collector is needed

Not just an assertion — here's the argument: every constructor here takes
*already-finished* values as input and produces a new parent pointing down at
them. Nothing mutates, so nothing can ever install a pointer from a finished
value's descendant back up to an ancestor (or to anything containing that
ancestor) after the fact. That makes the value graph a DAG by construction
order, full stop — no cycles are constructible.

The one case that looks suspicious is recursive `define`: `(define (f x) ...
(f ...) ...)`. This is *not* a structural cycle. A closure holds a pointer to
*the environment it closed over*, and `f`'s own name is looked up *by name, at
call time*, dynamically, through that environment — not baked in as a static
pointer. Recursion here is repeated dynamic lookup, not a structural
back-edge. (This is exactly why the current mutation-free `env_get` chain-walk
has never needed cycle handling either — same reason, unchanged by this
redesign.)

One sharp edge worth naming up front rather than discovering later: canonical
singletons (the empty list, the empty map) will be referenced from
practically everywhere, so their node-level refcount needs a width that won't
realistically overflow, or an explicit "sticky max, never freed" rule for
them — pick one and say so, rather than leaving it implicit.

## Equality and hashing

`equal?` (deep structural comparison) is the **only** equality sealisp
exposes — no `eq?`, no identity comparison. Once everything is immutable and
shared, "is this the same object" stops being a meaningful question a program
could even usefully ask; "is this the same value" is the only one that
matters, and `equal?` already answers it. This extends to lambdas too: two
closures are `equal?` if their params, body, *and* captured environment are
all deep-equal — not pointer identity, which is what falls out by accident
today. That's a deliberate choice for consistency, not a leftover.

One real subtlety: **map equality has to be order-independent.** Two maps
built by inserting the same keys in different orders can end up with
different physical trie shapes (insertion order can affect collision-node
placement), so map equality can't be "recurse on children in slot order" the
way list/vector equality can (where logical order and trie order coincide).
It needs to be "same size, and every key in A maps to an equal value in B," or
equivalent. This is the kind of bug that passes every obvious test and fails
only on adversarial input, so it's named here explicitly rather than assumed
to fall out for free.

Hashing throughout — map/HAMT branching, and any hashable key (symbols,
strings, numbers) — uses **FNV-1a**: one multiply and one xor per byte, no
dependencies, good enough distribution for this.

## Fully immutable, pure s-expressions

No `set!`. No mutating builtin, anywhere, ever. And no new literal syntax —
vectors, maps, and sets are constructed with ordinary function calls
(`vector`, `hash-map`, …), the same way everything else in sealisp is built.
The language surface doesn't grow; only what's underneath it changes.

## Persistent environments

`env_t`'s bespoke linked-list-of-entries (`env_entry_t`) goes away. An
environment becomes a value built from the *same* trie substrate as
everything else — but in two different shapes, depending on what kind of
scope it is. This split is the concrete payoff of having one substrate with
two branching disciplines, rather than a coincidence:

- **Global/top-level scope** — names introduced by `define` aren't known
  ahead of time, so this stays a persistent **map**: symbol → value, reusing
  the HAMT machinery directly.
- **Local/lambda-parameter scopes** — once lexical addressing (below) has
  resolved a variable reference to a `(depth, index)` pair, a local frame
  doesn't need name-keyed lookup at all. It's just a persistent **vector**,
  indexed positionally. No hashing, no string comparison — "go up `depth`
  frames, then index `index`."

Worth naming plainly: extending an environment (what `env_define` does today
by mutating a list in place) now has to *return a new environment value*
instead. That's a real, visible consequence of immutability — not just an
implementation detail hiding behind an unchanged interface — even though
exactly how that return value gets threaded through evaluation is a
build-order question for later, not this doc.

## Lambda calculus: currying and lexical addressing

Lambda calculus has no primitive data at all — booleans, numbers, pairs are
all just functions (Church encoding). That's elegant, but it's in direct
tension with everything above: refcounting and the trie substrate assume
tagged leaf values like numbers and booleans genuinely exist, and Church
arithmetic is unary under the hood (adding two Church numerals means
composing a function N times) — `(+ 2 2)` becoming a tower of nested closures
fails "correctness and clarity" on its own terms. So sealisp takes two
specific, narrower ideas from lambda calculus instead, both of which mesh
with what's already decided rather than fighting it.

### Currying

Every application is single-argument under the hood. `(lambda (x y) body)`
desugars to `(lambda (x) (lambda (y) body))`; a call `(f x y)` desugars to
`((f x) y)`. The payoff to lead with isn't theoretical purity — it's that
partial application falls out for free, with zero extra builtin machinery:
`(define add5 (f 5))` just works, because `(f 5)` is already a complete,
valid application that happens to return another function.

This applies to user-defined `lambda`s, deliberately *not* to builtins.
`+`, `cons`, and friends stay variadic/n-ary — they aren't "true" lambda
terms, and n-ary arithmetic doesn't gain anything from being curried
(`(((+ a) b) c)` isn't more useful than `(+ a b c)`). This is a named,
deliberate exception, the same way the performance note below is — not an
inconsistency to "fix" later.

### Lexical addressing (de Bruijn indices)

The *surface* form of code — what the reader produces, what `quote` and
macros see — stays fully symbol-named and human-readable. That matters
because "code is data" only means something if quoted code is actually
inspectable; losing variable names entirely would undermine the whole point.

*Internally*, when a lambda is evaluated and closes over its environment,
references inside its body to its own or an enclosing lambda's parameters get
resolved to a `(depth, index)` pair instead of staying name-based. This is
**lexical addressing** — a classic, well-documented technique (SICP §5.5.6
covers exactly this transformation), applied here deliberately, not invented
for this project. It buys two things at once: it eliminates
variable-capture/alpha-equivalence bookkeeping entirely, and it's *exactly*
why local environment frames can be plain vectors instead of maps (above) —
`(depth, index)` literally means "walk up `depth` parent frames, index
`index` into that frame's vector." References that don't resolve statically
(free variables pointing at global, dynamically-`define`d names) stay
ordinary symbol lookups against the persistent global map.

The result is two representations of "the program" living side by side on
purpose: the symbol-named surface AST, and the lexically-addressed internal
form actually evaluated. That split is a designed feature, not an accident to
untangle later — worth remembering before assuming there's only one "real"
AST.

## Code as persistent data

Kept deliberately short — two concrete examples, not a feature list:

1. Once a closure's captured environment is an immutable, independently
   refcounted value rather than a pointer into a mutable structure, an error
   value could hold onto "the environment live at the moment of failure."
   Persistence means that snapshot stays fully inspectable afterward — even
   bindings since shadowed by a later top-level `define` — with no extra
   machinery beyond "the error also holds a reference."
2. Once the reader's output is trie-backed (a direct consequence of the data
   model above, not a separate effort), two versions of "the same form,
   lightly edited" are just two ordinary sealisp values: `equal?`-comparable,
   structurally diffable, sharing whatever didn't change. `eval_cons`'s
   special-form dispatch doesn't need to change at all for this to be true —
   it falls out of the data model rather than requiring bespoke "AST-as-data"
   support.

## Priority: correctness and clarity over speed

Stated plainly, for this subsystem, for now: correctness and clarity come
before performance. This is a direct, deliberate exception to
`docs/style.md`'s "code performance is a priority" rule — named here so it
reads as a scoped, intentional departure for this rearchitecture, not a
contradiction someone (including future us) might "fix" later by quietly
drifting back toward raw cons cells.

## Open risks and questions

- `cdr` not freeing eagerly (above) is the biggest behavioral deviation from
  classic cons lists — a program that walks-and-discards a large list via
  repeated `cdr` keeps the *whole* backing trie alive for the duration.
- Map equality's order-independence needs to be deliberately implemented
  correctly, not assumed to fall out of a generic structural walk.
- Refcount width/overflow on heavily-shared canonical singletons (the empty
  list, the empty map) needs an explicit answer, not an implicit one.
- Uniform refcounting (every value, not just containers) is a bigger change
  than it sounds — every existing constructor needs a refcount field and a
  retain/release discipline, not only the new trie-backed types.
- The surface-AST vs. lexically-addressed-internal-form split (above) is real
  and intentional, but anything that wants to inspect "the code" needs to be
  clear about which of the two representations it means.

## Follow-up: content-addressed storage on disk

Everything above is about *in-memory* persistence — sharing structure across
versions of a value while the process is running. The same properties
(`equal?`, hash-based branching, a trie that only changes along one path per
edit) generalize naturally into a real **on-disk file format**, the same way
Git's object model generalizes "immutable, content-addressed,
structurally-shared trees" from an in-memory idea into a working
version-control storage engine. Saving a new version of a large nested sealisp
value would only need to write the handful of trie nodes that actually
changed; everything else already exists on disk from a previous save and gets
referenced again — the same way an unchanged file in a new Git commit is the
same blob as before, not a fresh copy.

This is a separate, optional subsystem layered on top of the in-memory
design, not a third pillar of the language itself — sealisp is complete as a
language without ever touching disk-backed values. What it would actually
need, beyond what's decided above:

- **A real on-disk encoding for a trie node** — what bytes represent a node's
  bitmap, kind, and slots, with child slots stored as references *by hash* to
  other files rather than inlined, so loading one subtree doesn't require
  loading the whole tree.
- **A content-addressing hash that isn't FNV-1a.** FNV-1a's job above is
  "spread keys across 32 trie branches cheaply" — a much lower bar than
  "uniquely and verifiably identify a blob of bytes on disk." Reusing FNV-1a
  for content addressing risks two different nodes silently colliding onto
  the same filename and corrupting unrelated data; this needs its own,
  explicitly separate decision (a wider/stronger hash), sized to however
  paranoid the threat model actually needs to be.
- **Refs.** A raw content store is just an anonymous pool of hash-named files
  — nothing makes a particular hash "the current save" or "checkpoint 3"
  without a separate, mutable layer of named pointers into the store, the
  same role Git branches/tags play over the commit graph.
- **Garbage collection.** A node becomes orphaned once no ref reachably
  points to it anymore. Reclaiming that space needs a mark-and-sweep
  reachability pass over the on-disk graph — structurally the same problem as
  `git gc`, and real machinery to build, not an afterthought to the write
  path.
- **Format versioning.** If the node encoding ever changes in a later sealisp
  version, old on-disk files need to stay recognizable (or be explicitly
  rejected) rather than silently misparsed — a version byte/magic header per
  file, decided up front rather than retrofitted after the first format
  change already shipped.

Open questions worth deciding explicitly before building any of this, not
after:

- **Threat model for the hash.** A purely local, single-user store has
  different requirements than one that's ever shared or synced between
  machines — decide whether collision resistance against adversarial input
  actually matters here, rather than inheriting an assumption from FNV-1a's
  very different job.
- **Eager vs. lazy loading.** Independently-addressable subtrees make
  partial/lazy loading possible — read only the part of a value you actually
  touch — but `value_t` as designed assumes a value is always fully resident
  in memory once loaded. Real lazy loading means either a new "not yet
  loaded" value variant or a loader indirection; the simpler alternative is
  always loading a value's whole tree eagerly on read, easier to reason
  about but giving up one of the more interesting properties of this idea.
- **Concurrent writers.** Unlike Git, sealisp doesn't have a concurrency story
  yet. Multiple processes writing into the same content store need at least
  the standard write-then-rename trick for atomicity — worth naming now even
  though it isn't solved here.
