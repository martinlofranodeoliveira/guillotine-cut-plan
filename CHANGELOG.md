# Changelog

## 0.1.0 — Unreleased

Development version with the contract in `docs/contract.md`.

### Added

- Original deterministic integer rectangle heuristic with per-item rotation, bounded identical stock sheets and full kerf reservation.
- Flat guillotine cut certificates recording placements, cut strips and remaining leaves.
- Independently implemented certificate replay and verified material summaries.
- Bounded strict JSON adapter, canonical plan serialization and explicit-input CLI.
- Offline self-checking human/JSON demo for the synthetic 10×6 example.
- Acceptance regressions, all 81 small ordered rectangle pairs and 96 four-item fragmented certificates with a separate cell-grid oracle, pairwise overlap checks, malformed certificates and parser adversarial cases.
- GCC AddressSanitizer/UndefinedBehaviorSanitizer targets that fail on detected violations, and read-only test CI.
- Additional adversarial certificate checks covering multi-node reference cycles, root dimension mismatches, and boundary-exceeding cut offsets.

### Changed

- Aligned CI to the locally inspected `gcc:14-bookworm` tag and recorded observed image and dependency identities.

### Limitations

- Heuristic only: no optimality or infeasibility proof.
- Integer geometry only: no machine control or physical-use guarantee.
- Container tag is documented with observed base image ID; no immutable registry digest lock is enforced by CI.
