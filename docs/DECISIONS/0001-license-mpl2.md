# ADR 0001: License MPL-2.0

## Status

Accepted (2026-10-10)

## Context

MyDesk needs a license that allows commercial use, requires share-alike for modified source files (file-level copyleft), and remains friendly to linking with Qt and future OCCT.

## Decision

Original MyDesk code is licensed under **Mozilla Public License 2.0**.

## Consequences

- Modified `.cpp`/`.hpp` must remain MPL-2.0.
- Larger works may use other licenses for their own files.
- Dependency licenses must be inventoried in `docs/legal/THIRD_PARTY.md`.
- Revisit if a critical dependency forces a different compatibility posture.
