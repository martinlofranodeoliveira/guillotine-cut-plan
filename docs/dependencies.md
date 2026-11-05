# Dependencies and reproducibility

Development version: 0.1.0. Author: Lofrano Tecnologia.

## Core

C++20 standard library only. The intended compiler is GCC 14.3 with GNU Make. There is no runtime model/API service, geometry package, network access, or telemetry. Sanitizer targets use the selected compiler's own AddressSanitizer and UndefinedBehaviorSanitizer runtimes. `-fno-sanitize-recover=all` makes a detected violation terminate unsuccessfully; checks and diagnostics remain enabled.

Primary references:

- https://isocpp.org/std/the-standard
- https://gcc.gnu.org/onlinedocs/libstdc++/manual/
- https://eel.is/c++draft/expr.pre

The implementation is original; these references describe language/library behavior, not copied packing code.

## CLI and tests

Dependency: nlohmann/json **3.11.2**, installed externally, not vendored. The adapter enforces the exact upstream version with a compile-time check. Debian Bookworm package: `nlohmann-json3-dev=3.11.2-2`. Its upstream project is https://github.com/nlohmann/json/tree/v3.11.2 and its license is MIT. The project's LICENSE covers original project code, not an assertion of authorship over this external package.

The local QA environment on 2026-11-05 resolved GCC 14.3.0, GNU Make 4.3 and `nlohmann-json3-dev=3.11.2-2`. Its `/usr/include/nlohmann/json.hpp` has SHA-256:

```text
bbf96a24a8d59ef79467761e2a803537e1476909bb7d6baed23094a1833c088c
```

The header bytes were compared with the official upstream v3.11.2 file (Git blob `18a7c875774527a2e08c5ab72e5564aa50381608`) and matched. The Debian package installs additional modular headers used by `json.hpp`; this single header hash is not a complete package lock. Package version, compile-time version check and this observed header hash identify the dependency used for local QA.

Relevant parser reference: https://json.nlohmann.me/api/basic_json/is_number_integer/ . That predicate includes both signed and unsigned integer types; the adapter instead dispatches on the exact type and checks ranges before conversion. A bounded SAX pass rejects float tokens before building the DOM.

## Container identity

CI selects the official `gcc:14-bookworm` tag. The locally observed base image provides GCC 14.3.0 on Debian Bookworm and has this image ID:

```text
sha256:5e927c284bf55a7dc796262e311a0703344f62f41f5621eb56843111b1d37e15
```

The tag is mutable; CI does not enforce an immutable registry digest lock. These observations do not establish which image a future tag resolves to.

Local tests use an image derived from that GCC environment with the JSON development package installed. Its image ID is `sha256:8233af0bae5a861eff36accff661f69dfcba1fefa0505152052ba2c763b3bc51`. This identifies the local QA runtime, and is not a public installation dependency. Normal and sanitizer suites and the asserting demo run offline once prerequisites are installed. The sanitizer gate fails on its first detected violation.

CI performs package installation, checks out source at a pinned action commit with read-only repository permissions, and has no publish, release or deployment step. Local QA evidence does not assert that hosted CI has executed.
