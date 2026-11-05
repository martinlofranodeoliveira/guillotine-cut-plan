# Contract — development version 0.1.0

This document defines the supported subset, not general packing optimality, general JSON acceptance, or physical manufacturability. Dimensions are caller-defined integer units; areas are square units.

## Native request

Namespace: `guillotine`. Public headers: `types.hpp`, `planner.hpp`, `verify.hpp`, `summary.hpp` under `include/guillotine/`.

- `Unit` is uint64_t. Negative native values are not representable; converted out-of-range values are rejected by bounds checks.
- `Request.stock.width`, `.height` and every item width/height: 1..1,000,000.
- `Request.kerf`: 0..1,000,000, a finite bound equal to the maximum supported dimension. Kerf need not fit the stock when there are no cuts.
- `Request.sheet_limit`: 1..32 identical sheets.
- `Request.items`: 0..128 entries. IDs are unique, 1..64 bytes, consisting only of ASCII letters, digits, `_`, `-`, `.`. ASCII comparison is case-sensitive.
- `Item.allow_rotation`: whether a 90-degree orientation is permitted. Rotation 0 always remains permitted. No other orientation is supported.
- Empty items yield a successful zero-sheet, zero-node plan with all summary totals zero.

`Result<T>` is `std::variant<T, Error>`. `Error` contains `code`, a diagnostic `message`, and bounded `unplaced` IDs. Diagnostic text is not a stable machine interface; code names are. Inputs are not modified; outputs own their storage.

| API | Success | Failure |
| --- | --- | --- |
| `plan(const Request&)` | Complete `Certificate` | `Error`, never a partial certificate |
| `verify_certificate(const Request&, const Certificate&)` | Verified `Summary` | `Error` |
| `summarize(const Request&, const Certificate&)` | Verified `Summary` | `Error`; identical verification policy |

These functions are pure, reentrant, clock-free, and have no persistence or shared mutable state. Calls do not retain references to inputs. Standard allocation can fail; errors are returned where error construction remains possible, without a noexcept guarantee.

## Deterministic planning order

Process descending item area, descending longest side, ascending ASCII ID. For each item evaluate all live leaves and permitted orientations, omitting duplicate 90-degree evaluation for squares. Choose ascending tuple `(leaf_area - item_area, sheet_index, y, x, rotation, node_index)`. Search existing leaves before opening one new sheet. Try normal and permitted rotated dimensions on stock; normal wins an otherwise equal tie.

A candidate requires `w <= leaf.w`, `h <= leaf.h`. Each gap must be either zero or at least the full kerf. Place at the leaf's top-left. Split vertically at item width if a width gap exists; then split the left slab horizontally at item height if a height gap exists. Positive kerf is never truncated. A gap exactly equal to kerf consumes the gap and creates no residual leaf. Zero kerf permits adjacent boundaries and produces zero-area, explicitly recorded kerf strips for actual cuts. Exact fit produces no cuts.

The heuristic continues past failed items. If any fail, UNPLACEABLE contains all unscheduled IDs in processing order, at most 128. This is not proof of infeasibility and includes no usable partial plan.

## Certificate geometry and native representation

Every occupied rectangle and remaining leaf is the half-open region `[x,x+w) × [y,y+h)`. Contact at boundaries is permitted. Every non-kerf node region is positive and contained in stock. Sheets are independent coordinate spaces whose indices follow `Certificate.roots` order.

A certificate has at most 32 roots, no more than the request's sheet limit; at most 544 nodes; and at most 256 cuts. It is a flat forest. Every node must be reachable exactly once from one root. Roots equal `(0,0,stock.width,stock.height)`. Out-of-range references, shared children, cycles, duplicated roots, and unreachable nodes are invalid. Root and node order need not follow planner order for verification.

A node has `region`, `kind`, and these active fields:

- `leaf`: no active fields; records remaining usable stock.
- `placement`: `item_id` and `rotation` (0 or permitted 90). Its region must equal the oriented item dimensions.
- `cut`: `axis`, `offset`, `first`, `second`, `kerf`. `offset` is measured from the region's left edge for vertical cuts, top edge for horizontal cuts.

For a vertical cut in `(x,y,w,h)`, require `0 < offset < w` and `offset + configured_kerf <= w`. First child is `(x,y,offset,h)`. Kerf is `(x+offset,y,configured_kerf,h)`. If `w-offset-configured_kerf > 0`, second child is `(x+offset+configured_kerf,y,w-offset-configured_kerf,h)`; otherwise second must be absent. Horizontal cuts use the analogous y/height partition, across the parent's full width. `first` always exists; absent `second` is native `no_node` or JSON null.

Native inactive fields must have defaults: axis vertical, offset 0, first/second `no_node`, empty kerf `(0,0,0,0)`, empty item ID, rotation 0, except the fields active for that kind. This prevents hidden data being silently dropped by canonical serialization.

Verification accepts both axes at any valid tree level; it does not require reproduction of planner decisions. Cuts and references must partition every sheet exactly. Every requested item appears once, even if two items have identical dimensions. Items may not occupy kerf strips. Terminal placed area + terminal remaining area + all cut kerf area must equal the area of all declared sheets. Accounting uses checked uint64 additions/products.

Summary fields: `sheets`, `placed_items`, `cuts`, `remaining_leaves`, `used_area`, `kerf_area`, `remaining_area`, `stock_area`. Counts and areas are uint64. Remaining area excludes kerf. Verification permits unused positive leaves, including an unused full-stock root, within certificate bounds; the planner opens sheets only for placements.

## Error codes and precedence

- INVALID_REQUEST: invalid native request or request schema.
- UNPLACEABLE: heuristic left items unplaced.
- RESOURCE_LIMIT: bounded certificate/input capacity exceeded, or allocation failure where representable.
- OUT_OF_BOUNDS: any node region is empty or outside stock, or a cut strip is outside stock; checked coordinate overflow is included.
- ITEM_MULTIPLICITY: unknown, repeated, or missing placement ID.
- ROTATION_NOT_ALLOWED: rotation other than 0 or permitted 90.
- CERTIFICATE_INVALID: malformed certificate schema, graph, active fields, cut partition, or placement dimensions.
- ARITHMETIC_OVERFLOW: checked area/accounting arithmetic failed.
- AREA_MISMATCH: material total differs from declared stock total.
- INVALID_JSON: invalid syntax or violation of the bounded token profile.
- IO_ERROR: explicit input could not be opened/read.

Native verifier precedence: validate request, check certificate size/sheet bounds, scan all node/strip bounds, check all placement multiplicities, check rotations and oriented sizes, replay structure/partitions, check reachability and material accounting. Thus moving a valid placement one unit outside stock gives OUT_OF_BOUNDS, and duplicating a valid item ID gives ITEM_MULTIPLICITY even if other structural checks would later fail. JSON syntax/schema checks necessarily occur before native verification. Beyond this staged precedence, first diagnostic within a stage is not a compatibility promise.

## JSON subset and CLI

nlohmann/json 3.11.2 is used only by the adapter. Raw inputs are at most 524288 bytes, nesting depth at most 16 containers, decoded strings and keys at most 64 bytes, and SAX event count at most 32768 (keys and container ends included). Duplicate decoded object keys are rejected at every depth. No comments or trailing data are accepted. Numeric fields accept only unsigned decimal integer tokens `0` or a nonzero digit followed by digits, representable in uint64 and in the field's range. Fractions, exponents, negative tokens including `-0`, booleans, strings, nulls and numeric coercions are not accepted as numbers. `allow_rotation` alone is boolean, and `second` alone can be null. Floats are rejected globally. Inputs must pass the bounded parser before schema decoding.

All fields listed below are mandatory; unknown fields are rejected.

Request shape, as in `examples/kerf.json`:

```json
{"stock":{"width":10,"height":6},"kerf":1,"sheet_limit":2,"items":[{"id":"A","width":6,"height":6,"allow_rotation":false},{"id":"B","width":3,"height":6,"allow_rotation":false}]}
```

Certificate shape for that request:

```json
{"roots":[0],"nodes":[{"kind":"cut","region":{"x":0,"y":0,"w":10,"h":6},"axis":"vertical","offset":6,"first":1,"second":2,"kerf":{"x":6,"y":0,"w":1,"h":6}},{"kind":"placement","region":{"x":0,"y":0,"w":6,"h":6},"item_id":"A","rotation":0},{"kind":"placement","region":{"x":7,"y":0,"w":3,"h":6},"item_id":"B","rotation":0}]}
```

A remaining leaf has exactly `kind:"leaf"` and `region`. Node rectangles use keys `x,y,w,h`. Native geometry checks, not unsafe casts, reject enormous certificate coordinates. Child indices are uint32 below `no_node`; valid references must also be within the actual node array.

Commands:

```text
cutplan plan --input FILE|- [--human]
cutplan verify --input FILE|- [--human]
cutplan demo [--json]
```

Default `plan` output and `verify` input have exactly `request` and `certificate`. Default `verify` and JSON demo output have `ok:true` and `summary`. Human success output is `PASS sheets=N used_area=U kerf_area=K`. Default plan output has no unverified summary metadata. Request items are canonicalized in ascending ASCII-ID order, object keys are sorted lexically, JSON is compact, and output ends with one newline. Permuting valid input items therefore produces byte-identical canonical plan output.

Failures normally emit one stdout JSON object with `ok:false` and `error:{code,message,unplaced}`, exit 2, and no certificate. Usage errors print help to stderr and exit 64. Final process-level allocation, serialization or output errors may instead print a fixed stderr diagnostic and exit 2; no success should be inferred from an incomplete output stream. Exit 0 is necessary for CLI success. The CLI never writes an output file itself, traverses directories, launches commands, or uses the network.
