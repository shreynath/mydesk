# Security

- Treat CAD/BIM/mesh inputs as untrusted. Parsers must fail closed with reports, not crash.
- Project scripts/macros/plugins must be inert until the user grants permissions.
- No telemetry by default. Opt-in diagnostics only.
- Report vulnerabilities privately to the repository maintainer via GitHub Security Advisories when enabled.

Manufacturing toolpaths and structural simulation (future) must display assumptions, units, solver provenance, and validation status — never advertise unvalidated output as safety-certified.
