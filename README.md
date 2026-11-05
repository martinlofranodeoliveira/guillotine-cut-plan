# Guillotine Cut Plan

Development version **0.1.0**, by [Lofrano Tecnologia](https://lofranotecnologia.com.br).

An original deterministic C++20 rectangle-packing heuristic with explicit kerf and replayable guillotine certificates. Intended for planning experiments, **not machine control, physical-use instructions, an optimality guarantee, or a proof of infeasibility**. All examples and tests are synthetic. No model, API, telemetry, network, clock, or persistence is used at runtime.

[Português](README.pt-BR.md) · [Contract](docs/contract.md) · [Design](docs/design.md) · [Dependencies](docs/dependencies.md)

## Build, test, demo

Use GCC 14.3, GNU Make, Bash, and nlohmann/json **3.11.2** headers. The library itself needs only the C++20 standard library. On Debian Bookworm, the CLI/test dependency is installed with:

```sh
apt-get update
apt-get install --no-install-recommends nlohmann-json3-dev=3.11.2-2
make
bash scripts/test.sh
make demo
make demo-json
```

The package installation requires an appropriately authorized package-management environment; the program does not install anything. `scripts/test.sh` runs both `make test` and `make sanitize-test`, using the compiler's AddressSanitizer and UndefinedBehaviorSanitizer runtimes without suppressions. Binaries are written only under `build/`. No generated lockfile or vendored dependency is required. See the dependency document for the supplied header checksum and the image-digest limitation.

Human-readable demo output:

```text
PASS sheets=1 used_area=54 kerf_area=6
```

Machine-readable demo output:

```json
{"ok":true,"summary":{"cuts":1,"kerf_area":6,"placed_items":2,"remaining_area":0,"remaining_leaves":0,"sheets":1,"stock_area":60,"used_area":54}}
```

The demo checks exact placements, independently replays the one cut into a cell grid, compares material accounting with the library verifier, and exits successfully only on agreement. No demonstration measurement is a benchmark.

## CLI

```sh
./build/cutplan plan --input examples/kerf.json --human
./build/cutplan plan --input examples/kerf.json > build/plan.json
./build/cutplan verify --input build/plan.json
./build/cutplan verify --input - < build/plan.json
```

Input is only an explicit file or explicit stdin (`--input -`). Default `plan` output is a single canonical JSON object containing `request` and `certificate`, directly usable by `verify`. The CLI verifies before emitting a completed plan. `--human` emits only a verified summary, not a certificate. `verify` emits the same summary envelope shown in the machine-readable demo. Exit codes: 0 success, 2 input/planning/verification/I/O failure, 64 usage error. Failure JSON has `ok:false` and an error with `code`, `message`, and `unplaced`. No partial certificate accompanies a failure.

The supplied fixture places A at `[0,6) × [0,6)`, removes `[6,7) × [0,6)` as kerf, and places B at `[7,10) × [0,6)`. Changing B to width 4 needs two sheets under this heuristic; it cannot consume the kerf strip.

## Library

```cpp
#include <guillotine/planner.hpp>
#include <guillotine/summary.hpp>

const guillotine::Request request{
    {10, 6}, 1, 2,
    {{"A", 6, 6, false}, {"B", 3, 6, false}}
};
auto result = guillotine::plan(request);
if (const auto* certificate = std::get_if<guillotine::Certificate>(&result)) {
    auto checked_summary = guillotine::summarize(request, *certificate);
    // Inspect Summary or Error with std::get_if; never assume success.
} else {
    const auto& error = std::get<guillotine::Error>(result);
    // UNPLACEABLE is a heuristic failure, not an infeasibility proof.
    (void)error;
}
```

Compile library-only programs with `-std=c++20 -Iinclude` and `src/validation.cpp src/planner.cpp src/verify.cpp src/summary.cpp`. Inputs are immutable; calls are reentrant and have no shared mutable state. `verify_certificate` and `summarize` return checked material totals or an explicit error.

## Why this design?

A flat, bounded forest makes cuts inspectable without deep recursive JSON. Integer half-open regions make exact boundary contact unambiguous. A separately written verifier reconstructs full-leaf partitions rather than trusting planner choices. An independent cell-grid test oracle and pairwise overlap checks test geometry beyond agreement between two production functions.

Items are sorted by descending area, descending longest side, then ascending ASCII ID. The planner chooses the least-waste existing leaf, with documented sheet/coordinate/orientation ties, before opening another sheet. It places at the top-left and splits vertically first, then horizontally. It does not backtrack.

## Limits

At most 128 uniquely named items, 32 identical sheets, dimensions 1..1,000,000, and kerf 0..1,000,000, in caller-defined integer units. JSON is restricted to a documented bounded schema; it is not a general JSON processing service. Fractional/exponent numeric tokens, duplicate keys, unknown fields, and excessive nesting are rejected.

This model omits tolerances, blade mechanics, deformation, material grain, clamping, sequencing constraints, and operational safety. Certificates establish only the documented integer geometry, not suitability for cutting physical material. Layouts can use unnecessary sheets, and a sheet-limit failure can occur even when another layout exists. There is no broad standards-compliance or production-readiness claim.

Repository: [martinlofranodeoliveira/guillotine-cut-plan](https://github.com/martinlofranodeoliveira/guillotine-cut-plan). General contact: [official homepage](https://lofranotecnologia.com.br). License: MIT, copyright 2026 Lofrano Tecnologia.
