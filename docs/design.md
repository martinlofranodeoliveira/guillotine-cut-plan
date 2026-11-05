# Design — development 0.1.0

## Separation of responsibilities

The core owns no JSON, filesystem, clock, random generator, global cache, or mutable singleton. Public APIs consume const references and return owned values. The CLI is an adapter with bounded JSON parsing and explicit file/stdin selection. Author: Lofrano Tecnologia.

`validation.cpp` checks request bounds and ASCII identifiers. `planner.cpp` implements an original greedy heuristic. `verify.cpp` independently derives geometric partitions from a submitted forest; it never calls the planner, its fit predicate, or its split routine. Only elementary checked arithmetic and request validation are shared. `summarize` delegates to verification so there is no unchecked accounting shortcut.

## Heuristic

Items are ranked by descending checked area, descending longest side, ascending ASCII ID. For every live leaf, each permitted distinct orientation is tested. A fit must reserve the entire kerf for every nonzero dimensional gap. Candidate score is ascending:

1. Leaf area minus item area, including potential kerf in this local waste score.
2. Zero-based sheet number.
3. Leaf y coordinate.
4. Leaf x coordinate.
5. Rotation, 0 before 90.
6. Flat node index, as a final deterministic tie.

Existing leaves are searched before considering a new stock sheet. New stock is opened only if an allowed orientation fits it. There is no lookahead, backtracking, merging of leaves, or lower-bound feasibility claim.

The selected item occupies the leaf's top-left. If its width is smaller than the leaf width, a vertical cut crosses the full leaf height. The left child has item width; the right child begins after the full kerf. Then, if needed, a horizontal cut crosses the full height-reduced target slab's width (the item width), leaving the item above the kerf. More precisely, the horizontal cut is made in the left slab, at the item's height; it does not cross the separate right child. Any zero-sized residual child is omitted. Exact fits need no cuts regardless of configured kerf.

A failed item is recorded and processing continues to determine a bounded list of unscheduled IDs. If any remain, the entire certificate is discarded from the result. This can describe a failed heuristic run, never establish infeasibility.

## Flat guillotine forest

Every sheet has a full-stock root. Each cut consumes exactly one full rectangular leaf into a positive first child, an explicit full-length kerf strip, and optionally a positive second child. Every terminal node is either a placement or a remaining leaf. Node references are indices, not pointers. There are no implicit offcuts, unrecorded slivers, or items inside kerf.

At most two cuts are introduced per placed item. A binary cut creates at most two child nodes. Thus planner bounds are 256 cuts and 32 + 4×128 = 544 total nodes. The parser checks array sizes before decoding nodes; the planner reserves fixed bounded capacities and checks before appending children. The verifier bounds node/cut counts and its iterative work stack. Caller-owned native inputs may already occupy memory before an API is called; those allocations are outside the API's control.

Verification accepts any legal bounded guillotine forest for the request, not just the planner's preferred vertical-first tree. It validates roots, dimensions, rotations, full partitions, all references, unique reachability, multiplicity, and exact checked material conservation. A horizontal-first regression fixture ensures the verifier is not merely reproducing the heuristic. Separate roots represent different identical stock sheets, so equal coordinates on different sheets are not overlap.

Full rectangular partition replay plus unique tree reachability establishes disjoint interiors inductively. Tests additionally paint terminal regions and kerf strips into a separate grid and compare placement/placement and placement/kerf rectangles pairwise. This independent oracle catches errors that agreement between production functions alone could miss.

## Resource and input boundaries

All geometry uses uint64 integers with checked additions and products. Coordinate overflow is classified as OUT_OF_BOUNDS during verifier preflight, before any unsafe arithmetic is used. Material totals are checked independently even though configured maxima fit within uint64.

Raw CLI input is capped at 512 KiB during reading. A string-aware lexical nesting guard runs before JSON parsing. A SAX pass rejects duplicates by decoded key, floats, negative numeric tokens, oversized strings, excessive nesting, and excessive token counts. Only after that pass is a bounded DOM built. Schema decoding rejects booleans in numeric fields, unknown fields, and wrong types. `allow_rotation` is deliberately boolean; boolean numeric coercion is never used. The CLI pins nlohmann/json 3.11.2 and does not vendor its source.

Both core and adapter map ordinary allocation exhaustion to RESOURCE_LIMIT where they can construct an error. As with other allocating C++ APIs, absolute process-wide memory exhaustion can also prevent constructing the error itself. No API is declared noexcept. The CLI catches remaining standard exceptions, avoids echoing attacker-controlled input, and checks output-stream completion. This is not a hard real-time or hostile multi-tenant resource-isolation system.

## Determinism and limitations

Canonical JSON uses sorted object keys, compact UTF-8, and one final newline. Request items are serialized in ASCII-ID order; certificate nodes are emitted in their deterministic creation order. No dependence on hash iteration, locale, timestamps, or input item ordering is present.

The integer model is deliberately small. It says nothing about machine operation, physical tolerances, material behavior, safe sequencing, or global packing optimality. A valid certificate is evidence of only the contract's geometric partition.
