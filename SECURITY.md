# Security policy

## Scope

Development version 0.1.0 processes potentially malformed bounded JSON requests and guillotine certificates. The parser and verifier aim to reject unsafe numeric values, duplicate keys, excessive input size/depth, malformed graphs and invalid geometry. This is not a claim of audit, certification, production readiness, or physical safety.

The native library has bounded work for accepted configurations, but callers own their input allocations. Absolute memory exhaustion can prevent even constructing an error. The CLI reads only the explicit input stream/file and emits stdout/stderr; it does not execute commands, traverse directories, use credentials, send telemetry, or contact a runtime service.

## Private reports

Please use GitHub private vulnerability reporting:

https://github.com/martinlofranodeoliveira/guillotine-cut-plan/security/advisories/new

Include a minimal synthetic reproducer, affected version/compiler, expected behavior, actual behavior and any sanitizer diagnostic. Do not include secrets, private client material or real personal data. Avoid public disclosure of an unresolved vulnerability through issues or pull requests. No response-time SLA is promised.

For general non-security contact, use the official homepage: https://lofranotecnologia.com.br .

## Operational boundary

Certificates describe a restricted integer model only. They are not machine commands or guidance for physically operating cutting equipment. Defects in that model should be reported as software issues without interpreting a valid certificate as physical-use approval.
