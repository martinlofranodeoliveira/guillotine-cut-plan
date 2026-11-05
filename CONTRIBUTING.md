# Contributing

Guillotine Cut Plan is a development-version 0.1.0 project by Lofrano Tecnologia. Contributions should preserve the precise integer contract and modest dependency surface.

## Local checks

Install GCC 14.3, GNU Make, Bash, and nlohmann/json 3.11.2 as described in the README, then run:

```sh
bash scripts/test.sh
make demo
make demo-json
```

The test script runs both ordinary and address/undefined-behavior sanitizer suites. Do not suppress sanitizer findings or add `-Werror` as a substitute for meaningful tests. Build artifacts belong in `build/` only.

## Change expectations

- Add independent geometric or adversarial regression evidence for geometry changes, not just tests that call the production verifier.
- Keep item ordering and canonical output deterministic under input permutations.
- Check additions/products before use, preserve bounds before growing certificate structures, and return explicit failures without partial success certificates.
- Update `docs/contract.md`, both READMEs where applicable, and the unreleased changelog for observable behavior changes.
- Keep fixtures synthetic; do not submit private client code, real personal data, secrets, fabricated usage claims, or screenshots implying deployment.
- Do not add telemetry, runtime services, release workflows, deployment workflows, or machine-control output.
- Explain any new dependency and its actual pin/provenance. Do not invent image digests or lockfiles.

Use repository issues or pull requests for non-sensitive discussion. For general contact use https://lofranotecnologia.com.br . Security vulnerabilities should not be posted in public issues; follow `SECURITY.md`.

Contributions are made under the repository's MIT license. Author metadata should use the organization name without an email address.
